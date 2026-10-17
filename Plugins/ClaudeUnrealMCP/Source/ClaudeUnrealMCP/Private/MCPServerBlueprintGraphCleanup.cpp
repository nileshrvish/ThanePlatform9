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

FString FMCPServer::HandleDeleteFunctionGraph(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("blueprint_path")) || !Params->HasField(TEXT("function_name")))
	{
		return MakeError(TEXT("Missing 'blueprint_path' or 'function_name' parameter"));
	}

	const FString BlueprintPath = Params->GetStringField(TEXT("blueprint_path"));
	const FString FunctionName = Params->GetStringField(TEXT("function_name"));

	UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
	}

	// Find the function graph
	UEdGraph* GraphToDelete = nullptr;
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph && Graph->GetName() == FunctionName)
		{
			GraphToDelete = Graph;
			break;
		}
	}

	if (!GraphToDelete)
	{
		return MakeError(FString::Printf(TEXT("Function '%s' not found in blueprint"), *FunctionName));
	}

	// Remove the function graph
	Blueprint->FunctionGraphs.Remove(GraphToDelete);

	// Mark the blueprint as modified
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("Function graph deleted successfully"));
	Data->SetStringField(TEXT("function_name"), FunctionName);
	Data->SetNumberField(TEXT("remaining_functions"), Blueprint->FunctionGraphs.Num());

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleClearEventGraph(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("blueprint_path")))
	{
		return MakeError(TEXT("Missing 'blueprint_path' parameter"));
	}

	const FString BlueprintPath = Params->GetStringField(TEXT("blueprint_path"));

	UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
	}

	// Get the event graph (typically UbergraphPages[0])
	if (Blueprint->UbergraphPages.Num() == 0)
	{
		return MakeError(TEXT("Blueprint has no event graph"));
	}

	UEdGraph* EventGraph = Blueprint->UbergraphPages[0];
	if (!EventGraph)
	{
		return MakeError(TEXT("Event graph is null"));
	}

	int32 NodesCleared = EventGraph->Nodes.Num();

	// Remove all nodes from the event graph
	EventGraph->Nodes.Empty();

	// Mark the blueprint as modified
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("Event graph cleared successfully"));
	Data->SetNumberField(TEXT("nodes_cleared"), NodesCleared);

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleRefreshNodes(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("blueprint_path")))
	{
		return MakeError(TEXT("Missing 'blueprint_path' parameter"));
	}

	const FString BlueprintPath = Params->GetStringField(TEXT("blueprint_path"));

	UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
	}

	int32 NodesRefreshed = 0;

	// First, try the built-in RefreshAllNodes which may handle orphaned pins better
	FBlueprintEditorUtils::RefreshAllNodes(Blueprint);

	// Refresh nodes in all graphs (UbergraphPages, FunctionGraphs, etc.)
	TArray<UEdGraph*> AllGraphs;
	Blueprint->GetAllGraphs(AllGraphs);

	for (UEdGraph* Graph : AllGraphs)
	{
		if (!Graph) continue;

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node)
			{
				// First, break links to orphaned/stale pins before reconstructing
				// This handles the case where pins have been renamed or removed in C++ function signatures
				for (UEdGraphPin* Pin : Node->Pins)
				{
					if (!Pin) continue;

					// Make a copy of linked pins since we'll be modifying the array
					TArray<UEdGraphPin*> LinkedToCopy = Pin->LinkedTo;
					for (UEdGraphPin* OtherPin : LinkedToCopy)
					{
						// If we are linked to a pin that its owner doesn't know about, break that link
						if (!OtherPin || !OtherPin->GetOwningNodeUnchecked() ||
							!OtherPin->GetOwningNode()->Pins.Contains(OtherPin))
						{
							Pin->BreakLinkTo(OtherPin);
						}
					}
				}

				// Reconstruct the node to get the updated pin configuration
				Node->ReconstructNode();

				// CRITICAL: After reconstruction, clean up stale incoming links from OTHER nodes
				// This fixes the "In use pin X no longer exists" errors
				for (UEdGraphNode* OtherNode : Graph->Nodes)
				{
					if (OtherNode && OtherNode != Node)
					{
						for (UEdGraphPin* OtherPin : OtherNode->Pins)
						{
							if (!OtherPin) continue;

							// Check if this pin is linked to any pins that no longer exist on the reconstructed node
							TArray<UEdGraphPin*> LinkedToCopy = OtherPin->LinkedTo;
							for (UEdGraphPin* LinkedPin : LinkedToCopy)
							{
								// If linked to a pin on the reconstructed node that no longer exists in its Pins array
								if (LinkedPin && LinkedPin->GetOwningNode() == Node &&
									!Node->Pins.Contains(LinkedPin))
								{
									OtherPin->BreakLinkTo(LinkedPin);
								}
							}
						}
					}
				}

				NodesRefreshed++;
			}
		}
	}

	// Mark the blueprint as modified
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("Nodes refreshed successfully"));
	Data->SetNumberField(TEXT("nodes_refreshed"), NodesRefreshed);

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleBreakOrphanedPins(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("blueprint_path")))
	{
		return MakeError(TEXT("Missing 'blueprint_path' parameter"));
	}

	const FString BlueprintPath = Params->GetStringField(TEXT("blueprint_path"));

	UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
	}

	int32 PinsBroken = 0;
	int32 PinsRemoved = 0;

	// Get all graphs
	TArray<UEdGraph*> AllGraphs;
	Blueprint->GetAllGraphs(AllGraphs);

	for (UEdGraph* Graph : AllGraphs)
	{
		if (!Graph) continue;

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node) continue;

			// Track pins to remove (can't remove while iterating)
			TArray<UEdGraphPin*> PinsToRemove;

			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (!Pin) continue;

				// Check if this pin is orphaned - it has the bOrphanedPin flag or has invalid connections
				bool bIsOrphaned = Pin->bOrphanedPin;

				// Also check if the pin name suggests it's orphaned (contains "DEPRECATED", "TRASH", or similar markers)
				if (Pin->PinName.ToString().Contains(TEXT("TRASH")) ||
					Pin->PinName.ToString().Contains(TEXT("DEPRECATED")))
				{
					bIsOrphaned = true;
				}

				// Break all connections to/from orphaned pins
				if (bIsOrphaned && Pin->LinkedTo.Num() > 0)
				{
					Pin->BreakAllPinLinks();
					PinsBroken += Pin->LinkedTo.Num();
				}

				// Mark orphaned pins for removal
				if (bIsOrphaned)
				{
					PinsToRemove.Add(Pin);
				}
			}

			// Remove orphaned pins from the node
			for (UEdGraphPin* PinToRemove : PinsToRemove)
			{
				Node->Pins.Remove(PinToRemove);
				PinsRemoved++;
			}
		}
	}

	// Mark the blueprint as modified
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("Orphaned pins cleaned up successfully"));
	Data->SetNumberField(TEXT("pins_broken"), PinsBroken);
	Data->SetNumberField(TEXT("pins_removed"), PinsRemoved);

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleDeleteUserDefinedStruct(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("struct_path")))
	{
		return MakeError(TEXT("Missing 'struct_path' parameter"));
	}

	const FString StructPath = Params->GetStringField(TEXT("struct_path"));

	// Load the struct asset
	UUserDefinedStruct* Struct = LoadObject<UUserDefinedStruct>(nullptr, *StructPath);
	if (!Struct)
	{
		return MakeError(FString::Printf(TEXT("Struct not found: %s"), *StructPath));
	}

	// Delete the asset
	TArray<UObject*> ObjectsToDelete;
	ObjectsToDelete.Add(Struct);

	int32 NumDeleted = ObjectTools::DeleteObjects(ObjectsToDelete, false);

	if (NumDeleted == 0)
	{
		return MakeError(TEXT("Failed to delete struct asset"));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("Struct deleted successfully"));
	Data->SetStringField(TEXT("struct_path"), StructPath);

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleModifyStructField(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("struct_path")) ||
		!Params->HasField(TEXT("field_name")) || !Params->HasField(TEXT("new_type")))
	{
		return MakeError(TEXT("Missing required parameters: struct_path, field_name, new_type"));
	}

	const FString StructPath = Params->GetStringField(TEXT("struct_path"));
	const FString FieldName = Params->GetStringField(TEXT("field_name"));
	const FString NewType = Params->GetStringField(TEXT("new_type"));

	// Load the struct asset
	UUserDefinedStruct* Struct = LoadObject<UUserDefinedStruct>(nullptr, *StructPath);
	if (!Struct)
	{
		return MakeError(FString::Printf(TEXT("Struct not found: %s"), *StructPath));
	}

	// Get the struct's variable descriptions
	TArray<FStructVariableDescription>& Variables = const_cast<TArray<FStructVariableDescription>&>(
		FStructureEditorUtils::GetVarDesc(Struct)
	);

	// Find the field to modify
	bool bFieldFound = false;
	for (FStructVariableDescription& Variable : Variables)
	{
		if (Variable.VarName.ToString() == FieldName)
		{
			bFieldFound = true;

			// Determine the new type based on the input
			// For struct types, we need to load the struct and set it as the SubCategoryObject
			if (NewType.StartsWith(TEXT("F")) || NewType.StartsWith(TEXT("S_")))
			{
				// This is a struct type
				FString StructTypePath = NewType;
				if (!StructTypePath.Contains(TEXT(".")))
				{
					// Try to find the struct - could be C++ or blueprint
					// For C++ structs, they're in /Script/UETest1.StructName format
					// For blueprint structs, they're in /Game/Path/StructName.StructName format

					if (NewType.StartsWith(TEXT("FS_")))
					{
						// C++ struct - construct the path
						StructTypePath = FString::Printf(TEXT("/Script/UETest1.%s"), *NewType);
					}
					else if (NewType.StartsWith(TEXT("S_")))
					{
						// Blueprint struct - try common locations
						StructTypePath = FString::Printf(TEXT("/Game/Levels/LevelPrototyping/Data/%s.%s"), *NewType, *NewType);
					}
				}

				// Try to load as UScriptStruct (C++ struct) - try multiple search patterns
				UScriptStruct* ScriptStruct = FindObject<UScriptStruct>(nullptr, *StructTypePath);

				// If not found, try searching by struct name alone (for C++ structs)
				if (!ScriptStruct && NewType.StartsWith(TEXT("FS_")))
				{
					// For C++ structs, use FindPackage to get the module package
					UPackage* Package = FindPackage(nullptr, TEXT("/Script/UETest1"));
					if (Package)
					{
						ScriptStruct = FindObject<UScriptStruct>(Package, *NewType);
					}

					// If still not found, try LoadObject with full path
					if (!ScriptStruct)
					{
						FString PackagePath = FString::Printf(TEXT("/Script/UETest1.%s"), *NewType);
						ScriptStruct = LoadObject<UScriptStruct>(nullptr, *PackagePath);
					}

					// Last resort: iterate through all UScriptStruct objects
					if (!ScriptStruct)
					{
						TArray<FString> FoundStructNames;
						for (TObjectIterator<UScriptStruct> It; It; ++It)
						{
							FString StructName = It->GetName();
							// Collect all struct names that start with "FS_" or "S_" for debugging
							if (StructName.StartsWith(TEXT("FS_")) || StructName.StartsWith(TEXT("S_")))
							{
								FoundStructNames.Add(FString::Printf(TEXT("%s (%s)"), *StructName, *It->GetPathName()));
							}

							if (StructName == NewType)
							{
								ScriptStruct = *It;
								break;
							}
						}

						// If not found, include debug info about similar structs
						if (!ScriptStruct && FoundStructNames.Num() > 0)
						{
							// Limit to first 20 structs for debugging
							TArray<FString> LimitedList;
							for (int32 i = 0; i < FMath::Min(20, FoundStructNames.Num()); i++)
							{
								LimitedList.Add(FoundStructNames[i]);
							}
							FString DebugInfo = FString::Printf(TEXT("Found %d structs (showing max 20): %s"),
								FoundStructNames.Num(),
								*FString::Join(LimitedList, TEXT(", ")));
							UE_LOG(LogTemp, Warning, TEXT("Struct not found. %s"), *DebugInfo);
						}
					}
				}

				if (!ScriptStruct)
				{
					// Try to load as UserDefinedStruct (blueprint struct)
					UUserDefinedStruct* TargetStruct = LoadObject<UUserDefinedStruct>(nullptr, *StructTypePath);
					if (TargetStruct)
					{
						ScriptStruct = TargetStruct;
					}
				}

				if (ScriptStruct)
				{
					Variable.Category = UEdGraphSchema_K2::PC_Struct;
					Variable.SubCategoryObject = ScriptStruct;
					Variable.PinValueType.TerminalCategory = UEdGraphSchema_K2::PC_Struct;
					Variable.PinValueType.TerminalSubCategoryObject = ScriptStruct;
				}
				else
				{
					return MakeError(FString::Printf(TEXT("Could not find struct type: %s (tried path: %s)"), *NewType, *StructTypePath));
				}
			}

			break;
		}
	}

	if (!bFieldFound)
	{
		return MakeError(FString::Printf(TEXT("Field not found: %s"), *FieldName));
	}

	// Recompile the struct
	FStructureEditorUtils::CompileStructure(Struct);

	// Mark as modified
	Struct->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("Struct field modified successfully"));
	Data->SetStringField(TEXT("struct_path"), StructPath);
	Data->SetStringField(TEXT("field_name"), FieldName);
	Data->SetStringField(TEXT("new_type"), NewType);

	return MakeResponse(true, Data);
}
