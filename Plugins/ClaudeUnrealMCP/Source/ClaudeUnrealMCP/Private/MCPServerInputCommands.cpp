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

FString FMCPServer::HandleAddInputMapping(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid())
	{
		return MakeError(TEXT("Missing parameters"));
	}

	FString ContextPath = Params->GetStringField(TEXT("context_path"));
	FString ActionPath = Params->GetStringField(TEXT("action_path"));
	FString KeyName = Params->GetStringField(TEXT("key"));

	if (ContextPath.IsEmpty() || ActionPath.IsEmpty() || KeyName.IsEmpty())
	{
		return MakeError(TEXT("Missing context_path, action_path, or key"));
	}

	// Load the input mapping context
	UInputMappingContext* Context = LoadObject<UInputMappingContext>(nullptr, *ContextPath);
	if (!Context)
	{
		return MakeError(FString::Printf(TEXT("Input mapping context not found: %s"), *ContextPath));
	}

	// Load the input action
	UInputAction* Action = LoadObject<UInputAction>(nullptr, *ActionPath);
	if (!Action)
	{
		return MakeError(FString::Printf(TEXT("Input action not found: %s"), *ActionPath));
	}

	// Parse the key
	FKey Key(*KeyName);
	if (!Key.IsValid())
	{
		return MakeError(FString::Printf(TEXT("Invalid key: %s"), *KeyName));
	}

	// Add the mapping
	FEnhancedActionKeyMapping& Mapping = Context->MapKey(Action, Key);

	// Mark as modified
	Context->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Mapped %s to %s in %s"), *KeyName, *Action->GetName(), *Context->GetName()));

	return MakeResponse(true, Data);
}


FString FMCPServer::HandleRemoveInputMapping(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid())
	{
		return MakeError(TEXT("Missing parameters"));
	}

	FString ContextPath = Params->GetStringField(TEXT("context_path"));
	FString ActionPath = Params->GetStringField(TEXT("action_path"));
	FString KeyName = Params->GetStringField(TEXT("key"));

	if (ContextPath.IsEmpty() || ActionPath.IsEmpty())
	{
		return MakeError(TEXT("Missing context_path or action_path"));
	}

	UInputMappingContext* Context = LoadObject<UInputMappingContext>(nullptr, *ContextPath);
	if (!Context)
	{
		return MakeError(FString::Printf(TEXT("Input mapping context not found: %s"), *ContextPath));
	}

	UInputAction* Action = LoadObject<UInputAction>(nullptr, *ActionPath);
	if (!Action)
	{
		return MakeError(FString::Printf(TEXT("Input action not found: %s"), *ActionPath));
	}

	// If a key is given, unmap just that one key from the action; otherwise
	// unmap every key currently mapped to this action in this context.
	if (!KeyName.IsEmpty())
	{
		FKey Key(*KeyName);
		if (!Key.IsValid())
		{
			return MakeError(FString::Printf(TEXT("Invalid key: %s"), *KeyName));
		}
		Context->UnmapKey(Action, Key);
	}
	else
	{
		Context->UnmapAllKeysFromAction(Action);
	}

	Context->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Unmapped %s from %s in %s"),
		KeyName.IsEmpty() ? TEXT("all keys") : *KeyName, *Action->GetName(), *Context->GetName()));
	return MakeResponse(true, Data);
}

FString FMCPServer::HandleReadInputMappingContext(const TSharedPtr<FJsonObject>& Params)
{
	FString Path = Params->GetStringField(TEXT("path"));
	if (Path.IsEmpty())
	{
		return MakeError(TEXT("Missing 'path' parameter"));
	}

	// Normalize the path
	FString FullPath = Path;
	if (!FullPath.StartsWith(TEXT("/")))
	{
		FullPath = TEXT("/Game/") + Path;
	}

	// Load the Input Mapping Context
	UInputMappingContext* IMC = LoadObject<UInputMappingContext>(nullptr, *FullPath);
	if (!IMC)
	{
		// Try with .IMC_Sandbox suffix
		FString AssetName = FPaths::GetBaseFilename(FullPath);
		FString TryPath = FullPath + TEXT(".") + AssetName;
		IMC = LoadObject<UInputMappingContext>(nullptr, *TryPath);
	}

	if (!IMC)
	{
		return MakeError(FString::Printf(TEXT("Input Mapping Context not found: %s"), *Path));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("name"), IMC->GetName());
	Data->SetStringField(TEXT("path"), IMC->GetPathName());

	// Get the mappings
	TArray<TSharedPtr<FJsonValue>> MappingsArray;
	const TArray<FEnhancedActionKeyMapping>& Mappings = IMC->GetMappings();

	for (const FEnhancedActionKeyMapping& Mapping : Mappings)
	{
		TSharedPtr<FJsonObject> MappingObj = MakeShared<FJsonObject>();

		// Input Action info
		if (Mapping.Action)
		{
			MappingObj->SetStringField(TEXT("action_name"), Mapping.Action->GetName());
			MappingObj->SetStringField(TEXT("action_path"), Mapping.Action->GetPathName());
		}
		else
		{
			MappingObj->SetStringField(TEXT("action_name"), TEXT("(None)"));
		}

		// Key info
		MappingObj->SetStringField(TEXT("key"), Mapping.Key.GetFName().ToString());
		MappingObj->SetStringField(TEXT("key_display"), Mapping.Key.GetDisplayName().ToString());

		// Modifiers
		TArray<TSharedPtr<FJsonValue>> ModifiersArray;
		for (UInputModifier* Modifier : Mapping.Modifiers)
		{
			if (Modifier)
			{
				TSharedPtr<FJsonObject> ModObj = MakeShared<FJsonObject>();
				ModObj->SetStringField(TEXT("class"), Modifier->GetClass()->GetName());
				ModifiersArray.Add(MakeShared<FJsonValueObject>(ModObj));
			}
		}
		MappingObj->SetArrayField(TEXT("modifiers"), ModifiersArray);

		// Triggers
		TArray<TSharedPtr<FJsonValue>> TriggersArray;
		for (UInputTrigger* Trigger : Mapping.Triggers)
		{
			if (Trigger)
			{
				TSharedPtr<FJsonObject> TrigObj = MakeShared<FJsonObject>();
				TrigObj->SetStringField(TEXT("class"), Trigger->GetClass()->GetName());
				TriggersArray.Add(MakeShared<FJsonValueObject>(TrigObj));
			}
		}
		MappingObj->SetArrayField(TEXT("triggers"), TriggersArray);

		MappingsArray.Add(MakeShared<FJsonValueObject>(MappingObj));
	}

	Data->SetArrayField(TEXT("mappings"), MappingsArray);

	return MakeResponse(true, Data);
}

