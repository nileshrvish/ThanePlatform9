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

FString FMCPServer::HandleListActors(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return MakeError(TEXT("No world available"));
	}

	TArray<TSharedPtr<FJsonValue>> ActorsArray;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor) continue;

		TSharedPtr<FJsonObject> ActorObj = MakeShared<FJsonObject>();
		ActorObj->SetStringField(TEXT("name"), Actor->GetName());
		ActorObj->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
		ActorObj->SetStringField(TEXT("label"), Actor->GetActorLabel());

		FVector Location = Actor->GetActorLocation();
		ActorObj->SetStringField(TEXT("location"), FString::Printf(TEXT("%.1f, %.1f, %.1f"), Location.X, Location.Y, Location.Z));

		ActorsArray.Add(MakeShared<FJsonValueObject>(ActorObj));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("actors"), ActorsArray);
	Data->SetNumberField(TEXT("count"), ActorsArray.Num());

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleReadActorComponents(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("actor_name")))
	{
		return MakeError(TEXT("Missing required parameter: 'actor_name'"));
	}

	const FString ActorName = Params->GetStringField(TEXT("actor_name"));

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return MakeError(TEXT("No world available"));
	}

	AActor* FoundActor = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor && Actor->GetName() == ActorName)
		{
			FoundActor = Actor;
			break;
		}
	}

	if (!FoundActor)
	{
		return MakeError(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
	}

	TArray<TSharedPtr<FJsonValue>> ComponentsArray;
	TInlineComponentArray<UActorComponent*> Components;
	FoundActor->GetComponents(Components);

	for (UActorComponent* Component : Components)
	{
		if (!Component) continue;

		TSharedPtr<FJsonObject> CompObj = MakeShared<FJsonObject>();
		CompObj->SetStringField(TEXT("name"), Component->GetName());
		CompObj->SetStringField(TEXT("class"), Component->GetClass()->GetName());
		CompObj->SetBoolField(TEXT("active"), Component->IsActive());
		CompObj->SetBoolField(TEXT("registered"), Component->IsRegistered());
		CompObj->SetBoolField(TEXT("editor_only"), Component->IsEditorOnly());
		ComponentsArray.Add(MakeShared<FJsonValueObject>(CompObj));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("actor_name"), ActorName);
	Data->SetStringField(TEXT("actor_class"), FoundActor->GetClass()->GetName());
	Data->SetArrayField(TEXT("components"), ComponentsArray);
	Data->SetNumberField(TEXT("count"), ComponentsArray.Num());

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleReadActorComponentProperties(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("actor_name")) || !Params->HasField(TEXT("component_name")))
	{
		return MakeError(TEXT("Missing required parameters: 'actor_name', 'component_name'"));
	}

	const FString ActorName = Params->GetStringField(TEXT("actor_name"));
	const FString ComponentName = Params->GetStringField(TEXT("component_name"));

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return MakeError(TEXT("No world available"));
	}

	AActor* FoundActor = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor && Actor->GetName() == ActorName)
		{
			FoundActor = Actor;
			break;
		}
	}

	if (!FoundActor)
	{
		return MakeError(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
	}

	UActorComponent* TargetComponent = nullptr;
	TInlineComponentArray<UActorComponent*> Components;
	FoundActor->GetComponents(Components);

	for (UActorComponent* Component : Components)
	{
		if (!Component) continue;
		const FString CompName = Component->GetName();
		if (CompName.Equals(ComponentName, ESearchCase::IgnoreCase))
		{
			TargetComponent = Component;
			break;
		}
	}

	if (!TargetComponent)
	{
		return MakeError(FString::Printf(TEXT("Component not found on actor '%s': %s"), *ActorName, *ComponentName));
	}

	TArray<TSharedPtr<FJsonValue>> PropsArray;
	UClass* ComponentClass = TargetComponent->GetClass();

	for (TFieldIterator<FProperty> PropIt(ComponentClass); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;
		if (!Property) continue;

		if (!Property->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible))
		{
			continue;
		}

		TSharedPtr<FJsonObject> PropObj = MakeShared<FJsonObject>();
		PropObj->SetStringField(TEXT("name"), Property->GetName());
		PropObj->SetStringField(TEXT("type"), Property->GetClass()->GetName());

		FString Category = Property->GetMetaData(TEXT("Category"));
		if (!Category.IsEmpty())
		{
			PropObj->SetStringField(TEXT("category"), Category);
		}

		void* ValuePtr = Property->ContainerPtrToValuePtr<void>(TargetComponent);
		FString ValueStr;
		Property->ExportTextItem_Direct(ValueStr, ValuePtr, nullptr, TargetComponent, PPF_None);
		PropObj->SetStringField(TEXT("value"), ValueStr);

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
	Data->SetStringField(TEXT("actor_name"), ActorName);
	Data->SetStringField(TEXT("component_name"), TargetComponent->GetName());
	Data->SetStringField(TEXT("component_class"), ComponentClass->GetName());
	Data->SetArrayField(TEXT("properties"), PropsArray);
	Data->SetNumberField(TEXT("count"), PropsArray.Num());

	return MakeResponse(true, Data);
}
