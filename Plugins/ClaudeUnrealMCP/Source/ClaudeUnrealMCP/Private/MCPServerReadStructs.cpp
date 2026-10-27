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
#include "MCPServerHelpers.h"

FString FMCPServer::HandleReadUserDefinedStruct(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("path")))
	{
		return MakeError(TEXT("Missing 'path' parameter"));
	}

	const FString Path = Params->GetStringField(TEXT("path"));
	UUserDefinedStruct* Struct = LoadObject<UUserDefinedStruct>(nullptr, *Path);
	if (!Struct)
	{
		return MakeError(FString::Printf(TEXT("Struct not found: %s"), *Path));
	}

	TArray<TSharedPtr<FJsonValue>> FieldsArray;
	for (TFieldIterator<FProperty> It(Struct); It; ++It)
	{
		const FProperty* Prop = *It;
		if (!Prop) continue;
		TSharedPtr<FJsonObject> FieldObj = MakeShared<FJsonObject>();
		SerializeProperty(Prop, FieldObj);
		FieldsArray.Add(MakeShared<FJsonValueObject>(FieldObj));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("name"), Struct->GetName());
	Data->SetStringField(TEXT("path"), Struct->GetPathName());
	Data->SetArrayField(TEXT("fields"), FieldsArray);
	Data->SetNumberField(TEXT("count"), FieldsArray.Num());

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleReadUserDefinedEnum(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("path")))
	{
		return MakeError(TEXT("Missing 'path' parameter"));
	}

	const FString Path = Params->GetStringField(TEXT("path"));
	UUserDefinedEnum* Enum = LoadObject<UUserDefinedEnum>(nullptr, *Path);
	if (!Enum)
	{
		return MakeError(FString::Printf(TEXT("Enum not found: %s"), *Path));
	}

	TArray<TSharedPtr<FJsonValue>> EntriesArray;
	const int32 NumEnums = Enum->NumEnums();
	for (int32 Index = 0; Index < NumEnums; ++Index)
	{
		if (Enum->HasMetaData(TEXT("Hidden"), Index))
		{
			continue;
		}

		FString Name = Enum->GetNameStringByIndex(Index);
		if (Name.EndsWith(TEXT("_MAX")))
		{
			continue;
		}

		TSharedPtr<FJsonObject> EntryObj = MakeShared<FJsonObject>();
		EntryObj->SetStringField(TEXT("name"), Name);
		EntryObj->SetStringField(TEXT("display_name"), Enum->GetDisplayNameTextByIndex(Index).ToString());
		EntryObj->SetNumberField(TEXT("value"), Enum->GetValueByIndex(Index));

		EntriesArray.Add(MakeShared<FJsonValueObject>(EntryObj));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("name"), Enum->GetName());
	Data->SetStringField(TEXT("path"), Enum->GetPathName());
	Data->SetArrayField(TEXT("entries"), EntriesArray);
	Data->SetNumberField(TEXT("count"), EntriesArray.Num());

	return MakeResponse(true, Data);
}
