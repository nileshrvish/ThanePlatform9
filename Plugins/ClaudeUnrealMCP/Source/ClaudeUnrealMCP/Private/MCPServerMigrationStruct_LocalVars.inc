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
#include "MCPServerHelpers.h"

FString FMCPServer::HandleRenameLocalVariable(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("blueprint_path")) || !Params->HasField(TEXT("function_name")) ||
		!Params->HasField(TEXT("old_name")) || !Params->HasField(TEXT("new_name")))
	{
		return MakeError(TEXT("Missing required parameters: blueprint_path, function_name, old_name, new_name"));
	}

	FString BPPath = Params->GetStringField(TEXT("blueprint_path"));
	FString FunctionName = Params->GetStringField(TEXT("function_name"));
	FString OldName = Params->GetStringField(TEXT("old_name"));
	FString NewName = Params->GetStringField(TEXT("new_name"));

	UBlueprint* Blueprint = LoadBlueprintFromPath(BPPath);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *BPPath));
	}

	// Find the function graph
	UEdGraph* FunctionGraph = nullptr;
	TArray<UEdGraph*> AllGraphs;
	Blueprint->GetAllGraphs(AllGraphs);
	for (UEdGraph* Graph : AllGraphs)
	{
		if (Graph && Graph->GetName() == FunctionName)
		{
			FunctionGraph = Graph;
			break;
		}
	}

	if (!FunctionGraph)
	{
		return MakeError(FString::Printf(TEXT("Function graph '%s' not found"), *FunctionName));
	}

	// Find the FunctionEntry node
	UK2Node_FunctionEntry* EntryNode = nullptr;
	for (UEdGraphNode* Node : FunctionGraph->Nodes)
	{
		EntryNode = Cast<UK2Node_FunctionEntry>(Node);
		if (EntryNode) break;
	}

	if (!EntryNode)
	{
		return MakeError(TEXT("FunctionEntry node not found in function graph"));
	}

	// Rename in LocalVariables
	int32 VarsRenamed = 0;
	FName OldFName(*OldName);
	FName NewFName(*NewName);

	for (FBPVariableDescription& LocalVar : EntryNode->LocalVariables)
	{
		if (LocalVar.VarName == OldFName)
		{
			LocalVar.VarName = NewFName;
			LocalVar.FriendlyName = NewName;
			VarsRenamed++;
		}
	}

	// Also update any VariableGet/Set nodes that reference the old name
	int32 NodesUpdated = 0;
	for (UEdGraphNode* Node : FunctionGraph->Nodes)
	{
		if (!Node) continue;

		// Check if this is a variable reference node
		if (UK2Node_Variable* VarNode = Cast<UK2Node_Variable>(Node))
		{
			if (VarNode->GetVarName() == OldFName)
			{
				VarNode->VariableReference.SetSelfMember(NewFName);
				NodesUpdated++;

				// Also rename the pin
				for (UEdGraphPin* Pin : VarNode->Pins)
				{
					if (Pin && Pin->PinName == OldFName)
					{
						Pin->PinName = NewFName;
					}
				}
			}
		}
	}

	if (VarsRenamed > 0 || NodesUpdated > 0)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("blueprint"), Blueprint->GetName());
	Data->SetStringField(TEXT("function"), FunctionName);
	Data->SetNumberField(TEXT("variables_renamed"), VarsRenamed);
	Data->SetNumberField(TEXT("nodes_updated"), NodesUpdated);
	Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Renamed '%s' to '%s'"), *OldName, *NewName));

	return MakeResponse(true, Data);
}
