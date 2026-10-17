// Sprint 20 — Data Tables
#include "MCPServer.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Engine/DataTable.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Factories/DataTableFactory.h"
#include "EditorAssetLibrary.h"

FString FMCPServer::HandleCreateDataTable(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString AssetPath, AssetName, RowStructPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
		return MakeError(TEXT("asset_path required"));
	if (!Params->TryGetStringField(TEXT("asset_name"), AssetName))
		return MakeError(TEXT("asset_name required"));
	if (!Params->TryGetStringField(TEXT("row_struct"), RowStructPath))
		return MakeError(TEXT("row_struct required (e.g. '/Script/Engine.DataTableRowHandle' or struct path)"));

	// Find the row struct
	UScriptStruct* RowStruct = FindObject<UScriptStruct>(nullptr, *RowStructPath);
	if (!RowStruct)
	{
		RowStruct = LoadObject<UScriptStruct>(nullptr, *RowStructPath);
	}
	if (!RowStruct)
	{
		// Try common engine structs
		for (TObjectIterator<UScriptStruct> It; It; ++It)
		{
			if (It->GetName() == RowStructPath || It->GetPathName() == RowStructPath)
			{
				RowStruct = *It;
				break;
			}
		}
	}
	if (!RowStruct)
		return MakeError(FString::Printf(TEXT("Row struct not found: %s"), *RowStructPath));

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UDataTableFactory* Factory = NewObject<UDataTableFactory>();
	Factory->Struct = RowStruct;

	UObject* NewAsset = AssetTools.CreateAsset(AssetName, AssetPath, UDataTable::StaticClass(), Factory);
	if (!NewAsset)
		return MakeError(TEXT("Failed to create data table"));

	UEditorAssetLibrary::SaveAsset(NewAsset->GetPathName(), false);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("path"), NewAsset->GetPathName());
	Data->SetStringField(TEXT("row_struct"), RowStruct->GetName());
	return MakeResponse(true, Data);
}

FString FMCPServer::HandleReadDataTable(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString TablePath;
	if (!Params->TryGetStringField(TEXT("path"), TablePath))
		return MakeError(TEXT("path required"));

	UDataTable* DataTable = LoadObject<UDataTable>(nullptr, *TablePath);
	if (!DataTable)
		return MakeError(FString::Printf(TEXT("Data table not found: %s"), *TablePath));

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("path"), TablePath);
	Data->SetStringField(TEXT("row_struct"), DataTable->GetRowStruct() ? DataTable->GetRowStruct()->GetName() : TEXT("None"));

	// List all row names
	TArray<FName> RowNames = DataTable->GetRowNames();
	Data->SetNumberField(TEXT("row_count"), RowNames.Num());

	TArray<TSharedPtr<FJsonValue>> Rows;
	for (const FName& RowName : RowNames)
	{
		TSharedPtr<FJsonObject> RowObj = MakeShared<FJsonObject>();
		RowObj->SetStringField(TEXT("name"), RowName.ToString());

		// Export row data as text
		uint8* RowData = DataTable->FindRowUnchecked(RowName);
		if (RowData && DataTable->GetRowStruct())
		{
			FString RowText;
			DataTable->GetRowStruct()->ExportText(RowText, RowData, RowData, nullptr, PPF_None, nullptr);
			RowObj->SetStringField(TEXT("data"), RowText);
		}

		Rows.Add(MakeShared<FJsonValueObject>(RowObj));
	}
	Data->SetArrayField(TEXT("rows"), Rows);

	// List columns (struct properties)
	if (DataTable->GetRowStruct())
	{
		TArray<TSharedPtr<FJsonValue>> Columns;
		for (TFieldIterator<FProperty> PropIt(DataTable->GetRowStruct()); PropIt; ++PropIt)
		{
			TSharedPtr<FJsonObject> ColObj = MakeShared<FJsonObject>();
			ColObj->SetStringField(TEXT("name"), PropIt->GetName());
			ColObj->SetStringField(TEXT("type"), PropIt->GetCPPType());
			Columns.Add(MakeShared<FJsonValueObject>(ColObj));
		}
		Data->SetArrayField(TEXT("columns"), Columns);
	}

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleAddDataTableRow(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString TablePath, RowName, RowData;
	if (!Params->TryGetStringField(TEXT("path"), TablePath))
		return MakeError(TEXT("path required"));
	if (!Params->TryGetStringField(TEXT("row_name"), RowName))
		return MakeError(TEXT("row_name required"));
	if (!Params->TryGetStringField(TEXT("row_data"), RowData))
		return MakeError(TEXT("row_data required (JSON string matching row struct fields)"));

	UDataTable* DataTable = LoadObject<UDataTable>(nullptr, *TablePath);
	if (!DataTable)
		return MakeError(FString::Printf(TEXT("Data table not found: %s"), *TablePath));

	if (!DataTable->GetRowStruct())
		return MakeError(TEXT("Data table has no row struct"));

	// Add or update row from JSON
	// DataTable expects CSV-like text import format: "RowName,Field1,Field2,..."
	// But we'll use the struct text import format
	FString Error;

	// Create a temporary row data buffer
	const UScriptStruct* RowStruct = DataTable->GetRowStruct();
	uint8* TempRowData = (uint8*)FMemory::Malloc(RowStruct->GetStructureSize());
	RowStruct->InitializeStruct(TempRowData);

	// Import from text
	const TCHAR* ImportResult = RowStruct->ImportText(*RowData, TempRowData, nullptr, PPF_None, GLog, RowStruct->GetName());

	if (!ImportResult)
	{
		RowStruct->DestroyStruct(TempRowData);
		FMemory::Free(TempRowData);
		return MakeError(FString::Printf(TEXT("Failed to parse row_data. Expected format matching struct %s"), *RowStruct->GetName()));
	}

	// Add/update the row
	DataTable->Modify();
	DataTable->AddRow(FName(*RowName), *reinterpret_cast<FTableRowBase*>(TempRowData));

	RowStruct->DestroyStruct(TempRowData);
	FMemory::Free(TempRowData);

	UEditorAssetLibrary::SaveAsset(TablePath, false);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("row_name"), RowName);
	Data->SetNumberField(TEXT("total_rows"), DataTable->GetRowNames().Num());
	return MakeResponse(true, Data);
}
