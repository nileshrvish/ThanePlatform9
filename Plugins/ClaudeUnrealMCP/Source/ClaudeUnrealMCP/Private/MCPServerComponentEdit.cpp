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

FString FMCPServer::HandleAddComponent(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid())
	{
		return MakeError(TEXT("Missing parameters"));
	}

	FString BlueprintPath = Params->GetStringField(TEXT("blueprint_path"));
	FString ComponentClass = Params->GetStringField(TEXT("component_class"));
	FString ComponentName = Params->GetStringField(TEXT("component_name"));

	if (BlueprintPath.IsEmpty() || ComponentClass.IsEmpty())
	{
		return MakeError(TEXT("Missing blueprint_path or component_class"));
	}

	UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
	}

	// Find the component class
	UClass* CompClass = FindFirstObject<UClass>(*ComponentClass, EFindFirstObjectOptions::ExactClass);
	if (!CompClass)
	{
		// Try with _C suffix for blueprint classes
		CompClass = FindFirstObject<UClass>(*(ComponentClass + TEXT("_C")), EFindFirstObjectOptions::ExactClass);
	}
	if (!CompClass)
	{
		// Try loading it
		CompClass = LoadClass<UActorComponent>(nullptr, *ComponentClass);
	}
	if (!CompClass)
	{
		return MakeError(FString::Printf(TEXT("Component class not found: %s"), *ComponentClass));
	}

	if (!Blueprint->SimpleConstructionScript)
	{
		return MakeError(TEXT("Blueprint has no SimpleConstructionScript"));
	}

	// Generate name if not provided
	if (ComponentName.IsEmpty())
	{
		ComponentName = CompClass->GetName();
		ComponentName.RemoveFromEnd(TEXT("Component"));
	}

	// Create the new node
	USCS_Node* NewNode = Blueprint->SimpleConstructionScript->CreateNode(CompClass, *ComponentName);
	if (!NewNode)
	{
		return MakeError(TEXT("Failed to create component node"));
	}

	// Add to root or default scene root
	USCS_Node* RootNode = Blueprint->SimpleConstructionScript->GetDefaultSceneRootNode();
	if (RootNode)
	{
		RootNode->AddChildNode(NewNode);
	}
	else
	{
		Blueprint->SimpleConstructionScript->AddNode(NewNode);
	}

	// Mark blueprint as modified
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("component_name"), NewNode->GetVariableName().ToString());
	Data->SetStringField(TEXT("message"), TEXT("Component added successfully"));

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleSetComponentProperty(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid())
	{
		return MakeError(TEXT("Missing parameters"));
	}

	FString BlueprintPath = Params->GetStringField(TEXT("blueprint_path"));
	FString ComponentName = Params->GetStringField(TEXT("component_name"));
	FString PropertyName = Params->GetStringField(TEXT("property_name"));
	FString PropertyValue = Params->GetStringField(TEXT("property_value"));

	if (BlueprintPath.IsEmpty() || ComponentName.IsEmpty() || PropertyName.IsEmpty())
	{
		return MakeError(TEXT("Missing required parameters"));
	}

	UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
	}

	if (!Blueprint->SimpleConstructionScript)
	{
		return MakeError(TEXT("Blueprint has no SimpleConstructionScript"));
	}

	// Find the component node
	USCS_Node* TargetNode = nullptr;
	for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
	{
		if (Node && Node->GetVariableName().ToString() == ComponentName)
		{
			TargetNode = Node;
			break;
		}
	}

	if (!TargetNode || !TargetNode->ComponentTemplate)
	{
		return MakeError(FString::Printf(TEXT("Component not found: %s"), *ComponentName));
	}

	UObject* ComponentTemplate = TargetNode->ComponentTemplate;

	// Find the property
	FProperty* Property = ComponentTemplate->GetClass()->FindPropertyByName(*PropertyName);
	if (!Property)
	{
		return MakeError(FString::Printf(TEXT("Property not found: %s"), *PropertyName));
	}

	// FClassProperty (TSubclassOf<T>) derives from FObjectProperty, so it
	// must be checked first - otherwise it falls into the FObjectProperty
	// branch below and gets a UBlueprint/UWidgetBlueprint *asset* stored
	// directly into a UClass* slot (a TSubclassOf's underlying value must
	// always be an actual UClass, e.g. the Blueprint's GeneratedClass, not
	// the editor asset that owns it). That mismatch doesn't fail loudly -
	// it silently corrupts the property and crashes the editor later, on
	// save/serialize, with an unrelated-looking assertion deep in
	// PropertyClass.cpp. Hit via DebugReloadTuningWidgetClass
	// (TSubclassOf<UUserWidget>) - see AGENTS.md.
	if (FClassProperty* ClassProp = CastField<FClassProperty>(Property))
	{
		UObject* Loaded = LoadObject<UObject>(nullptr, *PropertyValue);
		UClass* ResolvedClass = Cast<UClass>(Loaded);
		if (!ResolvedClass)
		{
			if (UBlueprint* ReferencedBlueprint = Cast<UBlueprint>(Loaded))
			{
				ResolvedClass = ReferencedBlueprint->GeneratedClass;
			}
		}

		if (!ResolvedClass)
		{
			return MakeError(FString::Printf(TEXT("Could not resolve class from: %s"), *PropertyValue));
		}

		ClassProp->SetObjectPropertyValue(ClassProp->ContainerPtrToValuePtr<void>(ComponentTemplate), ResolvedClass);
	}
	// Handle object reference properties (like UInputAction*)
	else if (FObjectProperty* ObjProp = CastField<FObjectProperty>(Property))
	{
		// Load the referenced object
		UObject* ReferencedObject = LoadObject<UObject>(nullptr, *PropertyValue);
		if (!ReferencedObject)
		{
			return MakeError(FString::Printf(TEXT("Could not load object: %s"), *PropertyValue));
		}

		ObjProp->SetObjectPropertyValue(ObjProp->ContainerPtrToValuePtr<void>(ComponentTemplate), ReferencedObject);
	}
	else
	{
		// Use generic property import for other types
		void* ValuePtr = Property->ContainerPtrToValuePtr<void>(ComponentTemplate);
		if (!Property->ImportText_Direct(*PropertyValue, ValuePtr, ComponentTemplate, PPF_None))
		{
			return MakeError(FString::Printf(TEXT("Failed to set property value: %s"), *PropertyValue));
		}
	}

	// Mark blueprint as modified
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Property %s set successfully"), *PropertyName));

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleReplaceComponentMapValue(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid())
	{
		return MakeError(TEXT("Missing parameters"));
	}

	FString BlueprintPath = Params->GetStringField(TEXT("blueprint_path"));
	FString ComponentName = Params->GetStringField(TEXT("component_name"));
	FString PropertyName = Params->GetStringField(TEXT("property_name"));
	FString MapKey = Params->GetStringField(TEXT("map_key"));
	FString TargetClassName = Params->GetStringField(TEXT("target_class"));

	if (BlueprintPath.IsEmpty() || ComponentName.IsEmpty() || PropertyName.IsEmpty() || MapKey.IsEmpty() || TargetClassName.IsEmpty())
	{
		return MakeError(TEXT("Missing required parameters"));
	}

	UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
	}

	if (!Blueprint->SimpleConstructionScript)
	{
		return MakeError(TEXT("Blueprint has no SimpleConstructionScript"));
	}

	// Find the component node
	USCS_Node* TargetNode = nullptr;
	for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
	{
		if (Node && Node->GetVariableName().ToString() == ComponentName)
		{
			TargetNode = Node;
			break;
		}
	}

	if (!TargetNode || !TargetNode->ComponentTemplate)
	{
		return MakeError(FString::Printf(TEXT("Component not found: %s"), *ComponentName));
	}

	UObject* ComponentTemplate = TargetNode->ComponentTemplate;

	// Find the map property
	FMapProperty* MapProp = FindFProperty<FMapProperty>(ComponentTemplate->GetClass(), *PropertyName);
	if (!MapProp)
	{
		return MakeError(FString::Printf(TEXT("Map property not found: %s"), *PropertyName));
	}

	// Get the map helper
	void* MapPtr = MapProp->ContainerPtrToValuePtr<void>(ComponentTemplate);
	FScriptMapHelper MapHelper(MapProp, MapPtr);

	// Find the key in the map
	int32 FoundIndex = INDEX_NONE;
	FString KeyToFind = MapKey;

	for (int32 i = 0; i < MapHelper.Num(); ++i)
	{
		if (!MapHelper.IsValidIndex(i)) continue;

		// Get the key
		void* KeyPtr = MapHelper.GetKeyPtr(i);
		FString KeyStr;
		MapProp->KeyProp->ExportTextItem_Direct(KeyStr, KeyPtr, nullptr, nullptr, PPF_None);

		// Remove quotes if present
		KeyStr = KeyStr.TrimQuotes();

		if (KeyStr == KeyToFind)
		{
			FoundIndex = i;
			break;
		}
	}

	if (FoundIndex == INDEX_NONE)
	{
		return MakeError(FString::Printf(TEXT("Map key not found: %s"), *MapKey));
	}

	// Get the current value
	void* ValuePtr = MapHelper.GetValuePtr(FoundIndex);
	FObjectProperty* ValueProp = CastField<FObjectProperty>(MapProp->ValueProp);

	if (!ValueProp)
	{
		return MakeError(TEXT("Map value is not an object property"));
	}

	UObject* CurrentValue = ValueProp->GetObjectPropertyValue(ValuePtr);
	if (!CurrentValue)
	{
		return MakeError(TEXT("Current map value is null"));
	}

	// Find or load the target class
	UClass* TargetClass = FindObject<UClass>(nullptr, *TargetClassName);
	if (!TargetClass)
	{
		TargetClass = LoadObject<UClass>(nullptr, *TargetClassName);
	}

	if (!TargetClass)
	{
		return MakeError(FString::Printf(TEXT("Target class not found: %s"), *TargetClassName));
	}

	// Create a new instance of the target class
	UObject* NewInstance = NewObject<UObject>(ComponentTemplate, TargetClass, NAME_None, RF_Transactional);
	if (!NewInstance)
	{
		return MakeError(FString::Printf(TEXT("Failed to create instance of class: %s"), *TargetClassName));
	}

	// Copy properties from old instance to new instance if they're compatible
	if (CurrentValue->GetClass()->IsChildOf(TargetClass) || TargetClass->IsChildOf(CurrentValue->GetClass()))
	{
		for (TFieldIterator<FProperty> PropIt(TargetClass); PropIt; ++PropIt)
		{
			FProperty* Property = *PropIt;
			if (!Property->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible))
			{
				continue;
			}

			// Try to find matching property in source
			FProperty* SourceProperty = CurrentValue->GetClass()->FindPropertyByName(Property->GetFName());
			if (SourceProperty && SourceProperty->SameType(Property))
			{
				void* SourceValuePtr = SourceProperty->ContainerPtrToValuePtr<void>(CurrentValue);
				void* DestValuePtr = Property->ContainerPtrToValuePtr<void>(NewInstance);
				Property->CopyCompleteValue(DestValuePtr, SourceValuePtr);
			}
		}
	}

	// Replace the map value
	ValueProp->SetObjectPropertyValue(ValuePtr, NewInstance);

	// Mark blueprint as modified
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Replaced map entry '%s' with instance of %s"), *MapKey, *TargetClassName));
	Data->SetStringField(TEXT("old_class"), CurrentValue->GetClass()->GetName());
	Data->SetStringField(TEXT("new_class"), TargetClass->GetName());

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleReplaceBlueprintArrayValue(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid())
	{
		return MakeError(TEXT("Missing parameters"));
	}

	FString BlueprintPath = Params->GetStringField(TEXT("blueprint_path"));
	FString PropertyName = Params->GetStringField(TEXT("property_name"));
	int32 ArrayIndex = Params->GetIntegerField(TEXT("array_index"));
	FString TargetClassName = Params->GetStringField(TEXT("target_class"));

	if (BlueprintPath.IsEmpty() || PropertyName.IsEmpty() || TargetClassName.IsEmpty())
	{
		return MakeError(TEXT("Missing required parameters"));
	}

	UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
	}

	UClass* GeneratedClass = Blueprint->GeneratedClass;
	if (!GeneratedClass)
	{
		return MakeError(TEXT("Blueprint has no generated class"));
	}

	UObject* CDO = GeneratedClass->GetDefaultObject();
	if (!CDO)
	{
		return MakeError(TEXT("Could not get Class Default Object"));
	}

	// Find the array property
	FArrayProperty* ArrayProp = FindFProperty<FArrayProperty>(CDO->GetClass(), *PropertyName);
	if (!ArrayProp)
	{
		return MakeError(FString::Printf(TEXT("Array property not found: %s"), *PropertyName));
	}

	// Get the array helper
	void* ArrayPtr = ArrayProp->ContainerPtrToValuePtr<void>(CDO);
	FScriptArrayHelper ArrayHelper(ArrayProp, ArrayPtr);

	// Check array bounds
	if (ArrayIndex < 0 || ArrayIndex >= ArrayHelper.Num())
	{
		return MakeError(FString::Printf(TEXT("Array index %d out of bounds (array size: %d)"), ArrayIndex, ArrayHelper.Num()));
	}

	// Get the element property
	FObjectProperty* ElementProp = CastField<FObjectProperty>(ArrayProp->Inner);
	if (!ElementProp)
	{
		return MakeError(TEXT("Array element is not an object property"));
	}

	// Get current value
	void* ElementPtr = ArrayHelper.GetRawPtr(ArrayIndex);
	UObject* CurrentValue = ElementProp->GetObjectPropertyValue(ElementPtr);
	if (!CurrentValue)
	{
		return MakeError(TEXT("Current array element is null"));
	}

	// Find or load the target class
	UClass* TargetClass = FindObject<UClass>(nullptr, *TargetClassName);
	if (!TargetClass)
	{
		TargetClass = LoadObject<UClass>(nullptr, *TargetClassName);
	}

	if (!TargetClass)
	{
		return MakeError(FString::Printf(TEXT("Target class not found: %s"), *TargetClassName));
	}

	// Create a new instance of the target class
	UObject* NewInstance = NewObject<UObject>(CDO, TargetClass, NAME_None, RF_Transactional | RF_ArchetypeObject | RF_Public);
	if (!NewInstance)
	{
		return MakeError(FString::Printf(TEXT("Failed to create instance of class: %s"), *TargetClassName));
	}

	// Copy properties from old instance to new instance if they're compatible
	if (CurrentValue->GetClass()->IsChildOf(TargetClass) || TargetClass->IsChildOf(CurrentValue->GetClass()))
	{
		for (TFieldIterator<FProperty> PropIt(TargetClass); PropIt; ++PropIt)
		{
			FProperty* Property = *PropIt;
			if (!Property->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible))
			{
				continue;
			}

			// Try to find matching property in source
			FProperty* SourceProperty = CurrentValue->GetClass()->FindPropertyByName(Property->GetFName());
			if (SourceProperty && SourceProperty->SameType(Property))
			{
				void* SourceValuePtr = SourceProperty->ContainerPtrToValuePtr<void>(CurrentValue);
				void* DestValuePtr = Property->ContainerPtrToValuePtr<void>(NewInstance);
				Property->CopyCompleteValue(DestValuePtr, SourceValuePtr);
			}
		}
	}

	// Replace the array element
	ElementProp->SetObjectPropertyValue(ElementPtr, NewInstance);

	// Mark blueprint as modified
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Replaced array[%d] with instance of %s"), ArrayIndex, *TargetClassName));
	Data->SetStringField(TEXT("old_class"), CurrentValue->GetClass()->GetName());
	Data->SetStringField(TEXT("new_class"), TargetClass->GetName());

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleClearComponentMapValueArray(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid())
	{
		return MakeError(TEXT("Invalid parameters"));
	}

	const FString BlueprintPath = Params->GetStringField(TEXT("blueprint_path"));
	const FString ComponentName = Params->GetStringField(TEXT("component_name"));
	const FString MapPropertyName = Params->GetStringField(TEXT("map_property_name"));
	const FString MapKey = Params->GetStringField(TEXT("map_key"));
	const FString ArrayPropertyName = Params->GetStringField(TEXT("array_property_name"));

	if (BlueprintPath.IsEmpty() || ComponentName.IsEmpty() || MapPropertyName.IsEmpty() ||
		MapKey.IsEmpty() || ArrayPropertyName.IsEmpty())
	{
		return MakeError(TEXT("Missing required parameters"));
	}

	// Load the blueprint
	UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Failed to load blueprint: %s"), *BlueprintPath));
	}

	// Get the SimpleConstructionScript
	USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
	if (!SCS)
	{
		return MakeError(TEXT("Blueprint has no SimpleConstructionScript"));
	}

	// Find the component node
	USCS_Node* ComponentNode = nullptr;
	for (USCS_Node* Node : SCS->GetAllNodes())
	{
		if (Node && Node->GetVariableName().ToString() == ComponentName)
		{
			ComponentNode = Node;
			break;
		}
	}

	if (!ComponentNode)
	{
		return MakeError(FString::Printf(TEXT("Component not found: %s"), *ComponentName));
	}

	// Get the component template
	UObject* ComponentTemplate = ComponentNode->ComponentTemplate;
	if (!ComponentTemplate)
	{
		return MakeError(TEXT("Component template is null"));
	}

	// Find the map property
	FMapProperty* MapProperty = FindFProperty<FMapProperty>(ComponentTemplate->GetClass(), *MapPropertyName);
	if (!MapProperty)
	{
		return MakeError(FString::Printf(TEXT("Map property not found: %s"), *MapPropertyName));
	}

	// Get the map helper
	void* MapPtr = MapProperty->ContainerPtrToValuePtr<void>(ComponentTemplate);
	FScriptMapHelper MapHelper(MapProperty, MapPtr);

	// Find the key in the map
	int32 FoundIndex = -1;
	FProperty* KeyProp = MapProperty->KeyProp;

	// Create a temporary key to search with
	void* TempKey = FMemory::Malloc(KeyProp->GetSize());
	KeyProp->InitializeValue(TempKey);

	if (FNameProperty* NameKeyProp = CastField<FNameProperty>(KeyProp))
	{
		FName KeyName(*MapKey);
		NameKeyProp->SetPropertyValue(TempKey, KeyName);
	}
	else if (FStrProperty* StrKeyProp = CastField<FStrProperty>(KeyProp))
	{
		StrKeyProp->SetPropertyValue(TempKey, MapKey);
	}
	else
	{
		KeyProp->DestroyValue(TempKey);
		FMemory::Free(TempKey);
		return MakeError(TEXT("Unsupported key type (only FName and FString supported)"));
	}

	// Find the key in the map
	for (int32 i = 0; i < MapHelper.Num(); i++)
	{
		if (MapHelper.IsValidIndex(i))
		{
			const void* KeyPtr = MapHelper.GetKeyPtr(i);
			if (KeyProp->Identical(KeyPtr, TempKey))
			{
				FoundIndex = i;
				break;
			}
		}
	}

	KeyProp->DestroyValue(TempKey);
	FMemory::Free(TempKey);

	if (FoundIndex == -1)
	{
		return MakeError(FString::Printf(TEXT("Key not found in map: %s"), *MapKey));
	}

	// Get the value object at this index
	uint8* ValuePtr = MapHelper.GetValuePtr(FoundIndex);
	FObjectProperty* ValueProp = CastField<FObjectProperty>(MapProperty->ValueProp);
	if (!ValueProp)
	{
		return MakeError(TEXT("Map value is not an object property"));
	}

	UObject* ValueObject = ValueProp->GetObjectPropertyValue(ValuePtr);
	if (!ValueObject)
	{
		return MakeError(TEXT("Map value object is null"));
	}

	// Find the array property in the value object
	FArrayProperty* ArrayProp = FindFProperty<FArrayProperty>(ValueObject->GetClass(), *ArrayPropertyName);
	if (!ArrayProp)
	{
		return MakeError(FString::Printf(TEXT("Array property not found in value object: %s"), *ArrayPropertyName));
	}

	// Mark for modification
	Blueprint->Modify();
	ComponentTemplate->Modify();
	ValueObject->Modify();

	// Get the array helper and clear it
	void* ArrayPtr = ArrayProp->ContainerPtrToValuePtr<void>(ValueObject);
	FScriptArrayHelper ArrayHelper(ArrayProp, ArrayPtr);

	int32 OldSize = ArrayHelper.Num();
	ArrayHelper.EmptyValues();

	// Mark as dirty
	Blueprint->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("Array cleared successfully"));
	Data->SetStringField(TEXT("blueprint_path"), BlueprintPath);
	Data->SetStringField(TEXT("component_name"), ComponentName);
	Data->SetStringField(TEXT("map_property"), MapPropertyName);
	Data->SetStringField(TEXT("map_key"), MapKey);
	Data->SetStringField(TEXT("array_property"), ArrayPropertyName);
	Data->SetNumberField(TEXT("elements_cleared"), OldSize);

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleReplaceComponentClass(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid())
	{
		return MakeError(TEXT("Missing parameters"));
	}

	FString BlueprintPath = Params->GetStringField(TEXT("blueprint_path"));
	FString ComponentName = Params->GetStringField(TEXT("component_name"));
	FString NewComponentClass = Params->GetStringField(TEXT("new_class"));

	if (BlueprintPath.IsEmpty() || ComponentName.IsEmpty() || NewComponentClass.IsEmpty())
	{
		return MakeError(TEXT("Missing required parameters: blueprint_path, component_name, or new_class"));
	}

	// Load the blueprint
	UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
	}

	if (!Blueprint->SimpleConstructionScript)
	{
		return MakeError(TEXT("Blueprint has no SimpleConstructionScript"));
	}

	// Find the component node
	USCS_Node* ComponentNode = nullptr;
	for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
	{
		if (Node && Node->GetVariableName().ToString() == ComponentName)
		{
			ComponentNode = Node;
			break;
		}
	}

	if (!ComponentNode)
	{
		return MakeError(FString::Printf(TEXT("Component not found: %s"), *ComponentName));
	}

	// Find the new component class
	UClass* NewClass = FindFirstObject<UClass>(*NewComponentClass, EFindFirstObjectOptions::ExactClass);
	if (!NewClass)
	{
		// Try with _C suffix for blueprint classes
		NewClass = FindFirstObject<UClass>(*(NewComponentClass + TEXT("_C")), EFindFirstObjectOptions::ExactClass);
	}
	if (!NewClass)
	{
		// Try loading as path
		NewClass = LoadClass<UActorComponent>(nullptr, *NewComponentClass);
	}
	if (!NewClass)
	{
		return MakeError(FString::Printf(TEXT("Component class not found: %s"), *NewComponentClass));
	}

	// Verify it's an ActorComponent
	if (!NewClass->IsChildOf(UActorComponent::StaticClass()))
	{
		return MakeError(FString::Printf(TEXT("Class is not an ActorComponent: %s"), *NewComponentClass));
	}

	FString OldClassName = ComponentNode->ComponentClass ? ComponentNode->ComponentClass->GetName() : TEXT("None");

	// Mark for modification
	Blueprint->Modify();
	if (ComponentNode->ComponentTemplate)
	{
		ComponentNode->ComponentTemplate->Modify();
	}

	// Replace the component class
	ComponentNode->ComponentClass = NewClass;

	// Recreate the component template with the new class
	if (ComponentNode->ComponentTemplate)
	{
		// Destroy old template
		ComponentNode->ComponentTemplate = nullptr;
	}

	// Create new template
	ComponentNode->ComponentTemplate = NewObject<UActorComponent>(
		Blueprint->SimpleConstructionScript,
		NewClass,
		*ComponentName,
		RF_ArchetypeObject | RF_Public | RF_Transactional
	);

	// Mark blueprint as modified
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	Blueprint->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("Component class replaced successfully"));
	Data->SetStringField(TEXT("blueprint_path"), BlueprintPath);
	Data->SetStringField(TEXT("component_name"), ComponentName);
	Data->SetStringField(TEXT("old_class"), OldClassName);
	Data->SetStringField(TEXT("new_class"), NewClass->GetName());

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleDeleteComponent(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid())
	{
		return MakeError(TEXT("Missing parameters"));
	}

	FString BlueprintPath = Params->GetStringField(TEXT("blueprint_path"));
	if (BlueprintPath.IsEmpty())
	{
		BlueprintPath = Params->GetStringField(TEXT("path"));
	}
	FString ComponentName = Params->GetStringField(TEXT("component_name"));

	if (BlueprintPath.IsEmpty() || ComponentName.IsEmpty())
	{
		return MakeError(TEXT("Missing 'blueprint_path' or 'component_name' parameter"));
	}

	UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Failed to load blueprint: %s"), *BlueprintPath));
	}

	if (!Blueprint->SimpleConstructionScript)
	{
		return MakeError(TEXT("Blueprint has no SimpleConstructionScript"));
	}

	USCS_Node* TargetNode = nullptr;
	for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
	{
		if (Node && Node->GetVariableName().ToString() == ComponentName)
		{
			TargetNode = Node;
			break;
		}
	}

	if (!TargetNode)
	{
		return MakeError(FString::Printf(TEXT("Component not found: %s"), *ComponentName));
	}

	// Remove the node from the SCS
	Blueprint->SimpleConstructionScript->RemoveNode(TargetNode);

	// Mark the blueprint as modified
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("deleted_component"), ComponentName);
	Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Successfully deleted component: %s"), *ComponentName));

	return MakeResponse(true, Data);
}
