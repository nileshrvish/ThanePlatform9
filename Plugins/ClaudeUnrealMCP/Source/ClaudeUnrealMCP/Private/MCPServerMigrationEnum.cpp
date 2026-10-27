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

FString FMCPServer::HandleMigrateEnumReferences(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid())
	{
		return MakeError(TEXT("Missing parameters"));
	}

	FString SourceEnumPath = Params->GetStringField(TEXT("source_enum_path"));
	FString TargetEnumPath = Params->GetStringField(TEXT("target_enum_path"));
	bool bDryRun = Params->HasField(TEXT("dry_run")) ? Params->GetBoolField(TEXT("dry_run")) : false;
	bool bSkipStructFields = Params->HasField(TEXT("skip_struct_fields")) ? Params->GetBoolField(TEXT("skip_struct_fields")) : false;

	// Optional: skip specific blueprint paths (e.g. ABPs with PropertyAccess nodes that break on enum type change)
	TSet<FString> SkipBlueprintPaths;
	if (Params->HasField(TEXT("skip_blueprint_paths")))
	{
		const TArray<TSharedPtr<FJsonValue>>& SkipArray = Params->GetArrayField(TEXT("skip_blueprint_paths"));
		for (const auto& Val : SkipArray)
		{
			SkipBlueprintPaths.Add(Val->AsString());
		}
	}

	// Optional: only process a specific blueprint path (whitelist mode)
	FString OnlyBlueprintPath = Params->HasField(TEXT("blueprint_path")) ? Params->GetStringField(TEXT("blueprint_path")) : TEXT("");

	if (SourceEnumPath.IsEmpty() || TargetEnumPath.IsEmpty())
	{
		return MakeError(TEXT("Missing source_enum_path or target_enum_path"));
	}

	// Load old enum (UserDefinedEnum)
	UUserDefinedEnum* OldEnum = LoadObject<UUserDefinedEnum>(nullptr, *SourceEnumPath);
	if (!OldEnum)
	{
		return MakeError(FString::Printf(TEXT("Could not load source UserDefinedEnum: %s"), *SourceEnumPath));
	}

	// Find new enum (C++ UEnum)
	UEnum* NewEnum = nullptr;
	for (TObjectIterator<UEnum> It; It; ++It)
	{
		if (It->GetPathName() == TargetEnumPath)
		{
			NewEnum = *It;
			break;
		}
	}
	if (!NewEnum)
	{
		return MakeError(FString::Printf(TEXT("Could not find target UEnum: %s"), *TargetEnumPath));
	}

	// Build value name mapping: BP enum display names -> C++ enum names
	// BP UserDefinedEnum values have display names like "Walk" and internal names like "E_Gait::NewEnumerator0"
	// C++ enum has values like "E_Gait::Walk"
	TMap<FName, FName> ValueNameMap;          // OldInternalName -> NewInternalName
	TMap<FString, FString> DisplayToNewValue; // DisplayName -> NewInternalName (for default value remapping)
	TArray<TSharedPtr<FJsonValue>> ValueMappingsArray;
	for (int32 i = 0; i < OldEnum->NumEnums() - 1; ++i) // -1 to skip _MAX
	{
		FText DisplayText = OldEnum->GetDisplayNameTextByIndex(i);
		FString DisplayName = DisplayText.ToString();
		FName OldValueName = FName(*OldEnum->GetNameStringByIndex(i));

		// Find matching C++ enum value by display name
		for (int32 j = 0; j < NewEnum->NumEnums() - 1; ++j)
		{
			FString NewDisplayName = NewEnum->GetDisplayNameTextByIndex(j).ToString();
			if (NewDisplayName == DisplayName)
			{
				FName NewValueName = FName(*NewEnum->GetNameStringByIndex(j));
				ValueNameMap.Add(OldValueName, NewValueName);
				DisplayToNewValue.Add(DisplayName, NewValueName.ToString());

				TSharedPtr<FJsonObject> Mapping = MakeShared<FJsonObject>();
				Mapping->SetStringField(TEXT("display_name"), DisplayName);
				Mapping->SetStringField(TEXT("old_value"), OldValueName.ToString());
				Mapping->SetStringField(TEXT("new_value"), NewValueName.ToString());
				ValueMappingsArray.Add(MakeShared<FJsonValueObject>(Mapping));
				break;
			}
		}
	}

	// Build default value fix map for legacy and display values
	TMap<FString, FString> DefaultFixMap;
	for (const auto& Pair : DisplayToNewValue)
	{
		DefaultFixMap.Add(Pair.Key, Pair.Value);
	}
	for (int32 i = 0; i < NewEnum->NumEnums() - 1; ++i)
	{
		FString NewName = NewEnum->GetNameStringByIndex(i);
		if (NewName.Contains(TEXT("::")))
		{
			NewName = NewName.RightChop(NewName.Find(TEXT("::")) + 2);
		}
		DefaultFixMap.Add(FString::Printf(TEXT("NewEnumerator%d"), i), NewName);
	}

	auto RemapEnumDefault = [&DefaultFixMap](FString& Value) -> bool
	{
		if (Value.IsEmpty())
		{
			return false;
		}
		if (const FString* NewVal = DefaultFixMap.Find(Value))
		{
			if (*NewVal != Value)
			{
				Value = *NewVal;
				return true;
			}
		}
		return false;
	};

	// --- Phase 0: Update UserDefinedStruct field definitions ---
	// BP structs may have fields typed as BP enums. Break/Set/Make nodes derive
	// sub-pin types from the struct definition, so we must update the source.
	// SKIP when skip_struct_fields=true: avoids corrupting BP struct assets with C++ enum pointers
	// (causes CppForm assertion crash on reload when struct SubCategoryObject points to C++ UEnum)
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	int32 TotalStructFieldsFixed = 0;
	TArray<TSharedPtr<FJsonValue>> StructFieldReportsArray;
	TArray<TSharedPtr<FJsonValue>> StructDiagArray;
	if (bSkipStructFields)
	{
		// Phase 0 skipped — struct field SubCategoryObject left as BP enum to avoid CppForm crash
	}
	else
	{
		TArray<FAssetData> AllStructAssets;
		AssetRegistry.GetAssetsByClass(UUserDefinedStruct::StaticClass()->GetClassPathName(), AllStructAssets, true);

		FString OldEnumName = OldEnum->GetName();

		for (const FAssetData& StructAssetData : AllStructAssets)
		{
			UUserDefinedStruct* UDStruct = Cast<UUserDefinedStruct>(StructAssetData.GetAsset());
			if (!UDStruct) continue;

			TArray<FStructVariableDescription>& Variables = const_cast<TArray<FStructVariableDescription>&>(
				FStructureEditorUtils::GetVarDesc(UDStruct)
			);

			bool bStructModified = false;
			for (FStructVariableDescription& Variable : Variables)
			{
				// Diagnostic: dump ALL fields that have any SubCategoryObject referencing enum name
				FString SubCatPath = Variable.SubCategoryObject.ToSoftObjectPath().ToString();
				bool bSubCatMatchesName = SubCatPath.Contains(OldEnumName);

				// Also check the compiled FProperty for this field
				FString CompiledEnumPath;
				if (FProperty* Prop = UDStruct->FindPropertyByName(Variable.VarName))
				{
					if (FByteProperty* ByteProp = CastField<FByteProperty>(Prop))
					{
						if (ByteProp->Enum)
						{
							CompiledEnumPath = ByteProp->Enum->GetPathName();
							if (!bSubCatMatchesName && ByteProp->Enum->GetName() == OldEnumName)
							{
								bSubCatMatchesName = true; // compiled property references this enum
							}
						}
					}
					else if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Prop))
					{
						if (EnumProp->GetEnum())
						{
							CompiledEnumPath = EnumProp->GetEnum()->GetPathName();
							if (!bSubCatMatchesName && EnumProp->GetEnum()->GetName() == OldEnumName)
							{
								bSubCatMatchesName = true;
							}
						}
					}
				}

				if (bSubCatMatchesName)
				{
					TSharedPtr<FJsonObject> Diag = MakeShared<FJsonObject>();
					Diag->SetStringField(TEXT("struct"), UDStruct->GetName());
					Diag->SetStringField(TEXT("field"), Variable.FriendlyName);
					Diag->SetStringField(TEXT("var_name"), Variable.VarName.ToString());
					Diag->SetStringField(TEXT("category"), Variable.Category.ToString());
					Diag->SetStringField(TEXT("sub_category"), Variable.SubCategory.ToString());
					Diag->SetStringField(TEXT("sub_category_object_path"), SubCatPath);
					Diag->SetStringField(TEXT("compiled_enum_path"), CompiledEnumPath);

					UObject* ResolvedObj = Variable.SubCategoryObject.LoadSynchronous();
					Diag->SetStringField(TEXT("resolved_obj"), ResolvedObj ? ResolvedObj->GetPathName() : TEXT("null"));
					Diag->SetStringField(TEXT("old_enum_path"), OldEnum->GetPathName());
					Diag->SetBoolField(TEXT("matches_old_enum"), ResolvedObj == OldEnum);
					Diag->SetStringField(TEXT("resolved_class"), ResolvedObj ? ResolvedObj->GetClass()->GetName() : TEXT("null"));
					Diag->SetBoolField(TEXT("subcat_is_null"), SubCatPath.IsEmpty());

					StructDiagArray.Add(MakeShared<FJsonValueObject>(Diag));
				}

				// Try multiple matching strategies for the actual fix
				UObject* ResolvedObj = Variable.SubCategoryObject.LoadSynchronous();
				bool bMatchesOld = (ResolvedObj == OldEnum);

				// Also match by compiled property enum pointer
				if (!bMatchesOld && !CompiledEnumPath.IsEmpty())
				{
					if (FProperty* Prop = UDStruct->FindPropertyByName(Variable.VarName))
					{
						UEnum* CompiledEnum = nullptr;
						if (FByteProperty* ByteProp = CastField<FByteProperty>(Prop))
							CompiledEnum = ByteProp->Enum;
						else if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Prop))
							CompiledEnum = EnumProp->GetEnum();

						if (CompiledEnum == OldEnum)
							bMatchesOld = true;
					}
				}

				if (bMatchesOld)
				{
					if (!bDryRun)
					{
						Variable.SubCategoryObject = TSoftObjectPtr<UObject>(NewEnum);

						// Also directly update the COMPILED FByteProperty::Enum pointer
						// This is critical because Break/Make/SetFieldsInStruct nodes derive
						// pin types from the compiled property, not from SubCategoryObject
						if (FProperty* Prop = UDStruct->FindPropertyByName(Variable.VarName))
						{
							if (FByteProperty* ByteProp = CastField<FByteProperty>(Prop))
							{
								ByteProp->Enum = NewEnum;
							}
							else if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Prop))
							{
								// EnumProperty doesn't have a simple setter, but we can
								// use the internal pointer if available
							}
						}

						// Remap default value if set
						if (!Variable.DefaultValue.IsEmpty())
						{
							FName OldVal = FName(*Variable.DefaultValue);
							if (const FName* NewVal = ValueNameMap.Find(OldVal))
							{
								Variable.DefaultValue = NewVal->ToString();
							}
							else if (const FString* NewValStr = DisplayToNewValue.Find(Variable.DefaultValue))
							{
								Variable.DefaultValue = *NewValStr;
							}
						}
					}

					bStructModified = true;
					TotalStructFieldsFixed++;

					TSharedPtr<FJsonObject> FieldReport = MakeShared<FJsonObject>();
					FieldReport->SetStringField(TEXT("struct"), UDStruct->GetName());
					FieldReport->SetStringField(TEXT("field"), Variable.FriendlyName);
					StructFieldReportsArray.Add(MakeShared<FJsonValueObject>(FieldReport));
				}
			}

			if (bStructModified && !bDryRun)
			{
				UDStruct->MarkPackageDirty();
			}
		}
	}

	// Find all affected blueprints
	TArray<FAssetData> AllBlueprintAssets;
	AssetRegistry.GetAssetsByClass(UBlueprint::StaticClass()->GetClassPathName(), AllBlueprintAssets, true);

	TArray<UBlueprint*> AffectedBlueprints;
	for (const FAssetData& AssetData : AllBlueprintAssets)
	{
		UBlueprint* Blueprint = Cast<UBlueprint>(AssetData.GetAsset());
		if (!Blueprint) continue;

		// Whitelist: only process a specific blueprint
		if (!OnlyBlueprintPath.IsEmpty())
		{
			FString BPPath = Blueprint->GetPathName();
			FString PackagePath = Blueprint->GetOutermost()->GetPathName();
			if (BPPath != OnlyBlueprintPath && !BPPath.StartsWith(OnlyBlueprintPath + TEXT(".")) && PackagePath != OnlyBlueprintPath)
			{
				continue;
			}
		}

		// Skip blueprints in the skip list
		if (SkipBlueprintPaths.Num() > 0)
		{
			FString BPPath = Blueprint->GetPathName();
			// Strip _C suffix if present, and also check the package path
			FString PackagePath = Blueprint->GetOutermost()->GetPathName();
			bool bShouldSkip = false;
			for (const FString& SkipPath : SkipBlueprintPaths)
			{
				if (BPPath == SkipPath || BPPath.StartsWith(SkipPath + TEXT(".")) || PackagePath == SkipPath)
				{
					bShouldSkip = true;
					break;
				}
			}
			if (bShouldSkip)
			{
				continue;
			}
		}

		if (DoesBlueprintReferenceEnum(Blueprint, OldEnum))
		{
			AffectedBlueprints.Add(Blueprint);
		}
	}

	TArray<TSharedPtr<FJsonValue>> BlueprintReportsArray;
	int32 TotalPinsMigrated = 0;
	int32 TotalVariablesMigrated = 0;

	for (UBlueprint* Blueprint : AffectedBlueprints)
	{
		TSharedPtr<FJsonObject> BPReport = MakeShared<FJsonObject>();
		BPReport->SetStringField(TEXT("path"), Blueprint->GetPathName());
		BPReport->SetStringField(TEXT("name"), Blueprint->GetName());

		int32 PinCount = 0;
		int32 VarCount = 0;

		if (!bDryRun)
		{
			// Migrate variables
			for (FBPVariableDescription& Var : Blueprint->NewVariables)
			{
				if (Var.VarType.PinSubCategoryObject.Get() == OldEnum)
				{
					Var.VarType.PinSubCategoryObject = NewEnum;
					VarCount++;
				}
			}

			// Collect all graphs (use GetAllGraphs like struct migration)
			TArray<UEdGraph*> AllGraphs;
			Blueprint->GetAllGraphs(AllGraphs);

			// Add sub-graphs recursively (GetAllGraphs may miss node sub-graphs)
			TArray<UEdGraph*> GraphsToProcess = AllGraphs;
			while (GraphsToProcess.Num() > 0)
			{
				UEdGraph* Graph = GraphsToProcess.Pop();
				for (UEdGraphNode* Node : Graph->Nodes)
				{
					for (UEdGraph* SubGraph : Node->GetSubGraphs())
					{
						if (SubGraph && !AllGraphs.Contains(SubGraph))
						{
							AllGraphs.Add(SubGraph);
							GraphsToProcess.Add(SubGraph);
						}
					}
				}
			}

			// Migrate all pins in all graphs (NO ReconstructNode — pin names don't change for enums)
			for (UEdGraph* Graph : AllGraphs)
			{
				for (UEdGraphNode* Node : Graph->Nodes)
				{
					// Handle Switch on Enum nodes — update internal Enum reference
					// Do NOT call SetEnum() as it rebuilds pins and breaks connections
					if (UK2Node_SwitchEnum* SwitchNode = Cast<UK2Node_SwitchEnum>(Node))
					{
						if (SwitchNode->Enum == OldEnum)
						{
							// Build pin FName rename map: OldEnum::OldInternalName → NewEnum::NewValueName
							// Pin FNames use qualified format "EnumName::ValueName"
							FString OldEnumPrefix = OldEnum->GetName() + TEXT("::");
							FString NewEnumPrefix = NewEnum->GetName() + TEXT("::");

							// Remap EnumEntries from old internal names to new names
							for (FName& Entry : SwitchNode->EnumEntries)
							{
								if (const FName* NewVal = ValueNameMap.Find(Entry))
								{
									Entry = *NewVal;
								}
							}

							// Rename Switch node pin FNames from BP-style to C++-style
							// so they match what the C++ enum generates on reload
							for (UEdGraphPin* Pin : SwitchNode->Pins)
							{
								if (!Pin || Pin->Direction != EGPD_Output) continue;
								FString PinNameStr = Pin->PinName.ToString();
								// Check if pin name starts with the enum prefix (qualified name)
								if (PinNameStr.StartsWith(OldEnumPrefix))
								{
									FString OldValuePart = PinNameStr.RightChop(OldEnumPrefix.Len());
									FName OldValueFName(*OldValuePart);
									if (const FName* NewVal = ValueNameMap.Find(OldValueFName))
									{
										Pin->PinName = FName(*(NewEnumPrefix + NewVal->ToString()));
									}
								}
								else
								{
									// Try bare name (without prefix) in case pin uses display name
									FName BareName = Pin->PinName;
									if (const FName* NewVal = ValueNameMap.Find(BareName))
									{
										Pin->PinName = FName(*(NewEnumPrefix + NewVal->ToString()));
									}
								}
							}

							// Directly update enum pointer without reconstructing
							SwitchNode->Enum = NewEnum;
							PinCount++;
							// Fall through to also update pin types below
						}
					}

					// Handle Byte to Enum conversion nodes — update Enum field and reconstruct
					if (UK2Node_CastByteToEnum* CastNode = Cast<UK2Node_CastByteToEnum>(Node))
					{
						if (CastNode->Enum == OldEnum)
						{
							// Save connections (simple: just byte input and enum output)
							struct FCastPinConn { FName PinName; EEdGraphPinDirection Dir; FGuid RemoteGuid; FName RemotePin; };
							TArray<FCastPinConn> SavedConns;
							for (UEdGraphPin* Pin : CastNode->Pins)
							{
								if (!Pin) continue;
								for (UEdGraphPin* Linked : Pin->LinkedTo)
								{
									if (Linked && Linked->GetOwningNode())
										SavedConns.Add({Pin->PinName, Pin->Direction, Linked->GetOwningNode()->NodeGuid, Linked->PinName});
								}
								Pin->BreakAllPinLinks();
							}

							CastNode->Enum = NewEnum;
							CastNode->ReconstructNode();

							// Restore connections
							for (const FCastPinConn& C : SavedConns)
							{
								UEdGraphPin* OurPin = nullptr;
								for (UEdGraphPin* Pin : CastNode->Pins)
								{
									if (Pin && Pin->PinName == C.PinName && Pin->Direction == C.Dir) { OurPin = Pin; break; }
								}
								if (!OurPin)
								{
									// Try matching by direction only (pin names might change)
									for (UEdGraphPin* Pin : CastNode->Pins)
									{
										if (Pin && Pin->Direction == C.Dir && Pin->LinkedTo.Num() == 0) { OurPin = Pin; break; }
									}
								}
								if (OurPin)
								{
									for (UEdGraphNode* SN : Graph->Nodes)
									{
										if (SN && SN->NodeGuid == C.RemoteGuid)
										{
											for (UEdGraphPin* RP : SN->Pins)
											{
												if (RP && RP->PinName == C.RemotePin) { OurPin->MakeLinkTo(RP); break; }
											}
											break;
										}
									}
								}
							}
							PinCount++;
							continue; // Skip generic pin loop — reconstruction handled pins
						}
					}

					// Handle Select nodes — update private fields via reflection (no SetEnum/ReconstructNode)
					if (UK2Node_Select* SelectNode = Cast<UK2Node_Select>(Node))
					{
						if (SelectNode->GetEnum() == OldEnum)
						{
							UClass* SelectClass = SelectNode->GetClass();

							// 1. Update private Enum field
							FObjectProperty* EnumProp = CastField<FObjectProperty>(SelectClass->FindPropertyByName(TEXT("Enum")));
							if (EnumProp)
							{
								void** EnumPtr = EnumProp->ContainerPtrToValuePtr<void*>(SelectNode);
								*EnumPtr = NewEnum;
							}

							// 2. Update private IndexPinType.PinSubCategoryObject
							FStructProperty* IndexPinTypeProp = CastField<FStructProperty>(SelectClass->FindPropertyByName(TEXT("IndexPinType")));
							if (IndexPinTypeProp)
							{
								FEdGraphPinType* IndexPinTypePtr = IndexPinTypeProp->ContainerPtrToValuePtr<FEdGraphPinType>(SelectNode);
								IndexPinTypePtr->PinSubCategoryObject = NewEnum;
							}

							// 3. Build new EnumEntries from NewEnum and update private field
							TArray<FName> NewEnumEntries;
							for (int32 i = 0; i < NewEnum->NumEnums() - 1; ++i)
							{
								bool bHidden = NewEnum->HasMetaData(TEXT("Hidden"), i) || NewEnum->HasMetaData(TEXT("Spacer"), i);
								if (!bHidden)
								{
									NewEnumEntries.Add(FName(*NewEnum->GetNameStringByIndex(i)));
								}
							}

							FArrayProperty* EnumEntriesProp = CastField<FArrayProperty>(SelectClass->FindPropertyByName(TEXT("EnumEntries")));
							if (EnumEntriesProp)
							{
								TArray<FName>* EntriesPtr = EnumEntriesProp->ContainerPtrToValuePtr<TArray<FName>>(SelectNode);

								// 4. Rename option pins: map old entry names to new entry names
								// Old EnumEntries has the BP enum internal names (NewEnumerator0, etc.)
								TArray<FName> OldEntries = *EntriesPtr;
								for (int32 i = 0; i < OldEntries.Num() && i < NewEnumEntries.Num(); ++i)
								{
									FName OldPinName = OldEntries[i];
									FName NewPinName = NewEnumEntries[i];
									for (UEdGraphPin* Pin : SelectNode->Pins)
									{
										if (Pin && Pin->PinName == OldPinName)
										{
											Pin->PinName = NewPinName;
											// Also update default value if it matches old enum value
											if (!Pin->DefaultValue.IsEmpty())
											{
												FName OldVal = FName(*Pin->DefaultValue);
												if (const FName* NewVal = ValueNameMap.Find(OldVal))
												{
													Pin->DefaultValue = NewVal->ToString();
												}
												else if (const FString* NewValStr = DisplayToNewValue.Find(Pin->DefaultValue))
												{
													Pin->DefaultValue = *NewValStr;
												}
											}
											break;
										}
									}
								}

								// Write new EnumEntries
								*EntriesPtr = NewEnumEntries;
							}

							// 5. Update Index pin's PinSubCategoryObject
							for (UEdGraphPin* Pin : SelectNode->Pins)
							{
								if (Pin && Pin->PinName == TEXT("Index"))
								{
									Pin->PinType.PinSubCategoryObject = NewEnum;
									// Remap default value
									if (!Pin->DefaultValue.IsEmpty())
									{
										FName OldVal = FName(*Pin->DefaultValue);
										if (const FName* NewVal = ValueNameMap.Find(OldVal))
										{
											Pin->DefaultValue = NewVal->ToString();
										}
										else if (const FString* NewValStr = DisplayToNewValue.Find(Pin->DefaultValue))
										{
											Pin->DefaultValue = *NewValStr;
										}
									}
									break;
								}
							}

							PinCount++;
							continue; // Skip generic pin loop — Select fully handled
						}
					}

					for (UEdGraphPin* Pin : Node->Pins)
					{
						if (Pin->PinType.PinSubCategoryObject.Get() == OldEnum)
						{
							Pin->PinType.PinSubCategoryObject = NewEnum;

							// Remap default value: try internal name first, then display name
							bool bDefaultFixed = false;
							if (!Pin->DefaultValue.IsEmpty())
							{
								FName OldVal = FName(*Pin->DefaultValue);
								if (const FName* NewVal = ValueNameMap.Find(OldVal))
								{
									Pin->DefaultValue = NewVal->ToString();
									bDefaultFixed = true;
								}
								else if (const FString* NewValStr = DisplayToNewValue.Find(Pin->DefaultValue))
								{
									Pin->DefaultValue = *NewValStr;
									bDefaultFixed = true;
								}
								else
								{
									bDefaultFixed = RemapEnumDefault(Pin->DefaultValue);
								}
							}

							bool bAutoFixed = RemapEnumDefault(Pin->AutogeneratedDefaultValue);

							if (bDefaultFixed || bAutoFixed)
							{
								PinCount++;
							}
						}
						// Cleanup pass: fix pins already typed as NewEnum but with stale NewEnumerator* defaults
						// (happens when struct reconstruction set C++ enum type but preserved old default values)
						else if (Pin->PinType.PinSubCategoryObject.Get() == NewEnum && !Pin->DefaultValue.IsEmpty())
						{
							bool bDefaultFixed = false;
							FName OldVal = FName(*Pin->DefaultValue);
							if (const FName* NewVal = ValueNameMap.Find(OldVal))
							{
								Pin->DefaultValue = NewVal->ToString();
								bDefaultFixed = true;
							}
							else if (const FString* NewValStr = DisplayToNewValue.Find(Pin->DefaultValue))
							{
								Pin->DefaultValue = *NewValStr;
								bDefaultFixed = true;
							}
							else
							{
								bDefaultFixed = RemapEnumDefault(Pin->DefaultValue);
							}

							bool bAutoFixed = RemapEnumDefault(Pin->AutogeneratedDefaultValue);
							if (bDefaultFixed || bAutoFixed)
							{
								PinCount++;
							}
						}
					}
				}
			}

			// Finalize — no RefreshAllNodes to avoid breaking connections
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		}
		else
		{
			// Dry run: just count
			for (const FBPVariableDescription& Var : Blueprint->NewVariables)
			{
				if (Var.VarType.PinSubCategoryObject.Get() == OldEnum)
					VarCount++;
			}
			TArray<UEdGraph*> AllGraphs;
			AllGraphs.Append(Blueprint->UbergraphPages);
			AllGraphs.Append(Blueprint->FunctionGraphs);
			TArray<UEdGraph*> GraphsToProcess = AllGraphs;
			while (GraphsToProcess.Num() > 0)
			{
				UEdGraph* Graph = GraphsToProcess.Pop();
				for (UEdGraphNode* Node : Graph->Nodes)
				{
					for (UEdGraph* SubGraph : Node->GetSubGraphs())
					{
						if (SubGraph && !AllGraphs.Contains(SubGraph))
						{
							AllGraphs.Add(SubGraph);
							GraphsToProcess.Add(SubGraph);
						}
					}
				}
			}
			for (UEdGraph* Graph : AllGraphs)
			{
				for (UEdGraphNode* Node : Graph->Nodes)
				{
					for (UEdGraphPin* Pin : Node->Pins)
					{
						if (Pin->PinType.PinSubCategoryObject.Get() == OldEnum)
							PinCount++;
					}
				}
			}
		}

		TotalPinsMigrated += PinCount;
		TotalVariablesMigrated += VarCount;

		BPReport->SetNumberField(TEXT("pins_migrated"), PinCount);
		BPReport->SetNumberField(TEXT("variables_migrated"), VarCount);
		BlueprintReportsArray.Add(MakeShared<FJsonValueObject>(BPReport));
	}

	// Build response
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("dry_run"), bDryRun);
	Data->SetStringField(TEXT("source_enum"), OldEnum->GetPathName());
	Data->SetStringField(TEXT("target_enum"), NewEnum->GetPathName());
	Data->SetNumberField(TEXT("value_mappings_count"), ValueNameMap.Num());
	Data->SetArrayField(TEXT("value_name_mapping"), ValueMappingsArray);
	Data->SetNumberField(TEXT("blueprints_affected"), AffectedBlueprints.Num());
	Data->SetArrayField(TEXT("affected_blueprints"), BlueprintReportsArray);
	Data->SetNumberField(TEXT("total_pins_migrated"), TotalPinsMigrated);
	Data->SetNumberField(TEXT("total_variables_migrated"), TotalVariablesMigrated);
	Data->SetNumberField(TEXT("struct_fields_fixed"), TotalStructFieldsFixed);
	if (StructFieldReportsArray.Num() > 0)
	{
		Data->SetArrayField(TEXT("struct_fields"), StructFieldReportsArray);
	}
	if (StructDiagArray.Num() > 0)
	{
		Data->SetArrayField(TEXT("struct_diagnostics"), StructDiagArray);
	}
	Data->SetStringField(TEXT("message"),
		bDryRun ? TEXT("Dry run complete - no changes made") : TEXT("Enum migration complete"));

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleFixPinEnumType(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("blueprint_path")) ||
		!Params->HasField(TEXT("wrong_enum_path")) || !Params->HasField(TEXT("correct_enum_path")))
	{
		return MakeError(TEXT("Missing required parameters: blueprint_path, wrong_enum_path, correct_enum_path"));
	}

	FString BPPath = Params->GetStringField(TEXT("blueprint_path"));
	FString WrongEnumPath = Params->GetStringField(TEXT("wrong_enum_path"));
	FString CorrectEnumPath = Params->GetStringField(TEXT("correct_enum_path"));
	FString TargetDefaultValue = Params->HasField(TEXT("default_value")) ? Params->GetStringField(TEXT("default_value")) : TEXT("");
	FString TargetNodeGuid = Params->HasField(TEXT("node_guid")) ? Params->GetStringField(TEXT("node_guid")) : TEXT("");

	UBlueprint* Blueprint = LoadBlueprintFromPath(BPPath);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *BPPath));
	}

	// Find both enums
	UEnum* WrongEnum = nullptr;
	UEnum* CorrectEnum = nullptr;

	for (TObjectIterator<UEnum> It; It; ++It)
	{
		FString EnumPath = It->GetPathName();
		if (EnumPath == WrongEnumPath || It->GetName() == FPaths::GetCleanFilename(WrongEnumPath))
		{
			WrongEnum = *It;
		}
		if (EnumPath == CorrectEnumPath || It->GetName() == FPaths::GetCleanFilename(CorrectEnumPath))
		{
			CorrectEnum = *It;
		}
	}

	if (!WrongEnum) return MakeError(FString::Printf(TEXT("Wrong enum not found: %s"), *WrongEnumPath));
	if (!CorrectEnum) return MakeError(FString::Printf(TEXT("Correct enum not found: %s"), *CorrectEnumPath));

	// Iterate all graphs including function graphs and subgraphs
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

	int32 PinsFixed = 0;
	TArray<TSharedPtr<FJsonValue>> FixedPinsArray;

	for (UEdGraph* Graph : AllGraphs)
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node) continue;

			// If a target node GUID is specified, only fix that node
			if (!TargetNodeGuid.IsEmpty() && Node->NodeGuid.ToString() != TargetNodeGuid) continue;

			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (!Pin) continue;
				if (Pin->PinType.PinSubCategoryObject.Get() != WrongEnum) continue;

				// If a target default value is specified, only fix pins with matching default
				if (!TargetDefaultValue.IsEmpty() && Pin->DefaultValue != TargetDefaultValue) continue;

				Pin->PinType.PinSubCategoryObject = CorrectEnum;
				PinsFixed++;

				TSharedPtr<FJsonObject> PinReport = MakeShared<FJsonObject>();
				PinReport->SetStringField(TEXT("node"), Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
				PinReport->SetStringField(TEXT("node_guid"), Node->NodeGuid.ToString());
				PinReport->SetStringField(TEXT("pin_name"), Pin->PinName.ToString());
				PinReport->SetStringField(TEXT("default_value"), Pin->DefaultValue);
				PinReport->SetStringField(TEXT("graph"), Graph->GetName());
				FixedPinsArray.Add(MakeShared<FJsonValueObject>(PinReport));
			}

			// Also fix K2Node_CustomEvent::UserDefinedPins
			if (UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(Node))
			{
				for (TSharedPtr<FUserPinInfo>& PinInfo : CustomEvent->UserDefinedPins)
				{
					if (PinInfo.IsValid() && PinInfo->PinType.PinSubCategoryObject.Get() == WrongEnum)
					{
						PinInfo->PinType.PinSubCategoryObject = CorrectEnum;
					}
				}
			}
		}
	}

	if (PinsFixed > 0)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("blueprint"), Blueprint->GetName());
	Data->SetNumberField(TEXT("pins_fixed"), PinsFixed);
	Data->SetArrayField(TEXT("fixed_pins"), FixedPinsArray);
	Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Fixed %d pins from %s to %s"), PinsFixed, *WrongEnum->GetName(), *CorrectEnum->GetName()));

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleFixEnumDefaults(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("blueprint_path")) || !Params->HasField(TEXT("enum_path")))
	{
		return MakeError(TEXT("Missing required parameters: blueprint_path, enum_path"));
	}

	FString BPPath = Params->GetStringField(TEXT("blueprint_path"));
	FString EnumPath = Params->GetStringField(TEXT("enum_path"));
	FString OldEnumPath = Params->HasField(TEXT("old_enum_path")) ? Params->GetStringField(TEXT("old_enum_path")) : TEXT("");
	bool bDryRun = Params->HasField(TEXT("dry_run")) && Params->GetBoolField(TEXT("dry_run"));

	UBlueprint* Blueprint = LoadBlueprintFromPath(BPPath);
	if (!Blueprint) return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *BPPath));

	UEnum* TargetEnum = nullptr;
	for (TObjectIterator<UEnum> It; It; ++It)
	{
		if (It->GetPathName() == EnumPath)
		{
			TargetEnum = *It;
			break;
		}
	}
	if (!TargetEnum) return MakeError(FString::Printf(TEXT("Enum not found: %s"), *EnumPath));

	// Load old enum if specified (for mapping old internal names)
	UUserDefinedEnum* OldEnum = nullptr;
	if (!OldEnumPath.IsEmpty())
	{
		OldEnum = Cast<UUserDefinedEnum>(StaticLoadObject(UUserDefinedEnum::StaticClass(), nullptr, *OldEnumPath));
	}

	// Build map of short/old name -> valid default value for target enum
	FString EnumPrefix = TargetEnum->GetName() + TEXT("::");
	TMap<FString, FString> ValueFixMap; // Maps invalid default values to valid ones
	for (int32 i = 0; i < TargetEnum->NumEnums() - 1; ++i)
	{
		FString FullName = TargetEnum->GetNameStringByIndex(i);
		FString DisplayName = TargetEnum->GetDisplayNameTextByIndex(i).ToString();

		// Determine the valid default value format
		// Use the name form that GetIndexByNameString accepts
		FString ValidForm = FullName;

		// Check if GetNameStringByIndex returns short or qualified form
		FString ShortName = FullName.Contains(TEXT("::")) ? FullName.RightChop(FullName.Find(TEXT("::")) + 2) : FullName;
		FString QualifiedName = FullName.Contains(TEXT("::")) ? FullName : EnumPrefix + FullName;

		// Map various possible default values to the valid form
		if (ShortName != ValidForm) ValueFixMap.Add(ShortName, ValidForm);
		if (QualifiedName != ValidForm) ValueFixMap.Add(QualifiedName, ValidForm);
		if (DisplayName != ValidForm && DisplayName != ShortName) ValueFixMap.Add(DisplayName, ValidForm);
		// Also map legacy BP internal names (NewEnumeratorN) to the valid C++ enum value
		ValueFixMap.Add(FString::Printf(TEXT("NewEnumerator%d"), i), ValidForm);

		// If old enum provided, also map old internal names
		if (OldEnum)
		{
			FText OldDisplayText = OldEnum->GetDisplayNameTextByIndex(i);
			if (OldDisplayText.ToString() == DisplayName && i < OldEnum->NumEnums() - 1)
			{
				FString OldInternalName = OldEnum->GetNameStringByIndex(i);
				if (OldInternalName != ValidForm)
				{
					ValueFixMap.Add(OldInternalName, ValidForm);
				}
			}
		}
	}

	// Also build old enum internal name mapping by iterating old enum directly
	if (OldEnum)
	{
		for (int32 i = 0; i < OldEnum->NumEnums() - 1; ++i)
		{
			FString OldInternalName = OldEnum->GetNameStringByIndex(i);
			FString OldDisplayName = OldEnum->GetDisplayNameTextByIndex(i).ToString();
			// Find matching new enum value by display name
			for (int32 j = 0; j < TargetEnum->NumEnums() - 1; ++j)
			{
				FString NewDisplayName = TargetEnum->GetDisplayNameTextByIndex(j).ToString();
				if (NewDisplayName == OldDisplayName)
				{
					FString ValidForm = TargetEnum->GetNameStringByIndex(j);
					ValueFixMap.Add(OldInternalName, ValidForm);
					break;
				}
			}
		}
	}

	auto RemapEnumDefault = [&ValueFixMap](FString& Value) -> bool
	{
		if (Value.IsEmpty())
		{
			return false;
		}
		if (const FString* NewVal = ValueFixMap.Find(Value))
		{
			if (*NewVal != Value)
			{
				Value = *NewVal;
				return true;
			}
		}
		return false;
	};

	// Log the mapping for debugging
	TArray<TSharedPtr<FJsonValue>> MappingsArray;
	for (auto& Pair : ValueFixMap)
	{
		TSharedPtr<FJsonObject> M = MakeShared<FJsonObject>();
		M->SetStringField(TEXT("from"), Pair.Key);
		M->SetStringField(TEXT("to"), Pair.Value);
		MappingsArray.Add(MakeShared<FJsonValueObject>(M));
	}

	// Collect all graphs
	TArray<UEdGraph*> AllGraphs;
	Blueprint->GetAllGraphs(AllGraphs);
	{
		TArray<UEdGraph*> GraphsToProcess = AllGraphs;
		while (GraphsToProcess.Num() > 0)
		{
			UEdGraph* G = GraphsToProcess.Pop();
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
	}

	int32 PinsFixed = 0;
	TArray<TSharedPtr<FJsonValue>> FixedPinsArray;
	TArray<TSharedPtr<FJsonValue>> DiagnosticArray;

	// Also find ALL enum objects named the same (to catch BP/C++ duplicates)
	FString TargetEnumName = TargetEnum->GetName();
	TArray<UEnum*> AllSameNameEnums;
	for (TObjectIterator<UEnum> It; It; ++It)
	{
		if (It->GetName() == TargetEnumName)
		{
			AllSameNameEnums.Add(*It);
		}
	}

	for (UEdGraph* Graph : AllGraphs)
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node) continue;

			// Fix UK2Node_CastByteToEnum node-level Enum reference
			if (UK2Node_CastByteToEnum* CastNode = Cast<UK2Node_CastByteToEnum>(Node))
			{
				if (CastNode->Enum)
				{
					bool bOldEnum = false;
					for (UEnum* E : AllSameNameEnums)
					{
						if (CastNode->Enum == E && CastNode->Enum != TargetEnum) { bOldEnum = true; break; }
					}
					if (bOldEnum && !bDryRun)
					{
						CastNode->Enum = TargetEnum;
						PinsFixed++;
					}
				}
			}

			// Fix UK2Node_SwitchEnum node-level Enum reference
			if (UK2Node_SwitchEnum* SwitchNode = Cast<UK2Node_SwitchEnum>(Node))
			{
				if (SwitchNode->Enum)
				{
					bool bOldEnum = false;
					for (UEnum* E : AllSameNameEnums)
					{
						if (SwitchNode->Enum == E && SwitchNode->Enum != TargetEnum) { bOldEnum = true; break; }
					}
					if (bOldEnum && !bDryRun)
					{
						SwitchNode->Enum = TargetEnum;
						PinsFixed++;
					}
				}
			}

			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (!Pin) continue;
				UObject* PinEnumObj = Pin->PinType.PinSubCategoryObject.Get();
				if (!PinEnumObj) continue;

				// Check if this pin references ANY enum with the same name
				bool bMatchesTarget = (PinEnumObj == TargetEnum);
				bool bMatchesSameName = false;
				for (UEnum* E : AllSameNameEnums)
				{
					if (PinEnumObj == E) { bMatchesSameName = true; break; }
				}

				if (!bMatchesSameName) continue;

				if (!bMatchesTarget)
				{
					// Wrong enum object — fix PinSubCategoryObject to target
					if (!bDryRun)
					{
						Pin->PinType.PinSubCategoryObject = TargetEnum;
					}
					PinsFixed++;
				}

				if (Pin->DefaultValue.IsEmpty() && Pin->AutogeneratedDefaultValue.IsEmpty()) continue;

				// Report for diagnostic (only pins with defaults)
				TSharedPtr<FJsonObject> Diag = MakeShared<FJsonObject>();
				Diag->SetStringField(TEXT("graph"), Graph->GetName());
				Diag->SetStringField(TEXT("node"), Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
				Diag->SetStringField(TEXT("node_class"), Node->GetClass()->GetName());
				Diag->SetStringField(TEXT("node_guid"), Node->NodeGuid.ToString());
				Diag->SetStringField(TEXT("pin_name"), Pin->PinName.ToString());
				Diag->SetStringField(TEXT("default_value"), Pin->DefaultValue);
				Diag->SetStringField(TEXT("auto_default_value"), Pin->AutogeneratedDefaultValue);
				Diag->SetStringField(TEXT("enum_path"), PinEnumObj->GetPathName());
				Diag->SetBoolField(TEXT("is_target_enum"), bMatchesTarget);
				DiagnosticArray.Add(MakeShared<FJsonValueObject>(Diag));

				bool bDefaultFixed = false;
				bool bAutoFixed = false;
				FString OldDefault = Pin->DefaultValue;
				FString OldAuto = Pin->AutogeneratedDefaultValue;

				if (!Pin->DefaultValue.IsEmpty())
				{
					// If invalid for target enum, remap using ValueFixMap
					int32 ValIndex = TargetEnum->GetIndexByNameString(Pin->DefaultValue);
					if (ValIndex == INDEX_NONE)
					{
						if (!bDryRun)
						{
							bDefaultFixed = RemapEnumDefault(Pin->DefaultValue);
						}
						else
						{
							bDefaultFixed = RemapEnumDefault(OldDefault);
						}
					}
				}

				if (!Pin->AutogeneratedDefaultValue.IsEmpty())
				{
					if (!bDryRun)
					{
						bAutoFixed = RemapEnumDefault(Pin->AutogeneratedDefaultValue);
					}
					else
					{
						bAutoFixed = RemapEnumDefault(OldAuto);
					}
				}

				if (bDefaultFixed || bAutoFixed)
				{
					TSharedPtr<FJsonObject> Report = MakeShared<FJsonObject>();
					Report->SetStringField(TEXT("graph"), Graph->GetName());
					Report->SetStringField(TEXT("node"), Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
					Report->SetStringField(TEXT("node_guid"), Node->NodeGuid.ToString());
					Report->SetStringField(TEXT("pin_name"), Pin->PinName.ToString());
					Report->SetStringField(TEXT("old_value"), OldDefault);
					Report->SetStringField(TEXT("new_value"), Pin->DefaultValue);
					Report->SetStringField(TEXT("old_auto_value"), OldAuto);
					Report->SetStringField(TEXT("new_auto_value"), Pin->AutogeneratedDefaultValue);
					FixedPinsArray.Add(MakeShared<FJsonValueObject>(Report));
					PinsFixed++;
				}
			}

			// Also fix UserDefinedPins on FunctionEntry and FunctionResult nodes
			// These define function signatures and override pin types
			if (UK2Node_FunctionEntry* EntryNode = Cast<UK2Node_FunctionEntry>(Node))
			{
				for (TSharedPtr<FUserPinInfo>& PinInfo : EntryNode->UserDefinedPins)
				{
					if (PinInfo.IsValid())
					{
						UObject* PinInfoEnum = PinInfo->PinType.PinSubCategoryObject.Get();
						bool bInfoMatch = false;
						for (UEnum* E : AllSameNameEnums)
						{
							if (PinInfoEnum == E && PinInfoEnum != TargetEnum) { bInfoMatch = true; break; }
						}
						if (bInfoMatch && !bDryRun)
						{
							PinInfo->PinType.PinSubCategoryObject = TargetEnum;
							PinsFixed++;
						}
					}
				}
			}
			if (UK2Node_FunctionResult* ResultNode = Cast<UK2Node_FunctionResult>(Node))
			{
				for (TSharedPtr<FUserPinInfo>& PinInfo : ResultNode->UserDefinedPins)
				{
					if (PinInfo.IsValid())
					{
						UObject* PinInfoEnum = PinInfo->PinType.PinSubCategoryObject.Get();
						bool bInfoMatch = false;
						for (UEnum* E : AllSameNameEnums)
						{
							if (PinInfoEnum == E && PinInfoEnum != TargetEnum) { bInfoMatch = true; break; }
						}
						if (bInfoMatch && !bDryRun)
						{
							PinInfo->PinType.PinSubCategoryObject = TargetEnum;
							PinsFixed++;
						}
					}
				}
			}
			// Also fix K2Node_CustomEvent UserDefinedPins
			if (UK2Node_CustomEvent* EventNode = Cast<UK2Node_CustomEvent>(Node))
			{
				for (TSharedPtr<FUserPinInfo>& PinInfo : EventNode->UserDefinedPins)
				{
					if (PinInfo.IsValid())
					{
						UObject* PinInfoEnum = PinInfo->PinType.PinSubCategoryObject.Get();
						bool bInfoMatch = false;
						for (UEnum* E : AllSameNameEnums)
						{
							if (PinInfoEnum == E && PinInfoEnum != TargetEnum) { bInfoMatch = true; break; }
						}
						if (bInfoMatch && !bDryRun)
						{
							PinInfo->PinType.PinSubCategoryObject = TargetEnum;
							PinsFixed++;
						}
					}
				}
			}
		}
	}

	// Also fix Blueprint variables
	for (FBPVariableDescription& Var : Blueprint->NewVariables)
	{
		UObject* VarEnum = Var.VarType.PinSubCategoryObject.Get();
		bool bVarMatch = false;
		for (UEnum* E : AllSameNameEnums)
		{
			if (VarEnum == E && VarEnum != TargetEnum) { bVarMatch = true; break; }
		}
		if (bVarMatch && !bDryRun)
		{
			Var.VarType.PinSubCategoryObject = TargetEnum;
			PinsFixed++;
		}
	}

	if ((PinsFixed > 0 || DiagnosticArray.Num() > 0) && !bDryRun)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("blueprint"), Blueprint->GetName());
	Data->SetStringField(TEXT("enum"), TargetEnum->GetName());
	Data->SetStringField(TEXT("enum_path"), TargetEnum->GetPathName());
	Data->SetBoolField(TEXT("dry_run"), bDryRun);
	Data->SetNumberField(TEXT("pins_fixed"), PinsFixed);
	Data->SetNumberField(TEXT("same_name_enums"), AllSameNameEnums.Num());
	Data->SetNumberField(TEXT("graphs_searched"), AllGraphs.Num());
	Data->SetArrayField(TEXT("mappings"), MappingsArray);
	Data->SetArrayField(TEXT("diagnostic"), DiagnosticArray);
	Data->SetArrayField(TEXT("fixed_pins"), FixedPinsArray);

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleForceFixEnumPinDefaults(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("blueprint_path")) || !Params->HasField(TEXT("enum_path")))
	{
		return MakeError(TEXT("Missing required parameters: blueprint_path, enum_path"));
	}

	const FString BPPath = Params->GetStringField(TEXT("blueprint_path"));
	const FString EnumPath = Params->GetStringField(TEXT("enum_path"));
	const FString PinNameFilter = Params->HasField(TEXT("pin_name_contains")) ? Params->GetStringField(TEXT("pin_name_contains")) : FString();

	UBlueprint* Blueprint = LoadBlueprintFromPath(BPPath);
	if (!Blueprint) return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *BPPath));

	UEnum* TargetEnum = nullptr;
	for (TObjectIterator<UEnum> It; It; ++It)
	{
		if (It->GetPathName() == EnumPath)
		{
			TargetEnum = *It;
			break;
		}
	}
	if (!TargetEnum) return MakeError(FString::Printf(TEXT("Enum not found: %s"), *EnumPath));

	auto RemapNewEnumerator = [TargetEnum](const FString& InValue, FString& OutValue) -> bool
	{
		const FString Token = TEXT("NewEnumerator");
		const int32 TokenPos = InValue.Find(Token);
		if (TokenPos == INDEX_NONE)
		{
			return false;
		}

		FString Suffix = InValue.Mid(TokenPos + Token.Len());
		int32 Index = FCString::Atoi(*Suffix);
		const int32 MaxIndex = TargetEnum->NumEnums() - 2;
		if (Index < 0 || Index > MaxIndex)
		{
			Index = 0;
		}

		FString NewName = TargetEnum->GetNameStringByIndex(Index);
		if (NewName.Contains(TEXT("::")))
		{
			NewName = NewName.RightChop(NewName.Find(TEXT("::")) + 2);
		}
		OutValue = NewName;
		return true;
	};

	auto CollectAllPins = [](UEdGraphNode* Node, TArray<UEdGraphPin*>& OutPins)
	{
		if (!Node) return;
		TFunction<void(UEdGraphPin*)> CollectRecursive = [&CollectRecursive, &OutPins](UEdGraphPin* Pin)
		{
			if (!Pin) return;
			OutPins.Add(Pin);
			for (UEdGraphPin* SubPin : Pin->SubPins)
			{
				CollectRecursive(SubPin);
			}
		};
		for (UEdGraphPin* Pin : Node->Pins)
		{
			CollectRecursive(Pin);
		}
	};

	// Collect all graphs including sub-graphs
	TArray<UEdGraph*> AllGraphs;
	Blueprint->GetAllGraphs(AllGraphs);
	TArray<UEdGraph*> GraphsToProcess = AllGraphs;
	while (GraphsToProcess.Num() > 0)
	{
		UEdGraph* G = GraphsToProcess.Pop();
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

	int32 PinsFixed = 0;
	TArray<TSharedPtr<FJsonValue>> FixedPinsArray;
	const FString TargetEnumName = TargetEnum->GetName();

	for (UEdGraph* Graph : AllGraphs)
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node) continue;

			TArray<UEdGraphPin*> AllPins;
			CollectAllPins(Node, AllPins);

			for (UEdGraphPin* Pin : AllPins)
			{
				if (!Pin) continue;

				const FString PinSubCategory = Pin->PinType.PinSubCategory.ToString();
				UObject* PinEnumObj = Pin->PinType.PinSubCategoryObject.Get();
				const FString PinNameStr = Pin->PinName.ToString();
				const bool bEnumMatch =
					(PinEnumObj == TargetEnum) ||
					(!PinSubCategory.IsEmpty() && (PinSubCategory == TargetEnumName || PinSubCategory.EndsWith(TargetEnumName)));
				const bool bNameMatch = !PinNameFilter.IsEmpty() && PinNameStr.Contains(PinNameFilter);

				if (!bEnumMatch && !bNameMatch) continue;

				bool bDefaultFixed = false;
				bool bAutoFixed = false;
				FString OldDefault = Pin->DefaultValue;
				FString OldAuto = Pin->AutogeneratedDefaultValue;

				if (!Pin->DefaultValue.IsEmpty())
				{
					FString NewDefault;
					if (RemapNewEnumerator(Pin->DefaultValue, NewDefault))
					{
						Pin->DefaultValue = NewDefault;
						bDefaultFixed = true;
					}
				}

				if (!Pin->AutogeneratedDefaultValue.IsEmpty())
				{
					FString NewAuto;
					if (RemapNewEnumerator(Pin->AutogeneratedDefaultValue, NewAuto))
					{
						Pin->AutogeneratedDefaultValue = NewAuto;
						bAutoFixed = true;
					}
				}

				if (bDefaultFixed || bAutoFixed)
				{
					TSharedPtr<FJsonObject> Report = MakeShared<FJsonObject>();
					Report->SetStringField(TEXT("graph"), Graph->GetName());
					Report->SetStringField(TEXT("node"), Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
					Report->SetStringField(TEXT("node_guid"), Node->NodeGuid.ToString());
					Report->SetStringField(TEXT("pin_name"), PinNameStr);
					Report->SetStringField(TEXT("old_default"), OldDefault);
					Report->SetStringField(TEXT("new_default"), Pin->DefaultValue);
					Report->SetStringField(TEXT("old_auto_default"), OldAuto);
					Report->SetStringField(TEXT("new_auto_default"), Pin->AutogeneratedDefaultValue);
					FixedPinsArray.Add(MakeShared<FJsonValueObject>(Report));
					PinsFixed++;
				}
			}
		}
	}

	if (PinsFixed > 0)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("blueprint"), Blueprint->GetName());
	Data->SetStringField(TEXT("enum"), TargetEnum->GetName());
	Data->SetNumberField(TEXT("pins_fixed"), PinsFixed);
	Data->SetArrayField(TEXT("fixed_pins"), FixedPinsArray);
	return MakeResponse(true, Data);
}
