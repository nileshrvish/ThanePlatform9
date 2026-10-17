#include "MCPServer.h"
#include "Engine/Blueprint.h"
#include "Animation/AnimBlueprint.h"
#include "WidgetBlueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Blueprint/BlueprintExtension.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node_Event.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_CallFunction.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/CompilerResultsLog.h"
#include "BlueprintEditorLibrary.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_SetFieldsInStruct.h"
#include "K2Node_BreakStruct.h"
#include "StructUtils/InstancedStruct.h"
#include "K2Node_MakeStruct.h"
#include "K2Node_SwitchEnum.h"
#include "K2Node_CastByteToEnum.h"
#include "K2Node_Select.h"
#include "K2Node_Message.h"
#include "EdGraphSchema_K2.h"
#include "StructUtils/UserDefinedStruct.h"
#include "Engine/UserDefinedEnum.h"
#include "Engine/TimelineTemplate.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "ObjectTools.h"
#include "UserDefinedStructure/UserDefinedStructEditorData.h"
#include "Kismet2/StructureEditorUtils.h"
#include "Curves/CurveFloat.h"
#include "Curves/CurveVector.h"
#include "Curves/CurveLinearColor.h"
#include "Curves/RichCurve.h"
#include "GameFramework/Actor.h"
#include "EngineUtils.h"
#include "Async/Async.h"
#include "UObject/SavePackage.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "EnhancedInputComponent.h"
#include "Components/ActorComponent.h"
#include "Kismet2/ComponentEditorUtils.h"

FString FMCPServer::HandleConnectNodes(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("blueprint_path")) ||
		!Params->HasField(TEXT("source_node_id")) || !Params->HasField(TEXT("source_pin")) ||
		!Params->HasField(TEXT("target_node_id")) || !Params->HasField(TEXT("target_pin")))
	{
		return MakeError(TEXT("Missing required parameters: blueprint_path, source_node_id, source_pin, target_node_id, target_pin"));
	}

	FString BlueprintPath = Params->GetStringField(TEXT("blueprint_path"));
	FString SourceNodeId = Params->GetStringField(TEXT("source_node_id"));
	FString SourcePinName = Params->GetStringField(TEXT("source_pin"));
	FString TargetNodeId = Params->GetStringField(TEXT("target_node_id"));
	FString TargetPinName = Params->GetStringField(TEXT("target_pin"));
	FString GraphName = Params->HasField(TEXT("graph_name")) ? Params->GetStringField(TEXT("graph_name")) : TEXT("EventGraph");

	UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Failed to load blueprint: %s"), *BlueprintPath));
	}

	// Find the target graph
	UEdGraph* TargetGraph = nullptr;
	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		if (Graph && Graph->GetName() == GraphName)
		{
			TargetGraph = Graph;
			break;
		}
	}

	// Also check function graphs if not found in ubergraph
	if (!TargetGraph)
	{
		for (UEdGraph* Graph : Blueprint->FunctionGraphs)
		{
			if (Graph && Graph->GetName() == GraphName)
			{
				TargetGraph = Graph;
				break;
			}
		}
	}

	// Also check interface implementation graphs
	if (!TargetGraph)
	{
		for (const FBPInterfaceDescription& Interface : Blueprint->ImplementedInterfaces)
		{
			for (UEdGraph* Graph : Interface.Graphs)
			{
				if (Graph && Graph->GetName() == GraphName)
				{
					TargetGraph = Graph;
					break;
				}
			}
			if (TargetGraph) break;
		}
	}

	if (!TargetGraph)
	{
		return MakeError(FString::Printf(TEXT("Graph not found: %s"), *GraphName));
	}

	// Find source and target nodes by GUID
	UEdGraphNode* SourceNode = nullptr;
	UEdGraphNode* TargetNode = nullptr;
	FGuid SourceGuid, TargetGuid;

	if (!FGuid::Parse(SourceNodeId, SourceGuid))
	{
		return MakeError(FString::Printf(TEXT("Invalid source node ID format: %s"), *SourceNodeId));
	}
	if (!FGuid::Parse(TargetNodeId, TargetGuid))
	{
		return MakeError(FString::Printf(TEXT("Invalid target node ID format: %s"), *TargetNodeId));
	}

	for (UEdGraphNode* Node : TargetGraph->Nodes)
	{
		if (Node)
		{
			if (Node->NodeGuid == SourceGuid)
			{
				SourceNode = Node;
			}
			if (Node->NodeGuid == TargetGuid)
			{
				TargetNode = Node;
			}
		}
	}

	if (!SourceNode)
	{
		return MakeError(FString::Printf(TEXT("Source node not found with ID: %s"), *SourceNodeId));
	}
	if (!TargetNode)
	{
		return MakeError(FString::Printf(TEXT("Target node not found with ID: %s"), *TargetNodeId));
	}

	// Find source and target pins
	UEdGraphPin* SourcePin = nullptr;
	UEdGraphPin* TargetPin = nullptr;

	for (UEdGraphPin* Pin : SourceNode->Pins)
	{
		if (Pin && Pin->PinName.ToString() == SourcePinName && Pin->Direction == EGPD_Output)
		{
			SourcePin = Pin;
			break;
		}
	}

	for (UEdGraphPin* Pin : TargetNode->Pins)
	{
		if (Pin && Pin->PinName.ToString() == TargetPinName && Pin->Direction == EGPD_Input)
		{
			TargetPin = Pin;
			break;
		}
	}

	if (!SourcePin)
	{
		// List available output pins for debugging
		TArray<FString> AvailablePins;
		for (UEdGraphPin* Pin : SourceNode->Pins)
		{
			if (Pin && Pin->Direction == EGPD_Output)
			{
				AvailablePins.Add(Pin->PinName.ToString());
			}
		}
		return MakeError(FString::Printf(TEXT("Source output pin not found: %s. Available output pins: %s"),
			*SourcePinName, *FString::Join(AvailablePins, TEXT(", "))));
	}

	if (!TargetPin)
	{
		// List available input pins for debugging
		TArray<FString> AvailablePins;
		for (UEdGraphPin* Pin : TargetNode->Pins)
		{
			if (Pin && Pin->Direction == EGPD_Input)
			{
				AvailablePins.Add(Pin->PinName.ToString());
			}
		}
		return MakeError(FString::Printf(TEXT("Target input pin not found: %s. Available input pins: %s"),
			*TargetPinName, *FString::Join(AvailablePins, TEXT(", "))));
	}

	// Check if connection can be made using the schema
	const UEdGraphSchema* Schema = TargetGraph->GetSchema();
	if (Schema)
	{
		FPinConnectionResponse Response = Schema->CanCreateConnection(SourcePin, TargetPin);
		if (Response.Response == CONNECT_RESPONSE_DISALLOW)
		{
			return MakeError(FString::Printf(TEXT("Cannot create connection: %s"), *Response.Message.ToString()));
		}
	}

	// Check if already connected
	if (SourcePin->LinkedTo.Contains(TargetPin))
	{
		TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("message"), TEXT("Pins are already connected"));
		Data->SetBoolField(TEXT("already_connected"), true);
		return MakeResponse(true, Data);
	}

	// Create the connection
	bool bModified = Schema->TryCreateConnection(SourcePin, TargetPin);

	if (!bModified)
	{
		// Fallback to direct link
		SourcePin->MakeLinkTo(TargetPin);
		bModified = true;
	}

	// Mark blueprint as modified
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

	// Compile the blueprint
	FKismetEditorUtilities::CompileBlueprint(Blueprint);

	// Check for compilation errors
	bool bCompiledSuccessfully = (Blueprint->Status != BS_Error);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("Nodes connected successfully"));
	Data->SetStringField(TEXT("source_node"), SourceNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
	Data->SetStringField(TEXT("source_pin"), SourcePinName);
	Data->SetStringField(TEXT("target_node"), TargetNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
	Data->SetStringField(TEXT("target_pin"), TargetPinName);
	Data->SetBoolField(TEXT("compiled_successfully"), bCompiledSuccessfully);

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleDisconnectPin(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("blueprint_path")) ||
		!Params->HasField(TEXT("node_id")) || !Params->HasField(TEXT("pin_name")))
	{
		return MakeError(TEXT("Missing required parameters: blueprint_path, node_id, pin_name"));
	}

	FString BlueprintPath = Params->GetStringField(TEXT("blueprint_path"));
	FString NodeId = Params->GetStringField(TEXT("node_id"));
	FString PinName = Params->GetStringField(TEXT("pin_name"));
	FString GraphName = Params->HasField(TEXT("graph_name")) ? Params->GetStringField(TEXT("graph_name")) : TEXT("EventGraph");

	UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Failed to load blueprint: %s"), *BlueprintPath));
	}

	// Find the target graph
	UEdGraph* TargetGraph = nullptr;
	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		if (Graph && Graph->GetName() == GraphName)
		{
			TargetGraph = Graph;
			break;
		}
	}

	if (!TargetGraph)
	{
		for (UEdGraph* Graph : Blueprint->FunctionGraphs)
		{
			if (Graph && Graph->GetName() == GraphName)
			{
				TargetGraph = Graph;
				break;
			}
		}
	}

	// Also check interface implementation graphs
	if (!TargetGraph)
	{
		for (const FBPInterfaceDescription& Interface : Blueprint->ImplementedInterfaces)
		{
			for (UEdGraph* Graph : Interface.Graphs)
			{
				if (Graph && Graph->GetName() == GraphName)
				{
					TargetGraph = Graph;
					break;
				}
			}
			if (TargetGraph) break;
		}
	}

	if (!TargetGraph)
	{
		return MakeError(FString::Printf(TEXT("Graph not found: %s"), *GraphName));
	}

	// Find node by GUID
	FGuid NodeGuid;
	if (!FGuid::Parse(NodeId, NodeGuid))
	{
		return MakeError(FString::Printf(TEXT("Invalid node ID format: %s"), *NodeId));
	}

	UEdGraphNode* Node = nullptr;
	for (UEdGraphNode* N : TargetGraph->Nodes)
	{
		if (N && N->NodeGuid == NodeGuid)
		{
			Node = N;
			break;
		}
	}

	if (!Node)
	{
		return MakeError(FString::Printf(TEXT("Node not found with ID: %s"), *NodeId));
	}

	// Find pin
	UEdGraphPin* Pin = nullptr;
	for (UEdGraphPin* P : Node->Pins)
	{
		if (P && P->PinName.ToString() == PinName)
		{
			Pin = P;
			break;
		}
	}

	if (!Pin)
	{
		TArray<FString> AvailablePins;
		for (UEdGraphPin* P : Node->Pins)
		{
			if (P)
			{
				AvailablePins.Add(P->PinName.ToString());
			}
		}
		return MakeError(FString::Printf(TEXT("Pin not found: %s. Available pins: %s"),
			*PinName, *FString::Join(AvailablePins, TEXT(", "))));
	}

	// Break all links
	int32 LinksCount = Pin->LinkedTo.Num();
	Pin->BreakAllPinLinks();

	// Mark blueprint as modified
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

	// Compile the blueprint
	FKismetEditorUtilities::CompileBlueprint(Blueprint);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Disconnected %d links from pin"), LinksCount));
	Data->SetStringField(TEXT("node"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
	Data->SetStringField(TEXT("pin"), PinName);
	Data->SetNumberField(TEXT("links_broken"), LinksCount);

	return MakeResponse(true, Data);
}
