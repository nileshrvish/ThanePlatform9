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
#include "EdGraphSchema_K2.h"
#include "UObject/UObjectGlobals.h"
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

FString FMCPServer::HandleDeleteNode(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("blueprint_path")) ||
		!Params->HasField(TEXT("node_id")))
	{
		return MakeError(TEXT("Missing required parameters: blueprint_path, node_id"));
	}

	FString BlueprintPath = Params->GetStringField(TEXT("blueprint_path"));
	FString NodeId = Params->GetStringField(TEXT("node_id"));
	// Optional narrowing filter. Empty/absent means "search every graph in the blueprint,
	// including nested ones" (AnimGraph state/transition sub-graphs, macro graphs, etc.) -
	// the node GUID is unique blueprint-wide, so a graph name is not required to disambiguate.
	FString GraphName = Params->HasField(TEXT("graph_name")) ? Params->GetStringField(TEXT("graph_name")) : TEXT("");

	UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Failed to load blueprint: %s"), *BlueprintPath));
	}

	// Collect all graphs, including nested ones (AnimGraph state machines nest a graph per
	// state and per transition rule; these are NOT reachable via UbergraphPages/FunctionGraphs
	// alone, only via each node's GetSubGraphs()).
	TArray<UEdGraph*> AllGraphs;
	Blueprint->GetAllGraphs(AllGraphs);
	TArray<UEdGraph*> GraphsToProcess = AllGraphs;
	while (GraphsToProcess.Num() > 0)
	{
		UEdGraph* G = GraphsToProcess.Pop();
		if (!G) continue;
		for (UEdGraphNode* N : G->Nodes)
		{
			if (!N) continue;
			for (UEdGraph* Sub : N->GetSubGraphs())
			{
				if (Sub && !AllGraphs.Contains(Sub))
				{
					AllGraphs.Add(Sub);
					GraphsToProcess.Add(Sub);
				}
			}
		}
	}

	// Find the node by GUID across every collected graph (optionally narrowed by graph_name)
	UEdGraph* TargetGraph = nullptr;
	UEdGraphNode* NodeToDelete = nullptr;
	FString NodeTitle;
	for (UEdGraph* Graph : AllGraphs)
	{
		if (!Graph) continue;
		if (!GraphName.IsEmpty() && Graph->GetName() != GraphName) continue;

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node) continue;
			FString NodeGuidStr = Node->NodeGuid.ToString(EGuidFormats::DigitsWithHyphensLower);
			FString NodeGuidStrUpper = Node->NodeGuid.ToString(EGuidFormats::Digits);
			if (NodeGuidStr.Equals(NodeId, ESearchCase::IgnoreCase) ||
				NodeGuidStrUpper.Equals(NodeId, ESearchCase::IgnoreCase))
			{
				TargetGraph = Graph;
				NodeToDelete = Node;
				NodeTitle = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
				break;
			}
		}
		if (NodeToDelete) break;
	}

	if (!NodeToDelete)
	{
		return MakeError(FString::Printf(TEXT("Node not found with ID: %s%s"), *NodeId,
			GraphName.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" (narrowed to graph '%s')"), *GraphName)));
	}

	// Remove the node via FBlueprintEditorUtils, not the graph's raw RemoveNode(): the raw
	// call just drops the node from the array without breaking pin links or notifying owning
	// blueprint extensions (e.g. AnimBlueprintExtension_StateMachine caches a TargetRootNode
	// per state/transition graph). Skipping that cleanup leaves dangling state that can crash
	// a later compile with an internal engine assertion, particularly for nodes inside
	// AnimGraph state machine sub-graphs.
	FBlueprintEditorUtils::RemoveNode(Blueprint, NodeToDelete, /*bDontRecompile=*/true);

	// Mark blueprint as modified
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

	// Compile the blueprint
	FKismetEditorUtilities::CompileBlueprint(Blueprint);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("Node deleted successfully"));
	Data->SetStringField(TEXT("node_id"), NodeId);
	Data->SetStringField(TEXT("node_title"), NodeTitle);

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleReconstructNode(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("blueprint_path")))
	{
		return MakeError(TEXT("Missing 'blueprint_path' parameter"));
	}

	const FString BlueprintPath = Params->GetStringField(TEXT("blueprint_path"));
	const FString NodeGuidStr = Params->HasField(TEXT("node_guid")) ? Params->GetStringField(TEXT("node_guid")) : TEXT("");
	const FString VariableFilter = Params->HasField(TEXT("variable_name")) ? Params->GetStringField(TEXT("variable_name")) : TEXT("");

	if (NodeGuidStr.IsEmpty() && VariableFilter.IsEmpty())
	{
		return MakeError(TEXT("Must provide 'node_guid' or 'variable_name' parameter"));
	}

	UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
	if (!Blueprint) return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));

	// Collect all graphs including sub-graphs
	TArray<UEdGraph*> AllGraphs;
	Blueprint->GetAllGraphs(AllGraphs);
	TArray<UEdGraph*> ToProcess = AllGraphs;
	while (ToProcess.Num() > 0)
	{
		UEdGraph* G = ToProcess.Pop();
		for (UEdGraphNode* N : G->Nodes)
		{
			if (!N) continue;
			for (UEdGraph* Sub : N->GetSubGraphs())
			{
				if (Sub && !AllGraphs.Contains(Sub))
				{
					AllGraphs.Add(Sub);
					ToProcess.Add(Sub);
				}
			}
		}
	}

	// Find nodes to reconstruct
	TArray<UEdGraphNode*> NodesToReconstruct;
	if (!NodeGuidStr.IsEmpty())
	{
		FGuid NodeGuid;
		if (!FGuid::Parse(NodeGuidStr, NodeGuid))
		{
			return MakeError(FString::Printf(TEXT("Invalid GUID: %s"), *NodeGuidStr));
		}
		for (UEdGraph* Graph : AllGraphs)
		{
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (Node && Node->NodeGuid == NodeGuid)
				{
					NodesToReconstruct.Add(Node);
					break;
				}
			}
			if (NodesToReconstruct.Num() > 0) break;
		}
	}
	else if (!VariableFilter.IsEmpty())
	{
		// Find all VariableGet/VariableSet nodes for the specified variable that have sub-pins (expanded)
		for (UEdGraph* Graph : AllGraphs)
		{
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (!Node) continue;
				// Check if this is a VariableGet or VariableSet node
				UK2Node_VariableGet* VarGet = Cast<UK2Node_VariableGet>(Node);
				UK2Node_VariableSet* VarSet = Cast<UK2Node_VariableSet>(Node);
				if (!VarGet && !VarSet) continue;

				// Check variable name
				FName VarName = VarGet ? VarGet->GetVarName() : VarSet->GetVarName();
				if (VarName.ToString() != VariableFilter) continue;

				// Check if any pin has sub-pins (expanded struct)
				bool bHasSubPins = false;
				for (UEdGraphPin* Pin : Node->Pins)
				{
					if (Pin && Pin->SubPins.Num() > 0)
					{
						bHasSubPins = true;
						break;
					}
				}
				if (bHasSubPins)
				{
					NodesToReconstruct.Add(Node);
				}
			}
		}
	}

	if (NodesToReconstruct.Num() == 0)
	{
		return MakeError(TEXT("No matching nodes found"));
	}

	// Reconstruct each node
	int32 TotalRestored = 0;
	int32 TotalFailed = 0;
	TArray<TSharedPtr<FJsonValue>> NodeReports;

	struct FSavedConnection
	{
		FName PinName;
		EEdGraphPinDirection Direction;
		FGuid RemoteNodeGuid;
		FName RemotePinName;
	};

	for (UEdGraphNode* TargetNode : NodesToReconstruct)
	{
		TArray<FSavedConnection> SavedConnections;
		for (UEdGraphPin* Pin : TargetNode->Pins)
		{
			if (!Pin) continue;
			for (UEdGraphPin* Linked : Pin->LinkedTo)
			{
				if (Linked && Linked->GetOwningNode())
				{
					SavedConnections.Add({Pin->PinName, Pin->Direction, Linked->GetOwningNode()->NodeGuid, Linked->PinName});
				}
			}
		}

		// Find which graph this node is in
		FString GraphName;
		for (UEdGraph* Graph : AllGraphs)
		{
			if (Graph->Nodes.Contains(TargetNode))
			{
				GraphName = Graph->GetName();
				break;
			}
		}

		TargetNode->ReconstructNode();

		int32 Restored = 0;
		int32 Failed = 0;
		for (const FSavedConnection& Conn : SavedConnections)
		{
			UEdGraphPin* OurPin = TargetNode->FindPin(Conn.PinName, Conn.Direction);
			if (!OurPin) { Failed++; continue; }

			UEdGraphNode* RemoteNode = nullptr;
			for (UEdGraph* Graph : AllGraphs)
			{
				for (UEdGraphNode* Node : Graph->Nodes)
				{
					if (Node && Node->NodeGuid == Conn.RemoteNodeGuid)
					{
						RemoteNode = Node;
						break;
					}
				}
				if (RemoteNode) break;
			}
			if (!RemoteNode) { Failed++; continue; }

			EEdGraphPinDirection RemoteDir = (Conn.Direction == EGPD_Input) ? EGPD_Output : EGPD_Input;
			UEdGraphPin* RemotePin = RemoteNode->FindPin(Conn.RemotePinName, RemoteDir);
			if (!RemotePin) { Failed++; continue; }

			OurPin->MakeLinkTo(RemotePin);
			Restored++;
		}
		TotalRestored += Restored;
		TotalFailed += Failed;

		TSharedPtr<FJsonObject> Report = MakeShared<FJsonObject>();
		Report->SetStringField(TEXT("node_guid"), TargetNode->NodeGuid.ToString());
		Report->SetStringField(TEXT("node_class"), TargetNode->GetClass()->GetName());
		Report->SetStringField(TEXT("graph"), GraphName);
		Report->SetNumberField(TEXT("pins_after"), TargetNode->Pins.Num());
		Report->SetNumberField(TEXT("connections_restored"), Restored);
		Report->SetNumberField(TEXT("connections_failed"), Failed);
		NodeReports.Add(MakeShared<FJsonValueObject>(Report));
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Reconstructed %d node(s)"), NodesToReconstruct.Num()));
	Data->SetNumberField(TEXT("nodes_reconstructed"), NodesToReconstruct.Num());
	Data->SetNumberField(TEXT("total_connections_restored"), TotalRestored);
	Data->SetNumberField(TEXT("total_connections_failed"), TotalFailed);
	Data->SetArrayField(TEXT("nodes"), NodeReports);
	return MakeResponse(true, Data);
}

FString FMCPServer::HandleSetPinDefault(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("blueprint_path")))
		return MakeError(TEXT("Missing 'blueprint_path' parameter"));

	FString BPPath = Params->GetStringField(TEXT("blueprint_path"));
	FString GraphName = Params->GetStringField(TEXT("graph_name"));
	FString NodeGuid = Params->GetStringField(TEXT("node_guid"));
	FString PinName = Params->GetStringField(TEXT("pin_name"));
	FString NewDefault = Params->GetStringField(TEXT("new_default"));

	UBlueprint* Blueprint = LoadBlueprintFromPath(BPPath);
	if (!Blueprint) return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *BPPath));

	// Search all graphs for the node
	TArray<UEdGraph*> AllGraphs;
	Blueprint->GetAllGraphs(AllGraphs);

	for (UEdGraph* Graph : AllGraphs)
	{
		if (!GraphName.IsEmpty() && Graph->GetName() != GraphName) continue;

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			FString GuidStr = Node->NodeGuid.ToString(EGuidFormats::DigitsWithHyphensInBraces);
			// Also try without braces
			FString GuidNoBraces = Node->NodeGuid.ToString(EGuidFormats::Digits);

			if (GuidStr != NodeGuid && GuidNoBraces != NodeGuid && Node->NodeGuid.ToString() != NodeGuid)
				continue;

			// Found the node, find the pin
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (Pin->PinName.ToString() == PinName)
				{
					// Object/class-reference pins (e.g. a StaticMesh* function
					// param) resolve their compiled default from
					// Pin->DefaultObject, not Pin->DefaultValue - writing only
					// DefaultValue (the old behavior) left the pin's real
					// value untouched, so the graph kept compiling against
					// whatever object was already assigned. Discovered live:
					// re-pointing ALS_CharacterBP's OnUpdateHeldObject "Torch"
					// AttachToHand call at a different StaticMesh appeared to
					// succeed but compilation kept citing the original mesh.
					const bool bIsObjectPin = Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Object
						|| Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_SoftObject
						|| Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Class
						|| Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_SoftClass;

					FString OldDefault = bIsObjectPin
						? (Pin->DefaultObject ? Pin->DefaultObject->GetPathName() : FString())
						: Pin->DefaultValue;

					if (bIsObjectPin)
					{
						// Accept either a bare asset path or the
						// ClassName'/Path/To/Asset.Asset' decorated form
						// ImportText/read_function_graphs displays.
						FString AssetPath = NewDefault;
						int32 QuoteIdx;
						if (!AssetPath.IsEmpty() && AssetPath.EndsWith(TEXT("'")) && AssetPath.FindChar('\'', QuoteIdx))
						{
							AssetPath = AssetPath.Mid(QuoteIdx + 1, AssetPath.Len() - QuoteIdx - 2);
						}

						UObject* ResolvedObject = nullptr;
						if (!AssetPath.IsEmpty())
						{
							ResolvedObject = StaticLoadObject(UObject::StaticClass(), nullptr, *AssetPath);
							if (!ResolvedObject)
							{
								return MakeError(FString::Printf(TEXT("Could not resolve object '%s' for pin '%s'"), *AssetPath, *PinName));
							}
						}

						Pin->DefaultObject = ResolvedObject;
						Pin->DefaultValue.Empty();
					}
					else
					{
						Pin->DefaultValue = NewDefault;
					}

					FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

					TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
					Data->SetStringField(TEXT("graph"), Graph->GetName());
					Data->SetStringField(TEXT("node"), Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
					Data->SetStringField(TEXT("pin"), PinName);
					Data->SetStringField(TEXT("old_default"), OldDefault);
					Data->SetStringField(TEXT("new_default"), NewDefault);
					return MakeResponse(true, Data);
				}
			}
			return MakeError(FString::Printf(TEXT("Pin '%s' not found on node"), *PinName));
		}
	}
	return MakeError(FString::Printf(TEXT("Node with GUID '%s' not found"), *NodeGuid));
}
