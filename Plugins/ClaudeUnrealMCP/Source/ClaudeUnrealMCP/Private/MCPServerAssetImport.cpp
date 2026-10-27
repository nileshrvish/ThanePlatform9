// Native (non-Interchange) asset import - see the "On AssetTools.import_asset_tasks
// reliably crashing the editor" note in the host project's AGENTS.md. Calling
// AssetTools::ImportAssetTasks from inside execute_console_command's python path
// crashes the editor with "Assertion failed: ++Queue(QueueIndex).RecursionGuard == 1
// [TaskGraph.cpp:705]" for texture imports: UE5's Interchange import pipeline queues
// and waits on its own nested TaskGraph work, and ClaudeUnrealMCP's own command
// handler is already running as a queued TaskGraph task at that point, so Interchange's
// wait can never be satisfied (a genuine engine-level reentrancy bug, not something we
// did wrong). The fix is to bypass Interchange entirely: UTextureFactory's legacy
// FactoryCreateBinary API is the same fully-synchronous, single-threaded import path
// texture import used for the entire history of UE4 and still works today when called
// directly instead of routed through AssetTools/InterchangeManager - it never touches
// the TaskGraph at all, so the recursion can't happen.

#include "MCPServer.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Factories/TextureFactory.h"
#include "Engine/Texture2D.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

FString FMCPServer::HandleImportTexture(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString ImagePath, DestinationPath;
	if (!Params->TryGetStringField(TEXT("image_path"), ImagePath))
		return MakeError(TEXT("image_path required (absolute path to a jpg/png/tga/etc. on disk)"));
	if (!Params->TryGetStringField(TEXT("destination_path"), DestinationPath))
		return MakeError(TEXT("destination_path required (e.g. /Game/ALSHost/Props/MedKit)"));

	if (!FPaths::FileExists(ImagePath))
		return MakeError(FString::Printf(TEXT("File not found: %s"), *ImagePath));

	FString AssetName;
	Params->TryGetStringField(TEXT("asset_name"), AssetName);
	if (AssetName.IsEmpty())
	{
		AssetName = FPaths::GetBaseFilename(ImagePath);
	}

	TArray<uint8> FileData;
	if (!FFileHelper::LoadFileToArray(FileData, *ImagePath))
		return MakeError(FString::Printf(TEXT("Failed to read file: %s"), *ImagePath));

	const FString PackageName = DestinationPath / AssetName;
	UPackage* Package = CreatePackage(*PackageName);
	if (!Package)
		return MakeError(FString::Printf(TEXT("Failed to create package: %s"), *PackageName));
	Package->FullyLoad();

	UTextureFactory* Factory = NewObject<UTextureFactory>();
	Factory->AddToRoot();
	Factory->SuppressImportOverwriteDialog();

	const FString Extension = FPaths::GetExtension(ImagePath);
	const uint8* BufferStart = FileData.GetData();
	const uint8* BufferEnd = BufferStart + FileData.Num();

	UObject* ImportedObject = Factory->FactoryCreateBinary(
		UTexture2D::StaticClass(), Package, FName(*AssetName),
		RF_Public | RF_Standalone | RF_Transactional,
		nullptr, *Extension, BufferStart, BufferEnd, GWarn);

	Factory->RemoveFromRoot();

	if (!ImportedObject)
		return MakeError(FString::Printf(TEXT("Texture import failed for: %s"), *ImagePath));

	FAssetRegistryModule::AssetCreated(ImportedObject);
	Package->MarkPackageDirty();

	const FString FullPackagePath = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	UPackage::SavePackage(Package, ImportedObject, *FullPackagePath, SaveArgs);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("imported_path"), PackageName + TEXT(".") + AssetName);
	return MakeResponse(true, Data);
}
