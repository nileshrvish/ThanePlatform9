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
#include "Editor.h"
#include "TimerManager.h"
#include "HAL/PlatformMisc.h"
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

FString FMCPServer::HandleReparentBlueprint(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("blueprint_path")) || !Params->HasField(TEXT("parent_class")))
	{
		return MakeError(TEXT("Missing 'blueprint_path' or 'parent_class' parameter"));
	}

	const FString BlueprintPath = Params->GetStringField(TEXT("blueprint_path"));
	const FString ParentClassPath = Params->GetStringField(TEXT("parent_class"));

	UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
	}

	UClass* NewParent = ResolveParentClass(ParentClassPath);
	if (!NewParent)
	{
		return MakeError(FString::Printf(TEXT("Parent class not found: %s"), *ParentClassPath));
	}

	UClass* OldParent = Blueprint->ParentClass;
	if (OldParent == NewParent)
	{
		TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("message"), TEXT("Blueprint already uses specified parent class"));
		Data->SetStringField(TEXT("parent_class"), NewParent->GetName());
		return MakeResponse(true, Data);
	}

	UBlueprintEditorLibrary::ReparentBlueprint(Blueprint, NewParent);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("Blueprint reparented successfully"));
	Data->SetStringField(TEXT("old_parent"), OldParent ? OldParent->GetName() : TEXT("None"));
	Data->SetStringField(TEXT("new_parent"), NewParent->GetName());

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleCompileBlueprint(const TSharedPtr<FJsonObject>& Params)
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

	// Compile the blueprint
	FCompilerResultsLog CompileLog;
	FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None, &CompileLog);

	bool bSuccess = (Blueprint->Status == BS_UpToDate || Blueprint->Status == BS_UpToDateWithWarnings);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("compiled"), bSuccess);

	// Get blueprint status string
	FString StatusString;
	switch (Blueprint->Status)
	{
	case BS_Unknown: StatusString = TEXT("Unknown"); break;
	case BS_Dirty: StatusString = TEXT("Dirty"); break;
	case BS_Error: StatusString = TEXT("Error"); break;
	case BS_UpToDate: StatusString = TEXT("UpToDate"); break;
	case BS_BeingCreated: StatusString = TEXT("BeingCreated"); break;
	case BS_UpToDateWithWarnings: StatusString = TEXT("UpToDateWithWarnings"); break;
	default: StatusString = TEXT("Unknown");
	}
	Data->SetStringField(TEXT("status"), StatusString);

	// Capture compilation errors and warnings
	TArray<TSharedPtr<FJsonValue>> ErrorsArray;
	TArray<TSharedPtr<FJsonValue>> WarningsArray;

	for (const TSharedRef<FTokenizedMessage>& Message : CompileLog.Messages)
	{
		TSharedPtr<FJsonObject> MsgObj = MakeShared<FJsonObject>();
		MsgObj->SetStringField(TEXT("message"), Message->ToText().ToString());
		MsgObj->SetStringField(TEXT("severity"), FString::FromInt((int32)Message->GetSeverity()));

		if (Message->GetSeverity() == EMessageSeverity::Error)
		{
			ErrorsArray.Add(MakeShared<FJsonValueObject>(MsgObj));
		}
		else if (Message->GetSeverity() == EMessageSeverity::Warning)
		{
			WarningsArray.Add(MakeShared<FJsonValueObject>(MsgObj));
		}
	}

	// If Blueprint has Error status but CompileLog captured no messages,
	// add a synthetic error so callers know something is wrong
	if (!bSuccess && ErrorsArray.Num() == 0)
	{
		TSharedPtr<FJsonObject> MsgObj = MakeShared<FJsonObject>();
		MsgObj->SetStringField(TEXT("message"), TEXT("Blueprint has Error status after compilation (errors may have occurred during asset loading/validation)"));
		MsgObj->SetStringField(TEXT("severity"), FString::FromInt((int32)EMessageSeverity::Error));
		ErrorsArray.Add(MakeShared<FJsonValueObject>(MsgObj));
	}

	Data->SetArrayField(TEXT("errors"), ErrorsArray);
	Data->SetArrayField(TEXT("warnings"), WarningsArray);
	Data->SetNumberField(TEXT("error_count"), ErrorsArray.Num());
	Data->SetNumberField(TEXT("warning_count"), WarningsArray.Num());

	// Return success=false when compilation actually failed, so callers
	// who only check 'success' will catch it
	if (!bSuccess)
	{
		FString ErrorMsg = FString::Printf(TEXT("Compilation failed (status: %s, %d errors)"), *StatusString, ErrorsArray.Num());
		return MakeResponse(false, Data, ErrorMsg);
	}

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleSaveAsset(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("path")))
	{
		return MakeError(TEXT("Missing 'path' parameter"));
	}

	FString Path = Params->GetStringField(TEXT("path"));

	// Try to load as blueprint first
	UObject* Asset = LoadObject<UBlueprint>(nullptr, *Path);
	if (!Asset)
	{
		// Try as generic object
		Asset = LoadObject<UObject>(nullptr, *Path);
	}

	if (!Asset)
	{
		return MakeError(FString::Printf(TEXT("Asset not found: %s"), *Path));
	}

	UPackage* Package = Asset->GetOutermost();
	if (!Package)
	{
		return MakeError(TEXT("Could not get package"));
	}

	// A level/world package must be saved as .umap, not .uasset - using the
	// asset extension unconditionally here silently wrote every level save
	// to a stray, wrongly-named .uasset file sitting next to the real .umap,
	// which the editor never actually loads on next launch. This was the
	// real cause of a long-standing "level-placed actors revert after
	// relaunch" bug misdiagnosed twice before (as a stale autosave, then as
	// an async SavePackage flush race) - neither was it; save_asset was
	// just writing to the wrong file the entire time.
	const FString Extension = Package->ContainsMap() ? FPackageName::GetMapPackageExtension() : FPackageName::GetAssetPackageExtension();
	FString PackageFileName = FPackageName::LongPackageNameToFilename(Package->GetName(), Extension);

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Standalone;

	bool bSaved = UPackage::SavePackage(Package, Asset, *PackageFileName, SaveArgs);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("saved"), bSaved);
	Data->SetStringField(TEXT("message"), bSaved ? TEXT("Asset saved successfully") : TEXT("Failed to save asset"));

	if (!bSaved)
	{
		return MakeError(TEXT("Failed to save asset"));
	}

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleSaveAll(const TSharedPtr<FJsonObject>& Params)
{
	// Save all dirty packages
	TArray<UPackage*> DirtyPackages;

	// Get all dirty packages
	for (TObjectIterator<UPackage> It; It; ++It)
	{
		UPackage* Package = *It;
		if (Package && Package->IsDirty() && !Package->HasAnyFlags(RF_Transient))
		{
			DirtyPackages.Add(Package);
		}
	}

	int32 SavedCount = 0;
	int32 FailedCount = 0;
	TArray<FString> FailedPackages;

	for (UPackage* Package : DirtyPackages)
	{
		FString PackageName = Package->GetName();

		// Skip packages that don't have a valid path (e.g., temp packages)
		if (!FPackageName::IsValidLongPackageName(PackageName))
		{
			continue;
		}

		// Same map-vs-asset extension fix as HandleSaveAsset above.
		const FString Extension = Package->ContainsMap() ? FPackageName::GetMapPackageExtension() : FPackageName::GetAssetPackageExtension();
		FString PackageFileName = FPackageName::LongPackageNameToFilename(PackageName, Extension);

		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Standalone;

		if (UPackage::SavePackage(Package, nullptr, *PackageFileName, SaveArgs))
		{
			SavedCount++;
		}
		else
		{
			FailedCount++;
			FailedPackages.Add(PackageName);
		}
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetNumberField(TEXT("saved_count"), SavedCount);
	Data->SetNumberField(TEXT("failed_count"), FailedCount);
	Data->SetNumberField(TEXT("total_dirty"), DirtyPackages.Num());

	if (FailedPackages.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> FailedArray;
		for (const FString& FailedPkg : FailedPackages)
		{
			FailedArray.Add(MakeShared<FJsonValueString>(FailedPkg));
		}
		Data->SetArrayField(TEXT("failed_packages"), FailedArray);
	}

	FString Message = FString::Printf(TEXT("Saved %d package(s), %d failed"), SavedCount, FailedCount);
	Data->SetStringField(TEXT("message"), Message);

	if (FailedCount > 0)
	{
		return MakeResponse(false, Data, Message);
	}

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleSaveAllAndClose(const TSharedPtr<FJsonObject>& Params)
{
	// Reuse HandleSaveAll's own logic (including the map-vs-asset extension fix noted above)
	// rather than duplicating the dirty-package walk here.
	const FString SaveResultJson = HandleSaveAll(Params);

	// Defer the actual exit to the next tick so this response has a chance to reach the client
	// over the socket before the process goes away - requesting exit synchronously here could
	// tear the process down before the send completes.
	if (GEditor)
	{
		GEditor->GetTimerManager()->SetTimerForNextTick([]()
		{
			FGenericPlatformMisc::RequestExit(false);
		});
	}
	else
	{
		FGenericPlatformMisc::RequestExit(false);
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("save_result"), SaveResultJson);
	Data->SetBoolField(TEXT("closing"), true);
	Data->SetStringField(TEXT("message"), TEXT("Saved all dirty packages; editor is now closing"));
	return MakeResponse(true, Data);
}

FString FMCPServer::HandleSetBlueprintCompileSettings(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("blueprint_path")))
	{
		return MakeError(TEXT("Missing 'blueprint_path' parameter"));
	}

	const FString BlueprintPath = Params->GetStringField(TEXT("blueprint_path"));

	UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
	}

	bool bModified = false;

	// Handle bRunConstructionScriptOnDrag setting
	if (Params->HasField(TEXT("run_construction_script_on_drag")))
	{
		bool bValue = Params->GetBoolField(TEXT("run_construction_script_on_drag"));
		Blueprint->bRunConstructionScriptOnDrag = bValue;
		bModified = true;
	}

	// Handle bGenerateConstClass setting
	if (Params->HasField(TEXT("generate_const_class")))
	{
		bool bValue = Params->GetBoolField(TEXT("generate_const_class"));
		Blueprint->bGenerateConstClass = bValue;
		bModified = true;
	}

	// Handle bForceFullEditor setting
	if (Params->HasField(TEXT("force_full_editor")))
	{
		bool bValue = Params->GetBoolField(TEXT("force_full_editor"));
		Blueprint->bForceFullEditor = bValue;
		bModified = true;
	}

	if (!bModified)
	{
		return MakeError(TEXT("No valid settings provided. Available settings: run_construction_script_on_drag, generate_const_class, force_full_editor"));
	}

	// Mark the blueprint as modified
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("Blueprint compile settings updated successfully"));

	return MakeResponse(true, Data);
}
