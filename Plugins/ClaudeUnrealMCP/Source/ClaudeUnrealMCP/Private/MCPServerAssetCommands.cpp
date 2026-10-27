#include "MCPServer.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"

FString FMCPServer::HandleSearchAssets(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString Query;
	if (!Params->TryGetStringField(TEXT("query"), Query))
		return MakeError(TEXT("query required (asset name pattern)"));

	FString ClassFilter;
	Params->TryGetStringField(TEXT("class_filter"), ClassFilter);

	FString PathFilter = TEXT("/Game");
	Params->TryGetStringField(TEXT("path_filter"), PathFilter);

	int32 MaxResults = 50;
	Params->TryGetNumberField(TEXT("max_results"), MaxResults);

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	FARFilter Filter;
	Filter.bRecursivePaths = true;
	Filter.PackagePaths.Add(FName(*PathFilter));
	if (!ClassFilter.IsEmpty())
	{
		Filter.ClassPaths.Add(FTopLevelAssetPath(*ClassFilter));
	}

	TArray<FAssetData> AssetList;
	AssetRegistry.GetAssets(Filter, AssetList);

	TArray<TSharedPtr<FJsonValue>> Results;
	int32 Count = 0;
	for (const FAssetData& Asset : AssetList)
	{
		if (Count >= MaxResults) break;

		FString AssetName = Asset.AssetName.ToString();
		if (AssetName.Contains(Query, ESearchCase::IgnoreCase) || Query == TEXT("*"))
		{
			TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
			Item->SetStringField(TEXT("name"), AssetName);
			Item->SetStringField(TEXT("path"), Asset.GetObjectPathString());
			Item->SetStringField(TEXT("class"), Asset.AssetClassPath.GetAssetName().ToString());
			Item->SetStringField(TEXT("package"), Asset.PackageName.ToString());
			Results.Add(MakeShared<FJsonValueObject>(Item));
			Count++;
		}
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("results"), Results);
	Data->SetNumberField(TEXT("count"), Results.Num());
	return MakeResponse(true, Data);
}

FString FMCPServer::HandleRenameAsset(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString SourcePath, DestPath;
	if (!Params->TryGetStringField(TEXT("source_path"), SourcePath))
		return MakeError(TEXT("source_path required"));
	if (!Params->TryGetStringField(TEXT("dest_path"), DestPath))
		return MakeError(TEXT("dest_path required (full new path including name)"));

	if (!UEditorAssetLibrary::DoesAssetExist(SourcePath))
		return MakeError(FString::Printf(TEXT("Asset not found: %s"), *SourcePath));

	const bool bSuccess = UEditorAssetLibrary::RenameAsset(SourcePath, DestPath);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("source"), SourcePath);
	Data->SetStringField(TEXT("dest"), DestPath);
	Data->SetBoolField(TEXT("renamed"), bSuccess);
	return MakeResponse(bSuccess, Data, bSuccess ? TEXT("") : TEXT("Rename failed — check for references or name conflicts"));
}

FString FMCPServer::HandleDeleteAsset(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("path"), AssetPath))
		return MakeError(TEXT("path required"));

	if (!UEditorAssetLibrary::DoesAssetExist(AssetPath))
		return MakeError(FString::Printf(TEXT("Asset not found: %s"), *AssetPath));

	// Check references first
	bool bCheckRefs = true;
	Params->TryGetBoolField(TEXT("check_references"), bCheckRefs);

	if (bCheckRefs)
	{
		TArray<FName> Referencers;
		IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		AssetRegistry.GetReferencers(FName(*AssetPath), Referencers);
		if (Referencers.Num() > 0)
		{
			TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
			Data->SetBoolField(TEXT("deleted"), false);
			Data->SetNumberField(TEXT("referencer_count"), Referencers.Num());
			TArray<TSharedPtr<FJsonValue>> Refs;
			for (const FName& Ref : Referencers)
			{
				Refs.Add(MakeShared<FJsonValueString>(Ref.ToString()));
			}
			Data->SetArrayField(TEXT("referencers"), Refs);
			return MakeResponse(false, Data, TEXT("Asset has references — set check_references=false to force delete"));
		}
	}

	const bool bDeleted = UEditorAssetLibrary::DeleteAsset(AssetPath);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("path"), AssetPath);
	Data->SetBoolField(TEXT("deleted"), bDeleted);
	return MakeResponse(bDeleted, Data, bDeleted ? TEXT("") : TEXT("Delete failed"));
}
