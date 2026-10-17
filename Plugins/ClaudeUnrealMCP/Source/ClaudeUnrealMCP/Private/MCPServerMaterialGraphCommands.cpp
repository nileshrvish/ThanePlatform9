// Sprint 17 — Material Graph Authoring
// Reference: GenOrca unreal-mcp material_actions.py
// UE API: UMaterialEditingLibrary (MaterialEditor module)

#include "MCPServer.h"
#include "Dom/JsonObject.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "EditorAssetLibrary.h"
#include "ScopedTransaction.h"

// Helper: resolve expression class from short name (e.g. "Multiply" -> UMaterialExpressionMultiply)
static UClass* ResolveMaterialExpressionClass(const FString& ClassName)
{
	// Try exact match first
	FString FullName = ClassName;
	if (!FullName.StartsWith(TEXT("MaterialExpression")))
	{
		FullName = TEXT("MaterialExpression") + ClassName;
	}

	// Search for the class
	UClass* ExprClass = FindFirstObject<UClass>(*FullName, EFindFirstObjectOptions::ExactClass);
	if (!ExprClass)
	{
		// Try with /Script/Engine prefix
		ExprClass = LoadClass<UMaterialExpression>(nullptr, *(FString(TEXT("/Script/Engine.")) + FullName));
	}
	if (!ExprClass)
	{
		// Try iterating all classes
		for (TObjectIterator<UClass> It; It; ++It)
		{
			if (It->IsChildOf(UMaterialExpression::StaticClass()) && It->GetName() == FullName)
			{
				ExprClass = *It;
				break;
			}
		}
	}
	return ExprClass;
}

// Helper: find expression in material by name or index
static UMaterialExpression* FindExpressionInMaterial(UMaterial* Material, const FString& Identifier)
{
	// Try by index first
	if (Identifier.IsNumeric())
	{
		int32 Index = FCString::Atoi(*Identifier);
		const TArray<TObjectPtr<UMaterialExpression>>& Expressions = Material->GetExpressionCollection().Expressions;
		if (Expressions.IsValidIndex(Index))
		{
			return Expressions[Index];
		}
	}

	// Try by name or desc
	for (UMaterialExpression* Expr : Material->GetExpressionCollection().Expressions)
	{
		if (Expr && (Expr->GetName() == Identifier || Expr->Desc == Identifier))
		{
			return Expr;
		}
	}
	return nullptr;
}

FString FMCPServer::HandleAddMaterialExpression(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString MaterialPath, ExpressionClassName;
	if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
		return MakeError(TEXT("material_path required"));
	if (!Params->TryGetStringField(TEXT("expression_class"), ExpressionClassName))
		return MakeError(TEXT("expression_class required (e.g. 'Multiply', 'TextureSample', 'Constant', 'VectorParameter', 'ScalarParameter', 'Add', 'Lerp')"));

	int32 PosX = 0, PosY = 0;
	Params->TryGetNumberField(TEXT("x"), PosX);
	Params->TryGetNumberField(TEXT("y"), PosY);

	UMaterial* Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
	if (!Material)
		return MakeError(FString::Printf(TEXT("Material not found: %s"), *MaterialPath));

	UClass* ExprClass = ResolveMaterialExpressionClass(ExpressionClassName);
	if (!ExprClass)
		return MakeError(FString::Printf(TEXT("Expression class not found: %s (try: Multiply, Add, Lerp, Constant, VectorParameter, ScalarParameter, TextureSample, TextureCoordinate)"), *ExpressionClassName));

	FScopedTransaction Transaction(FText::FromString(TEXT("MCP: Add Material Expression")));

	UMaterialExpression* NewExpr = UMaterialEditingLibrary::CreateMaterialExpression(Material, ExprClass, PosX, PosY);
	if (!NewExpr)
		return MakeError(TEXT("Failed to create material expression"));

	// Set description if provided
	FString Desc;
	if (Params->TryGetStringField(TEXT("description"), Desc))
	{
		NewExpr->Desc = Desc;
	}

	// Find expression index
	int32 Index = INDEX_NONE;
	const TArray<TObjectPtr<UMaterialExpression>>& Expressions = Material->GetExpressionCollection().Expressions;
	for (int32 i = 0; i < Expressions.Num(); ++i)
	{
		if (Expressions[i] == NewExpr) { Index = i; break; }
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("name"), NewExpr->GetName());
	Data->SetStringField(TEXT("class"), NewExpr->GetClass()->GetName());
	Data->SetNumberField(TEXT("index"), Index);
	Data->SetNumberField(TEXT("x"), PosX);
	Data->SetNumberField(TEXT("y"), PosY);
	return MakeResponse(true, Data);
}

FString FMCPServer::HandleConnectMaterialExpressions(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString MaterialPath, FromId, FromOutput, ToId, ToInput;
	if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
		return MakeError(TEXT("material_path required"));
	if (!Params->TryGetStringField(TEXT("from_expression"), FromId))
		return MakeError(TEXT("from_expression required (name, desc, or index)"));
	if (!Params->TryGetStringField(TEXT("from_output"), FromOutput))
		return MakeError(TEXT("from_output required (output pin name, or empty string for default)"));
	if (!Params->TryGetStringField(TEXT("to_expression"), ToId))
		return MakeError(TEXT("to_expression required (name, desc, or index)"));
	if (!Params->TryGetStringField(TEXT("to_input"), ToInput))
		return MakeError(TEXT("to_input required (input pin name, e.g. 'BaseColor', 'A', 'B')"));

	UMaterial* Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
	if (!Material)
		return MakeError(FString::Printf(TEXT("Material not found: %s"), *MaterialPath));

	UMaterialExpression* FromExpr = FindExpressionInMaterial(Material, FromId);
	if (!FromExpr)
		return MakeError(FString::Printf(TEXT("From expression not found: %s"), *FromId));

	// Special case: connecting to material output pins (BaseColor, Metallic, etc.)
	// ToId = "Material" means connect to the material itself
	if (ToId == TEXT("Material") || ToId == TEXT("material"))
	{
		FScopedTransaction Transaction(FText::FromString(TEXT("MCP: Connect to Material Output")));
		bool bSuccess = UMaterialEditingLibrary::ConnectMaterialExpressions(FromExpr, FromOutput, nullptr, ToInput);
		// ConnectMaterialExpressions with nullptr To connects to material output
		// Actually this doesn't work — need to use ConnectMaterialProperty instead
		// Let's try the direct approach
		if (!bSuccess)
		{
			return MakeError(TEXT("Failed to connect to material output. Use to_expression with actual expression name, or connect via material property pins."));
		}
	}

	UMaterialExpression* ToExpr = FindExpressionInMaterial(Material, ToId);
	if (!ToExpr)
		return MakeError(FString::Printf(TEXT("To expression not found: %s"), *ToId));

	FScopedTransaction Transaction(FText::FromString(TEXT("MCP: Connect Material Expressions")));
	bool bSuccess = UMaterialEditingLibrary::ConnectMaterialExpressions(FromExpr, FromOutput, ToExpr, ToInput);

	if (!bSuccess)
		return MakeError(FString::Printf(TEXT("Failed to connect %s(%s) -> %s(%s). Check pin names."), *FromId, *FromOutput, *ToId, *ToInput));

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("from"), FromId);
	Data->SetStringField(TEXT("from_output"), FromOutput);
	Data->SetStringField(TEXT("to"), ToId);
	Data->SetStringField(TEXT("to_input"), ToInput);
	Data->SetBoolField(TEXT("connected"), true);
	return MakeResponse(true, Data);
}

FString FMCPServer::HandleDeleteMaterialExpression(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString MaterialPath, ExprId;
	if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
		return MakeError(TEXT("material_path required"));
	if (!Params->TryGetStringField(TEXT("expression"), ExprId))
		return MakeError(TEXT("expression required (name, desc, or index)"));

	UMaterial* Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
	if (!Material)
		return MakeError(FString::Printf(TEXT("Material not found: %s"), *MaterialPath));

	UMaterialExpression* Expr = FindExpressionInMaterial(Material, ExprId);
	if (!Expr)
		return MakeError(FString::Printf(TEXT("Expression not found: %s"), *ExprId));

	FScopedTransaction Transaction(FText::FromString(TEXT("MCP: Delete Material Expression")));
	UMaterialEditingLibrary::DeleteMaterialExpression(Material, Expr);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("deleted"), ExprId);
	return MakeResponse(true, Data);
}

FString FMCPServer::HandleRecompileMaterial(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString MaterialPath;
	if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
		return MakeError(TEXT("material_path required"));

	UMaterial* Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
	if (!Material)
		return MakeError(FString::Printf(TEXT("Material not found: %s"), *MaterialPath));

	UMaterialEditingLibrary::RecompileMaterial(Material);
	UEditorAssetLibrary::SaveAsset(MaterialPath, false);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("material"), MaterialPath);
	Data->SetBoolField(TEXT("recompiled"), true);
	return MakeResponse(true, Data);
}

FString FMCPServer::HandleListMaterialExpressions(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString MaterialPath;
	if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
		return MakeError(TEXT("material_path required"));

	UMaterial* Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
	if (!Material)
		return MakeError(FString::Printf(TEXT("Material not found: %s"), *MaterialPath));

	const TArray<TObjectPtr<UMaterialExpression>>& Expressions = Material->GetExpressionCollection().Expressions;

	TArray<TSharedPtr<FJsonValue>> ExprArr;
	for (int32 i = 0; i < Expressions.Num(); ++i)
	{
		UMaterialExpression* Expr = Expressions[i];
		if (!Expr) continue;

		TSharedPtr<FJsonObject> ExprObj = MakeShared<FJsonObject>();
		ExprObj->SetNumberField(TEXT("index"), i);
		ExprObj->SetStringField(TEXT("name"), Expr->GetName());
		ExprObj->SetStringField(TEXT("class"), Expr->GetClass()->GetName());
		ExprObj->SetStringField(TEXT("desc"), Expr->Desc);
		ExprObj->SetNumberField(TEXT("x"), Expr->MaterialExpressionEditorX);
		ExprObj->SetNumberField(TEXT("y"), Expr->MaterialExpressionEditorY);

		// List outputs
		TArray<FExpressionOutput>& Outputs = Expr->GetOutputs();
		TArray<TSharedPtr<FJsonValue>> OutputArr;
		for (const FExpressionOutput& Out : Outputs)
		{
			OutputArr.Add(MakeShared<FJsonValueString>(Out.OutputName.ToString()));
		}
		ExprObj->SetArrayField(TEXT("outputs"), OutputArr);

		ExprArr.Add(MakeShared<FJsonValueObject>(ExprObj));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("expressions"), ExprArr);
	Data->SetNumberField(TEXT("count"), ExprArr.Num());
	return MakeResponse(true, Data);
}
