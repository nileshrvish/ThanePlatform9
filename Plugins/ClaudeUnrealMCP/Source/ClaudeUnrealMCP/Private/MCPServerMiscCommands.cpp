// Sprints 21-23: GameplayTags, Splines, Physics/Collision
#include "MCPServer.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "GameplayTagsManager.h"
#include "GameplayTagsSettings.h"
#include "Components/SplineComponent.h"
#include "Components/PrimitiveComponent.h"
#include "ScopedTransaction.h"
#include "EngineUtils.h"

// ===== GAMEPLAY TAGS (Sprint 21) =====

FString FMCPServer::HandleManageGameplayTags(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString Op;
	if (!Params->TryGetStringField(TEXT("operation"), Op))
		return MakeError(TEXT("operation required (list, add, request)"));

	UGameplayTagsManager& TagManager = UGameplayTagsManager::Get();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();

	if (Op == TEXT("list"))
	{
		// List all tags, optionally filtered by parent
		FString ParentFilter;
		Params->TryGetStringField(TEXT("parent"), ParentFilter);

		FGameplayTagContainer AllTags;
		if (!ParentFilter.IsEmpty())
		{
			FGameplayTag ParentTag = TagManager.RequestGameplayTag(FName(*ParentFilter), false);
			if (ParentTag.IsValid())
			{
				AllTags = TagManager.RequestGameplayTagChildren(ParentTag);
			}
		}

		TArray<TSharedPtr<FJsonValue>> TagArr;

		if (AllTags.IsValid())
		{
			for (const FGameplayTag& Tag : AllTags)
			{
				TagArr.Add(MakeShared<FJsonValueString>(Tag.ToString()));
			}
		}
		else if (ParentFilter.IsEmpty())
		{
			// Get root-level overview: list source tag tables
			TArray<const FGameplayTagSource*> Sources;
			TagManager.FindTagSourcesWithType(EGameplayTagSourceType::TagList, Sources);
			Data->SetNumberField(TEXT("source_count"), Sources.Num());
		}

		Data->SetArrayField(TEXT("tags"), TagArr);
		Data->SetNumberField(TEXT("count"), TagArr.Num());
		return MakeResponse(true, Data);
	}
	else if (Op == TEXT("add"))
	{
		FString TagName, Comment;
		if (!Params->TryGetStringField(TEXT("tag"), TagName))
			return MakeError(TEXT("tag required (e.g. 'Ability.Skill.Fireball')"));
		Params->TryGetStringField(TEXT("comment"), Comment);

		// Add tag via settings (persists to DefaultGameplayTags.ini)
		UGameplayTagsSettings* Settings = GetMutableDefault<UGameplayTagsSettings>();
		if (Settings)
		{
			FGameplayTagTableRow NewRow;
			NewRow.Tag = FName(*TagName);
			NewRow.DevComment = Comment.IsEmpty() ? TEXT("Added via MCP") : Comment;
			Settings->GameplayTagList.Add(NewRow);
			Settings->SaveConfig();

			// Refresh tag manager
			TagManager.EditorRefreshGameplayTagTree();
		}

		Data->SetStringField(TEXT("tag"), TagName);
		Data->SetBoolField(TEXT("added"), true);
		return MakeResponse(true, Data);
	}
	else if (Op == TEXT("request"))
	{
		FString TagName;
		if (!Params->TryGetStringField(TEXT("tag"), TagName))
			return MakeError(TEXT("tag required"));

		FGameplayTag Tag = TagManager.RequestGameplayTag(FName(*TagName), false);
		Data->SetStringField(TEXT("tag"), TagName);
		Data->SetBoolField(TEXT("valid"), Tag.IsValid());
		if (Tag.IsValid())
		{
			FGameplayTagContainer Parents = TagManager.RequestGameplayTagParents(Tag);
			TArray<TSharedPtr<FJsonValue>> ParentArr;
			for (const FGameplayTag& P : Parents)
			{
				ParentArr.Add(MakeShared<FJsonValueString>(P.ToString()));
			}
			Data->SetArrayField(TEXT("parents"), ParentArr);
		}
		return MakeResponse(true, Data);
	}

	return MakeError(FString::Printf(TEXT("Unknown operation: %s"), *Op));
}

// ===== SPLINE OPS (Sprint 22) =====

FString FMCPServer::HandleSplineOps(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString ActorName, Op;
	if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
		return MakeError(TEXT("actor_name required"));
	if (!Params->TryGetStringField(TEXT("operation"), Op))
		return MakeError(TEXT("operation required (read, add_point, clear, set_closed)"));

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World) return MakeError(TEXT("No editor world"));

	USplineComponent* SplineComp = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->GetName() == ActorName || It->GetActorLabel() == ActorName)
		{
			SplineComp = It->FindComponentByClass<USplineComponent>();
			break;
		}
	}
	if (!SplineComp)
		return MakeError(FString::Printf(TEXT("Actor '%s' not found or has no SplineComponent"), *ActorName));

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();

	if (Op == TEXT("read"))
	{
		int32 NumPoints = SplineComp->GetNumberOfSplinePoints();
		Data->SetNumberField(TEXT("point_count"), NumPoints);
		Data->SetBoolField(TEXT("closed_loop"), SplineComp->IsClosedLoop());
		Data->SetNumberField(TEXT("spline_length"), SplineComp->GetSplineLength());

		TArray<TSharedPtr<FJsonValue>> Points;
		for (int32 i = 0; i < NumPoints; ++i)
		{
			FVector Loc = SplineComp->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World);
			TSharedPtr<FJsonObject> P = MakeShared<FJsonObject>();
			P->SetNumberField(TEXT("index"), i);
			P->SetNumberField(TEXT("x"), Loc.X);
			P->SetNumberField(TEXT("y"), Loc.Y);
			P->SetNumberField(TEXT("z"), Loc.Z);
			Points.Add(MakeShared<FJsonValueObject>(P));
		}
		Data->SetArrayField(TEXT("points"), Points);
	}
	else if (Op == TEXT("add_point"))
	{
		double X = 0, Y = 0, Z = 0;
		Params->TryGetNumberField(TEXT("x"), X);
		Params->TryGetNumberField(TEXT("y"), Y);
		Params->TryGetNumberField(TEXT("z"), Z);

		FScopedTransaction Transaction(FText::FromString(TEXT("MCP: Add Spline Point")));
		SplineComp->Modify();
		SplineComp->AddSplinePoint(FVector(X, Y, Z), ESplineCoordinateSpace::World, true);

		Data->SetNumberField(TEXT("point_count"), SplineComp->GetNumberOfSplinePoints());
		Data->SetStringField(TEXT("added_at"), FString::Printf(TEXT("(%.1f, %.1f, %.1f)"), X, Y, Z));
	}
	else if (Op == TEXT("clear"))
	{
		FScopedTransaction Transaction(FText::FromString(TEXT("MCP: Clear Spline")));
		SplineComp->Modify();
		SplineComp->ClearSplinePoints(true);
		Data->SetBoolField(TEXT("cleared"), true);
	}
	else if (Op == TEXT("set_closed"))
	{
		bool bClosed = false;
		Params->TryGetBoolField(TEXT("closed"), bClosed);
		FScopedTransaction Transaction(FText::FromString(TEXT("MCP: Set Spline Closed")));
		SplineComp->Modify();
		SplineComp->SetClosedLoop(bClosed, true);
		Data->SetBoolField(TEXT("closed_loop"), bClosed);
	}
	else
	{
		return MakeError(FString::Printf(TEXT("Unknown operation: %s"), *Op));
	}

	return MakeResponse(true, Data);
}

// ===== PHYSICS (Sprint 23) =====

FString FMCPServer::HandleSetPhysics(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString ActorName;
	if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
		return MakeError(TEXT("actor_name required"));

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World) return MakeError(TEXT("No editor world"));

	UPrimitiveComponent* PrimComp = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->GetName() == ActorName || It->GetActorLabel() == ActorName)
		{
			PrimComp = It->FindComponentByClass<UPrimitiveComponent>();
			break;
		}
	}
	if (!PrimComp)
		return MakeError(FString::Printf(TEXT("Actor '%s' not found or has no PrimitiveComponent"), *ActorName));

	FScopedTransaction Transaction(FText::FromString(TEXT("MCP: Set Physics")));
	PrimComp->Modify();

	bool bSimulate = false;
	if (Params->TryGetBoolField(TEXT("simulate_physics"), bSimulate))
	{
		PrimComp->SetSimulatePhysics(bSimulate);
	}

	bool bGravity = true;
	if (Params->TryGetBoolField(TEXT("enable_gravity"), bGravity))
	{
		PrimComp->SetEnableGravity(bGravity);
	}

	double Mass = -1;
	if (Params->TryGetNumberField(TEXT("mass"), Mass) && Mass > 0)
	{
		PrimComp->SetMassOverrideInKg(NAME_None, static_cast<float>(Mass));
	}

	double LinearDamping = -1;
	if (Params->TryGetNumberField(TEXT("linear_damping"), LinearDamping) && LinearDamping >= 0)
	{
		PrimComp->SetLinearDamping(static_cast<float>(LinearDamping));
	}

	double AngularDamping = -1;
	if (Params->TryGetNumberField(TEXT("angular_damping"), AngularDamping) && AngularDamping >= 0)
	{
		PrimComp->SetAngularDamping(static_cast<float>(AngularDamping));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("actor"), ActorName);
	Data->SetBoolField(TEXT("simulate_physics"), PrimComp->IsSimulatingPhysics());
	Data->SetBoolField(TEXT("gravity"), PrimComp->IsGravityEnabled());
	return MakeResponse(true, Data);
}

FString FMCPServer::HandleSetCollision(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString ActorName;
	if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
		return MakeError(TEXT("actor_name required"));

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World) return MakeError(TEXT("No editor world"));

	UPrimitiveComponent* PrimComp = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->GetName() == ActorName || It->GetActorLabel() == ActorName)
		{
			PrimComp = It->FindComponentByClass<UPrimitiveComponent>();
			break;
		}
	}
	if (!PrimComp)
		return MakeError(FString::Printf(TEXT("Actor '%s' not found or has no PrimitiveComponent"), *ActorName));

	FScopedTransaction Transaction(FText::FromString(TEXT("MCP: Set Collision")));
	PrimComp->Modify();

	FString CollisionProfile;
	if (Params->TryGetStringField(TEXT("collision_profile"), CollisionProfile))
	{
		PrimComp->SetCollisionProfileName(FName(*CollisionProfile));
	}

	bool bGenerateOverlaps = false;
	if (Params->TryGetBoolField(TEXT("generate_overlap_events"), bGenerateOverlaps))
	{
		PrimComp->SetGenerateOverlapEvents(bGenerateOverlaps);
	}

	FString CollisionEnabled;
	if (Params->TryGetStringField(TEXT("collision_enabled"), CollisionEnabled))
	{
		if (CollisionEnabled == TEXT("NoCollision"))
			PrimComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		else if (CollisionEnabled == TEXT("QueryOnly"))
			PrimComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		else if (CollisionEnabled == TEXT("PhysicsOnly"))
			PrimComp->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
		else if (CollisionEnabled == TEXT("QueryAndPhysics"))
			PrimComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("actor"), ActorName);
	Data->SetStringField(TEXT("collision_profile"), PrimComp->GetCollisionProfileName().ToString());
	Data->SetStringField(TEXT("collision_enabled"), UEnum::GetValueAsString(PrimComp->GetCollisionEnabled()));
	Data->SetBoolField(TEXT("generate_overlaps"), PrimComp->GetGenerateOverlapEvents());
	return MakeResponse(true, Data);
}
