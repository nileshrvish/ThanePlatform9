#include "MCPServer.h"
#include "Chooser.h"
#include "ChooserPropertyAccess.h"
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

FString FMCPServer::HandleFixAssetStructReference(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("asset_path")) ||
		!Params->HasField(TEXT("old_struct_path")) || !Params->HasField(TEXT("new_struct_path")))
	{
		return MakeError(TEXT("Missing required parameters: asset_path, old_struct_path, new_struct_path"));
	}

	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString OldStructPath = Params->GetStringField(TEXT("old_struct_path"));
	FString NewStructPath = Params->GetStringField(TEXT("new_struct_path"));

	// Load old struct
	FString FullOldPath = OldStructPath;
	if (!FullOldPath.Contains(TEXT(".")))
	{
		FullOldPath = OldStructPath + TEXT(".") + FPaths::GetCleanFilename(OldStructPath);
	}
	UScriptStruct* OldStruct = LoadObject<UScriptStruct>(nullptr, *FullOldPath);
	if (!OldStruct)
	{
		OldStruct = LoadObject<UScriptStruct>(nullptr, *OldStructPath);
	}
	if (!OldStruct)
	{
		return MakeError(FString::Printf(TEXT("Old struct not found: %s"), *OldStructPath));
	}

	// Load new struct
	UScriptStruct* NewStruct = nullptr;
	FString TypeNameToFind = NewStructPath;
	if (NewStructPath.StartsWith(TEXT("/Script/")))
	{
		FString Remainder = NewStructPath;
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
		return MakeError(FString::Printf(TEXT("New struct not found: %s"), *NewStructPath));
	}

	// Load the asset
	FString FullAssetPath = AssetPath;
	if (!FullAssetPath.Contains(TEXT(".")))
	{
		FullAssetPath = AssetPath + TEXT(".") + FPaths::GetCleanFilename(AssetPath);
	}
	UObject* Asset = StaticLoadObject(UObject::StaticClass(), nullptr, *FullAssetPath);
	if (!Asset)
	{
		Asset = StaticLoadObject(UObject::StaticClass(), nullptr, *AssetPath);
	}
	if (!Asset)
	{
		return MakeError(FString::Printf(TEXT("Asset not found: %s"), *AssetPath));
	}

	// Deep property traversal to find struct references (including inside FInstancedStruct)
	int32 PropertiesFixed = 0;
	TArray<TSharedPtr<FJsonValue>> FixedArray;

	// Get FInstancedStruct type for special handling
	UScriptStruct* InstancedStructType = TBaseStructure<FInstancedStruct>::Get();

	// Recursive lambda to walk struct data
	TSet<void*> VisitedPtrs;
	TFunction<void(void*, const UStruct*, const FString&)> WalkStructData;
	WalkStructData = [&](void* DataPtr, const UStruct* StructType, const FString& Prefix)
	{
		if (!DataPtr || !StructType) return;
		if (VisitedPtrs.Contains(DataPtr)) return;
		VisitedPtrs.Add(DataPtr);

		for (TFieldIterator<FProperty> PropIt(StructType); PropIt; ++PropIt)
		{
			FProperty* Prop = *PropIt;
			FString PropPath = Prefix + Prop->GetName();

			// Check FObjectPropertyBase values (TObjectPtr<UScriptStruct>, etc.)
			if (FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(Prop))
			{
				void* ValuePtr = ObjProp->ContainerPtrToValuePtr<void>(DataPtr);
				UObject* ObjValue = ObjProp->GetObjectPropertyValue(ValuePtr);
				if (ObjValue == OldStruct)
				{
					ObjProp->SetObjectPropertyValue(ValuePtr, NewStruct);
					PropertiesFixed++;

					TSharedPtr<FJsonObject> Report = MakeShared<FJsonObject>();
					Report->SetStringField(TEXT("property"), PropPath);
					Report->SetStringField(TEXT("type"), TEXT("ObjectProperty"));
					FixedArray.Add(MakeShared<FJsonValueObject>(Report));
				}
			}

			// Recurse into struct properties
			if (FStructProperty* StructProp = CastField<FStructProperty>(Prop))
			{
				void* StructDataPtr = StructProp->ContainerPtrToValuePtr<void>(DataPtr);

				// Special handling for FInstancedStruct
				if (StructProp->Struct == InstancedStructType)
				{
					FInstancedStruct* Instanced = static_cast<FInstancedStruct*>(StructDataPtr);
					if (Instanced && Instanced->IsValid())
					{
						const UScriptStruct* InnerType = Instanced->GetScriptStruct();
						if (InnerType == OldStruct)
						{
							// The FInstancedStruct itself holds the old struct type — reinit with new type
							Instanced->InitializeAs(NewStruct);
							PropertiesFixed++;

							TSharedPtr<FJsonObject> Report = MakeShared<FJsonObject>();
							Report->SetStringField(TEXT("property"), PropPath + TEXT(" (FInstancedStruct::ScriptStruct)"));
							Report->SetStringField(TEXT("type"), TEXT("FInstancedStruct type"));
							FixedArray.Add(MakeShared<FJsonValueObject>(Report));
						}
						else if (InnerType)
						{
							// Recurse into the FInstancedStruct's data to find nested references
							void* InnerData = Instanced->GetMutableMemory();
							if (InnerData)
							{
								WalkStructData(InnerData, InnerType, PropPath + TEXT("."));
							}
						}
					}
				}
				else
				{
					// Regular struct — recurse
					WalkStructData(StructDataPtr, StructProp->Struct, PropPath + TEXT("."));
				}
			}

			// Recurse into arrays
			if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Prop))
			{
				FScriptArrayHelper ArrayHelper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(DataPtr));
				for (int32 i = 0; i < ArrayHelper.Num(); i++)
				{
					FString ElemPath = FString::Printf(TEXT("%s[%d]."), *PropPath, i);
					void* ElemPtr = ArrayHelper.GetRawPtr(i);

					if (FStructProperty* InnerStructProp = CastField<FStructProperty>(ArrayProp->Inner))
					{
						// Special handling for TArray<FInstancedStruct>
						if (InnerStructProp->Struct == InstancedStructType)
						{
							FInstancedStruct* Instanced = static_cast<FInstancedStruct*>(ElemPtr);
							if (Instanced && Instanced->IsValid())
							{
								const UScriptStruct* InnerType = Instanced->GetScriptStruct();
								if (InnerType == OldStruct)
								{
									Instanced->InitializeAs(NewStruct);
									PropertiesFixed++;

									TSharedPtr<FJsonObject> Report = MakeShared<FJsonObject>();
									Report->SetStringField(TEXT("property"), ElemPath + TEXT("(FInstancedStruct::ScriptStruct)"));
									Report->SetStringField(TEXT("type"), TEXT("FInstancedStruct type in array"));
									FixedArray.Add(MakeShared<FJsonValueObject>(Report));
								}
								else if (InnerType)
								{
									void* InnerData = Instanced->GetMutableMemory();
									if (InnerData)
									{
										WalkStructData(InnerData, InnerType, ElemPath);
									}
								}
							}
						}
						else
						{
							// Regular struct array element — recurse
							WalkStructData(ElemPtr, InnerStructProp->Struct, ElemPath);
						}
					}

					// Object array elements
					if (FObjectPropertyBase* InnerObjProp = CastField<FObjectPropertyBase>(ArrayProp->Inner))
					{
						UObject* ObjValue = InnerObjProp->GetObjectPropertyValue(ElemPtr);
						if (ObjValue == OldStruct)
						{
							InnerObjProp->SetObjectPropertyValue(ElemPtr, NewStruct);
							PropertiesFixed++;

							TSharedPtr<FJsonObject> Report = MakeShared<FJsonObject>();
							Report->SetStringField(TEXT("property"), ElemPath);
							Report->SetStringField(TEXT("type"), TEXT("ObjectProperty in array"));
							FixedArray.Add(MakeShared<FJsonValueObject>(Report));
						}
					}
				}
			}
		}
	};

	// Walk the main asset object and all subobjects
	TSet<UObject*> VisitedObjs;
	TArray<UObject*> ObjQueue;
	ObjQueue.Add(Asset);
	TArray<UObject*> SubObjects;
	GetObjectsWithOuter(Asset, SubObjects, true);
	ObjQueue.Append(SubObjects);

	for (UObject* Obj : ObjQueue)
	{
		if (!Obj || VisitedObjs.Contains(Obj)) continue;
		VisitedObjs.Add(Obj);
		WalkStructData(Obj, Obj->GetClass(), Obj == Asset ? TEXT("") : Obj->GetName() + TEXT("."));
	}

	if (PropertiesFixed > 0)
	{
		Asset->MarkPackageDirty();
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("asset"), Asset->GetPathName());
	Data->SetStringField(TEXT("asset_class"), Asset->GetClass()->GetName());
	Data->SetNumberField(TEXT("properties_fixed"), PropertiesFixed);
	Data->SetArrayField(TEXT("fixed_properties"), FixedArray);
	Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Fixed %d struct references in %s"), PropertiesFixed, *Asset->GetName()));

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleMigrateChooserTable(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("chooser_path")) ||
		!Params->HasField(TEXT("field_name_map")))
	{
		return MakeError(TEXT("Missing required parameters: chooser_path, field_name_map"));
	}

	FString ChooserPath = Params->GetStringField(TEXT("chooser_path"));
	const TSharedPtr<FJsonObject>* FieldNameMapPtr = nullptr;
	if (!Params->TryGetObjectField(TEXT("field_name_map"), FieldNameMapPtr) || !FieldNameMapPtr)
	{
		return MakeError(TEXT("field_name_map must be a JSON object mapping old names to new names"));
	}
	const TSharedPtr<FJsonObject>& FieldNameMap = *FieldNameMapPtr;

	// Optional: struct_map to replace ContextData struct pointers
	// Maps old struct name (e.g. "S_TraversalChooserInputs") to new C++ struct path
	TMap<FString, UScriptStruct*> StructMap;
	const TSharedPtr<FJsonObject>* StructMapPtr = nullptr;
	if (Params->TryGetObjectField(TEXT("struct_map"), StructMapPtr) && StructMapPtr)
	{
		for (const auto& Pair : (*StructMapPtr)->Values)
		{
			FString OldStructName = FString(Pair.Key);
			FString NewStructPath;
			if (!Pair.Value.IsValid() || !Pair.Value->TryGetString(NewStructPath)) continue;

			FString TypeNameToFind = NewStructPath;
			if (NewStructPath.StartsWith(TEXT("/Script/")))
			{
				FString Remainder = NewStructPath;
				Remainder.RemoveFromStart(TEXT("/Script/"));
				FString ModuleName;
				Remainder.Split(TEXT("."), &ModuleName, &TypeNameToFind);
			}
			UScriptStruct* FoundStruct = nullptr;
			for (TObjectIterator<UScriptStruct> It; It; ++It)
			{
				if (It->GetName() == TypeNameToFind)
				{
					FoundStruct = *It;
					break;
				}
			}
			if (!FoundStruct)
			{
				return MakeError(FString::Printf(TEXT("New struct not found: %s (for old struct %s)"), *NewStructPath, *OldStructName));
			}
			StructMap.Add(OldStructName, FoundStruct);
		}
	}
	// Also support legacy single new_struct_path parameter
	if (Params->HasField(TEXT("new_struct_path")) && StructMap.Num() == 0)
	{
		FString NewStructPath = Params->GetStringField(TEXT("new_struct_path"));
		FString TypeNameToFind = NewStructPath;
		if (NewStructPath.StartsWith(TEXT("/Script/")))
		{
			FString Remainder = NewStructPath;
			Remainder.RemoveFromStart(TEXT("/Script/"));
			FString ModuleName;
			Remainder.Split(TEXT("."), &ModuleName, &TypeNameToFind);
		}
		UScriptStruct* NewStruct = nullptr;
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
			return MakeError(FString::Printf(TEXT("New struct not found: %s"), *NewStructPath));
		}
		// Apply to all struct contexts
		StructMap.Add(TEXT("*"), NewStruct);
	}

	// Load the Chooser Table asset
	FString FullPath = ChooserPath;
	if (!FullPath.Contains(TEXT(".")))
	{
		FullPath = ChooserPath + TEXT(".") + FPaths::GetCleanFilename(ChooserPath);
	}
	UObject* AssetObj = StaticLoadObject(UObject::StaticClass(), nullptr, *FullPath);
	if (!AssetObj)
	{
		AssetObj = StaticLoadObject(UObject::StaticClass(), nullptr, *ChooserPath);
	}
	if (!AssetObj)
	{
		return MakeError(FString::Printf(TEXT("Chooser table not found: %s"), *ChooserPath));
	}

	UChooserTable* ChooserTable = Cast<UChooserTable>(AssetObj);
	if (!ChooserTable)
	{
		return MakeError(FString::Printf(TEXT("Asset is not a UChooserTable: %s (class: %s)"),
			*ChooserPath, *AssetObj->GetClass()->GetName()));
	}

	// Build the GUID-to-clean name map
	TMap<FName, FName> NameMap;
	for (const auto& Pair : FieldNameMap->Values)
	{
		FString OldName = FString(Pair.Key);
		FString NewName;
		if (Pair.Value.IsValid() && Pair.Value->TryGetString(NewName))
		{
			NameMap.Add(FName(*OldName), FName(*NewName));
		}
	}

	TArray<TSharedPtr<FJsonValue>> ReportArray;
	int32 BindingsFixed = 0;
	int32 ContextDataFixed = 0;

	// --- Phase 1: Update ContextData struct pointers ---
	if (StructMap.Num() > 0)
	{
		for (int32 i = 0; i < ChooserTable->ContextData.Num(); i++)
		{
			FInstancedStruct& CtxEntry = ChooserTable->ContextData[i];
			if (!CtxEntry.IsValid()) continue;

			const UScriptStruct* EntryType = CtxEntry.GetScriptStruct();
			if (!EntryType) continue;

			// Look for FContextObjectTypeStruct which has a Struct property
			void* EntryData = CtxEntry.GetMutableMemory();
			if (!EntryData) continue;

			for (TFieldIterator<FObjectPropertyBase> PropIt(EntryType); PropIt; ++PropIt)
			{
				FObjectPropertyBase* ObjProp = *PropIt;
				if (ObjProp->GetName() == TEXT("Struct"))
				{
					void* ValuePtr = ObjProp->ContainerPtrToValuePtr<void>(EntryData);
					UObject* CurrentValue = ObjProp->GetObjectPropertyValue(ValuePtr);

					if (CurrentValue && CurrentValue->IsA<UScriptStruct>())
					{
						UScriptStruct* CurrentStruct = Cast<UScriptStruct>(CurrentValue);
						FString CurrentStructName = CurrentStruct ? CurrentStruct->GetName() : TEXT("null");

						// Find the replacement: try exact match first, then wildcard
						UScriptStruct* ReplacementStruct = nullptr;
						if (UScriptStruct** Found = StructMap.Find(CurrentStructName))
						{
							ReplacementStruct = *Found;
						}
						else if (UScriptStruct** Wildcard = StructMap.Find(TEXT("*")))
						{
							ReplacementStruct = *Wildcard;
						}

						if (ReplacementStruct)
						{
							ObjProp->SetObjectPropertyValue(ValuePtr, ReplacementStruct);
							ContextDataFixed++;

							TSharedPtr<FJsonObject> Report = MakeShared<FJsonObject>();
							Report->SetStringField(TEXT("action"), TEXT("context_data_updated"));
							Report->SetNumberField(TEXT("index"), i);
							Report->SetStringField(TEXT("old_struct"), CurrentStructName);
							Report->SetStringField(TEXT("new_struct"), ReplacementStruct->GetName());
							ReportArray.Add(MakeShared<FJsonValueObject>(Report));
						}
					}
				}
			}
		}
	}

	// --- Phase 2: Update column PropertyBindingChain entries ---
	// Use a recursive lambda to find all PropertyBindingChain arrays in the column data
	UScriptStruct* InstancedStructType = TBaseStructure<FInstancedStruct>::Get();

	TSet<void*> VisitedPtrs;
	TFunction<void(void*, const UStruct*, const FString&)> WalkAndFixBindings;
	WalkAndFixBindings = [&](void* DataPtr, const UStruct* StructType, const FString& Path)
	{
		if (!DataPtr || !StructType) return;
		if (VisitedPtrs.Contains(DataPtr)) return;
		VisitedPtrs.Add(DataPtr);

		for (TFieldIterator<FProperty> PropIt(StructType); PropIt; ++PropIt)
		{
			FProperty* Prop = *PropIt;
			FString PropName = Prop->GetName();
			FString PropPath = Path + PropName;

			// Check for PropertyBindingChain (TArray<FName>)
			if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Prop))
			{
				if (PropName == TEXT("PropertyBindingChain"))
				{
					if (CastField<FNameProperty>(ArrayProp->Inner))
					{
						FScriptArrayHelper ArrayHelper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(DataPtr));
						UE_LOG(LogTemp, Warning, TEXT("CHT-DIAG: Found PropertyBindingChain at %s with %d entries"), *PropPath, ArrayHelper.Num());
						for (int32 i = 0; i < ArrayHelper.Num(); i++)
						{
							FName* NamePtr = reinterpret_cast<FName*>(ArrayHelper.GetRawPtr(i));
							if (NamePtr)
							{
								FString NameStr = NamePtr->ToString();
								UE_LOG(LogTemp, Warning, TEXT("CHT-DIAG:   [%d] = %s"), i, *NameStr);
								// Check each mapping: find the clean name prefix in the GUID-suffixed name
								for (const auto& MapEntry : NameMap)
								{
									if (*NamePtr == MapEntry.Key)
									{
										TSharedPtr<FJsonObject> Report = MakeShared<FJsonObject>();
										Report->SetStringField(TEXT("action"), TEXT("binding_updated"));
										Report->SetStringField(TEXT("path"), PropPath);
										Report->SetStringField(TEXT("old_name"), NamePtr->ToString());
										Report->SetStringField(TEXT("new_name"), MapEntry.Value.ToString());
										ReportArray.Add(MakeShared<FJsonValueObject>(Report));

										*NamePtr = MapEntry.Value;
										BindingsFixed++;
										break;
									}
								}
							}
						}
					}
					continue; // Don't recurse into the array we just fixed
				}

				// Recurse into struct arrays
				if (FStructProperty* InnerStructProp = CastField<FStructProperty>(ArrayProp->Inner))
				{
					FScriptArrayHelper ArrayHelper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(DataPtr));
					for (int32 i = 0; i < ArrayHelper.Num(); i++)
					{
						void* ElemPtr = ArrayHelper.GetRawPtr(i);
						FString ElemPath = FString::Printf(TEXT("%s[%d]."), *PropPath, i);

						if (InnerStructProp->Struct == InstancedStructType)
						{
							FInstancedStruct* Instanced = static_cast<FInstancedStruct*>(ElemPtr);
							if (Instanced && Instanced->IsValid())
							{
								void* InnerData = Instanced->GetMutableMemory();
								const UScriptStruct* InnerType = Instanced->GetScriptStruct();
								if (InnerData && InnerType)
								{
									WalkAndFixBindings(InnerData, InnerType, ElemPath);
								}
							}
						}
						else
						{
							WalkAndFixBindings(ElemPtr, InnerStructProp->Struct, ElemPath);
						}
					}
				}
			}
			// Recurse into struct properties
			else if (FStructProperty* StructProp = CastField<FStructProperty>(Prop))
			{
				void* StructDataPtr = StructProp->ContainerPtrToValuePtr<void>(DataPtr);
				if (StructProp->Struct == InstancedStructType)
				{
					FInstancedStruct* Instanced = static_cast<FInstancedStruct*>(StructDataPtr);
					if (Instanced && Instanced->IsValid())
					{
						void* InnerData = Instanced->GetMutableMemory();
						const UScriptStruct* InnerType = Instanced->GetScriptStruct();
						if (InnerData && InnerType)
						{
							WalkAndFixBindings(InnerData, InnerType, PropPath + TEXT("."));
						}
					}
				}
				else
				{
					WalkAndFixBindings(StructDataPtr, StructProp->Struct, PropPath + TEXT("."));
				}
			}
		}
	};

	// Walk ALL properties on the ChooserTable object (not just TArray<FInstancedStruct>)
	// This catches ColumnsStructs, ResultsStructs, ContextData, and any nested columns/bindings
	for (TFieldIterator<FProperty> PropIt(ChooserTable->GetClass()); PropIt; ++PropIt)
	{
		FProperty* Prop = *PropIt;
		FString PropName = Prop->GetName();

		if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Prop))
		{
			if (FStructProperty* InnerStructProp = CastField<FStructProperty>(ArrayProp->Inner))
			{
				FScriptArrayHelper ArrayHelper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(ChooserTable));
				UE_LOG(LogTemp, Warning, TEXT("CHT-DIAG: Walking array %s (%s) with %d entries"),
					*PropName, *InnerStructProp->Struct->GetName(), ArrayHelper.Num());

				for (int32 i = 0; i < ArrayHelper.Num(); i++)
				{
					void* ElemPtr = ArrayHelper.GetRawPtr(i);
					FString ElemPath = FString::Printf(TEXT("%s[%d]."), *PropName, i);

					if (InnerStructProp->Struct == InstancedStructType)
					{
						FInstancedStruct* Entry = reinterpret_cast<FInstancedStruct*>(ElemPtr);
						if (Entry && Entry->IsValid())
						{
							void* EntryData = Entry->GetMutableMemory();
							const UScriptStruct* EntryType = Entry->GetScriptStruct();
							if (EntryData && EntryType)
							{
								UE_LOG(LogTemp, Warning, TEXT("CHT-DIAG:   [%d] type=%s"), i, *EntryType->GetName());
								WalkAndFixBindings(EntryData, EntryType, ElemPath);
							}
						}
					}
					else
					{
						UE_LOG(LogTemp, Warning, TEXT("CHT-DIAG:   [%d] struct=%s"), i, *InnerStructProp->Struct->GetName());
						WalkAndFixBindings(ElemPtr, InnerStructProp->Struct, ElemPath);
					}
				}
			}
			else
			{
				// Non-struct array - log it for diagnostics
				FScriptArrayHelper ArrayHelper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(ChooserTable));
				if (ArrayHelper.Num() > 0)
				{
					UE_LOG(LogTemp, Warning, TEXT("CHT-DIAG: Skipping non-struct array %s (inner type: %s) with %d entries"),
						*PropName, *ArrayProp->Inner->GetClass()->GetName(), ArrayHelper.Num());
				}
			}
		}
		else if (FStructProperty* StructProp = CastField<FStructProperty>(Prop))
		{
			void* StructDataPtr = StructProp->ContainerPtrToValuePtr<void>(ChooserTable);
			UE_LOG(LogTemp, Warning, TEXT("CHT-DIAG: Walking struct prop %s (%s)"), *PropName, *StructProp->Struct->GetName());
			if (StructProp->Struct == InstancedStructType)
			{
				FInstancedStruct* Instanced = static_cast<FInstancedStruct*>(StructDataPtr);
				if (Instanced && Instanced->IsValid())
				{
					void* InnerData = Instanced->GetMutableMemory();
					const UScriptStruct* InnerType = Instanced->GetScriptStruct();
					if (InnerData && InnerType)
					{
						WalkAndFixBindings(InnerData, InnerType, PropName + TEXT("."));
					}
				}
			}
			else
			{
				WalkAndFixBindings(StructDataPtr, StructProp->Struct, PropName + TEXT("."));
			}
		}
	}

	// --- Phase 2b: Walk nested UChooserTable objects in NestedObjects array ---
	// NestedObjects is TArray<UObject*> — contains nested chooser tables with their own columns
	for (TFieldIterator<FProperty> PropIt(ChooserTable->GetClass()); PropIt; ++PropIt)
	{
		FArrayProperty* ArrayProp = CastField<FArrayProperty>(*PropIt);
		if (!ArrayProp) continue;

		FObjectPropertyBase* InnerObjProp = CastField<FObjectPropertyBase>(ArrayProp->Inner);
		if (!InnerObjProp) continue;

		FScriptArrayHelper ArrayHelper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(ChooserTable));
		for (int32 i = 0; i < ArrayHelper.Num(); i++)
		{
			UObject* Obj = InnerObjProp->GetObjectPropertyValue(ArrayHelper.GetRawPtr(i));
			UChooserTable* NestedChooser = Cast<UChooserTable>(Obj);
			if (!NestedChooser) continue;

			UE_LOG(LogTemp, Warning, TEXT("CHT-DIAG: Walking nested UChooserTable %s[%d] = %s"),
				*ArrayProp->GetName(), i, *NestedChooser->GetName());

			// Walk ALL properties on the nested chooser (same logic as parent)
			VisitedPtrs.Reset(); // Reset visited set for each nested chooser

			for (TFieldIterator<FProperty> NestedPropIt(NestedChooser->GetClass()); NestedPropIt; ++NestedPropIt)
			{
				FProperty* NProp = *NestedPropIt;
				FString NPropName = NProp->GetName();
				FString NPath = FString::Printf(TEXT("Nested[%d].%s"), i, *NPropName);

				if (FArrayProperty* NArrayProp = CastField<FArrayProperty>(NProp))
				{
					if (FStructProperty* NInnerStructProp = CastField<FStructProperty>(NArrayProp->Inner))
					{
						FScriptArrayHelper NArrayHelper(NArrayProp, NArrayProp->ContainerPtrToValuePtr<void>(NestedChooser));
						for (int32 j = 0; j < NArrayHelper.Num(); j++)
						{
							void* ElemPtr = NArrayHelper.GetRawPtr(j);
							FString ElemPath = FString::Printf(TEXT("%s[%d]."), *NPath, j);

							if (NInnerStructProp->Struct == InstancedStructType)
							{
								FInstancedStruct* Entry = reinterpret_cast<FInstancedStruct*>(ElemPtr);
								if (Entry && Entry->IsValid())
								{
									void* EntryData = Entry->GetMutableMemory();
									const UScriptStruct* EntryType = Entry->GetScriptStruct();
									if (EntryData && EntryType)
									{
										UE_LOG(LogTemp, Warning, TEXT("CHT-DIAG:   Nested[%d].%s[%d] type=%s"),
											i, *NPropName, j, *EntryType->GetName());
										WalkAndFixBindings(EntryData, EntryType, ElemPath);
									}
								}
							}
							else
							{
								WalkAndFixBindings(ElemPtr, NInnerStructProp->Struct, ElemPath);
							}
						}
					}
				}
				else if (FStructProperty* NStructProp = CastField<FStructProperty>(NProp))
				{
					void* StructDataPtr = NStructProp->ContainerPtrToValuePtr<void>(NestedChooser);
					if (NStructProp->Struct == InstancedStructType)
					{
						FInstancedStruct* Instanced = static_cast<FInstancedStruct*>(StructDataPtr);
						if (Instanced && Instanced->IsValid())
						{
							void* InnerData = Instanced->GetMutableMemory();
							const UScriptStruct* InnerType = Instanced->GetScriptStruct();
							if (InnerData && InnerType)
							{
								WalkAndFixBindings(InnerData, InnerType, NPath + TEXT("."));
							}
						}
					}
					else
					{
						WalkAndFixBindings(StructDataPtr, NStructProp->Struct, NPath + TEXT("."));
					}
				}
			}

			// Also update ContextData struct pointers in nested chooser
			if (StructMap.Num() > 0)
			{
				for (int32 ci = 0; ci < NestedChooser->ContextData.Num(); ci++)
				{
					FInstancedStruct& CtxEntry = NestedChooser->ContextData[ci];
					if (!CtxEntry.IsValid()) continue;
					const UScriptStruct* EntryType = CtxEntry.GetScriptStruct();
					if (!EntryType) continue;
					void* EntryData = CtxEntry.GetMutableMemory();
					if (!EntryData) continue;

					for (TFieldIterator<FObjectPropertyBase> ObjPropIt(EntryType); ObjPropIt; ++ObjPropIt)
					{
						FObjectPropertyBase* ObjProp = *ObjPropIt;
						if (ObjProp->GetName() != TEXT("Struct")) continue;
						void* ValuePtr = ObjProp->ContainerPtrToValuePtr<void>(EntryData);
						UObject* CurrentValue = ObjProp->GetObjectPropertyValue(ValuePtr);
						if (!CurrentValue || !CurrentValue->IsA<UScriptStruct>()) continue;

						UScriptStruct* CurrentStruct = Cast<UScriptStruct>(CurrentValue);
						FString CurrentStructName = CurrentStruct ? CurrentStruct->GetName() : TEXT("null");
						UScriptStruct* ReplacementStruct = nullptr;
						if (UScriptStruct** Found = StructMap.Find(CurrentStructName))
							ReplacementStruct = *Found;
						else if (UScriptStruct** Wildcard = StructMap.Find(TEXT("*")))
							ReplacementStruct = *Wildcard;

						if (ReplacementStruct)
						{
							ObjProp->SetObjectPropertyValue(ValuePtr, ReplacementStruct);
							ContextDataFixed++;
							TSharedPtr<FJsonObject> Report = MakeShared<FJsonObject>();
							Report->SetStringField(TEXT("action"), TEXT("nested_context_data_updated"));
							Report->SetNumberField(TEXT("nested_index"), i);
							Report->SetNumberField(TEXT("context_index"), ci);
							Report->SetStringField(TEXT("old_struct"), CurrentStructName);
							Report->SetStringField(TEXT("new_struct"), ReplacementStruct->GetName());
							ReportArray.Add(MakeShared<FJsonValueObject>(Report));
						}
					}
				}
			}

			NestedChooser->Compile(true);
			NestedChooser->MarkPackageDirty();
		}
	}

	// --- Phase 3: Recompile ---
	ChooserTable->Compile(true);
	ChooserTable->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("chooser"), ChooserTable->GetPathName());
	Data->SetNumberField(TEXT("context_data_fixed"), ContextDataFixed);
	Data->SetNumberField(TEXT("bindings_fixed"), BindingsFixed);
	Data->SetArrayField(TEXT("changes"), ReportArray);
	Data->SetStringField(TEXT("message"), FString::Printf(
		TEXT("Migrated chooser table: %d context data updates, %d binding updates"),
		ContextDataFixed, BindingsFixed));

	return MakeResponse(true, Data);
}
