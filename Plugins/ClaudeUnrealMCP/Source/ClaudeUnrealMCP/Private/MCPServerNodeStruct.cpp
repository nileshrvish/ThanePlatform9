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

FString FMCPServer::HandleAddSetStructNode(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("blueprint_path")) ||
		!Params->HasField(TEXT("struct_type")))
	{
		return MakeError(TEXT("Missing required parameters: blueprint_path, struct_type"));
	}

	FString BlueprintPath = Params->GetStringField(TEXT("blueprint_path"));
	FString StructType = Params->GetStringField(TEXT("struct_type"));
	FString GraphName = Params->HasField(TEXT("graph_name")) ? Params->GetStringField(TEXT("graph_name")) : TEXT("EventGraph");
	int32 NodeX = Params->HasField(TEXT("x")) ? Params->GetIntegerField(TEXT("x")) : 0;
	int32 NodeY = Params->HasField(TEXT("y")) ? Params->GetIntegerField(TEXT("y")) : 0;

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
		return MakeError(FString::Printf(TEXT("Graph not found: %s"), *GraphName));
	}

	// Find the struct type
	UScriptStruct* Struct = nullptr;

	// Try loading from path first (works for blueprint structs)
	Struct = LoadObject<UScriptStruct>(nullptr, *StructType);

	if (!Struct)
	{
		// Try to find in any package by iterating
		for (TObjectIterator<UScriptStruct> It; It; ++It)
		{
			if (It->GetName() == StructType)
			{
				Struct = *It;
				break;
			}
		}
	}

	if (!Struct)
	{
		return MakeError(FString::Printf(TEXT("Struct type not found: %s"), *StructType));
	}

	// Get fields to expose (optional)
	TArray<FString> FieldsToExpose;
	if (Params->HasField(TEXT("fields")))
	{
		const TArray<TSharedPtr<FJsonValue>>* FieldsArray;
		if (Params->TryGetArrayField(TEXT("fields"), FieldsArray))
		{
			for (const auto& Field : *FieldsArray)
			{
				FieldsToExpose.Add(Field->AsString());
			}
		}
	}

	// Create the node
	UK2Node_SetFieldsInStruct* NewNode = NewObject<UK2Node_SetFieldsInStruct>(TargetGraph);
	NewNode->StructType = Struct;
	NewNode->NodePosX = NodeX;
	NewNode->NodePosY = NodeY;
	NewNode->CreateNewGuid();

	// Allocate default pins first (this populates ShowPinForProperties with all fields hidden by default)
	NewNode->AllocateDefaultPins();

	// Use RestoreAllPins() to show all struct field pins if any fields were requested
	// This is the official way to expose pins on K2Node_SetFieldsInStruct
	if (FieldsToExpose.Num() > 0)
	{
		// RestoreAllPins() will show all struct member pins
		NewNode->RestoreAllPins();
	}

	TargetGraph->AddNode(NewNode, false, false);

	// Mark blueprint as modified
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

	// Compile the blueprint
	FKismetEditorUtilities::CompileBlueprint(Blueprint);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("Set struct node created successfully"));
	Data->SetStringField(TEXT("node_id"), NewNode->NodeGuid.ToString(EGuidFormats::DigitsWithHyphensLower));
	Data->SetStringField(TEXT("struct_type"), Struct->GetName());

	// List available pins
	TArray<TSharedPtr<FJsonValue>> PinsArray;
	for (UEdGraphPin* Pin : NewNode->Pins)
	{
		if (Pin)
		{
			TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
			PinObj->SetStringField(TEXT("name"), Pin->PinName.ToString());
			PinObj->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("Input") : TEXT("Output"));
			PinsArray.Add(MakeShared<FJsonValueObject>(PinObj));
		}
	}
	Data->SetArrayField(TEXT("pins"), PinsArray);

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleRestoreStructNodePins(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("blueprint_path")) || !Params->HasField(TEXT("node_guid")))
	{
		return MakeError(TEXT("Missing required parameters: blueprint_path, node_guid"));
	}

	FString BlueprintPath = Params->GetStringField(TEXT("blueprint_path"));
	FString NodeGuidStr = Params->GetStringField(TEXT("node_guid"));

	UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Failed to load blueprint: %s"), *BlueprintPath));
	}

	FGuid NodeGuid;
	FGuid::Parse(NodeGuidStr, NodeGuid);

	// Search all graphs for the node
	UK2Node_SetFieldsInStruct* FoundNode = nullptr;
	TArray<UEdGraph*> AllGraphs;
	Blueprint->GetAllGraphs(AllGraphs);

	for (UEdGraph* Graph : AllGraphs)
	{
		if (!Graph) continue;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node && Node->NodeGuid == NodeGuid)
			{
				FoundNode = Cast<UK2Node_SetFieldsInStruct>(Node);
				break;
			}
		}
		if (FoundNode) break;
	}

	if (!FoundNode)
	{
		return MakeError(FString::Printf(TEXT("K2Node_SetFieldsInStruct not found with GUID: %s"), *NodeGuidStr));
	}

	// Save existing connections before RestoreAllPins
	TMap<FName, TPair<UEdGraphNode*, FName>> SavedConnections;
	for (UEdGraphPin* Pin : FoundNode->Pins)
	{
		if (Pin && Pin->LinkedTo.Num() > 0)
		{
			UEdGraphPin* LinkedPin = Pin->LinkedTo[0];
			if (LinkedPin)
			{
				SavedConnections.Add(Pin->PinName, TPair<UEdGraphNode*, FName>(LinkedPin->GetOwningNode(), LinkedPin->PinName));
			}
		}
	}

	// Call RestoreAllPins to show all struct field pins
	FoundNode->RestoreAllPins();

	// Restore connections
	int32 ConnectionsRestored = 0;
	for (auto& Pair : SavedConnections)
	{
		UEdGraphPin* MyPin = FoundNode->FindPin(Pair.Key);
		if (MyPin)
		{
			UEdGraphNode* OtherNode = Pair.Value.Key;
			UEdGraphPin* OtherPin = OtherNode ? OtherNode->FindPin(Pair.Value.Value) : nullptr;
			if (OtherPin && !MyPin->LinkedTo.Contains(OtherPin))
			{
				MyPin->MakeLinkTo(OtherPin);
				ConnectionsRestored++;
			}
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("Struct node pins restored"));
	Data->SetStringField(TEXT("node_guid"), NodeGuidStr);

	TArray<TSharedPtr<FJsonValue>> PinsArray;
	for (UEdGraphPin* Pin : FoundNode->Pins)
	{
		if (Pin)
		{
			TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
			PinObj->SetStringField(TEXT("name"), Pin->PinName.ToString());
			PinObj->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("Input") : TEXT("Output"));
			PinObj->SetBoolField(TEXT("is_linked"), Pin->LinkedTo.Num() > 0);
			PinsArray.Add(MakeShared<FJsonValueObject>(PinObj));
		}
	}
	Data->SetArrayField(TEXT("pins"), PinsArray);
	Data->SetNumberField(TEXT("connections_restored"), ConnectionsRestored);

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleFixStructEnumFieldDefaults(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("blueprint_path")))
	{
		return MakeError(TEXT("Missing required parameter: blueprint_path"));
	}

	FString BPPath = Params->GetStringField(TEXT("blueprint_path"));

	UBlueprint* Blueprint = LoadBlueprintFromPath(BPPath);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *BPPath));
	}

	// Build a mapping: NewEnumeratorN -> correct C++ enum name for all loaded C++ enums
	// We'll map by checking each enum
	TMap<FString, TMap<FString, FString>> EnumFixMaps; // EnumName -> (OldValue -> NewValue)
	for (TObjectIterator<UEnum> It; It; ++It)
	{
		UEnum* Enum = *It;
		if (!Enum || Cast<UUserDefinedEnum>(Enum)) continue; // skip BP enums

		TMap<FString, FString> FixMap;
		for (int32 i = 0; i < Enum->NumEnums() - 1; ++i)
		{
			FString OldName = FString::Printf(TEXT("NewEnumerator%d"), i);
			FString NewName = Enum->GetNameStringByIndex(i);
			// Strip enum prefix if present (e.g., "E_TraversalActionType::None" -> "None")
			if (NewName.Contains(TEXT("::")))
			{
				NewName = NewName.RightChop(NewName.Find(TEXT("::")) + 2);
			}
			FixMap.Add(OldName, NewName);
		}
		EnumFixMaps.Add(Enum->GetName(), FixMap);
	}

	int32 VarsFixed = 0;
	TArray<TSharedPtr<FJsonValue>> FixedArray;

	// Fix variable default value strings in FBPVariableDescription
	for (FBPVariableDescription& VarDesc : Blueprint->NewVariables)
	{
		FString DefaultValue = VarDesc.DefaultValue;
		if (DefaultValue.IsEmpty()) continue;

		if (DefaultValue.Contains(TEXT("NewEnumerator")))
		{
			FString OldDefault = DefaultValue;

			// Replace all NewEnumeratorN occurrences
			// We need to figure out which enum each field uses
			// Strategy: replace any "NewEnumeratorN" with the correct value
			// by checking all known enums. Since the value is typically "NewEnumerator0",
			// and most enums map index 0 to "None", we do a simple regex-like replacement.

			// For struct variables, the default is like: (FieldName_GUID=NewEnumerator0,...)
			// We need to find each NewEnumeratorN and replace with the correct enum value

			// First, try to determine the struct type to get field->enum mappings
			UScriptStruct* StructType = nullptr;
			UObject* PinSubCategoryObj = VarDesc.VarType.PinSubCategoryObject.Get();
			if (PinSubCategoryObj)
			{
				StructType = Cast<UScriptStruct>(PinSubCategoryObj);
				if (!StructType)
				{
					// Could be a UserDefinedStruct
					UUserDefinedStruct* UDS = Cast<UUserDefinedStruct>(PinSubCategoryObj);
					if (UDS) StructType = UDS;
				}
			}

			bool bFixed = false;
			if (StructType)
			{
				// Walk through struct properties to find enum fields and their types
				for (TFieldIterator<FProperty> PropIt(StructType); PropIt; ++PropIt)
				{
					FProperty* Prop = *PropIt;
					UEnum* PropEnum = nullptr;
					FByteProperty* ByteProp = CastField<FByteProperty>(Prop);
					FEnumProperty* EnumProp = CastField<FEnumProperty>(Prop);

					if (ByteProp && ByteProp->Enum) PropEnum = ByteProp->Enum;
					else if (EnumProp && EnumProp->GetEnum()) PropEnum = EnumProp->GetEnum();

					if (!PropEnum) continue;

					// Build replacement map for this specific enum
					for (int32 i = PropEnum->NumEnums() - 2; i >= 0; --i) // reverse order to handle NewEnumerator10 before NewEnumerator1
					{
						FString OldName = FString::Printf(TEXT("NewEnumerator%d"), i);
						FString NewName = PropEnum->GetNameStringByIndex(i);
						if (NewName.Contains(TEXT("::")))
						{
							NewName = NewName.RightChop(NewName.Find(TEXT("::")) + 2);
						}
						if (DefaultValue.Contains(OldName))
						{
							DefaultValue.ReplaceInline(*OldName, *NewName);
							bFixed = true;
						}
					}
				}
			}

			if (!bFixed)
			{
				// Fallback: generic replacement for any NewEnumeratorN -> index 0 = "None"
				for (int32 i = 20; i >= 0; --i)
				{
					FString OldName = FString::Printf(TEXT("NewEnumerator%d"), i);
					if (DefaultValue.Contains(OldName))
					{
						DefaultValue.ReplaceInline(*OldName, TEXT("None"));
						bFixed = true;
					}
				}
			}

			if (bFixed && DefaultValue != OldDefault)
			{
				VarDesc.DefaultValue = DefaultValue;

				TSharedPtr<FJsonObject> FixObj = MakeShared<FJsonObject>();
				FixObj->SetStringField(TEXT("variable"), VarDesc.FriendlyName);
				FixObj->SetStringField(TEXT("old_default"), OldDefault);
				FixObj->SetStringField(TEXT("new_default"), DefaultValue);
				FixedArray.Add(MakeShared<FJsonValueObject>(FixObj));
				VarsFixed++;
			}
		}
	}

	// Fix UserDefinedStruct field default value strings (FStructVariableDescription::DefaultValue)
	// Collect all UserDefinedStructs — scan all loaded objects plus BP variables and pins
	TSet<UUserDefinedStruct*> UsedStructs;

	// Method 1: Iterate all loaded UserDefinedStructs with "Traversal" or struct names used by the BP
	TSet<FString> StructSubtypes;
	for (FBPVariableDescription& VarDesc : Blueprint->NewVariables)
	{
		UObject* SubCatObj = VarDesc.VarType.PinSubCategoryObject.Get();
		if (SubCatObj)
		{
			if (UUserDefinedStruct* UDS = Cast<UUserDefinedStruct>(SubCatObj))
			{
				UsedStructs.Add(UDS);
			}
		}
		// Also try by PinSubCategory string
		if (!VarDesc.VarType.PinSubCategory.IsNone())
		{
			StructSubtypes.Add(VarDesc.VarType.PinSubCategory.ToString());
		}
	}

	// Method 2: Scan graph pins
	{
		TArray<UEdGraph*> TempGraphs;
		Blueprint->GetAllGraphs(TempGraphs);
		for (UEdGraph* G : TempGraphs)
		{
			if (!G) continue;
			for (UEdGraphNode* N : G->Nodes)
			{
				if (!N) continue;
				for (UEdGraphPin* P : N->Pins)
				{
					if (!P) continue;
					UObject* PinObj = P->PinType.PinSubCategoryObject.Get();
					if (PinObj)
					{
						if (UUserDefinedStruct* UDS = Cast<UUserDefinedStruct>(PinObj))
						{
							UsedStructs.Add(UDS);
						}
					}
				}
			}
		}
	}

	// Method 3: Search all loaded UserDefinedStructs that have C++ enum fields
	// NOTE: Runtime DefaultValue may already show correct names (e.g. "None") even when
	// on-disk serialized value still has "NewEnumerator0". Check compiled properties instead.
	for (TObjectIterator<UUserDefinedStruct> It; It; ++It)
	{
		UUserDefinedStruct* UDS = *It;
		if (!UDS) continue;
		for (TFieldIterator<FProperty> PropIt(UDS); PropIt; ++PropIt)
		{
			FByteProperty* BP = CastField<FByteProperty>(*PropIt);
			FEnumProperty* EP = CastField<FEnumProperty>(*PropIt);
			UEnum* PropEnum = nullptr;
			if (BP && BP->Enum) PropEnum = BP->Enum;
			else if (EP && EP->GetEnum()) PropEnum = EP->GetEnum();
			if (PropEnum && !Cast<UUserDefinedEnum>(PropEnum))
			{
				UsedStructs.Add(UDS);
				break;
			}
		}
	}

	// Fix struct enum field SubCategoryObject, compiled FByteProperty::Enum, and DefaultValue
	// Build map of BP enum -> C++ enum by name
	TMap<FString, UEnum*> CppEnumsByName;
	for (TObjectIterator<UEnum> It; It; ++It)
	{
		if (!*It || Cast<UUserDefinedEnum>(*It)) continue;
		CppEnumsByName.Add(It->GetName(), *It);
	}

	TArray<TSharedPtr<FJsonValue>> DiagArray;
	for (UUserDefinedStruct* UDS : UsedStructs)
	{
		// Collect deferred ChangeVariableDefaultValue calls — calling it inside the loop
		// would trigger recompilation and potentially invalidate our SVarDesc references.
		struct FDeferredDefaultChange
		{
			FGuid VarGuid;
			FString NewDefault;
		};
		TArray<FDeferredDefaultChange> DeferredChanges;

		const TArray<FStructVariableDescription>& StructVars = FStructureEditorUtils::GetVarDesc(UDS);

		for (const FStructVariableDescription& SVarDesc : StructVars)
		{
			// Get the compiled property
			FProperty* Prop = UDS->FindPropertyByName(SVarDesc.VarName);
			if (!Prop) continue;

			FByteProperty* ByteProp = CastField<FByteProperty>(Prop);
			FEnumProperty* EnumProp = CastField<FEnumProperty>(Prop);
			UEnum* CurrentEnum = nullptr;
			if (ByteProp && ByteProp->Enum) CurrentEnum = ByteProp->Enum;
			else if (EnumProp && EnumProp->GetEnum()) CurrentEnum = EnumProp->GetEnum();
			if (!CurrentEnum) continue;

			FString EnumName = CurrentEnum->GetName();
			bool bIsBPEnum = Cast<UUserDefinedEnum>(CurrentEnum) != nullptr;

			// Diagnostic info
			TSharedPtr<FJsonObject> DiagObj = MakeShared<FJsonObject>();
			DiagObj->SetStringField(TEXT("struct"), UDS->GetName());
			DiagObj->SetStringField(TEXT("field"), SVarDesc.FriendlyName);
			DiagObj->SetStringField(TEXT("default"), SVarDesc.DefaultValue);
			DiagObj->SetStringField(TEXT("enum"), EnumName);
			DiagObj->SetBoolField(TEXT("is_bp_enum"), bIsBPEnum);
			DiagObj->SetStringField(TEXT("enum_path"), CurrentEnum->GetPathName());

			// Check SubCategoryObject
			UObject* SubCatObj = SVarDesc.SubCategoryObject.LoadSynchronous();
			DiagObj->SetStringField(TEXT("sub_cat_obj"), SubCatObj ? SubCatObj->GetPathName() : TEXT("null"));
			DiagObj->SetBoolField(TEXT("sub_cat_is_bp"), SubCatObj ? (Cast<UUserDefinedEnum>(SubCatObj) != nullptr) : false);

			// If compiled enum OR SubCategoryObject points to BP enum, fix it
			UEnum** CppEnumPtr = CppEnumsByName.Find(EnumName);
			UEnum* CppEnum = CppEnumPtr ? *CppEnumPtr : nullptr;

			bool bFixed = false;
			if (CppEnum)
			{
				// Fix compiled property enum pointer
				if (bIsBPEnum)
				{
					if (ByteProp) ByteProp->Enum = CppEnum;
					// EnumProperty doesn't have a simple setter
					bFixed = true;
				}

				// Fix SubCategoryObject — must use const_cast since we're iterating const refs
				bool bSubCatIsBP = SubCatObj && Cast<UUserDefinedEnum>(SubCatObj);
				if (bSubCatIsBP || !SubCatObj)
				{
					const_cast<FStructVariableDescription&>(SVarDesc).SubCategoryObject = CppEnum;
					bFixed = true;
				}

				// Fix DefaultValue string — defer ChangeVariableDefaultValue to after the loop
				// to avoid invalidating our iterator.
				FString FieldDefault = SVarDesc.DefaultValue;
				// Replace any remaining NewEnumeratorN patterns
				if (FieldDefault.Contains(TEXT("NewEnumerator")))
				{
					for (int32 i = CppEnum->NumEnums() - 2; i >= 0; --i)
					{
						FString OldName = FString::Printf(TEXT("NewEnumerator%d"), i);
						// Use the enum's name string verbatim for struct defaults to satisfy validation
						FString NewName = CppEnum->GetNameStringByIndex(i);
						FieldDefault.ReplaceInline(*OldName, *NewName);
					}
				}
				// Always defer ChangeVariableDefaultValue for C++ enum fields to force
				// on-disk re-serialization with the correct value
				DeferredChanges.Add({SVarDesc.VarGuid, FieldDefault});
				bFixed = true;
				DiagObj->SetStringField(TEXT("new_default"), FieldDefault);
			}

			DiagObj->SetBoolField(TEXT("fixed"), bFixed);
			DiagObj->SetStringField(TEXT("cpp_enum_found"), CppEnum ? CppEnum->GetPathName() : TEXT("not found"));
			DiagArray.Add(MakeShared<FJsonValueObject>(DiagObj));

			if (bFixed)
			{
				TSharedPtr<FJsonObject> FixObj = MakeShared<FJsonObject>();
				FixObj->SetStringField(TEXT("source"), TEXT("struct_enum_field"));
				FixObj->SetStringField(TEXT("struct"), UDS->GetName());
				FixObj->SetStringField(TEXT("field"), SVarDesc.FriendlyName);
				FixObj->SetStringField(TEXT("enum"), EnumName);
				FixedArray.Add(MakeShared<FJsonValueObject>(FixObj));
				VarsFixed++;
			}
		}

		// Apply deferred ChangeVariableDefaultValue calls now that we're done iterating
		for (const FDeferredDefaultChange& Change : DeferredChanges)
		{
			FStructureEditorUtils::ChangeVariableDefaultValue(UDS, Change.VarGuid, Change.NewDefault);
		}
	}

	// Also fix local variable defaults in function graphs
	TArray<UEdGraph*> AllGraphs;
	Blueprint->GetAllGraphs(AllGraphs);

	// Helper lambda to collect all pins including sub-pins recursively
	TFunction<void(UEdGraphPin*, TArray<UEdGraphPin*>&)> CollectAllPins;
	CollectAllPins = [&CollectAllPins](UEdGraphPin* Pin, TArray<UEdGraphPin*>& OutPins) {
		if (!Pin) return;
		OutPins.Add(Pin);
		for (UEdGraphPin* SubPin : Pin->SubPins)
		{
			CollectAllPins(SubPin, OutPins);
		}
	};

	for (UEdGraph* Graph : AllGraphs)
	{
		if (!Graph) continue;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node) continue;
			// Collect all pins including sub-pins
			TArray<UEdGraphPin*> AllPins;
			for (UEdGraphPin* TopPin : Node->Pins)
			{
				CollectAllPins(TopPin, AllPins);
			}

			// Helper lambda to fix NewEnumerator values in a string
			auto FixEnumString = [](const FString& Input, UEdGraphPin* Pin) -> FString
			{
				FString Result = Input;
				UObject* PinSubCatObj = Pin->PinType.PinSubCategoryObject.Get();
				UEnum* DirectEnum = PinSubCatObj ? Cast<UEnum>(PinSubCatObj) : nullptr;

				if (DirectEnum)
				{
					for (int32 i = DirectEnum->NumEnums() - 2; i >= 0; --i)
					{
						FString OldName = FString::Printf(TEXT("NewEnumerator%d"), i);
						FString NewName = DirectEnum->GetNameStringByIndex(i);
						if (NewName.Contains(TEXT("::")))
						{
							NewName = NewName.RightChop(NewName.Find(TEXT("::")) + 2);
						}
						Result.ReplaceInline(*OldName, *NewName);
					}
				}
				else
				{
					UScriptStruct* PinStruct = PinSubCatObj ? Cast<UScriptStruct>(PinSubCatObj) : nullptr;
					if (PinStruct)
					{
						for (TFieldIterator<FProperty> PropIt(PinStruct); PropIt; ++PropIt)
						{
							UEnum* PropEnum = nullptr;
							FByteProperty* BP2 = CastField<FByteProperty>(*PropIt);
							FEnumProperty* EP2 = CastField<FEnumProperty>(*PropIt);
							if (BP2 && BP2->Enum) PropEnum = BP2->Enum;
							else if (EP2 && EP2->GetEnum()) PropEnum = EP2->GetEnum();
							if (!PropEnum) continue;
							for (int32 i = PropEnum->NumEnums() - 2; i >= 0; --i)
							{
								FString OldName = FString::Printf(TEXT("NewEnumerator%d"), i);
								FString NewName = PropEnum->GetNameStringByIndex(i);
								if (NewName.Contains(TEXT("::")))
									NewName = NewName.RightChop(NewName.Find(TEXT("::")) + 2);
								Result.ReplaceInline(*OldName, *NewName);
							}
						}
					}
					else
					{
						for (int32 i = 20; i >= 0; --i)
						{
							FString OldName = FString::Printf(TEXT("NewEnumerator%d"), i);
							Result.ReplaceInline(*OldName, TEXT("None"));
						}
					}
				}
				return Result;
			};

			for (UEdGraphPin* Pin : AllPins)
			{
				if (!Pin) continue;
				bool bHasNewEnum = (!Pin->DefaultValue.IsEmpty() && Pin->DefaultValue.Contains(TEXT("NewEnumerator")));
				bool bAutoHasNewEnum = (!Pin->AutogeneratedDefaultValue.IsEmpty() && Pin->AutogeneratedDefaultValue.Contains(TEXT("NewEnumerator")));
				if (!bHasNewEnum && !bAutoHasNewEnum) continue;

				bool bFixed = false;

				// Fix DefaultValue
				if (bHasNewEnum)
				{
					FString OldDefault = Pin->DefaultValue;
					FString NewDefault = FixEnumString(Pin->DefaultValue, Pin);
					if (NewDefault != OldDefault)
					{
						Pin->DefaultValue = NewDefault;
						bFixed = true;
					}
				}

				// Fix AutogeneratedDefaultValue
				if (bAutoHasNewEnum)
				{
					FString OldAuto = Pin->AutogeneratedDefaultValue;
					FString NewAuto = FixEnumString(Pin->AutogeneratedDefaultValue, Pin);
					if (NewAuto != OldAuto)
					{
						Pin->AutogeneratedDefaultValue = NewAuto;
						bFixed = true;
					}
				}

				if (bFixed)
				{
					TSharedPtr<FJsonObject> FixObj = MakeShared<FJsonObject>();
					FixObj->SetStringField(TEXT("source"), TEXT("pin"));
					FixObj->SetStringField(TEXT("graph"), Graph->GetName());
					FixObj->SetStringField(TEXT("node"), Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
					FixObj->SetStringField(TEXT("pin"), Pin->PinName.ToString());
					FixObj->SetStringField(TEXT("default_value"), Pin->DefaultValue);
					FixObj->SetStringField(TEXT("auto_default"), Pin->AutogeneratedDefaultValue);
					FixObj->SetBoolField(TEXT("is_sub_pin"), Pin->ParentPin != nullptr);
					FixObj->SetBoolField(TEXT("fixed_default"), bHasNewEnum);
					FixObj->SetBoolField(TEXT("fixed_auto"), bAutoHasNewEnum);
					FixedArray.Add(MakeShared<FJsonValueObject>(FixObj));
					VarsFixed++;
				}
			}
		}
	}

	if (VarsFixed > 0)
	{
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("blueprint_path"), BPPath);
	Data->SetNumberField(TEXT("vars_fixed"), VarsFixed);
	Data->SetNumberField(TEXT("structs_found"), UsedStructs.Num());
	Data->SetArrayField(TEXT("fixed"), FixedArray);
	Data->SetArrayField(TEXT("diagnostics"), DiagArray);

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleFixOptionalStructPinDefaults(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("blueprint_path")))
	{
		return MakeError(TEXT("Missing required parameter: blueprint_path"));
	}

	FString BPPath = Params->GetStringField(TEXT("blueprint_path"));
	UBlueprint* Blueprint = LoadBlueprintFromPath(BPPath);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *BPPath));
	}

	int32 NodesFixed = 0;
	int32 DefaultsFixed = 0;
	TArray<TSharedPtr<FJsonValue>> FixedArray;

	auto CollectAllPins = [](UEdGraphNode* Node, TArray<UEdGraphPin*>& OutPins)
	{
		if (!Node) return;
		TFunction<void(UEdGraphPin*)> CollectPinRecursive = [&CollectPinRecursive, &OutPins](UEdGraphPin* Pin)
		{
			if (!Pin) return;
			OutPins.Add(Pin);
			for (UEdGraphPin* SubPin : Pin->SubPins)
			{
				CollectPinRecursive(SubPin);
			}
		};
		for (UEdGraphPin* Pin : Node->Pins)
		{
			CollectPinRecursive(Pin);
		}
	};

	auto FixEnumDefaultString = [](const FString& Input, UEnum* Enum) -> FString
	{
		if (!Enum)
		{
			return Input;
		}

		FString Result = Input;
		for (int32 i = Enum->NumEnums() - 2; i >= 0; --i)
		{
			const FString OldName = FString::Printf(TEXT("NewEnumerator%d"), i);
			FString NewName = Enum->GetNameStringByIndex(i);
			if (NewName.Contains(TEXT("::")))
			{
				NewName = NewName.RightChop(NewName.Find(TEXT("::")) + 2);
			}
			Result.ReplaceInline(*OldName, *NewName);
		}
		return Result;
	};

	auto GetEnumForProperty = [](FProperty* Prop) -> UEnum*
	{
		if (!Prop)
		{
			return nullptr;
		}

		if (FByteProperty* ByteProp = CastField<FByteProperty>(Prop))
		{
			return ByteProp->Enum;
		}

		if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Prop))
		{
			return EnumProp->GetEnum();
		}

		return nullptr;
	};

	TArray<UEdGraph*> AllGraphs;
	Blueprint->GetAllGraphs(AllGraphs);

	for (UEdGraph* Graph : AllGraphs)
	{
		if (!Graph) continue;

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node) continue;

			UScriptStruct* StructType = nullptr;
			TArray<FOptionalPinFromProperty>* OptionalPins = nullptr;
			const TCHAR* NodeType = TEXT("");

			if (UK2Node_SetFieldsInStruct* SetNode = Cast<UK2Node_SetFieldsInStruct>(Node))
			{
				StructType = SetNode->StructType;
				OptionalPins = &SetNode->ShowPinForProperties;
				NodeType = TEXT("SetFieldsInStruct");
			}
			else if (UK2Node_MakeStruct* MakeNode = Cast<UK2Node_MakeStruct>(Node))
			{
				StructType = MakeNode->StructType;
				OptionalPins = &MakeNode->ShowPinForProperties;
				NodeType = TEXT("MakeStruct");
			}

			if (!StructType || !OptionalPins)
			{
				continue;
			}

			bool bNodeFixed = false;
			TArray<UEdGraphPin*> AllPins;
			CollectAllPins(Node, AllPins);

			for (FOptionalPinFromProperty& Optional : *OptionalPins)
			{
				if (Optional.PropertyName.IsNone())
				{
					continue;
				}

				FProperty* Prop = StructType->FindPropertyByName(Optional.PropertyName);
				UEnum* PropEnum = GetEnumForProperty(Prop);
				if (!PropEnum)
				{
					continue;
				}

				// Fix any pins that match this property name
				for (UEdGraphPin* Pin : AllPins)
				{
					if (!Pin) continue;
					const FString PinName = Pin->PinName.ToString();
					if (!PinName.StartsWith(Optional.PropertyName.ToString()))
					{
						continue;
					}

					bool bPinFixed = false;
					FString OldDefault = Pin->DefaultValue;
					FString OldAuto = Pin->AutogeneratedDefaultValue;

					if (Pin->DefaultValue.Contains(TEXT("NewEnumerator")))
					{
						Pin->DefaultValue = FixEnumDefaultString(Pin->DefaultValue, PropEnum);
						bPinFixed = true;
					}
					if (Pin->AutogeneratedDefaultValue.Contains(TEXT("NewEnumerator")))
					{
						Pin->AutogeneratedDefaultValue = FixEnumDefaultString(Pin->AutogeneratedDefaultValue, PropEnum);
						bPinFixed = true;
					}

					if (bPinFixed)
					{
						bNodeFixed = true;
						DefaultsFixed++;

						TSharedPtr<FJsonObject> FixObj = MakeShared<FJsonObject>();
						FixObj->SetStringField(TEXT("graph"), Graph->GetName());
						FixObj->SetStringField(TEXT("node_type"), NodeType);
						FixObj->SetStringField(TEXT("node"), Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
						FixObj->SetStringField(TEXT("property"), Optional.PropertyName.ToString());
						FixObj->SetStringField(TEXT("pin"), Pin->PinName.ToString());
						FixObj->SetStringField(TEXT("old_default"), OldDefault);
						FixObj->SetStringField(TEXT("new_default"), Pin->DefaultValue);
						FixObj->SetStringField(TEXT("old_auto_default"), OldAuto);
						FixObj->SetStringField(TEXT("new_auto_default"), Pin->AutogeneratedDefaultValue);
						FixObj->SetStringField(TEXT("enum"), PropEnum->GetName());
						FixedArray.Add(MakeShared<FJsonValueObject>(FixObj));
					}
				}
			}

			if (bNodeFixed)
			{
				NodesFixed++;
			}
		}
	}

	if (DefaultsFixed > 0)
	{
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("blueprint_path"), BPPath);
	Data->SetNumberField(TEXT("nodes_fixed"), NodesFixed);
	Data->SetNumberField(TEXT("defaults_fixed"), DefaultsFixed);
	Data->SetArrayField(TEXT("fixed"), FixedArray);

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleSetStructFieldDefault(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("struct_path")) ||
		!Params->HasField(TEXT("field_name")) || !Params->HasField(TEXT("new_default")))
	{
		return MakeError(TEXT("Missing required parameters: struct_path, field_name, new_default"));
	}

	const FString StructPath = Params->GetStringField(TEXT("struct_path"));
	const FString FieldName = Params->GetStringField(TEXT("field_name"));
	const FString NewDefault = Params->GetStringField(TEXT("new_default"));

	UUserDefinedStruct* Struct = Cast<UUserDefinedStruct>(LoadObject<UObject>(nullptr, *StructPath));
	if (!Struct)
	{
		return MakeError(FString::Printf(TEXT("UserDefinedStruct not found: %s"), *StructPath));
	}

	FStructVariableDescription* TargetVar = nullptr;
	for (FStructVariableDescription& VarDesc : FStructureEditorUtils::GetVarDesc(Struct))
	{
		if (VarDesc.VarName.ToString() == FieldName || VarDesc.FriendlyName == FieldName)
		{
			TargetVar = &VarDesc;
			break;
		}
	}

	if (!TargetVar)
	{
		return MakeError(FString::Printf(TEXT("Field not found on struct: %s"), *FieldName));
	}

	const FString OldDefault = TargetVar->DefaultValue;
	const bool bChanged = FStructureEditorUtils::ChangeVariableDefaultValue(Struct, TargetVar->VarGuid, NewDefault);

	if (!bChanged)
	{
		return MakeError(FString::Printf(TEXT("Failed to set default for %s (validation failed)"), *FieldName));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("struct"), Struct->GetName());
	Data->SetStringField(TEXT("field"), FieldName);
	Data->SetStringField(TEXT("old_default"), OldDefault);
	Data->SetStringField(TEXT("new_default"), NewDefault);
	return MakeResponse(true, Data);
}
