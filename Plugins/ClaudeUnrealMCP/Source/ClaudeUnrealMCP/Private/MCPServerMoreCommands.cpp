// Sprints 24-26: State Trees, Audio, Landscape
#include "MCPServer.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "EditorAssetLibrary.h"
#include "ScopedTransaction.h"
#include "EngineUtils.h"

// StateTree
#include "StateTree.h"
#include "StateTreeFactory.h"

// Audio
#include "Sound/SoundCue.h"
#include "Sound/SoundAttenuation.h"
#include "Factories/SoundCueFactoryNew.h"

// Landscape
#include "Landscape.h"
#include "LandscapeProxy.h"
#include "LandscapeInfo.h"

// ===== STATE TREES (Sprint 24) =====

FString FMCPServer::HandleCreateStateTree(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString AssetPath, AssetName;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
		return MakeError(TEXT("asset_path required"));
	if (!Params->TryGetStringField(TEXT("asset_name"), AssetName))
		return MakeError(TEXT("asset_name required"));

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UStateTreeFactory* Factory = NewObject<UStateTreeFactory>();

	UObject* NewAsset = AssetTools.CreateAsset(AssetName, AssetPath, UStateTree::StaticClass(), Factory);
	if (!NewAsset)
		return MakeError(TEXT("Failed to create StateTree"));

	UEditorAssetLibrary::SaveAsset(NewAsset->GetPathName(), false);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("path"), NewAsset->GetPathName());
	Data->SetStringField(TEXT("name"), AssetName);
	return MakeResponse(true, Data);
}

// ===== AUDIO (Sprint 25) =====

FString FMCPServer::HandleCreateSoundCue(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString AssetPath, AssetName;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
		return MakeError(TEXT("asset_path required"));
	if (!Params->TryGetStringField(TEXT("asset_name"), AssetName))
		return MakeError(TEXT("asset_name required"));

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	USoundCueFactoryNew* Factory = NewObject<USoundCueFactoryNew>();

	UObject* NewAsset = AssetTools.CreateAsset(AssetName, AssetPath, USoundCue::StaticClass(), Factory);
	if (!NewAsset)
		return MakeError(TEXT("Failed to create SoundCue"));

	UEditorAssetLibrary::SaveAsset(NewAsset->GetPathName(), false);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("path"), NewAsset->GetPathName());
	Data->SetStringField(TEXT("name"), AssetName);
	return MakeResponse(true, Data);
}

FString FMCPServer::HandleCreateSoundAttenuation(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString AssetPath, AssetName;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
		return MakeError(TEXT("asset_path required"));
	if (!Params->TryGetStringField(TEXT("asset_name"), AssetName))
		return MakeError(TEXT("asset_name required"));

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	UObject* NewAsset = AssetTools.CreateAsset(AssetName, AssetPath, USoundAttenuation::StaticClass(), nullptr);
	if (!NewAsset)
		return MakeError(TEXT("Failed to create SoundAttenuation"));

	UEditorAssetLibrary::SaveAsset(NewAsset->GetPathName(), false);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("path"), NewAsset->GetPathName());
	Data->SetStringField(TEXT("name"), AssetName);
	return MakeResponse(true, Data);
}

// ===== LANDSCAPE (Sprint 26) =====

FString FMCPServer::HandleGetLandscapeInfo(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World) return MakeError(TEXT("No editor world"));

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();

	TArray<TSharedPtr<FJsonValue>> Landscapes;
	for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
	{
		ALandscapeProxy* Landscape = *It;
		TSharedPtr<FJsonObject> LandObj = MakeShared<FJsonObject>();
		LandObj->SetStringField(TEXT("name"), Landscape->GetName());
		LandObj->SetStringField(TEXT("class"), Landscape->GetClass()->GetName());

		FVector Origin, Extent;
		Landscape->GetActorBounds(false, Origin, Extent);
		LandObj->SetStringField(TEXT("origin"), FString::Printf(TEXT("(%.0f, %.0f, %.0f)"), Origin.X, Origin.Y, Origin.Z));
		LandObj->SetStringField(TEXT("extent"), FString::Printf(TEXT("(%.0f, %.0f, %.0f)"), Extent.X, Extent.Y, Extent.Z));

		// Component count
		LandObj->SetNumberField(TEXT("component_count"), Landscape->LandscapeComponents.Num());

		// Material
		UMaterialInterface* LandMat = Landscape->GetLandscapeMaterial();
		LandObj->SetStringField(TEXT("material"), LandMat ? LandMat->GetPathName() : TEXT("None"));

		Landscapes.Add(MakeShared<FJsonValueObject>(LandObj));
	}

	Data->SetArrayField(TEXT("landscapes"), Landscapes);
	Data->SetNumberField(TEXT("count"), Landscapes.Num());
	return MakeResponse(true, Data);
}
