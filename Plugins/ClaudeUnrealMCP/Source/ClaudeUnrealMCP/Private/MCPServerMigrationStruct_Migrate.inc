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

FString FMCPServer::HandleMigrateStructReferences(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("source_struct_path")) || !Params->HasField(TEXT("target_struct_path")))
	{
		return MakeError(TEXT("Missing 'source_struct_path' or 'target_struct_path' parameter"));
	}

	FString SourceStructPath = Params->GetStringField(TEXT("source_struct_path"));
	FString TargetStructPath = Params->GetStringField(TEXT("target_struct_path"));
	bool bDryRun = Params->HasField(TEXT("dry_run")) ? Params->GetBoolField(TEXT("dry_run")) : false;

	// Load old BP struct
	FString FullSourcePath = SourceStructPath;
	if (!FullSourcePath.EndsWith(FPaths::GetCleanFilename(FullSourcePath)))
	{
		FullSourcePath = SourceStructPath + TEXT(".") + FPaths::GetCleanFilename(SourceStructPath);
	}
	UUserDefinedStruct* OldStruct = LoadObject<UUserDefinedStruct>(nullptr, *FullSourcePath);
	if (!OldStruct)
	{
		OldStruct = LoadObject<UUserDefinedStruct>(nullptr, *SourceStructPath);
	}
	if (!OldStruct)
	{
		return MakeError(FString::Printf(TEXT("Source UserDefinedStruct not found at '%s'"), *SourceStructPath));
	}

	// Load new C++ struct
	UScriptStruct* NewStruct = nullptr;
	FString TypeNameToFind = TargetStructPath;
	if (TargetStructPath.StartsWith(TEXT("/Script/")))
	{
		FString Remainder = TargetStructPath;
		Remainder.RemoveFromStart(TEXT("/Script/"));
		FString ModuleName;
		Remainder.Split(TEXT("."), &ModuleName, &TypeNameToFind);
	}
	for (TObjectIterator<UScriptStruct> It; It; ++It)
	{
		if (It->GetName() == TypeNameToFind)
		{
			NewStruct = *It;
			break;
		}
	}
	if (!NewStruct)
	{
		return MakeError(FString::Printf(TEXT("Target C++ struct '%s' not found"), *TargetStructPath));
	}

	// Build GUID → clean name field mapping
	TMap<FName, FName> FieldNameMap;
	TArray<TSharedPtr<FJsonValue>> MappingsJsonArray;

	// Build display name → C++ property FName map from target struct
	TMap<FString, FName> DisplayNameToCppPropName;
	for (TFieldIterator<FProperty> PropIt(NewStruct); PropIt; ++PropIt)
	{
		FString DisplayName = PropIt->GetDisplayNameText().ToString();
		DisplayNameToCppPropName.Add(DisplayName, PropIt->GetFName());
	}

	const TArray<FStructVariableDescription>& VarDescs = FStructureEditorUtils::GetVarDesc(OldStruct);
	for (const FStructVariableDescription& Desc : VarDescs)
	{
		FName OldName = Desc.VarName;
		// Match BP FriendlyName to C++ property via display name
		FName CleanName;
		FName* FoundCppName = DisplayNameToCppPropName.Find(Desc.FriendlyName);
		if (FoundCppName)
		{
			CleanName = *FoundCppName;
		}
		else
		{
			// Fallback: use FriendlyName directly (works if names match exactly)
			CleanName = FName(*Desc.FriendlyName);
		}
		FieldNameMap.Add(OldName, CleanName);

		TSharedPtr<FJsonObject> MapObj = MakeShared<FJsonObject>();
		MapObj->SetStringField(TEXT("old_name"), OldName.ToString());
		MapObj->SetStringField(TEXT("new_name"), CleanName.ToString());
		MappingsJsonArray.Add(MakeShared<FJsonValueObject>(MapObj));
	}

	// Find all blueprint assets
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetData> AllBPAssets;
	AssetRegistry.GetAssetsByClass(UBlueprint::StaticClass()->GetClassPathName(), AllBPAssets);
	TArray<FAssetData> AnimBPAssets;
	AssetRegistry.GetAssetsByClass(UAnimBlueprint::StaticClass()->GetClassPathName(), AnimBPAssets);
	AllBPAssets.Append(AnimBPAssets);

	// Find affected blueprints
	TArray<UBlueprint*> AffectedBlueprints;
	for (const FAssetData& Asset : AllBPAssets)
	{
		FString AssetPath = Asset.GetObjectPathString();
		if (!AssetPath.StartsWith(TEXT("/Game/"))) continue;

		UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *AssetPath);
		if (!BP) continue;

		if (DoesBlueprintReferenceStruct(BP, OldStruct))
		{
			AffectedBlueprints.Add(BP);
		}
	}

	// Migrate each affected blueprint
	TArray<TSharedPtr<FJsonValue>> BlueprintReportsArray;
	int32 TotalVariablesMigrated = 0;
	int32 TotalNodesMigrated = 0;
	int32 TotalConnectionsRestored = 0;
	int32 TotalConnectionsFailed = 0;

	for (UBlueprint* Blueprint : AffectedBlueprints)
	{
		TSharedPtr<FJsonObject> BPReport = MakeShared<FJsonObject>();
		BPReport->SetStringField(TEXT("path"), Blueprint->GetPathName());
		BPReport->SetStringField(TEXT("name"), Blueprint->GetName());

		int32 VarCount = 0;
		int32 NodeCount = 0;

		// --- Update member variables ---
		for (FBPVariableDescription& Var : Blueprint->NewVariables)
		{
			if (Var.VarType.PinCategory == UEdGraphSchema_K2::PC_Struct)
			{
				UObject* VarObj = Var.VarType.PinSubCategoryObject.Get();
				if (VarObj == OldStruct || (VarObj && VarObj->GetName() == OldStruct->GetName()))
				{
					VarCount++;
					if (!bDryRun)
					{
						Var.VarType.PinSubCategoryObject = NewStruct;
					}
				}
			}
		}

		// --- Update function-local variables (stored on FunctionEntry nodes) ---
		{
			TArray<UEdGraph*> TempGraphs;
			Blueprint->GetAllGraphs(TempGraphs);
			for (UEdGraph* Graph : TempGraphs)
			{
				for (UEdGraphNode* Node : Graph->Nodes)
				{
					UK2Node_FunctionEntry* EntryNode = Cast<UK2Node_FunctionEntry>(Node);
					if (!EntryNode) continue;
					for (FBPVariableDescription& LocalVar : EntryNode->LocalVariables)
					{
						if (LocalVar.VarType.PinCategory == UEdGraphSchema_K2::PC_Struct)
						{
							UObject* VarObj = LocalVar.VarType.PinSubCategoryObject.Get();
							if (VarObj == OldStruct || (VarObj && VarObj->GetName() == OldStruct->GetName()))
							{
								VarCount++;
								if (!bDryRun)
								{
									LocalVar.VarType.PinSubCategoryObject = NewStruct;
								}
							}
						}
					}
				}
			}
		}

		// --- Collect all affected nodes across all graphs ---
		struct FNodeMigrationInfo
		{
			UEdGraphNode* Node;
			UEdGraph* Graph;
			bool bIsStructNode; // Break/Make/Set struct node (needs ReconstructNode)
			bool bIsEditablePinNode; // FunctionEntry/FunctionResult (needs UserDefinedPins update + ReconstructNode)
			TArray<FSavedPinConnection> SavedConnections;
		};
		TArray<FNodeMigrationInfo> NodesToMigrate;

		TArray<UEdGraph*> AllGraphs;
		Blueprint->GetAllGraphs(AllGraphs);

		// Add sub-graphs recursively (GetAllGraphs may not include node sub-graphs)
		{
			TArray<UEdGraph*> GraphsToProcess = AllGraphs;
			while (GraphsToProcess.Num() > 0)
			{
				UEdGraph* CurrentGraph = GraphsToProcess.Pop();
				for (UEdGraphNode* GraphNode : CurrentGraph->Nodes)
				{
					if (!GraphNode) continue;
					for (UEdGraph* SubGraph : GraphNode->GetSubGraphs())
					{
						if (SubGraph && !AllGraphs.Contains(SubGraph))
						{
							AllGraphs.Add(SubGraph);
							GraphsToProcess.Add(SubGraph);
						}
					}
				}
			}
		}

		for (UEdGraph* Graph : AllGraphs)
		{
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (!Node) continue;
				bool bNodeAffected = false;
				bool bIsStructNode = false;
				bool bIsEditablePinNode = false;

				// Check Break/Make/Set struct nodes
				// IMPORTANT: Check SetFieldsInStruct BEFORE MakeStruct because SetFieldsInStruct inherits from MakeStruct
				if (UK2Node_SetFieldsInStruct* SetNode = Cast<UK2Node_SetFieldsInStruct>(Node))
				{
					if (SetNode->StructType == OldStruct || (SetNode->StructType && SetNode->StructType->GetName() == OldStruct->GetName()))
					{
						bNodeAffected = true; bIsStructNode = true;
					}
				}
				else if (UK2Node_BreakStruct* BreakNode = Cast<UK2Node_BreakStruct>(Node))
				{
					if (BreakNode->StructType == OldStruct || (BreakNode->StructType && BreakNode->StructType->GetName() == OldStruct->GetName()))
					{
						bNodeAffected = true; bIsStructNode = true;
					}
				}
				else if (UK2Node_MakeStruct* MakeNode = Cast<UK2Node_MakeStruct>(Node))
				{
					if (MakeNode->StructType == OldStruct || (MakeNode->StructType && MakeNode->StructType->GetName() == OldStruct->GetName()))
					{
						bNodeAffected = true; bIsStructNode = true;
					}
				}
				else
				{
					// Check FunctionEntry/FunctionResult UserDefinedPins
					if (UK2Node_EditablePinBase* EditableNode = Cast<UK2Node_EditablePinBase>(Node))
					{
						for (const TSharedPtr<FUserPinInfo>& PinInfo : EditableNode->UserDefinedPins)
						{
							if (PinInfo.IsValid() &&
								PinInfo->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct)
							{
								UObject* PinObj = PinInfo->PinType.PinSubCategoryObject.Get();
								if (PinObj == OldStruct || (PinObj && PinObj->GetName() == OldStruct->GetName()))
								{
									bNodeAffected = true;
									bIsEditablePinNode = true;
									break;
								}
							}
						}
					}

					// Generic: check any pin referencing old struct
					if (!bNodeAffected)
					{
						for (UEdGraphPin* Pin : Node->Pins)
						{
							if (Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct)
							{
								UObject* PinObj = Pin->PinType.PinSubCategoryObject.Get();
								if (PinObj == OldStruct || (PinObj && PinObj->GetName() == OldStruct->GetName()))
								{
									bNodeAffected = true;
									break;
								}
							}
						}
					}
				}

				if (bNodeAffected)
				{
					FNodeMigrationInfo Info;
					Info.Node = Node;
					Info.Graph = Graph;
					Info.bIsStructNode = bIsStructNode;
					Info.bIsEditablePinNode = bIsEditablePinNode;
					SaveNodeConnections(Node, Info.SavedConnections);
					NodesToMigrate.Add(MoveTemp(Info));
				}
			}
		}

		NodeCount = NodesToMigrate.Num();

		// --- Migrate nodes ---
		if (!bDryRun)
		{
			// Pass 1: Update struct references
			for (FNodeMigrationInfo& Info : NodesToMigrate)
			{
				UEdGraphNode* Node = Info.Node;

				if (Info.bIsStructNode)
				{
					// Break/Make/Set nodes: need ReconstructNode to rebuild pins with new field names
					for (UEdGraphPin* Pin : Node->Pins)
					{
						if (Pin) Pin->BreakAllPinLinks();
					}

					// IMPORTANT: Check SetFieldsInStruct BEFORE MakeStruct because SetFieldsInStruct inherits from MakeStruct
					if (UK2Node_SetFieldsInStruct* SetNode = Cast<UK2Node_SetFieldsInStruct>(Node))
					{
						// Save which properties were visible (by remapped C++ name) and their pin default values
						TSet<FName> VisibleCppProperties;
						TMap<FName, FString> PinDefaultValues;
						for (const FOptionalPinFromProperty& PinProp : SetNode->ShowPinForProperties)
						{
							if (PinProp.bShowPin)
							{
								const FName* NewName = FieldNameMap.Find(PinProp.PropertyName);
								FName CppName = NewName ? *NewName : PinProp.PropertyName;
								VisibleCppProperties.Add(CppName);
								UEdGraphPin* OldPin = SetNode->FindPin(PinProp.PropertyName, EGPD_Input);
								if (OldPin && !OldPin->DefaultValue.IsEmpty())
								{
									PinDefaultValues.Add(CppName, OldPin->DefaultValue);
								}
							}
						}

						SetNode->StructType = NewStruct;
						// Remap ShowPinForProperties from GUID-suffixed names to C++ property names
						for (FOptionalPinFromProperty& PinProp : SetNode->ShowPinForProperties)
						{
							const FName* NewName = FieldNameMap.Find(PinProp.PropertyName);
							if (NewName)
							{
								PinProp.PropertyName = *NewName;
							}
						}

						Node->ReconstructNode();

						// After ReconstructNode, force-enable properties that were visible before migration
						bool bNeedsSecondReconstruct = false;
						for (FOptionalPinFromProperty& PinProp : SetNode->ShowPinForProperties)
						{
							if (!PinProp.bShowPin && VisibleCppProperties.Contains(PinProp.PropertyName))
							{
								PinProp.bShowPin = true;
								bNeedsSecondReconstruct = true;
							}
						}
						if (bNeedsSecondReconstruct)
						{
							Node->ReconstructNode();
						}

						// Restore pin default values
						for (const auto& Pair : PinDefaultValues)
						{
							UEdGraphPin* NewPin = SetNode->FindPin(Pair.Key, EGPD_Input);
							if (NewPin)
							{
								NewPin->DefaultValue = Pair.Value;
							}
						}
						continue; // Skip the generic ReconstructNode below
					}
					// Break/Make struct nodes: just set StructType and let ReconstructNode rebuild
					if (UK2Node_BreakStruct* BreakNode = Cast<UK2Node_BreakStruct>(Node))
					{
						BreakNode->StructType = NewStruct;
					}
					else if (UK2Node_MakeStruct* MakeNode = Cast<UK2Node_MakeStruct>(Node))
					{
						MakeNode->StructType = NewStruct;
					}
					Node->ReconstructNode();
				}
				else if (Info.bIsEditablePinNode)
				{
					// FunctionEntry/FunctionResult nodes: update UserDefinedPins then reconstruct
					// UserDefinedPins store the function signature - must be updated for persistence
					UK2Node_EditablePinBase* EditableNode = Cast<UK2Node_EditablePinBase>(Node);
					if (EditableNode)
					{
						// Update UserDefinedPins to reference new struct
						for (TSharedPtr<FUserPinInfo>& PinInfo : EditableNode->UserDefinedPins)
						{
							if (PinInfo.IsValid() &&
								PinInfo->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct)
							{
								UObject* PinObj = PinInfo->PinType.PinSubCategoryObject.Get();
								if (PinObj == OldStruct || (PinObj && PinObj->GetName() == OldStruct->GetName()))
								{
									PinInfo->PinType.PinSubCategoryObject = NewStruct;
								}
							}
						}

						// Break links and reconstruct to regenerate sub-pins with clean field names
						for (UEdGraphPin* Pin : Node->Pins)
						{
							if (Pin) Pin->BreakAllPinLinks();
						}
						Node->ReconstructNode();
					}
				}
				else
				{
					// Generic nodes: update pin types in-place WITHOUT ReconstructNode
					// ReconstructNode on generic nodes rebuilds from internal state and reverts our changes
					for (UEdGraphPin* Pin : Node->Pins)
					{
						if (Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct)
						{
							UObject* PinObj = Pin->PinType.PinSubCategoryObject.Get();
							if (PinObj == OldStruct || (PinObj && PinObj->GetName() == OldStruct->GetName()))
							{
								Pin->PinType.PinSubCategoryObject = NewStruct;
							}
						}
					}

					// Also update UserDefinedPins on editable pin nodes that weren't caught above
					if (UK2Node_EditablePinBase* EditableNode = Cast<UK2Node_EditablePinBase>(Node))
					{
						for (TSharedPtr<FUserPinInfo>& PinInfo : EditableNode->UserDefinedPins)
						{
							if (PinInfo.IsValid() &&
								PinInfo->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct)
							{
								UObject* PinObj = PinInfo->PinType.PinSubCategoryObject.Get();
								if (PinObj == OldStruct || (PinObj && PinObj->GetName() == OldStruct->GetName()))
								{
									PinInfo->PinType.PinSubCategoryObject = NewStruct;
								}
							}
						}
					}
				}
			}

			// Pass 2: Restore connections on reconstructed nodes (generic nodes kept their connections)
			for (FNodeMigrationInfo& Info : NodesToMigrate)
			{
				if (Info.bIsStructNode || Info.bIsEditablePinNode)
				{
					// Both struct nodes and editable pin nodes were reconstructed — restore connections
					int32 ConnectionsBefore = Info.SavedConnections.Num();
					RestoreNodeConnections(Info.Node, Info.SavedConnections, FieldNameMap, Info.Graph);

					int32 RestoredCount = 0;
					for (UEdGraphPin* Pin : Info.Node->Pins)
					{
						if (Pin) RestoredCount += Pin->LinkedTo.Num();
					}
					TotalConnectionsRestored += RestoredCount;
					int32 FailedCount = ConnectionsBefore - RestoredCount;
					if (FailedCount > 0) TotalConnectionsFailed += FailedCount;
				}
				else
				{
					// Generic nodes kept connections — count them as restored
					for (UEdGraphPin* Pin : Info.Node->Pins)
					{
						if (Pin) TotalConnectionsRestored += Pin->LinkedTo.Num();
					}
				}
			}

			// Finalize
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		}

		TotalVariablesMigrated += VarCount;
		TotalNodesMigrated += NodeCount;

		BPReport->SetNumberField(TEXT("variables_migrated"), VarCount);
		BPReport->SetNumberField(TEXT("nodes_migrated"), NodeCount);
		BlueprintReportsArray.Add(MakeShared<FJsonValueObject>(BPReport));
	}

	// Build response
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("dry_run"), bDryRun);
	Data->SetStringField(TEXT("source_struct"), OldStruct->GetPathName());
	Data->SetStringField(TEXT("target_struct"), NewStruct->GetPathName());
	Data->SetNumberField(TEXT("field_mappings_count"), FieldNameMap.Num());
	Data->SetArrayField(TEXT("field_name_mapping"), MappingsJsonArray);
	Data->SetNumberField(TEXT("blueprints_affected"), AffectedBlueprints.Num());
	Data->SetArrayField(TEXT("affected_blueprints"), BlueprintReportsArray);
	Data->SetNumberField(TEXT("total_variables_migrated"), TotalVariablesMigrated);
	Data->SetNumberField(TEXT("total_nodes_migrated"), TotalNodesMigrated);
	if (!bDryRun)
	{
		Data->SetNumberField(TEXT("connections_restored"), TotalConnectionsRestored);
		Data->SetNumberField(TEXT("connections_failed"), TotalConnectionsFailed);
	}
	Data->SetStringField(TEXT("message"),
		bDryRun ? TEXT("Dry run complete - no changes made") : TEXT("Struct migration complete"));

	return MakeResponse(true, Data);
}
