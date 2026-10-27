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

FString FMCPServer::HandleFixPropertyAccessPaths(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("source_struct_path")) || !Params->HasField(TEXT("target_struct_path")))
	{
		return MakeError(TEXT("Missing 'source_struct_path' or 'target_struct_path' parameter"));
	}

	FString SourceStructPath = Params->GetStringField(TEXT("source_struct_path"));
	FString TargetStructPath = Params->GetStringField(TEXT("target_struct_path"));
	bool bDryRun = Params->HasField(TEXT("dry_run")) ? Params->GetBoolField(TEXT("dry_run")) : false;
	FString BlueprintFilter = Params->HasField(TEXT("blueprint_path")) ? Params->GetStringField(TEXT("blueprint_path")) : TEXT("");

	// Load old BP struct for field name mapping
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

	// Load new C++ struct (to verify it exists)
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

	// Build GUID-suffixed → clean field name mapping
	TMap<FString, FString> FieldNameMap; // String-based for path segment matching
	TArray<TSharedPtr<FJsonValue>> MappingsJsonArray;

	const TArray<FStructVariableDescription>& VarDescs = FStructureEditorUtils::GetVarDesc(OldStruct);
	for (const FStructVariableDescription& Desc : VarDescs)
	{
		FString OldName = Desc.VarName.ToString();
		FString CleanName = Desc.FriendlyName;
		FieldNameMap.Add(OldName, CleanName);

		TSharedPtr<FJsonObject> MapObj = MakeShared<FJsonObject>();
		MapObj->SetStringField(TEXT("old_name"), OldName);
		MapObj->SetStringField(TEXT("new_name"), CleanName);
		MappingsJsonArray.Add(MakeShared<FJsonValueObject>(MapObj));
	}

	// Find the K2Node_PropertyAccess class via reflection (it's in a Private header)
	UClass* PropertyAccessNodeClass = nullptr;
	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	{
		if (ClassIt->GetName() == TEXT("K2Node_PropertyAccess"))
		{
			PropertyAccessNodeClass = *ClassIt;
			break;
		}
	}
	if (!PropertyAccessNodeClass)
	{
		return MakeError(TEXT("K2Node_PropertyAccess class not found - PropertyAccessNode plugin may not be loaded"));
	}

	// Find the Path property via reflection
	FArrayProperty* PathProperty = CastField<FArrayProperty>(PropertyAccessNodeClass->FindPropertyByName(TEXT("Path")));
	if (!PathProperty)
	{
		return MakeError(TEXT("Could not find 'Path' property on K2Node_PropertyAccess via reflection"));
	}

	// Find blueprints to scan
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetData> AllBPAssets;
	AssetRegistry.GetAssetsByClass(UBlueprint::StaticClass()->GetClassPathName(), AllBPAssets);
	TArray<FAssetData> AnimBPAssets;
	AssetRegistry.GetAssetsByClass(UAnimBlueprint::StaticClass()->GetClassPathName(), AnimBPAssets);
	AllBPAssets.Append(AnimBPAssets);

	TArray<TSharedPtr<FJsonValue>> BlueprintReportsArray;
	int32 TotalNodesFixed = 0;
	int32 TotalPathSegmentsUpdated = 0;

	for (const FAssetData& Asset : AllBPAssets)
	{
		FString AssetPath = Asset.GetObjectPathString();
		if (!AssetPath.StartsWith(TEXT("/Game/"))) continue;
		if (!BlueprintFilter.IsEmpty() && !AssetPath.Contains(BlueprintFilter)) continue;

		UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath);
		if (!Blueprint) continue;

		// Collect all graphs including sub-graphs
		TArray<UEdGraph*> AllGraphs;
		Blueprint->GetAllGraphs(AllGraphs);
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

		int32 NodesFixed = 0;
		int32 SegmentsUpdated = 0;
		TArray<TSharedPtr<FJsonValue>> NodeDetailsArray;

		for (UEdGraph* Graph : AllGraphs)
		{
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (!Node || !Node->IsA(PropertyAccessNodeClass)) continue;

				// Read Path via reflection
				TArray<FString>* PathPtr = PathProperty->ContainerPtrToValuePtr<TArray<FString>>(Node);
				if (!PathPtr || PathPtr->Num() == 0) continue;

				// Check if any segment needs remapping
				bool bNeedsUpdate = false;
				TArray<FString> NewPath = *PathPtr;
				TSharedPtr<FJsonObject> NodeDetail = MakeShared<FJsonObject>();
				NodeDetail->SetStringField(TEXT("node_guid"), Node->NodeGuid.ToString());
				NodeDetail->SetStringField(TEXT("graph"), Graph->GetName());

				TArray<TSharedPtr<FJsonValue>> OldPathJson;
				for (const FString& Seg : *PathPtr)
				{
					OldPathJson.Add(MakeShared<FJsonValueString>(Seg));
				}
				NodeDetail->SetArrayField(TEXT("old_path"), OldPathJson);

				for (int32 i = 0; i < NewPath.Num(); i++)
				{
					if (FString* CleanName = FieldNameMap.Find(NewPath[i]))
					{
						NewPath[i] = *CleanName;
						bNeedsUpdate = true;
						SegmentsUpdated++;
					}
				}

				if (bNeedsUpdate)
				{
					TArray<TSharedPtr<FJsonValue>> NewPathJson;
					for (const FString& Seg : NewPath)
					{
						NewPathJson.Add(MakeShared<FJsonValueString>(Seg));
					}
					NodeDetail->SetArrayField(TEXT("new_path"), NewPathJson);

					if (!bDryRun)
					{
						// Save connections from the output pin
						struct FPAConnection
						{
							FGuid RemoteNodeGuid;
							FName RemotePinName;
						};
						TArray<FPAConnection> SavedConnections;

						UEdGraphPin* OutputPin = Node->FindPin(TEXT("Value"), EGPD_Output);
						if (OutputPin)
						{
							for (UEdGraphPin* Linked : OutputPin->LinkedTo)
							{
								if (Linked && Linked->GetOwningNode())
								{
									SavedConnections.Add({Linked->GetOwningNode()->NodeGuid, Linked->PinName});
								}
							}
							OutputPin->BreakAllPinLinks();
						}

						// Update Path via reflection
						*PathPtr = NewPath;

						// Reconstruct node - this calls AllocatePins which resolves
						// the property and updates TextPath + ResolvedPinType
						Node->ReconstructNode();

						// Restore connections
						UEdGraphPin* NewOutputPin = Node->FindPin(TEXT("Value"), EGPD_Output);
						if (NewOutputPin)
						{
							for (const FPAConnection& Conn : SavedConnections)
							{
								for (UEdGraphNode* SearchNode : Graph->Nodes)
								{
									if (SearchNode && SearchNode->NodeGuid == Conn.RemoteNodeGuid)
									{
										for (UEdGraphPin* RemotePin : SearchNode->Pins)
										{
											if (RemotePin && RemotePin->PinName == Conn.RemotePinName)
											{
												NewOutputPin->MakeLinkTo(RemotePin);
												break;
											}
										}
										break;
									}
								}
							}
						}

						int32 RestoredCount = NewOutputPin ? NewOutputPin->LinkedTo.Num() : 0;
						NodeDetail->SetNumberField(TEXT("connections_restored"), RestoredCount);
						NodeDetail->SetNumberField(TEXT("connections_expected"), SavedConnections.Num());
					}

					NodeDetailsArray.Add(MakeShared<FJsonValueObject>(NodeDetail));
					NodesFixed++;
				}
			}
		}

		if (NodesFixed > 0)
		{
			if (!bDryRun)
			{
				FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
			}

			TSharedPtr<FJsonObject> BPReport = MakeShared<FJsonObject>();
			BPReport->SetStringField(TEXT("path"), Blueprint->GetPathName());
			BPReport->SetStringField(TEXT("name"), Blueprint->GetName());
			BPReport->SetNumberField(TEXT("nodes_fixed"), NodesFixed);
			BPReport->SetNumberField(TEXT("segments_updated"), SegmentsUpdated);
			BPReport->SetArrayField(TEXT("nodes"), NodeDetailsArray);
			BlueprintReportsArray.Add(MakeShared<FJsonValueObject>(BPReport));

			TotalNodesFixed += NodesFixed;
			TotalPathSegmentsUpdated += SegmentsUpdated;
		}
	}

	// Build response
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("dry_run"), bDryRun);
	Data->SetStringField(TEXT("source_struct"), OldStruct->GetPathName());
	Data->SetStringField(TEXT("target_struct"), NewStruct->GetPathName());
	Data->SetNumberField(TEXT("field_mappings_count"), FieldNameMap.Num());
	Data->SetArrayField(TEXT("field_name_mapping"), MappingsJsonArray);
	Data->SetNumberField(TEXT("blueprints_affected"), BlueprintReportsArray.Num());
	Data->SetArrayField(TEXT("affected_blueprints"), BlueprintReportsArray);
	Data->SetNumberField(TEXT("total_nodes_fixed"), TotalNodesFixed);
	Data->SetNumberField(TEXT("total_segments_updated"), TotalPathSegmentsUpdated);
	Data->SetStringField(TEXT("message"),
		bDryRun ? TEXT("Dry run complete - no changes made") : TEXT("PropertyAccess paths updated"));

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleCleanPropertyAccessPaths(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("blueprint_path")))
	{
		return MakeError(TEXT("Missing 'blueprint_path' parameter"));
	}

	const FString BlueprintPath = Params->GetStringField(TEXT("blueprint_path"));
	const FString RemoveSegment = Params->HasField(TEXT("remove_segment")) ? Params->GetStringField(TEXT("remove_segment")) : TEXT("None");

	UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
	if (!Blueprint) return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));

	// Find the K2Node_PropertyAccess class via reflection (it's in a Private header)
	UClass* PropertyAccessNodeClass = nullptr;
	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	{
		if (ClassIt->GetName() == TEXT("K2Node_PropertyAccess"))
		{
			PropertyAccessNodeClass = *ClassIt;
			break;
		}
	}
	if (!PropertyAccessNodeClass)
	{
		return MakeError(TEXT("K2Node_PropertyAccess class not found - PropertyAccessNode plugin may not be loaded"));
	}

	// Find the Path property via reflection
	FArrayProperty* PathProperty = CastField<FArrayProperty>(PropertyAccessNodeClass->FindPropertyByName(TEXT("Path")));
	if (!PathProperty)
	{
		return MakeError(TEXT("Could not find 'Path' property on K2Node_PropertyAccess via reflection"));
	}

	// Collect all graphs including sub-graphs
	TArray<UEdGraph*> AllGraphs;
	Blueprint->GetAllGraphs(AllGraphs);
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

	int32 NodesFixed = 0;
	int32 SegmentsRemoved = 0;
	TArray<TSharedPtr<FJsonValue>> NodeDetailsArray;

	for (UEdGraph* Graph : AllGraphs)
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node || !Node->IsA(PropertyAccessNodeClass)) continue;

			// Read Path via reflection
			TArray<FString>* PathPtr = PathProperty->ContainerPtrToValuePtr<TArray<FString>>(Node);
			if (!PathPtr || PathPtr->Num() == 0) continue;

			const int32 BeforeCount = PathPtr->Num();
			PathPtr->RemoveAll([&RemoveSegment](const FString& Segment)
			{
				return Segment.IsEmpty() || Segment == RemoveSegment;
			});
			const int32 AfterCount = PathPtr->Num();
			const int32 Removed = BeforeCount - AfterCount;
			if (Removed <= 0) continue;

			SegmentsRemoved += Removed;
			NodesFixed++;

			TSharedPtr<FJsonObject> NodeDetail = MakeShared<FJsonObject>();
			NodeDetail->SetStringField(TEXT("node_guid"), Node->NodeGuid.ToString());
			NodeDetail->SetStringField(TEXT("graph"), Graph->GetName());
			NodeDetail->SetNumberField(TEXT("segments_removed"), Removed);
			NodeDetailsArray.Add(MakeShared<FJsonValueObject>(NodeDetail));
		}
	}

	if (NodesFixed > 0)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("blueprint"), Blueprint->GetName());
	Data->SetNumberField(TEXT("nodes_fixed"), NodesFixed);
	Data->SetNumberField(TEXT("segments_removed"), SegmentsRemoved);
	Data->SetArrayField(TEXT("nodes"), NodeDetailsArray);
	return MakeResponse(true, Data);
}
