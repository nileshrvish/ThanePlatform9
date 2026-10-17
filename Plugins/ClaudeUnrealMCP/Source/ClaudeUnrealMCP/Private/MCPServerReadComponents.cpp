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

FString FMCPServer::HandleReadComponents(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("path")))
	{
		return MakeError(TEXT("Missing 'path' parameter"));
	}

	FString Path = Params->GetStringField(TEXT("path"));
	UBlueprint* Blueprint = LoadBlueprintFromPath(Path);

	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *Path));
	}

	TArray<TSharedPtr<FJsonValue>> CompArray;

	if (Blueprint->SimpleConstructionScript)
	{
		const TArray<USCS_Node*>& Nodes = Blueprint->SimpleConstructionScript->GetAllNodes();
		for (USCS_Node* Node : Nodes)
		{
			if (!Node) continue;

			TSharedPtr<FJsonObject> CompObj = MakeShared<FJsonObject>();
			CompObj->SetStringField(TEXT("name"), Node->GetVariableName().ToString());

			if (Node->ComponentClass)
			{
				CompObj->SetStringField(TEXT("class"), Node->ComponentClass->GetName());
			}

			if (Node->ComponentTemplate)
			{
				CompObj->SetStringField(TEXT("template_name"), Node->ComponentTemplate->GetName());
			}

			// Get parent
			if (USCS_Node* Parent = Blueprint->SimpleConstructionScript->FindParentNode(Node))
			{
				CompObj->SetStringField(TEXT("parent"), Parent->GetVariableName().ToString());
			}

			CompArray.Add(MakeShared<FJsonValueObject>(CompObj));
		}
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("components"), CompArray);
	Data->SetNumberField(TEXT("count"), CompArray.Num());

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleReadComponentProperties(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("path")) || !Params->HasField(TEXT("component_name")))
	{
		return MakeError(TEXT("Missing 'path' or 'component_name' parameter"));
	}

	FString Path = Params->GetStringField(TEXT("path"));
	FString ComponentName = Params->GetStringField(TEXT("component_name"));

	UBlueprint* Blueprint = LoadBlueprintFromPath(Path);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *Path));
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
	UClass* ComponentClass = ComponentTemplate->GetClass();

	TArray<TSharedPtr<FJsonValue>> PropsArray;

	// Iterate through all properties
	for (TFieldIterator<FProperty> PropIt(ComponentClass); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;
		if (!Property) continue;

		// Skip properties that aren't editable
		if (!Property->HasAnyPropertyFlags(CPF_Edit))
		{
			continue;
		}

		TSharedPtr<FJsonObject> PropObj = MakeShared<FJsonObject>();
		PropObj->SetStringField(TEXT("name"), Property->GetName());
		PropObj->SetStringField(TEXT("type"), Property->GetClass()->GetName());

		// Get property category
		FString Category = Property->GetMetaData(TEXT("Category"));
		if (!Category.IsEmpty())
		{
			PropObj->SetStringField(TEXT("category"), Category);
		}

		// Get property value as string
		void* ValuePtr = Property->ContainerPtrToValuePtr<void>(ComponentTemplate);
		FString ValueStr;
		Property->ExportTextItem_Direct(ValueStr, ValuePtr, nullptr, ComponentTemplate, PPF_None);
		PropObj->SetStringField(TEXT("value"), ValueStr);

		// For object/class properties, also export the object path
		if (FObjectProperty* ObjProp = CastField<FObjectProperty>(Property))
		{
			UObject* ObjValue = ObjProp->GetObjectPropertyValue(ValuePtr);
			if (ObjValue)
			{
				PropObj->SetStringField(TEXT("object_value"), ObjValue->GetPathName());
				PropObj->SetStringField(TEXT("object_class"), ObjValue->GetClass()->GetName());
			}
		}
		else if (FClassProperty* ClassProp = CastField<FClassProperty>(Property))
		{
			UClass* ClassValue = Cast<UClass>(ClassProp->GetObjectPropertyValue(ValuePtr));
			if (ClassValue)
			{
				PropObj->SetStringField(TEXT("class_value"), ClassValue->GetPathName());
				PropObj->SetStringField(TEXT("class_name"), ClassValue->GetName());
			}
		}

		PropsArray.Add(MakeShared<FJsonValueObject>(PropObj));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("properties"), PropsArray);
	Data->SetNumberField(TEXT("count"), PropsArray.Num());
	Data->SetStringField(TEXT("component_name"), ComponentName);
	Data->SetStringField(TEXT("component_class"), ComponentClass->GetName());

	return MakeResponse(true, Data);
}

