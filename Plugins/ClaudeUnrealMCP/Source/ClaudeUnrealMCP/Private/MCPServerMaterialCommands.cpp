#include "MCPServer.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Factories/MaterialFactoryNew.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Engine/Texture.h"
#include "Components/PrimitiveComponent.h"
#include "Components/MeshComponent.h"
#include "EditorAssetLibrary.h"
#include "ScopedTransaction.h"
#include "EngineUtils.h"

FString FMCPServer::HandleCreateMaterial(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString AssetPath, AssetName;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
		return MakeError(TEXT("asset_path required (e.g. '/Game/Materials')"));
	if (!Params->TryGetStringField(TEXT("asset_name"), AssetName))
		return MakeError(TEXT("asset_name required"));

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();

	UObject* NewAsset = AssetTools.CreateAsset(AssetName, AssetPath, UMaterial::StaticClass(), Factory);
	if (!NewAsset)
		return MakeError(FString::Printf(TEXT("Failed to create material %s/%s"), *AssetPath, *AssetName));

	UEditorAssetLibrary::SaveAsset(NewAsset->GetPathName(), false);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("path"), NewAsset->GetPathName());
	Data->SetStringField(TEXT("name"), AssetName);
	return MakeResponse(true, Data);
}

FString FMCPServer::HandleCreateMaterialInstance(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString ParentPath, AssetPath, AssetName;
	if (!Params->TryGetStringField(TEXT("parent_path"), ParentPath))
		return MakeError(TEXT("parent_path required (path to parent material)"));
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
		return MakeError(TEXT("asset_path required (folder path)"));
	if (!Params->TryGetStringField(TEXT("asset_name"), AssetName))
		return MakeError(TEXT("asset_name required"));

	// Load parent material
	UMaterialInterface* ParentMaterial = LoadObject<UMaterialInterface>(nullptr, *ParentPath);
	if (!ParentMaterial)
		return MakeError(FString::Printf(TEXT("Parent material not found: %s"), *ParentPath));

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UMaterialInstanceConstantFactoryNew* Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
	Factory->InitialParent = ParentMaterial;

	UObject* NewAsset = AssetTools.CreateAsset(AssetName, AssetPath, UMaterialInstanceConstant::StaticClass(), Factory);
	if (!NewAsset)
		return MakeError(TEXT("Failed to create material instance"));

	UEditorAssetLibrary::SaveAsset(NewAsset->GetPathName(), false);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("path"), NewAsset->GetPathName());
	Data->SetStringField(TEXT("parent"), ParentPath);
	return MakeResponse(true, Data);
}

FString FMCPServer::HandleSetMaterialParameter(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString MaterialPath, ParamName, ParamType;
	if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
		return MakeError(TEXT("material_path required"));
	if (!Params->TryGetStringField(TEXT("param_name"), ParamName))
		return MakeError(TEXT("param_name required"));
	if (!Params->TryGetStringField(TEXT("param_type"), ParamType))
		return MakeError(TEXT("param_type required (scalar, vector, texture)"));

	UMaterialInstanceConstant* MIC = LoadObject<UMaterialInstanceConstant>(nullptr, *MaterialPath);
	if (!MIC)
		return MakeError(FString::Printf(TEXT("Material instance not found: %s"), *MaterialPath));

	FMaterialParameterInfo ParamInfo(*ParamName);

	FScopedTransaction Transaction(FText::FromString(TEXT("MCP: Set Material Parameter")));
	MIC->Modify();

	if (ParamType == TEXT("scalar"))
	{
		double Value = 0.0;
		if (!Params->TryGetNumberField(TEXT("value"), Value))
			return MakeError(TEXT("'value' (number) required for scalar parameter"));
		MIC->SetScalarParameterValueEditorOnly(ParamInfo, static_cast<float>(Value));
	}
	else if (ParamType == TEXT("vector"))
	{
		double R = 0, G = 0, B = 0, A = 1;
		Params->TryGetNumberField(TEXT("r"), R);
		Params->TryGetNumberField(TEXT("g"), G);
		Params->TryGetNumberField(TEXT("b"), B);
		Params->TryGetNumberField(TEXT("a"), A);
		MIC->SetVectorParameterValueEditorOnly(ParamInfo, FLinearColor(R, G, B, A));
	}
	else if (ParamType == TEXT("texture"))
	{
		FString TexturePath;
		if (!Params->TryGetStringField(TEXT("texture_path"), TexturePath))
			return MakeError(TEXT("'texture_path' required for texture parameter"));
		UTexture* Texture = LoadObject<UTexture>(nullptr, *TexturePath);
		if (!Texture)
			return MakeError(FString::Printf(TEXT("Texture not found: %s"), *TexturePath));
		MIC->SetTextureParameterValueEditorOnly(ParamInfo, Texture);
	}
	else
	{
		return MakeError(FString::Printf(TEXT("Unknown param_type: %s (use scalar, vector, or texture)"), *ParamType));
	}

	UEditorAssetLibrary::SaveAsset(MaterialPath, false);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("param_name"), ParamName);
	Data->SetStringField(TEXT("param_type"), ParamType);
	Data->SetBoolField(TEXT("set"), true);
	return MakeResponse(true, Data);
}

FString FMCPServer::HandleListMaterialParameters(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString MaterialPath;
	if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
		return MakeError(TEXT("material_path required"));

	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
	if (!Material)
		return MakeError(FString::Printf(TEXT("Material not found: %s"), *MaterialPath));

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();

	// Scalar parameters
	{
		TArray<FMaterialParameterInfo> ParamInfos;
		TArray<FGuid> ParamGuids;
		Material->GetAllScalarParameterInfo(ParamInfos, ParamGuids);
		TArray<TSharedPtr<FJsonValue>> Arr;
		for (int32 i = 0; i < ParamInfos.Num(); ++i)
		{
			TSharedPtr<FJsonObject> P = MakeShared<FJsonObject>();
			P->SetStringField(TEXT("name"), ParamInfos[i].Name.ToString());
			float Value = 0;
			Material->GetScalarParameterValue(ParamInfos[i], Value);
			P->SetNumberField(TEXT("value"), Value);
			Arr.Add(MakeShared<FJsonValueObject>(P));
		}
		Data->SetArrayField(TEXT("scalar"), Arr);
	}

	// Vector parameters
	{
		TArray<FMaterialParameterInfo> ParamInfos;
		TArray<FGuid> ParamGuids;
		Material->GetAllVectorParameterInfo(ParamInfos, ParamGuids);
		TArray<TSharedPtr<FJsonValue>> Arr;
		for (int32 i = 0; i < ParamInfos.Num(); ++i)
		{
			TSharedPtr<FJsonObject> P = MakeShared<FJsonObject>();
			P->SetStringField(TEXT("name"), ParamInfos[i].Name.ToString());
			FLinearColor Value;
			Material->GetVectorParameterValue(ParamInfos[i], Value);
			P->SetStringField(TEXT("value"), FString::Printf(TEXT("(R=%.3f,G=%.3f,B=%.3f,A=%.3f)"), Value.R, Value.G, Value.B, Value.A));
			Arr.Add(MakeShared<FJsonValueObject>(P));
		}
		Data->SetArrayField(TEXT("vector"), Arr);
	}

	// Texture parameters
	{
		TArray<FMaterialParameterInfo> ParamInfos;
		TArray<FGuid> ParamGuids;
		Material->GetAllTextureParameterInfo(ParamInfos, ParamGuids);
		TArray<TSharedPtr<FJsonValue>> Arr;
		for (int32 i = 0; i < ParamInfos.Num(); ++i)
		{
			TSharedPtr<FJsonObject> P = MakeShared<FJsonObject>();
			P->SetStringField(TEXT("name"), ParamInfos[i].Name.ToString());
			UTexture* Value = nullptr;
			Material->GetTextureParameterValue(ParamInfos[i], Value);
			P->SetStringField(TEXT("value"), Value ? Value->GetPathName() : TEXT("None"));
			Arr.Add(MakeShared<FJsonValueObject>(P));
		}
		Data->SetArrayField(TEXT("texture"), Arr);
	}

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleAssignMaterialToActor(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString ActorName, MaterialPath;
	if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
		return MakeError(TEXT("actor_name required"));
	if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
		return MakeError(TEXT("material_path required"));

	int32 SlotIndex = 0;
	Params->TryGetNumberField(TEXT("slot_index"), SlotIndex);

	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
	if (!Material)
		return MakeError(FString::Printf(TEXT("Material not found: %s"), *MaterialPath));

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World) return MakeError(TEXT("No editor world"));

	AActor* FoundActor = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->GetName() == ActorName || It->GetActorLabel() == ActorName)
		{
			FoundActor = *It;
			break;
		}
	}
	if (!FoundActor)
		return MakeError(FString::Printf(TEXT("Actor not found: %s"), *ActorName));

	// Find first primitive component with a mesh
	UPrimitiveComponent* PrimComp = FoundActor->FindComponentByClass<UPrimitiveComponent>();
	if (!PrimComp)
		return MakeError(TEXT("Actor has no primitive component to assign material to"));

	FScopedTransaction Transaction(FText::FromString(TEXT("MCP: Assign Material")));
	PrimComp->Modify();
	PrimComp->SetMaterial(SlotIndex, Material);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("actor"), ActorName);
	Data->SetStringField(TEXT("material"), MaterialPath);
	Data->SetNumberField(TEXT("slot"), SlotIndex);
	return MakeResponse(true, Data);
}
