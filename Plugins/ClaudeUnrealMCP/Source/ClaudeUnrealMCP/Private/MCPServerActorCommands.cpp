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

FString FMCPServer::HandleReadActorProperties(const TSharedPtr<FJsonObject>& Params)
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

	// Find the actor by name in the current level
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

	// Serialize actor properties to JSON
	TSharedPtr<FJsonObject> PropertiesObj = MakeShared<FJsonObject>();
	UClass* ActorClass = FoundActor->GetClass();

	for (TFieldIterator<FProperty> PropIt(ActorClass); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;

		// Only export EditAnywhere properties to preserve instance data
		if (!Property->HasAnyPropertyFlags(CPF_Edit))
		{
			continue;
		}

		FString PropertyName = Property->GetName();
		FString PropertyValue;
		Property->ExportTextItem_Direct(PropertyValue, Property->ContainerPtrToValuePtr<void>(FoundActor), nullptr, FoundActor, PPF_None);

		PropertiesObj->SetStringField(PropertyName, PropertyValue);
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("actor_name"), ActorName);
	Data->SetStringField(TEXT("actor_class"), ActorClass->GetName());
	Data->SetStringField(TEXT("actor_label"), FoundActor->GetActorLabel());
	Data->SetObjectField(TEXT("properties"), PropertiesObj);

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleSetActorProperties(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("actor_name")) || !Params->HasField(TEXT("properties")))
	{
		return MakeError(TEXT("Missing required parameters: 'actor_name', 'properties'"));
	}

	const FString ActorName = Params->GetStringField(TEXT("actor_name"));
	const TSharedPtr<FJsonObject> PropertiesObj = Params->GetObjectField(TEXT("properties"));

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return MakeError(TEXT("No world available"));
	}

	// Find the actor by name
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

	// Mark the actor as modified
	FoundActor->Modify();

	// Deserialize properties from JSON back to actor
	UClass* ActorClass = FoundActor->GetClass();
	int32 PropertiesSet = 0;

	for (auto& Pair : PropertiesObj->Values)
	{
		FString PropertyName = FString(Pair.Key);
		FString PropertyValue = Pair.Value->AsString();

		FProperty* Property = ActorClass->FindPropertyByName(FName(*PropertyName));
		if (!Property)
		{
			UE_LOG(LogTemp, Warning, TEXT("Property not found: %s"), *PropertyName);
			continue;
		}

		// Only set EditAnywhere properties
		if (!Property->HasAnyPropertyFlags(CPF_Edit))
		{
			UE_LOG(LogTemp, Warning, TEXT("Property not editable: %s"), *PropertyName);
			continue;
		}

		// Import the property value
		Property->ImportText_Direct(*PropertyValue, Property->ContainerPtrToValuePtr<void>(FoundActor), FoundActor, PPF_None);
		PropertiesSet++;
	}

	// Mark the actor for saving
	FoundActor->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("Actor properties set successfully"));
	Data->SetStringField(TEXT("actor_name"), ActorName);
	Data->SetNumberField(TEXT("properties_set"), PropertiesSet);

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleSetActorComponentProperty(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("actor_name")) || !Params->HasField(TEXT("component_name")) ||
		!Params->HasField(TEXT("property_name")) || !Params->HasField(TEXT("property_value")))
	{
		return MakeError(TEXT("Missing required parameters: 'actor_name', 'component_name', 'property_name', 'property_value'"));
	}

	const FString ActorName = Params->GetStringField(TEXT("actor_name"));
	const FString ComponentName = Params->GetStringField(TEXT("component_name"));
	const FString PropertyName = Params->GetStringField(TEXT("property_name"));
	const FString PropertyValue = Params->GetStringField(TEXT("property_value"));

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
		if (Component->GetName().Equals(ComponentName, ESearchCase::IgnoreCase))
		{
			TargetComponent = Component;
			break;
		}
	}

	if (!TargetComponent)
	{
		return MakeError(FString::Printf(TEXT("Component not found on actor '%s': %s"), *ActorName, *ComponentName));
	}

	TargetComponent->Modify();

	bool bSet = false;
	if (PropertyName.Equals(TEXT("CollisionProfileName"), ESearchCase::IgnoreCase))
	{
		if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(TargetComponent))
		{
			Prim->SetCollisionProfileName(FName(*PropertyValue));
			bSet = true;
		}
	}

	if (!bSet)
	{
		FProperty* Property = TargetComponent->GetClass()->FindPropertyByName(FName(*PropertyName));
		if (!Property)
		{
			return MakeError(FString::Printf(TEXT("Property not found: %s"), *PropertyName));
		}

		if (!Property->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible))
		{
			return MakeError(FString::Printf(TEXT("Property not editable: %s"), *PropertyName));
		}

		if (!Property->ImportText_Direct(*PropertyValue, Property->ContainerPtrToValuePtr<void>(TargetComponent), TargetComponent, PPF_None))
		{
			return MakeError(FString::Printf(TEXT("Failed to set property value: %s"), *PropertyValue));
		}
	}

	if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(TargetComponent))
	{
		Prim->MarkRenderStateDirty();
	}

	FoundActor->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("Actor component property set successfully"));
	Data->SetStringField(TEXT("actor_name"), ActorName);
	Data->SetStringField(TEXT("component_name"), TargetComponent->GetName());
	Data->SetStringField(TEXT("property_name"), PropertyName);
	Data->SetStringField(TEXT("property_value"), PropertyValue);

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleReconstructActor(const TSharedPtr<FJsonObject>& Params)
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

	// Find the actor by name
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

	// Mark the actor as modified
	FoundActor->Modify();

	// Rerun construction scripts to trigger OnConstruction
	FoundActor->RerunConstructionScripts();

	// Mark the actor for saving
	FoundActor->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("Actor reconstructed successfully"));
	Data->SetStringField(TEXT("actor_name"), ActorName);
	Data->SetStringField(TEXT("actor_class"), FoundActor->GetClass()->GetName());

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleFindActorsByName(const TSharedPtr<FJsonObject>& Params)
{
	FString NamePattern = Params->GetStringField(TEXT("name_pattern"));
	FString ActorClass = Params->HasField(TEXT("actor_class")) ? Params->GetStringField(TEXT("actor_class")) : TEXT("");

	if (NamePattern.IsEmpty())
	{
		return MakeError(TEXT("name_pattern parameter is required"));
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return MakeError(TEXT("No world available"));
	}

	// Convert wildcard pattern to regex-like matching
	// * = any characters, ? = single character
	FString Pattern = NamePattern;
	Pattern.ReplaceInline(TEXT("*"), TEXT(".*"));
	Pattern.ReplaceInline(TEXT("?"), TEXT("."));

	TArray<TSharedPtr<FJsonValue>> MatchedActors;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor) continue;

		// Filter by class if specified
		if (!ActorClass.IsEmpty() && Actor->GetClass()->GetName() != ActorClass)
		{
			continue;
		}

		// Check if name matches pattern (simple wildcard matching)
		FString ActorName = Actor->GetName();
		bool bMatches = false;

		if (NamePattern.Contains(TEXT("*")) || NamePattern.Contains(TEXT("?")))
		{
			// Simple wildcard matching - check if pattern matches
			FString Remaining = ActorName;
			TArray<FString> Parts;
			NamePattern.ParseIntoArray(Parts, TEXT("*"), true);

			if (Parts.Num() == 0)
			{
				bMatches = true; // Pattern is just "*"
			}
			else
			{
				bMatches = true;
				int32 SearchStart = 0;

				for (int32 i = 0; i < Parts.Num(); i++)
				{
					if (Parts[i].IsEmpty()) continue;

					int32 FoundIndex = Remaining.Find(Parts[i], ESearchCase::IgnoreCase, ESearchDir::FromStart, SearchStart);
					if (FoundIndex == INDEX_NONE || (i == 0 && !NamePattern.StartsWith(TEXT("*")) && FoundIndex != 0))
					{
						bMatches = false;
						break;
					}
					SearchStart = FoundIndex + Parts[i].Len();
				}

				// Check end match
				if (bMatches && !NamePattern.EndsWith(TEXT("*")) && Parts.Num() > 0)
				{
					if (!Remaining.EndsWith(Parts.Last()))
					{
						bMatches = false;
					}
				}
			}
		}
		else
		{
			// Exact match or contains
			bMatches = ActorName.Contains(NamePattern);
		}

		if (bMatches)
		{
			TSharedPtr<FJsonObject> ActorObj = MakeShared<FJsonObject>();
			ActorObj->SetStringField(TEXT("name"), Actor->GetName());
			ActorObj->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
			ActorObj->SetStringField(TEXT("label"), Actor->GetActorLabel());

			FVector Location = Actor->GetActorLocation();
			ActorObj->SetStringField(TEXT("location"), FString::Printf(TEXT("%.1f, %.1f, %.1f"), Location.X, Location.Y, Location.Z));

			MatchedActors.Add(MakeShared<FJsonValueObject>(ActorObj));
		}
	}

	TSharedPtr<FJsonObject> ResponseData = MakeShared<FJsonObject>();
	ResponseData->SetArrayField(TEXT("actors"), MatchedActors);
	ResponseData->SetNumberField(TEXT("count"), MatchedActors.Num());
	ResponseData->SetStringField(TEXT("pattern"), NamePattern);

	return MakeResponse(true, ResponseData);
}

FString FMCPServer::HandleGetActorMaterialInfo(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName = Params->GetStringField(TEXT("actor_name"));

	if (ActorName.IsEmpty())
	{
		return MakeError(TEXT("actor_name parameter is required"));
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return MakeError(TEXT("No world available"));
	}

	// Find the actor
	AActor* FoundActor = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->GetName() == ActorName)
		{
			FoundActor = *It;
			break;
		}
	}

	if (!FoundActor)
	{
		return MakeError(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
	}

	// Collect material information from all components
	TArray<TSharedPtr<FJsonValue>> MaterialsArray;

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	FoundActor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);

	for (UPrimitiveComponent* PrimComp : PrimitiveComponents)
	{
		if (!PrimComp) continue;

		int32 NumMaterials = PrimComp->GetNumMaterials();
		for (int32 i = 0; i < NumMaterials; i++)
		{
			UMaterialInterface* Material = PrimComp->GetMaterial(i);
			if (!Material) continue;

			TSharedPtr<FJsonObject> MaterialObj = MakeShared<FJsonObject>();
			MaterialObj->SetStringField(TEXT("component"), PrimComp->GetName());
			MaterialObj->SetNumberField(TEXT("slot"), i);
			MaterialObj->SetStringField(TEXT("material_name"), Material->GetName());
			MaterialObj->SetStringField(TEXT("material_path"), Material->GetPathName());
			MaterialObj->SetStringField(TEXT("material_class"), Material->GetClass()->GetName());

			// Check if it's a dynamic material instance
			if (UMaterialInstanceDynamic* DynMaterial = Cast<UMaterialInstanceDynamic>(Material))
			{
				MaterialObj->SetBoolField(TEXT("is_dynamic"), true);
				if (DynMaterial->Parent)
				{
					MaterialObj->SetStringField(TEXT("parent_material"), DynMaterial->Parent->GetPathName());
				}
			}
			else
			{
				MaterialObj->SetBoolField(TEXT("is_dynamic"), false);
			}

			MaterialsArray.Add(MakeShared<FJsonValueObject>(MaterialObj));
		}
	}

	TSharedPtr<FJsonObject> ResponseData = MakeShared<FJsonObject>();
	ResponseData->SetStringField(TEXT("actor_name"), ActorName);
	ResponseData->SetStringField(TEXT("actor_class"), FoundActor->GetClass()->GetName());
	ResponseData->SetArrayField(TEXT("materials"), MaterialsArray);
	ResponseData->SetNumberField(TEXT("material_count"), MaterialsArray.Num());

	return MakeResponse(true, ResponseData);
}

FString FMCPServer::HandleGetSceneSummary(const TSharedPtr<FJsonObject>& Params)
{
	bool bIncludeDetails = Params->HasField(TEXT("include_details")) ? Params->GetBoolField(TEXT("include_details")) : true;

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return MakeError(TEXT("No world available"));
	}

	// Count actors by class
	TMap<FString, int32> ActorCountByClass;
	int32 TotalActors = 0;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor) continue;

		FString ClassName = Actor->GetClass()->GetName();
		ActorCountByClass.FindOrAdd(ClassName)++;
		TotalActors++;
	}

	// Build response
	TSharedPtr<FJsonObject> ResponseData = MakeShared<FJsonObject>();
	ResponseData->SetStringField(TEXT("level_name"), World->GetName());
	ResponseData->SetNumberField(TEXT("total_actors"), TotalActors);
	ResponseData->SetNumberField(TEXT("unique_actor_classes"), ActorCountByClass.Num());

	if (bIncludeDetails)
	{
		TSharedPtr<FJsonObject> ClassBreakdown = MakeShared<FJsonObject>();

		// Sort by count (highest first)
		TArray<TPair<FString, int32>> SortedCounts;
		for (const auto& Pair : ActorCountByClass)
		{
			SortedCounts.Add(Pair);
		}
		SortedCounts.Sort([](const TPair<FString, int32>& A, const TPair<FString, int32>& B) {
			return A.Value > B.Value;
		});

		for (const auto& Pair : SortedCounts)
		{
			ClassBreakdown->SetNumberField(Pair.Key, Pair.Value);
		}

		ResponseData->SetObjectField(TEXT("actor_classes"), ClassBreakdown);
	}

	return MakeResponse(true, ResponseData);
}

// ==================================================
// Sprint 5: Blueprint Node Manipulation Commands
// ==================================================
