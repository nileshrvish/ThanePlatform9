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

FString FMCPServer::HandleSetBlueprintCDOClassReference(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid())
	{
		return MakeError(TEXT("Missing parameters"));
	}

	FString BlueprintPath = Params->GetStringField(TEXT("blueprint_path"));
	FString ComponentName = Params->GetStringField(TEXT("component_name"));
	FString PropertyName = Params->GetStringField(TEXT("property_name"));
	FString ClassName = Params->GetStringField(TEXT("class_name"));

	if (BlueprintPath.IsEmpty() || ComponentName.IsEmpty() || PropertyName.IsEmpty() || ClassName.IsEmpty())
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

	// Handle class properties (FClassProperty)
	if (FClassProperty* ClassProp = CastField<FClassProperty>(Property))
	{
		// Try to find the class by name
		UClass* TargetClass = FindObject<UClass>(nullptr, *ClassName);
		if (!TargetClass)
		{
			// Try loading as a full path
			TargetClass = LoadObject<UClass>(nullptr, *ClassName);
		}

		if (!TargetClass)
		{
			return MakeError(FString::Printf(TEXT("Class not found: %s"), *ClassName));
		}

		// Verify the class is compatible with the property's metaclass
		if (!TargetClass->IsChildOf(ClassProp->MetaClass))
		{
			return MakeError(FString::Printf(TEXT("Class %s is not compatible with property metaclass %s"),
				*TargetClass->GetName(), *ClassProp->MetaClass->GetName()));
		}

		// Set the class reference
		ClassProp->SetObjectPropertyValue(ClassProp->ContainerPtrToValuePtr<void>(ComponentTemplate), TargetClass);
	}
	// Handle soft class properties (FSoftClassProperty)
	else if (FSoftClassProperty* SoftClassProp = CastField<FSoftClassProperty>(Property))
	{
		// Try to find the class by name
		UClass* TargetClass = FindObject<UClass>(nullptr, *ClassName);
		if (!TargetClass)
		{
			// Try loading as a full path
			TargetClass = LoadObject<UClass>(nullptr, *ClassName);
		}

		if (!TargetClass)
		{
			return MakeError(FString::Printf(TEXT("Class not found: %s"), *ClassName));
		}

		// Create a soft object ptr to the class
		FSoftObjectPtr SoftPtr(TargetClass);
		void* ValuePtr = SoftClassProp->ContainerPtrToValuePtr<void>(ComponentTemplate);
		SoftClassProp->SetPropertyValue(ValuePtr, SoftPtr);
	}
	else
	{
		return MakeError(FString::Printf(TEXT("Property %s is not a class property (type: %s)"),
			*PropertyName, *Property->GetClass()->GetName()));
	}

	// Mark blueprint as modified
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Class reference %s set successfully to %s"), *PropertyName, *ClassName));

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleSetBlueprintCDOProperty(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid())
	{
		return MakeError(TEXT("Missing parameters"));
	}

	FString BlueprintPath = Params->GetStringField(TEXT("blueprint_path"));
	FString PropertyName = Params->GetStringField(TEXT("property_name"));
	FString PropertyValue = Params->GetStringField(TEXT("property_value"));

	if (BlueprintPath.IsEmpty() || PropertyName.IsEmpty())
	{
		return MakeError(TEXT("Missing required parameters: blueprint_path or property_name"));
	}

	// Load the blueprint
	UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
	}

	// Get the generated class and its CDO
	UClass* GeneratedClass = Blueprint->GeneratedClass;
	if (!GeneratedClass)
	{
		return MakeError(TEXT("Blueprint has no generated class"));
	}

	UObject* CDO = GeneratedClass->GetDefaultObject();
	if (!CDO)
	{
		return MakeError(TEXT("Failed to get class default object"));
	}

	// Find the property (search through entire class hierarchy including inherited properties)
	FProperty* Property = nullptr;
	for (TFieldIterator<FProperty> PropIt(GeneratedClass); PropIt; ++PropIt)
	{
		if (PropIt->GetName() == PropertyName)
		{
			Property = *PropIt;
			break;
		}
	}

	if (!Property)
	{
		// Also try FindPropertyByName as fallback
		Property = GeneratedClass->FindPropertyByName(*PropertyName);
	}

	if (!Property)
	{
		return MakeError(FString::Printf(TEXT("Property not found: %s"), *PropertyName));
	}

	// Mark for modification
	Blueprint->Modify();
	CDO->Modify();

	// Handle different property types
	void* ValuePtr = Property->ContainerPtrToValuePtr<void>(CDO);

	// FClassProperty (TSubclassOf<T>) derives from FObjectProperty in the
	// FFieldClass hierarchy too, so CastField<FObjectProperty> below would
	// already match it - meaning the dedicated "Handle class property"
	// branch further down (which correctly uses LoadClass, resolving
	// Blueprint _C generated classes properly) was actually dead,
	// unreachable code. Same bug class as HandleSetComponentProperty (see
	// MCPServerComponentEdit.cpp / AGENTS.md): without this check running
	// first, a TSubclassOf property would get a UBlueprint/UWidgetBlueprint
	// *asset* stored directly into its UClass* slot instead of the actual
	// class - no error at set time, corrupts the property, crashes later.
	if (FClassProperty* CDOClassProp = CastField<FClassProperty>(Property))
	{
		if (PropertyValue.IsEmpty() || PropertyValue.Equals(TEXT("None"), ESearchCase::IgnoreCase))
		{
			CDOClassProp->SetObjectPropertyValue(ValuePtr, nullptr);
		}
		else
		{
			UClass* ClassValue = LoadClass<UObject>(nullptr, *PropertyValue);
			if (!ClassValue)
			{
				return MakeError(FString::Printf(TEXT("Could not load class: %s"), *PropertyValue));
			}

			if (!ClassValue->IsChildOf(CDOClassProp->MetaClass))
			{
				return MakeError(FString::Printf(TEXT("Class %s is not compatible with metaclass %s"),
					*ClassValue->GetName(), *CDOClassProp->MetaClass->GetName()));
			}

			CDOClassProp->SetObjectPropertyValue(ValuePtr, ClassValue);
		}
	}
	// Handle object property (TObjectPtr<>, UObject*, etc.)
	else if (FObjectProperty* ObjectProp = CastField<FObjectProperty>(Property))
	{
		if (PropertyValue.IsEmpty() || PropertyValue.Equals(TEXT("None"), ESearchCase::IgnoreCase))
		{
			// Clear the property
			ObjectProp->SetObjectPropertyValue(ValuePtr, nullptr);
		}
		else
		{
			// Load the object from the path
			UObject* ObjectValue = LoadObject<UObject>(nullptr, *PropertyValue);
			if (!ObjectValue)
			{
				// Try adding the asset name suffix (e.g., /Game/Input/IA_Sprint -> /Game/Input/IA_Sprint.IA_Sprint)
				FString AssetPath = PropertyValue;
				if (!AssetPath.Contains(TEXT(".")))
				{
					FString AssetName = FPaths::GetBaseFilename(AssetPath);
					AssetPath = AssetPath + TEXT(".") + AssetName;
				}
				ObjectValue = LoadObject<UObject>(nullptr, *AssetPath);
			}

			if (!ObjectValue)
			{
				return MakeError(FString::Printf(TEXT("Could not load object: %s"), *PropertyValue));
			}

			// Verify type compatibility
			if (!ObjectValue->IsA(ObjectProp->PropertyClass))
			{
				return MakeError(FString::Printf(TEXT("Object %s is not compatible with property type %s"),
					*ObjectValue->GetClass()->GetName(), *ObjectProp->PropertyClass->GetName()));
			}

			ObjectProp->SetObjectPropertyValue(ValuePtr, ObjectValue);
		}
	}
	// Handle soft object property (TSoftObjectPtr<>)
	else if (FSoftObjectProperty* SoftObjectProp = CastField<FSoftObjectProperty>(Property))
	{
		if (PropertyValue.IsEmpty() || PropertyValue.Equals(TEXT("None"), ESearchCase::IgnoreCase))
		{
			FSoftObjectPtr NullPtr;
			SoftObjectProp->SetPropertyValue(ValuePtr, NullPtr);
		}
		else
		{
			FSoftObjectPath SoftPath(PropertyValue);
			FSoftObjectPtr SoftPtr(SoftPath);
			SoftObjectProp->SetPropertyValue(ValuePtr, SoftPtr);
		}
	}
	// Handle class property (TSubclassOf<>)
	else if (FClassProperty* ClassProp = CastField<FClassProperty>(Property))
	{
		if (PropertyValue.IsEmpty() || PropertyValue.Equals(TEXT("None"), ESearchCase::IgnoreCase))
		{
			ClassProp->SetObjectPropertyValue(ValuePtr, nullptr);
		}
		else
		{
			UClass* ClassValue = LoadClass<UObject>(nullptr, *PropertyValue);
			if (!ClassValue)
			{
				return MakeError(FString::Printf(TEXT("Could not load class: %s"), *PropertyValue));
			}

			if (!ClassValue->IsChildOf(ClassProp->MetaClass))
			{
				return MakeError(FString::Printf(TEXT("Class %s is not compatible with metaclass %s"),
					*ClassValue->GetName(), *ClassProp->MetaClass->GetName()));
			}

			ClassProp->SetObjectPropertyValue(ValuePtr, ClassValue);
		}
	}
	// Handle basic types through text import
	else
	{
		const TCHAR* ImportBuffer = *PropertyValue;
		Property->ImportText_Direct(ImportBuffer, ValuePtr, CDO, PPF_None);
	}

	// Mark blueprint as modified
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	Blueprint->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Property %s set successfully"), *PropertyName));
	Data->SetStringField(TEXT("blueprint_path"), BlueprintPath);
	Data->SetStringField(TEXT("property_name"), PropertyName);
	Data->SetStringField(TEXT("property_value"), PropertyValue);

	return MakeResponse(true, Data);
}
