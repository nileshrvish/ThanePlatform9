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

FString FMCPServer::HandleListBlueprints(const TSharedPtr<FJsonObject>& Params)
{
	FString PathFilter = TEXT("/Game/");
	if (Params.IsValid() && Params->HasField(TEXT("path")))
	{
		PathFilter = Params->GetStringField(TEXT("path"));
	}

	FAssetRegistryModule& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

	TArray<FAssetData> Assets;
	AssetRegistry.Get().GetAssetsByClass(UBlueprint::StaticClass()->GetClassPathName(), Assets);

	// Also include Animation Blueprints
	TArray<FAssetData> AnimAssets;
	AssetRegistry.Get().GetAssetsByClass(UAnimBlueprint::StaticClass()->GetClassPathName(), AnimAssets);
	Assets.Append(AnimAssets);

	TArray<TSharedPtr<FJsonValue>> BlueprintArray;
	for (const FAssetData& Asset : Assets)
	{
		FString PackagePath = Asset.PackagePath.ToString();
		if (PackagePath.StartsWith(PathFilter))
		{
			TSharedPtr<FJsonObject> BPObj = MakeShared<FJsonObject>();
			BPObj->SetStringField(TEXT("name"), Asset.AssetName.ToString());
			BPObj->SetStringField(TEXT("path"), Asset.GetObjectPathString());
			BlueprintArray.Add(MakeShared<FJsonValueObject>(BPObj));
		}
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("blueprints"), BlueprintArray);
	Data->SetNumberField(TEXT("count"), BlueprintArray.Num());

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleCheckAllBlueprints(const TSharedPtr<FJsonObject>& Params)
{
	FString PathFilter = TEXT("/Game/");
	if (Params.IsValid() && Params->HasField(TEXT("path")))
	{
		PathFilter = Params->GetStringField(TEXT("path"));
	}

	bool bIncludeWarnings = false;
	if (Params.IsValid() && Params->HasField(TEXT("include_warnings")))
	{
		bIncludeWarnings = Params->GetBoolField(TEXT("include_warnings"));
	}

	FAssetRegistryModule& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

	TArray<FAssetData> Assets;
	AssetRegistry.Get().GetAssetsByClass(UBlueprint::StaticClass()->GetClassPathName(), Assets);

	// Also include Animation Blueprints
	TArray<FAssetData> AnimAssets;
	AssetRegistry.Get().GetAssetsByClass(UAnimBlueprint::StaticClass()->GetClassPathName(), AnimAssets);
	Assets.Append(AnimAssets);

	// Also include Widget Blueprints (UMG widgets, EditorUtilityWidgets)
	TArray<FAssetData> WidgetAssets;
	AssetRegistry.Get().GetAssetsByClass(UWidgetBlueprint::StaticClass()->GetClassPathName(), WidgetAssets);
	Assets.Append(WidgetAssets);

	TArray<TSharedPtr<FJsonValue>> BlueprintsWithErrors;
	int32 TotalChecked = 0;
	int32 TotalErrors = 0;
	int32 TotalWarnings = 0;

	for (const FAssetData& Asset : Assets)
	{
		FString PackagePath = Asset.PackagePath.ToString();
		if (!PackagePath.StartsWith(PathFilter))
		{
			continue;
		}

		FString BlueprintPath = Asset.GetObjectPathString();
		UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);

		if (!Blueprint)
		{
			continue;
		}

		TotalChecked++;

		// Compile the blueprint
		FCompilerResultsLog CompileLog;
		FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None, &CompileLog);

		// Count errors and warnings
		int32 ErrorCount = 0;
		int32 WarningCount = 0;
		TArray<TSharedPtr<FJsonValue>> ErrorsArray;
		TArray<TSharedPtr<FJsonValue>> WarningsArray;

		for (const TSharedRef<FTokenizedMessage>& Message : CompileLog.Messages)
		{
			if (Message->GetSeverity() == EMessageSeverity::Error)
			{
				ErrorCount++;
				TSharedPtr<FJsonObject> MsgObj = MakeShared<FJsonObject>();
				MsgObj->SetStringField(TEXT("message"), Message->ToText().ToString());
				ErrorsArray.Add(MakeShared<FJsonValueObject>(MsgObj));
			}
			else if (Message->GetSeverity() == EMessageSeverity::Warning)
			{
				WarningCount++;
				TSharedPtr<FJsonObject> MsgObj = MakeShared<FJsonObject>();
				MsgObj->SetStringField(TEXT("message"), Message->ToText().ToString());
				WarningsArray.Add(MakeShared<FJsonValueObject>(MsgObj));
			}
		}

		// If Blueprint has Error status but CompileLog captured no messages,
		// add a synthetic error so the BP is still reported
		if (ErrorCount == 0 && Blueprint->Status == BS_Error)
		{
			ErrorCount++;
			TSharedPtr<FJsonObject> MsgObj = MakeShared<FJsonObject>();
			MsgObj->SetStringField(TEXT("message"), TEXT("Blueprint has Error status after compilation (errors may have occurred during asset loading/validation)"));
			ErrorsArray.Add(MakeShared<FJsonValueObject>(MsgObj));
		}

		TotalErrors += ErrorCount;
		TotalWarnings += WarningCount;

		// Only include blueprints with errors (or warnings if requested)
		if (ErrorCount > 0 || (bIncludeWarnings && WarningCount > 0))
		{
			TSharedPtr<FJsonObject> BPObj = MakeShared<FJsonObject>();
			BPObj->SetStringField(TEXT("name"), Asset.AssetName.ToString());
			BPObj->SetStringField(TEXT("path"), BlueprintPath);
			BPObj->SetNumberField(TEXT("error_count"), ErrorCount);
			BPObj->SetNumberField(TEXT("warning_count"), WarningCount);
			BPObj->SetArrayField(TEXT("errors"), ErrorsArray);
			if (bIncludeWarnings)
			{
				BPObj->SetArrayField(TEXT("warnings"), WarningsArray);
			}
			BlueprintsWithErrors.Add(MakeShared<FJsonValueObject>(BPObj));
		}
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetNumberField(TEXT("total_checked"), TotalChecked);
	Data->SetNumberField(TEXT("total_errors"), TotalErrors);
	Data->SetNumberField(TEXT("total_warnings"), TotalWarnings);
	Data->SetNumberField(TEXT("blueprints_with_issues"), BlueprintsWithErrors.Num());
	Data->SetArrayField(TEXT("blueprints"), BlueprintsWithErrors);

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleReadBlueprint(const TSharedPtr<FJsonObject>& Params)
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

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("name"), Blueprint->GetName());
	Data->SetStringField(TEXT("path"), Blueprint->GetPathName());

	if (Blueprint->ParentClass)
	{
		Data->SetStringField(TEXT("parent_class"), Blueprint->ParentClass->GetName());
	}

	// Count variables, components, graphs
	Data->SetNumberField(TEXT("variable_count"), Blueprint->NewVariables.Num());

	int32 ComponentCount = 0;
	if (Blueprint->SimpleConstructionScript)
	{
		ComponentCount = Blueprint->SimpleConstructionScript->GetAllNodes().Num();
	}
	Data->SetNumberField(TEXT("component_count"), ComponentCount);
	Data->SetNumberField(TEXT("graph_count"), Blueprint->UbergraphPages.Num());

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleReadVariables(const TSharedPtr<FJsonObject>& Params)
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

	TArray<TSharedPtr<FJsonValue>> VarArray;
	for (const FBPVariableDescription& Var : Blueprint->NewVariables)
	{
		TSharedPtr<FJsonObject> VarObj = MakeShared<FJsonObject>();
		VarObj->SetStringField(TEXT("name"), Var.VarName.ToString());
		VarObj->SetStringField(TEXT("type"), Var.VarType.PinCategory.ToString());

		if (Var.VarType.PinSubCategoryObject.IsValid())
		{
			VarObj->SetStringField(TEXT("subtype"), Var.VarType.PinSubCategoryObject->GetName());
		}

		VarObj->SetBoolField(TEXT("is_array"), Var.VarType.IsArray());
		VarObj->SetBoolField(TEXT("is_instance_editable"), Var.HasMetaData(TEXT("ExposeOnSpawn")) || (Var.PropertyFlags & CPF_Edit) != 0);
		VarObj->SetBoolField(TEXT("is_blueprint_read_only"), (Var.PropertyFlags & CPF_BlueprintReadOnly) != 0);

		VarArray.Add(MakeShared<FJsonValueObject>(VarObj));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("variables"), VarArray);
	Data->SetNumberField(TEXT("count"), VarArray.Num());

	return MakeResponse(true, Data);
}

/**
 * HandleReadClassDefaults - Read Blueprint Class Default Object (CDO) properties
 *
 * CRITICAL LIMITATION (2026-02-03):
 * This function reads the Blueprint Class Default Object, which contains class-level default values.
 * It does NOT read property overrides set on level actor instances in the Details panel.
 *
 * WHY THIS MATTERS:
 * - When you place a Blueprint actor in a level and modify properties in the Details panel,
 *   those overrides are stored in the LEVEL FILE (.umap), not the Blueprint asset (.uasset)
 * - Blueprint CDO values are often "first iteration" or placeholder values
 * - The actual working behavior typically comes from level instance overrides
 * - Using CDO values for C++ conversion can result in incorrect implementations
 *
 * WHEN TO USE EACH COMMAND:
 * - read_class_defaults: Reading Blueprint schema (available properties, types, metadata)
 * - read_actor_properties: Reading actual working values from a level instance
 *
 * EXAMPLE - LevelVisuals FogColor:
 * - CDO value (from this function): Light purple (0.79, 0.75, 1.0) - WRONG for dark mode
 * - Level instance value: Dark gray (0.261, 0.261, 0.302) - CORRECT working value
 *
 * SOLUTION WORKFLOW:
 * When copying reference data from a working project:
 * 1. Copy the LEVEL FILE (.umap), not just Blueprint assets
 * 2. Open the level in Unreal Editor
 * 3. Use read_actor_properties to get actual instance values
 * 4. Use those values in your C++ implementation
 */

FString FMCPServer::HandleReadClassDefaults(const TSharedPtr<FJsonObject>& Params)
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

	// Get the generated class and its CDO
	// NOTE: This is the class-level CDO, not a level instance with property overrides!
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

	TArray<TSharedPtr<FJsonValue>> PropertyArray;

	// Iterate through all properties (including inherited ones)
	for (TFieldIterator<FProperty> PropIt(GeneratedClass); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;

		// Skip properties that are not editable or visible
		if (!Property->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible))
		{
			continue;
		}

		TSharedPtr<FJsonObject> PropObj = MakeShared<FJsonObject>();
		PropObj->SetStringField(TEXT("name"), Property->GetName());
		PropObj->SetStringField(TEXT("type"), Property->GetCPPType());
		PropObj->SetStringField(TEXT("category"), Property->GetMetaData(TEXT("Category")));

		// Get the property value from CDO
		void* PropertyValue = Property->ContainerPtrToValuePtr<void>(CDO);
		FString ValueString;
		Property->ExportTextItem_Direct(ValueString, PropertyValue, nullptr, nullptr, PPF_None);
		PropObj->SetStringField(TEXT("value"), ValueString);

		// Check if it's an object property
		if (FObjectProperty* ObjectProp = CastField<FObjectProperty>(Property))
		{
			UObject* ObjectValue = ObjectProp->GetObjectPropertyValue(PropertyValue);
			if (ObjectValue)
			{
				PropObj->SetStringField(TEXT("object_value"), ObjectValue->GetPathName());
				PropObj->SetStringField(TEXT("object_class"), ObjectValue->GetClass()->GetName());
			}
		}
		// Check if it's an array property
		else if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
		{
			FScriptArrayHelper ArrayHelper(ArrayProp, PropertyValue);
			PropObj->SetNumberField(TEXT("array_size"), ArrayHelper.Num());

			// For object arrays, list the objects
			if (FObjectProperty* InnerObjectProp = CastField<FObjectProperty>(ArrayProp->Inner))
			{
				TArray<TSharedPtr<FJsonValue>> ObjectArray;
				for (int32 i = 0; i < ArrayHelper.Num(); ++i)
				{
					UObject* ArrayObject = InnerObjectProp->GetObjectPropertyValue(ArrayHelper.GetRawPtr(i));
					if (ArrayObject)
					{
						TSharedPtr<FJsonObject> ArrayObjData = MakeShared<FJsonObject>();
						ArrayObjData->SetStringField(TEXT("path"), ArrayObject->GetPathName());
						ArrayObjData->SetStringField(TEXT("class"), ArrayObject->GetClass()->GetName());
						ObjectArray.Add(MakeShared<FJsonValueObject>(ArrayObjData));
					}
				}
				PropObj->SetArrayField(TEXT("array_values"), ObjectArray);
			}
		}

		PropertyArray.Add(MakeShared<FJsonValueObject>(PropObj));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("properties"), PropertyArray);
	Data->SetNumberField(TEXT("count"), PropertyArray.Num());
	Data->SetStringField(TEXT("class_name"), GeneratedClass->GetName());
	Data->SetStringField(TEXT("parent_class"), GeneratedClass->GetSuperClass() ? GeneratedClass->GetSuperClass()->GetName() : TEXT("None"));

	return MakeResponse(true, Data);
}
