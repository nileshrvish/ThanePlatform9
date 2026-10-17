#include "MCPServer.h"
#include "EngineUtils.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Engine/LevelStreaming.h"
#include "EditorLevelUtils.h"
#include "FileHelpers.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "ScopedTransaction.h"
#include "GameFramework/Actor.h"
#include "UObject/UObjectGlobals.h"
#include "Kismet/GameplayStatics.h"

// ===== ACTOR MANAGEMENT =====

FString FMCPServer::HandleSpawnActor(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid())
	{
		return MakeError(TEXT("Missing params"));
	}

	FString ClassPath;
	if (!Params->TryGetStringField(TEXT("class_path"), ClassPath))
	{
		return MakeError(TEXT("class_path parameter is required (e.g. '/Script/Engine.StaticMeshActor' or '/Game/Blueprints/MyBP')"));
	}

	// Parse location
	FVector Location = FVector::ZeroVector;
	if (Params->HasField(TEXT("x"))) Location.X = Params->GetNumberField(TEXT("x"));
	if (Params->HasField(TEXT("y"))) Location.Y = Params->GetNumberField(TEXT("y"));
	if (Params->HasField(TEXT("z"))) Location.Z = Params->GetNumberField(TEXT("z"));

	// Parse rotation
	FRotator Rotation = FRotator::ZeroRotator;
	if (Params->HasField(TEXT("pitch"))) Rotation.Pitch = Params->GetNumberField(TEXT("pitch"));
	if (Params->HasField(TEXT("yaw"))) Rotation.Yaw = Params->GetNumberField(TEXT("yaw"));
	if (Params->HasField(TEXT("roll"))) Rotation.Roll = Params->GetNumberField(TEXT("roll"));

	// Resolve actor class
	UClass* ActorClass = nullptr;

	// Try loading as blueprint first
	UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *ClassPath);
	if (BP && BP->GeneratedClass)
	{
		ActorClass = BP->GeneratedClass;
	}
	else
	{
		// Try as native class
		ActorClass = LoadClass<AActor>(nullptr, *ClassPath);
	}

	if (!ActorClass)
	{
		return MakeError(FString::Printf(TEXT("Could not find actor class: %s"), *ClassPath));
	}

	// Get editor actor subsystem
	UEditorActorSubsystem* EditorActorSubsystem = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
	if (!EditorActorSubsystem)
	{
		return MakeError(TEXT("Could not get EditorActorSubsystem"));
	}

	// Spawn with undo support
	FScopedTransaction Transaction(FText::FromString(TEXT("MCP: Spawn Actor")));

	AActor* NewActor = EditorActorSubsystem->SpawnActorFromClass(ActorClass, Location, Rotation);
	if (!NewActor)
	{
		return MakeError(TEXT("Failed to spawn actor"));
	}

	// Set label if provided
	FString Label;
	if (Params->TryGetStringField(TEXT("label"), Label) && !Label.IsEmpty())
	{
		NewActor->SetActorLabel(Label);
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("name"), NewActor->GetName());
	Data->SetStringField(TEXT("label"), NewActor->GetActorLabel());
	Data->SetStringField(TEXT("class"), ActorClass->GetName());
	Data->SetStringField(TEXT("location"), FString::Printf(TEXT("(%.1f, %.1f, %.1f)"), Location.X, Location.Y, Location.Z));
	return MakeResponse(true, Data);
}

FString FMCPServer::HandleDestroyActor(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid())
	{
		return MakeError(TEXT("Missing params"));
	}

	FString ActorName;
	if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
	{
		return MakeError(TEXT("actor_name parameter is required"));
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World) return MakeError(TEXT("No editor world"));

	// Find the actor
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
	{
		return MakeError(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
	}

	UEditorActorSubsystem* EditorActorSubsystem = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
	if (!EditorActorSubsystem)
	{
		return MakeError(TEXT("Could not get EditorActorSubsystem"));
	}

	FScopedTransaction Transaction(FText::FromString(TEXT("MCP: Destroy Actor")));
	const bool bDestroyed = EditorActorSubsystem->DestroyActor(FoundActor);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("destroyed"), bDestroyed);
	Data->SetStringField(TEXT("actor_name"), ActorName);
	return MakeResponse(bDestroyed, Data, bDestroyed ? TEXT("") : TEXT("Failed to destroy actor"));
}

FString FMCPServer::HandleDuplicateActor(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid())
	{
		return MakeError(TEXT("Missing params"));
	}

	FString ActorName;
	if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
	{
		return MakeError(TEXT("actor_name parameter is required"));
	}

	FVector Offset = FVector::ZeroVector;
	if (Params->HasField(TEXT("offset_x"))) Offset.X = Params->GetNumberField(TEXT("offset_x"));
	if (Params->HasField(TEXT("offset_y"))) Offset.Y = Params->GetNumberField(TEXT("offset_y"));
	if (Params->HasField(TEXT("offset_z"))) Offset.Z = Params->GetNumberField(TEXT("offset_z"));

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
	{
		return MakeError(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
	}

	UEditorActorSubsystem* EditorActorSubsystem = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
	if (!EditorActorSubsystem)
	{
		return MakeError(TEXT("Could not get EditorActorSubsystem"));
	}

	FScopedTransaction Transaction(FText::FromString(TEXT("MCP: Duplicate Actor")));
	AActor* DuplicatedActor = EditorActorSubsystem->DuplicateActor(FoundActor, nullptr, Offset);

	if (!DuplicatedActor)
	{
		return MakeError(TEXT("Failed to duplicate actor"));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("name"), DuplicatedActor->GetName());
	Data->SetStringField(TEXT("label"), DuplicatedActor->GetActorLabel());
	Data->SetStringField(TEXT("source"), ActorName);
	FVector Loc = DuplicatedActor->GetActorLocation();
	Data->SetStringField(TEXT("location"), FString::Printf(TEXT("(%.1f, %.1f, %.1f)"), Loc.X, Loc.Y, Loc.Z));
	return MakeResponse(true, Data);
}

// ===== LEVEL MANAGEMENT =====

FString FMCPServer::HandleGetCurrentLevel(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		return MakeError(TEXT("No editor world"));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("world_name"), World->GetName());
	Data->SetStringField(TEXT("map_name"), World->GetMapName());

	UPackage* Package = World->GetOutermost();
	if (Package)
	{
		Data->SetStringField(TEXT("package_path"), Package->GetName());
	}

	// Actor count
	int32 ActorCount = 0;
	for (TActorIterator<AActor> It(World); It; ++It) { ActorCount++; }
	Data->SetNumberField(TEXT("actor_count"), ActorCount);

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleLoadLevel(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid())
	{
		return MakeError(TEXT("Missing params"));
	}

	FString LevelPath;
	if (!Params->TryGetStringField(TEXT("level_path"), LevelPath))
	{
		return MakeError(TEXT("level_path parameter is required (e.g. '/Game/Maps/MyLevel')"));
	}

	// Ensure path ends with the level name
	FString MapFilePath = LevelPath;
	if (!MapFilePath.EndsWith(TEXT(".umap")))
	{
		// Convert asset path to file path if needed
		MapFilePath = LevelPath;
	}

	const bool bSaved = FEditorFileUtils::SaveDirtyPackages(
		/*bPromptUserToSave=*/false,
		/*bSaveContentPackages=*/true,
		/*bSaveMapPackages=*/true);

	FEditorFileUtils::LoadMap(MapFilePath);

	UWorld* World = GEditor->GetEditorWorldContext().World();
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("loaded_level"), World ? World->GetMapName() : TEXT("unknown"));
	return MakeResponse(true, Data);
}

FString FMCPServer::HandleListLevels(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		return MakeError(TEXT("No editor world"));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();

	// Current persistent level
	Data->SetStringField(TEXT("persistent_level"), World->GetMapName());

	// Streaming levels (sublevels)
	TArray<TSharedPtr<FJsonValue>> SubLevels;
	for (ULevelStreaming* StreamingLevel : World->GetStreamingLevels())
	{
		if (StreamingLevel)
		{
			TSharedPtr<FJsonObject> LevelInfo = MakeShared<FJsonObject>();
			LevelInfo->SetStringField(TEXT("name"), StreamingLevel->GetWorldAssetPackageName());
			LevelInfo->SetBoolField(TEXT("loaded"), StreamingLevel->HasLoadedLevel());
			LevelInfo->SetBoolField(TEXT("visible"), StreamingLevel->GetShouldBeVisibleFlag());
			SubLevels.Add(MakeShared<FJsonValueObject>(LevelInfo));
		}
	}
	Data->SetArrayField(TEXT("streaming_levels"), SubLevels);
	Data->SetNumberField(TEXT("streaming_level_count"), SubLevels.Num());

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleMoveActor(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("actor_name")))
		return MakeError(TEXT("actor_name required"));

	const FString ActorName = Params->GetStringField(TEXT("actor_name"));
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World) return MakeError(TEXT("No world available"));

	AActor* FoundActor = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if ((*It)->GetName() == ActorName || (*It)->GetActorLabel() == ActorName)
		{
			FoundActor = *It;
			break;
		}
	}
	if (!FoundActor)
		return MakeError(FString::Printf(TEXT("Actor not found: %s"), *ActorName));

	FScopedTransaction Transaction(FText::FromString(TEXT("MCP: Move Actor")));
	FoundActor->Modify();

	FVector NewLoc = FoundActor->GetActorLocation();
	FRotator NewRot = FoundActor->GetActorRotation();
	FVector NewScale = FoundActor->GetActorScale3D();

	double V;
	if (Params->TryGetNumberField(TEXT("x"), V)) NewLoc.X = V;
	if (Params->TryGetNumberField(TEXT("y"), V)) NewLoc.Y = V;
	if (Params->TryGetNumberField(TEXT("z"), V)) NewLoc.Z = V;
	if (Params->TryGetNumberField(TEXT("pitch"), V)) NewRot.Pitch = V;
	if (Params->TryGetNumberField(TEXT("yaw"), V)) NewRot.Yaw = V;
	if (Params->TryGetNumberField(TEXT("roll"), V)) NewRot.Roll = V;
	if (Params->TryGetNumberField(TEXT("scale_x"), V)) NewScale.X = V;
	if (Params->TryGetNumberField(TEXT("scale_y"), V)) NewScale.Y = V;
	if (Params->TryGetNumberField(TEXT("scale_z"), V)) NewScale.Z = V;

	FoundActor->SetActorLocation(NewLoc);
	FoundActor->SetActorRotation(NewRot);
	FoundActor->SetActorScale3D(NewScale);
	FoundActor->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("actor"), FoundActor->GetActorLabel());
	Data->SetStringField(TEXT("location"), FString::Printf(TEXT("(%.1f, %.1f, %.1f)"), NewLoc.X, NewLoc.Y, NewLoc.Z));
	Data->SetStringField(TEXT("rotation"), FString::Printf(TEXT("(P=%.1f,Y=%.1f,R=%.1f)"), NewRot.Pitch, NewRot.Yaw, NewRot.Roll));
	Data->SetStringField(TEXT("scale"), FString::Printf(TEXT("(%.2f, %.2f, %.2f)"), NewScale.X, NewScale.Y, NewScale.Z));
	return MakeResponse(true, Data);
}
