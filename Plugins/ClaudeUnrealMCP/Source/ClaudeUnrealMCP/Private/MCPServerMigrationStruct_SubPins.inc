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

FString FMCPServer::HandleFixStructSubPins(const TSharedPtr<FJsonObject>& Params)
{
	FString SourceStructPath = Params->GetStringField(TEXT("source_struct_path"));
	FString TargetStructPath = Params->GetStringField(TEXT("target_struct_path"));
	bool bDryRun = Params->HasField(TEXT("dry_run")) ? Params->GetBoolField(TEXT("dry_run")) : false;
	FString SpecificBPPath = Params->HasField(TEXT("blueprint_path")) ? Params->GetStringField(TEXT("blueprint_path")) : TEXT("");
	bool bReconstructEvents = Params->HasField(TEXT("reconstruct_events")) ? Params->GetBoolField(TEXT("reconstruct_events")) : false;

	// Load old BP struct
	UUserDefinedStruct* OldStruct = Cast<UUserDefinedStruct>(
		StaticLoadObject(UScriptStruct::StaticClass(), nullptr, *SourceStructPath));
	if (!OldStruct)
	{
		return MakeError(FString::Printf(TEXT("Source BP struct not found: %s"), *SourceStructPath));
	}

	// Find new C++ struct
	FString StructNameToFind = TargetStructPath;
	if (TargetStructPath.StartsWith(TEXT("/Script/")))
	{
		FString Remainder = TargetStructPath.RightChop(8);
		FString ModuleName;
		Remainder.Split(TEXT("."), &ModuleName, &StructNameToFind);
	}
	UScriptStruct* NewStruct = nullptr;
	for (TObjectIterator<UScriptStruct> It; It; ++It)
	{
		if (It->GetName() == StructNameToFind) { NewStruct = *It; break; }
	}
	if (!NewStruct)
	{
		return MakeError(FString::Printf(TEXT("Target C++ struct not found: %s"), *TargetStructPath));
	}

	// Build FriendlyName → C++ property name mapping
	TMap<FName, FName> FriendlyToPropertyMap;
	// Build VarName (GUID-suffixed) → C++ property name mapping for sub-pin renaming
	TMap<FString, FString> GUIDToCleanMap;
	const TArray<FStructVariableDescription>& VarDescs = FStructureEditorUtils::GetVarDesc(OldStruct);
	for (const FStructVariableDescription& Desc : VarDescs)
	{
		// Get the clean name from VarName (strip GUID suffix)
		FString VarNameStr = Desc.VarName.ToString();
		// Format: "FieldName_Number_GUID" — find the matching C++ property
		for (TFieldIterator<FProperty> PropIt(NewStruct); PropIt; ++PropIt)
		{
			FProperty* Prop = *PropIt;
			// Check if VarNameStr starts with the property name
			if (VarNameStr.StartsWith(Prop->GetName()))
			{
				FriendlyToPropertyMap.Add(FName(*Desc.FriendlyName), FName(*Prop->GetName()));
				GUIDToCleanMap.Add(VarNameStr, Prop->GetName());
				break;
			}
		}
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("dry_run"), bDryRun);

	// Build mappings array for report
	TArray<TSharedPtr<FJsonValue>> MappingsArray;
	for (const auto& Pair : FriendlyToPropertyMap)
	{
		TSharedPtr<FJsonObject> M = MakeShared<FJsonObject>();
		M->SetStringField(TEXT("friendly_name"), Pair.Key.ToString());
		M->SetStringField(TEXT("property_name"), Pair.Value.ToString());
		MappingsArray.Add(MakeShared<FJsonValueObject>(M));
	}
	Data->SetArrayField(TEXT("field_mappings"), MappingsArray);

	// Find affected blueprints
	TArray<UBlueprint*> BlueprintsToProcess;
	if (!SpecificBPPath.IsEmpty())
	{
		UBlueprint* BP = LoadBlueprintFromPath(SpecificBPPath);
		if (BP) BlueprintsToProcess.Add(BP);
	}
	else
	{
		FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		TArray<FAssetData> AllBPs;
		ARM.Get().GetAssetsByClass(UBlueprint::StaticClass()->GetClassPathName(), AllBPs, true);
		for (const FAssetData& AD : AllBPs)
		{
			UBlueprint* BP = Cast<UBlueprint>(AD.GetAsset());
			if (BP) BlueprintsToProcess.Add(BP);
		}
	}

	int32 TotalPinsRenamed = 0;
	int32 TotalEventsReconstructed = 0;
	TArray<TSharedPtr<FJsonValue>> BPReportsArray;

	for (UBlueprint* Blueprint : BlueprintsToProcess)
	{
		int32 BPPinsRenamed = 0;
		int32 BPEventsReconstructed = 0;

		TArray<UEdGraph*> AllGraphs;
		Blueprint->GetAllGraphs(AllGraphs);
		// Recursively add sub-graphs
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

		for (UEdGraph* Graph : AllGraphs)
		{
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (!Node) continue;

				// Rename struct sub-pins that use old friendly names or GUID-suffixed names
				for (UEdGraphPin* Pin : Node->Pins)
				{
					if (!Pin) continue;

					// Check if this pin is struct-typed with the new C++ struct
					bool bIsTargetStruct = (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct &&
						Pin->PinType.PinSubCategoryObject.Get() == NewStruct);
					// Check if this pin is a sub-pin of a struct parent (has ParentPin with struct type)
					bool bIsSubPinOfStruct = (Pin->ParentPin != nullptr &&
						Pin->ParentPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct &&
						Pin->ParentPin->PinType.PinSubCategoryObject.Get() == NewStruct);

					// If this is a sub-pin of a C++ struct parent, try GUID-suffixed rename
					if (bIsSubPinOfStruct)
					{
						FString ParentPrefix = Pin->ParentPin->PinName.ToString() + TEXT("_");
						FString PinNameStr = Pin->PinName.ToString();
						if (PinNameStr.StartsWith(ParentPrefix))
						{
							FString FieldPart = PinNameStr.RightChop(ParentPrefix.Len());
							if (FString* CleanName = GUIDToCleanMap.Find(FieldPart))
							{
								FString NewPinName = ParentPrefix + *CleanName;
								if (!bDryRun)
								{
									Pin->PinName = FName(*NewPinName);
								}
								BPPinsRenamed++;
							}
						}
					}

					// Also check if the pin itself or its parent references the struct
					bool bPinNameMatches = FriendlyToPropertyMap.Contains(Pin->PinName);

					if (bPinNameMatches)
					{
						const FName* NewName = FriendlyToPropertyMap.Find(Pin->PinName);
						if (NewName && !bDryRun)
						{
							Pin->PinName = *NewName;
							BPPinsRenamed++;
						}
						else if (NewName)
						{
							BPPinsRenamed++;
						}
					}

					// For struct-typed pins with sub-pins, rename GUID-suffixed sub-pin names
					if (bIsTargetStruct && Pin->SubPins.Num() > 0)
					{
						FString ParentPrefix = Pin->PinName.ToString() + TEXT("_");
						for (UEdGraphPin* SubPin : Pin->SubPins)
						{
							if (!SubPin) continue;
							FString SubPinName = SubPin->PinName.ToString();
							if (SubPinName.StartsWith(ParentPrefix))
							{
								FString FieldPart = SubPinName.RightChop(ParentPrefix.Len());
								// Check GUID-suffixed map first
								if (FString* CleanName = GUIDToCleanMap.Find(FieldPart))
								{
									FString NewPinName = ParentPrefix + *CleanName;
									if (!bDryRun)
									{
										SubPin->PinName = FName(*NewPinName);
									}
									BPPinsRenamed++;
								}
								// Also check FriendlyName map (for non-GUID names)
								else if (FriendlyToPropertyMap.Contains(FName(*FieldPart)))
								{
									const FName* NewName = FriendlyToPropertyMap.Find(FName(*FieldPart));
									if (NewName)
									{
										FString NewPinName = ParentPrefix + NewName->ToString();
										if (!bDryRun)
										{
											SubPin->PinName = FName(*NewPinName);
										}
										BPPinsRenamed++;
									}
								}
							}
						}
					}
					// Legacy: also check sub-pins by exact name match
					else
					{
						for (UEdGraphPin* SubPin : Pin->SubPins)
						{
							if (SubPin && FriendlyToPropertyMap.Contains(SubPin->PinName))
							{
								const FName* NewName = FriendlyToPropertyMap.Find(SubPin->PinName);
								if (NewName && !bDryRun)
								{
									SubPin->PinName = *NewName;
									BPPinsRenamed++;
								}
								else if (NewName)
								{
									BPPinsRenamed++;
								}
							}
						}
					}
				}

				// Reconstruct event nodes that reference the struct
				if (bReconstructEvents && !bDryRun)
				{
					if (Node->IsA<UK2Node_CustomEvent>() || Node->IsA<UK2Node_Event>())
					{
						bool bHasStructPin = false;
						for (UEdGraphPin* Pin : Node->Pins)
						{
							if (Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct &&
								Pin->PinType.PinSubCategoryObject.Get() == NewStruct)
							{
								bHasStructPin = true;
								break;
							}
						}
						if (bHasStructPin)
						{
							Node->ReconstructNode();
							BPEventsReconstructed++;
						}
					}
				}
			}
		}

		if (BPPinsRenamed > 0 || BPEventsReconstructed > 0)
		{
			if (!bDryRun)
			{
				FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
			}

			TSharedPtr<FJsonObject> BPReport = MakeShared<FJsonObject>();
			BPReport->SetStringField(TEXT("path"), Blueprint->GetPathName());
			BPReport->SetStringField(TEXT("name"), Blueprint->GetName());
			BPReport->SetNumberField(TEXT("pins_renamed"), BPPinsRenamed);
			BPReport->SetNumberField(TEXT("events_reconstructed"), BPEventsReconstructed);
			BPReportsArray.Add(MakeShared<FJsonValueObject>(BPReport));

			TotalPinsRenamed += BPPinsRenamed;
			TotalEventsReconstructed += BPEventsReconstructed;
		}
	}

	Data->SetNumberField(TEXT("blueprints_affected"), BPReportsArray.Num());
	Data->SetArrayField(TEXT("affected_blueprints"), BPReportsArray);
	Data->SetNumberField(TEXT("total_pins_renamed"), TotalPinsRenamed);
	Data->SetNumberField(TEXT("total_events_reconstructed"), TotalEventsReconstructed);
	Data->SetStringField(TEXT("message"), bDryRun ? TEXT("Dry run complete") : TEXT("Struct sub-pins fixed"));

	return MakeResponse(true, Data);
}
