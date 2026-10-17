// Sprint 19 — Niagara VFX
#include "MCPServer.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "ScopedTransaction.h"
#include "EngineUtils.h"

FString FMCPServer::HandleSpawnNiagaraSystem(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString SystemPath;
	if (!Params->TryGetStringField(TEXT("system_path"), SystemPath))
		return MakeError(TEXT("system_path required (e.g. '/Game/FX/NS_Fire')"));

	UNiagaraSystem* NiagaraSystem = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
	if (!NiagaraSystem)
		return MakeError(FString::Printf(TEXT("Niagara system not found: %s"), *SystemPath));

	FVector Location = FVector::ZeroVector;
	FRotator Rotation = FRotator::ZeroRotator;
	if (Params->HasField(TEXT("x"))) Location.X = Params->GetNumberField(TEXT("x"));
	if (Params->HasField(TEXT("y"))) Location.Y = Params->GetNumberField(TEXT("y"));
	if (Params->HasField(TEXT("z"))) Location.Z = Params->GetNumberField(TEXT("z"));
	if (Params->HasField(TEXT("pitch"))) Rotation.Pitch = Params->GetNumberField(TEXT("pitch"));
	if (Params->HasField(TEXT("yaw"))) Rotation.Yaw = Params->GetNumberField(TEXT("yaw"));

	bool bAutoDestroy = false;
	Params->TryGetBoolField(TEXT("auto_destroy"), bAutoDestroy);

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World) return MakeError(TEXT("No editor world"));

	FScopedTransaction Transaction(FText::FromString(TEXT("MCP: Spawn Niagara System")));

	UNiagaraComponent* NiagaraComp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		World, NiagaraSystem, Location, Rotation, FVector(1.f), bAutoDestroy, true);

	if (!NiagaraComp)
		return MakeError(TEXT("Failed to spawn Niagara system"));

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("system"), SystemPath);
	Data->SetStringField(TEXT("actor"), NiagaraComp->GetOwner() ? NiagaraComp->GetOwner()->GetName() : TEXT("None"));
	Data->SetStringField(TEXT("location"), FString::Printf(TEXT("(%.1f, %.1f, %.1f)"), Location.X, Location.Y, Location.Z));
	return MakeResponse(true, Data);
}

FString FMCPServer::HandleSetNiagaraParameter(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString ActorName, ParamName, ParamType;
	if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
		return MakeError(TEXT("actor_name required"));
	if (!Params->TryGetStringField(TEXT("param_name"), ParamName))
		return MakeError(TEXT("param_name required"));
	if (!Params->TryGetStringField(TEXT("param_type"), ParamType))
		return MakeError(TEXT("param_type required (float, int, vector, color)"));

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World) return MakeError(TEXT("No editor world"));

	// Find actor with Niagara component
	UNiagaraComponent* NiagaraComp = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->GetName() == ActorName || It->GetActorLabel() == ActorName)
		{
			NiagaraComp = It->FindComponentByClass<UNiagaraComponent>();
			break;
		}
	}
	if (!NiagaraComp)
		return MakeError(FString::Printf(TEXT("Actor '%s' not found or has no NiagaraComponent"), *ActorName));

	if (ParamType == TEXT("float"))
	{
		double Value = 0;
		Params->TryGetNumberField(TEXT("value"), Value);
		NiagaraComp->SetNiagaraVariableFloat(ParamName, static_cast<float>(Value));
	}
	else if (ParamType == TEXT("int"))
	{
		int32 Value = 0;
		Params->TryGetNumberField(TEXT("value"), Value);
		NiagaraComp->SetNiagaraVariableInt(ParamName, Value);
	}
	else if (ParamType == TEXT("vector"))
	{
		double X = 0, Y = 0, Z = 0;
		Params->TryGetNumberField(TEXT("x"), X);
		Params->TryGetNumberField(TEXT("y"), Y);
		Params->TryGetNumberField(TEXT("z"), Z);
		NiagaraComp->SetNiagaraVariableVec3(ParamName, FVector(X, Y, Z));
	}
	else if (ParamType == TEXT("color"))
	{
		double R = 1, G = 1, B = 1, A = 1;
		Params->TryGetNumberField(TEXT("r"), R);
		Params->TryGetNumberField(TEXT("g"), G);
		Params->TryGetNumberField(TEXT("b"), B);
		Params->TryGetNumberField(TEXT("a"), A);
		NiagaraComp->SetNiagaraVariableLinearColor(ParamName, FLinearColor(R, G, B, A));
	}
	else
	{
		return MakeError(FString::Printf(TEXT("Unknown param_type: %s"), *ParamType));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("param_name"), ParamName);
	Data->SetStringField(TEXT("param_type"), ParamType);
	Data->SetBoolField(TEXT("set"), true);
	return MakeResponse(true, Data);
}

FString FMCPServer::HandleNiagaraControl(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString ActorName, Op;
	if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
		return MakeError(TEXT("actor_name required"));
	if (!Params->TryGetStringField(TEXT("operation"), Op))
		return MakeError(TEXT("operation required (activate, deactivate, reset)"));

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World) return MakeError(TEXT("No editor world"));

	UNiagaraComponent* NiagaraComp = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->GetName() == ActorName || It->GetActorLabel() == ActorName)
		{
			NiagaraComp = It->FindComponentByClass<UNiagaraComponent>();
			break;
		}
	}
	if (!NiagaraComp)
		return MakeError(FString::Printf(TEXT("Actor '%s' not found or has no NiagaraComponent"), *ActorName));

	if (Op == TEXT("activate"))
	{
		NiagaraComp->Activate(true);
	}
	else if (Op == TEXT("deactivate"))
	{
		NiagaraComp->Deactivate();
	}
	else if (Op == TEXT("reset"))
	{
		NiagaraComp->Activate(true);
	}
	else
	{
		return MakeError(FString::Printf(TEXT("Unknown operation: %s"), *Op));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("actor"), ActorName);
	Data->SetStringField(TEXT("operation"), Op);
	return MakeResponse(true, Data);
}
