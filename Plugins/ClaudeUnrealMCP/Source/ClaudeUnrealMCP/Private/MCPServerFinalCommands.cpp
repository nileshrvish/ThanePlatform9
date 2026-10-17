// Sprints 28-31: PCG, GAS, Networking, Volumes, Procedural Mesh
#include "MCPServer.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "EditorAssetLibrary.h"
#include "ScopedTransaction.h"
#include "EngineUtils.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/TriggerVolume.h"
#include "Engine/BlockingVolume.h"
#include "Sound/AudioVolume.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "ProceduralMeshComponent.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "Factories/BlueprintFactory.h"
#include "Engine/Blueprint.h"

// ===== PCG (Sprint 28) =====

FString FMCPServer::HandlePCGOps(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString Op;
	if (!Params->TryGetStringField(TEXT("operation"), Op))
		return MakeError(TEXT("operation required (list_components, execute)"));

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World) return MakeError(TEXT("No editor world"));

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();

	if (Op == TEXT("list_components"))
	{
		// Find all actors with PCG components
		TArray<TSharedPtr<FJsonValue>> PCGActors;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			TArray<UActorComponent*> Components;
			It->GetComponents(Components);
			for (UActorComponent* Comp : Components)
			{
				if (Comp && Comp->GetClass()->GetName().Contains(TEXT("PCG")))
				{
					TSharedPtr<FJsonObject> ActorObj = MakeShared<FJsonObject>();
					ActorObj->SetStringField(TEXT("actor"), It->GetName());
					ActorObj->SetStringField(TEXT("component"), Comp->GetName());
					ActorObj->SetStringField(TEXT("class"), Comp->GetClass()->GetName());
					PCGActors.Add(MakeShared<FJsonValueObject>(ActorObj));
				}
			}
		}
		Data->SetArrayField(TEXT("pcg_components"), PCGActors);
		Data->SetNumberField(TEXT("count"), PCGActors.Num());
	}
	else if (Op == TEXT("execute"))
	{
		FString ActorName;
		if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
			return MakeError(TEXT("actor_name required for execute"));

		// Find actor and call GenerateGraph via reflection
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->GetName() == ActorName || It->GetActorLabel() == ActorName)
			{
				// Find PCG component and call Generate
				TArray<UActorComponent*> Components;
				It->GetComponents(Components);
				for (UActorComponent* Comp : Components)
				{
					if (Comp && Comp->GetClass()->GetName().Contains(TEXT("PCG")))
					{
						UFunction* GenFunc = Comp->FindFunction(TEXT("Generate"));
						if (!GenFunc) GenFunc = Comp->FindFunction(TEXT("GenerateLocal"));
						if (GenFunc)
						{
							Comp->ProcessEvent(GenFunc, nullptr);
							Data->SetBoolField(TEXT("executed"), true);
							Data->SetStringField(TEXT("actor"), ActorName);
							return MakeResponse(true, Data);
						}
					}
				}
				return MakeError(TEXT("PCG component found but no Generate function"));
			}
		}
		return MakeError(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
	}
	else
	{
		return MakeError(FString::Printf(TEXT("Unknown operation: %s"), *Op));
	}

	return MakeResponse(true, Data);
}

// ===== GAS (Sprint 29) =====

FString FMCPServer::HandleCreateGameplayAbility(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString AssetPath, AssetName;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
		return MakeError(TEXT("asset_path required"));
	if (!Params->TryGetStringField(TEXT("asset_name"), AssetName))
		return MakeError(TEXT("asset_name required"));

	// GameplayAbility is a Blueprint-based class
	// Create a Blueprint with parent GameplayAbility
	FString ParentClass = TEXT("/Script/GameplayAbilities.GameplayAbility");
	Params->TryGetStringField(TEXT("parent_class"), ParentClass);

	UClass* AbilityClass = LoadClass<UObject>(nullptr, *ParentClass);
	if (!AbilityClass)
		return MakeError(FString::Printf(TEXT("Parent class not found: %s"), *ParentClass));

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
	Factory->ParentClass = AbilityClass;

	UObject* NewAsset = AssetTools.CreateAsset(AssetName, AssetPath, UBlueprint::StaticClass(), Factory);
	if (!NewAsset)
		return MakeError(TEXT("Failed to create GameplayAbility blueprint"));

	UEditorAssetLibrary::SaveAsset(NewAsset->GetPathName(), false);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("path"), NewAsset->GetPathName());
	Data->SetStringField(TEXT("parent"), ParentClass);
	return MakeResponse(true, Data);
}

FString FMCPServer::HandleCreateGameplayEffect(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString AssetPath, AssetName;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
		return MakeError(TEXT("asset_path required"));
	if (!Params->TryGetStringField(TEXT("asset_name"), AssetName))
		return MakeError(TEXT("asset_name required"));

	FString ParentClass = TEXT("/Script/GameplayAbilities.GameplayEffect");

	UClass* EffectClass = LoadClass<UObject>(nullptr, *ParentClass);
	if (!EffectClass)
		return MakeError(TEXT("GameplayAbilities plugin not enabled or GameplayEffect class not found"));

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
	Factory->ParentClass = EffectClass;

	UObject* NewAsset = AssetTools.CreateAsset(AssetName, AssetPath, UBlueprint::StaticClass(), Factory);
	if (!NewAsset)
		return MakeError(TEXT("Failed to create GameplayEffect blueprint"));

	UEditorAssetLibrary::SaveAsset(NewAsset->GetPathName(), false);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("path"), NewAsset->GetPathName());
	return MakeResponse(true, Data);
}

// ===== NETWORKING (Sprint 30) =====

FString FMCPServer::HandleSetReplication(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString ActorName;
	if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
		return MakeError(TEXT("actor_name required"));

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

	FScopedTransaction Transaction(FText::FromString(TEXT("MCP: Set Replication")));
	FoundActor->Modify();

	bool bReplicates = false;
	if (Params->TryGetBoolField(TEXT("replicate"), bReplicates))
	{
		FoundActor->SetReplicates(bReplicates);
	}

	bool bReplicateMovement = false;
	if (Params->TryGetBoolField(TEXT("replicate_movement"), bReplicateMovement))
	{
		FoundActor->SetReplicateMovement(bReplicateMovement);
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("actor"), ActorName);
	Data->SetBoolField(TEXT("replicates"), FoundActor->GetIsReplicated());
	Data->SetBoolField(TEXT("replicate_movement"), FoundActor->IsReplicatingMovement());
	return MakeResponse(true, Data);
}

// ===== VOLUMES (Sprint 31) =====

FString FMCPServer::HandleCreateVolume(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString VolumeType;
	if (!Params->TryGetStringField(TEXT("volume_type"), VolumeType))
		return MakeError(TEXT("volume_type required (Trigger, Blocking, Audio, NavMesh)"));

	static const TMap<FString, UClass*> VolumeClasses = {
		{TEXT("Trigger"), ATriggerVolume::StaticClass()},
		{TEXT("Blocking"), ABlockingVolume::StaticClass()},
		{TEXT("Audio"), AAudioVolume::StaticClass()},
		{TEXT("NavMesh"), ANavMeshBoundsVolume::StaticClass()},
	};

	const UClass* const* VolClass = VolumeClasses.Find(VolumeType);
	if (!VolClass)
		return MakeError(FString::Printf(TEXT("Unknown volume type: %s"), *VolumeType));

	FVector Location = FVector::ZeroVector;
	if (Params->HasField(TEXT("x"))) Location.X = Params->GetNumberField(TEXT("x"));
	if (Params->HasField(TEXT("y"))) Location.Y = Params->GetNumberField(TEXT("y"));
	if (Params->HasField(TEXT("z"))) Location.Z = Params->GetNumberField(TEXT("z"));

	UEditorActorSubsystem* EditorActorSubsystem = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
	if (!EditorActorSubsystem)
		return MakeError(TEXT("Could not get EditorActorSubsystem"));

	FScopedTransaction Transaction(FText::FromString(TEXT("MCP: Create Volume")));
	AActor* NewActor = EditorActorSubsystem->SpawnActorFromClass(const_cast<UClass*>(*VolClass), Location);

	if (!NewActor)
		return MakeError(TEXT("Failed to spawn volume"));

	FString Label;
	if (Params->TryGetStringField(TEXT("label"), Label))
	{
		NewActor->SetActorLabel(Label);
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("name"), NewActor->GetName());
	Data->SetStringField(TEXT("type"), VolumeType);
	Data->SetStringField(TEXT("location"), FString::Printf(TEXT("(%.1f, %.1f, %.1f)"), Location.X, Location.Y, Location.Z));
	return MakeResponse(true, Data);
}

// ===== PROCEDURAL MESH (Sprint 31) =====

FString FMCPServer::HandleCreateProceduralMesh(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString ActorName;
	if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
		return MakeError(TEXT("actor_name required (actor to add ProceduralMeshComponent to)"));

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

	// Check if already has a ProceduralMeshComponent
	UProceduralMeshComponent* ProcMesh = FoundActor->FindComponentByClass<UProceduralMeshComponent>();
	if (!ProcMesh)
	{
		// Create one
		ProcMesh = NewObject<UProceduralMeshComponent>(FoundActor, TEXT("ProceduralMesh"));
		ProcMesh->RegisterComponent();
		ProcMesh->AttachToComponent(FoundActor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	}

	// Parse vertices and triangles from params
	FString VerticesStr, TrianglesStr;
	if (Params->TryGetStringField(TEXT("vertices"), VerticesStr) &&
		Params->TryGetStringField(TEXT("triangles"), TrianglesStr))
	{
		// Parse vertices: "x1,y1,z1;x2,y2,z2;..."
		TArray<FVector> Vertices;
		TArray<FString> VertParts;
		VerticesStr.ParseIntoArray(VertParts, TEXT(";"));
		for (const FString& VP : VertParts)
		{
			TArray<FString> Coords;
			VP.ParseIntoArray(Coords, TEXT(","));
			if (Coords.Num() >= 3)
			{
				Vertices.Add(FVector(FCString::Atof(*Coords[0]), FCString::Atof(*Coords[1]), FCString::Atof(*Coords[2])));
			}
		}

		// Parse triangles: "0,1,2;3,4,5;..."
		TArray<int32> Triangles;
		TArray<FString> TriParts;
		TrianglesStr.ParseIntoArray(TriParts, TEXT(";"));
		for (const FString& TP : TriParts)
		{
			TArray<FString> Indices;
			TP.ParseIntoArray(Indices, TEXT(","));
			for (const FString& Idx : Indices)
			{
				Triangles.Add(FCString::Atoi(*Idx));
			}
		}

		TArray<FVector> Normals;
		TArray<FVector2D> UVs;
		TArray<FColor> Colors;
		TArray<FProcMeshTangent> Tangents;

		int32 SectionIndex = 0;
		Params->TryGetNumberField(TEXT("section"), SectionIndex);

		ProcMesh->CreateMeshSection(SectionIndex, Vertices, Triangles, Normals, UVs, Colors, Tangents, true);

		TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("actor"), ActorName);
		Data->SetNumberField(TEXT("vertices"), Vertices.Num());
		Data->SetNumberField(TEXT("triangles"), Triangles.Num() / 3);
		Data->SetNumberField(TEXT("section"), SectionIndex);
		return MakeResponse(true, Data);
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("actor"), ActorName);
	Data->SetStringField(TEXT("component"), ProcMesh->GetName());
	Data->SetStringField(TEXT("note"), TEXT("ProceduralMeshComponent added. Provide vertices and triangles to create geometry."));
	return MakeResponse(true, Data);
}
