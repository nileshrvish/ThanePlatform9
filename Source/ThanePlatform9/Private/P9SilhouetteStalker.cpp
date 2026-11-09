#include "P9SilhouetteStalker.h"
#include "P9Captions.h"
#include "P9NPCAnimInstance.h"
#include "P9NPCLocomotion.h"

#include "P9CampaignDirector.h"
#include "P9Train.h"
#include "ThanePlatform9.h"
#include "Animation/AnimSequence.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

namespace P9Silhouette
{
	static FAutoConsoleCommandWithWorld StatusCommand(TEXT("p9.Silhouette.Status"), TEXT("p9.Silhouette.Status - log the silhouette's state"),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			for (TActorIterator<AP9SilhouetteStalker> It(World); It; ++It) It->LogStatus();
		}));
	static FAutoConsoleCommandWithWorldAndArgs ForceCommand(TEXT("p9.Silhouette.Force"), TEXT("p9.Silhouette.Force <0|1> - ignore the day/anomaly rules (test)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			for (TActorIterator<AP9SilhouetteStalker> It(World); It; ++It)
			{
				It->bForceActive = Args.Num() ? FCString::Atoi(*Args[0]) != 0 : true;
				It->LogStatus();
			}
		}));
}

AP9SilhouetteStalker::AP9SilhouetteStalker()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Body = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Root);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetGenerateOverlapEvents(false);
	Body->SetCanEverAffectNavigation(false);
	Body->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	Body->bComponentUseFixedSkelBounds = true;
	Body->bReceivesDecals = false;
#if WITH_EDITORONLY_DATA
	Body->bDisplayBones = false;
#endif
	Voice = CreateDefaultSubobject<UAudioComponent>(TEXT("Voice"));
	Voice->SetupAttachment(Root);
	Voice->SetRelativeLocation(FVector(0.0f, 0.0f, 150.0f));
	Voice->bAutoActivate = false;
	Voice->bOverrideAttenuation = true;
	Voice->AttenuationOverrides.bAttenuate = true;
	Voice->AttenuationOverrides.bSpatialize = true;
	Voice->AttenuationOverrides.AttenuationShapeExtents = FVector(600.0f, 0.0f, 0.0f);   // close and present even across a platform
	Voice->AttenuationOverrides.FalloffDistance = 3000.0f;
	// Stage 7B: body capsule (feet at the root): query-only Pawn overlapping pawns only, no overlap events, not in any trace
	BodyCapsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("BodyCapsule"));
	BodyCapsule->SetupAttachment(Root);
	BodyCapsule->InitCapsuleSize(25.0f, 82.0f);
	BodyCapsule->SetRelativeLocation(FVector(0.0f, 0.0f, 82.0f));
	BodyCapsule->SetCollisionObjectType(ECC_Pawn);
	BodyCapsule->SetCollisionResponseToAllChannels(ECR_Ignore);
	BodyCapsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	BodyCapsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BodyCapsule->SetGenerateOverlapEvents(false);
	BodyCapsule->SetCanEverAffectNavigation(false);
	BodyCapsule->CanCharacterStepUpOn = ECB_No;
}

void AP9SilhouetteStalker::BeginPlay()
{
	Super::BeginPlay();
	if (StalkerMesh) Body->SetSkeletalMeshAsset(StalkerMesh);
	// Stage 5C: crossfading clip player (falls back to single-node clips if it cannot be created)
	Body->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	Body->SetAnimInstanceClass(UP9NPCAnimInstance::StaticClass());
	AnimInst = Cast<UP9NPCAnimInstance>(Body->GetAnimInstance());
	if (!AnimInst) Body->SetAnimationMode(EAnimationMode::AnimationSingleNode);
#if UE_ENABLE_DEBUG_DRAWING
	Body->SetDrawDebugSkeleton(false);
#endif
	for (TActorIterator<AP9CampaignDirector> It(GetWorld()); It; ++It) { Director = *It; break; }
	// Stage 7B: body size, mesh offset base, registry (friends keep clear of her while she is visible)
	BodyCapsule->SetCapsuleSize(BodyRadius, FMath::Max(BodyRadius, BodyHalfHeight));
	BodyCapsule->SetRelativeLocation(FVector(0.0f, 0.0f, FMath::Max(BodyRadius, BodyHalfHeight)));
	BodyBaseRel = Body->GetRelativeLocation();
	P9NPCLoco::RegisterBody(this, BodyRadius, BodyHalfHeight, 0.0f);
	Hide();
	NextAllowed = 10.0f;
	UE_LOG(LogP9, Log, TEXT("Silhouette: ready (%d lines, director %s, day chance %.2f)"), Lines.Num(), Director ? TEXT("found") : TEXT("missing"), DayChance);
}

void AP9SilhouetteStalker::EndPlay(const EEndPlayReason::Type Reason)
{
	P9NPCLoco::UnregisterBody(this);
	Super::EndPlay(Reason);
}

// ---------------------------------------------------------------------------------------------------------------------
// Stage 7B: clips and mesh placement on stairs, side-step round friends (presentation only - navigation unchanged)
// ---------------------------------------------------------------------------------------------------------------------
void AP9SilhouetteStalker::PlayMove(float Speed, float DeltaSeconds)
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(P9SilhouetteFloor), false, this);
	Params.AddIgnoredActor(UGameplayStatics::GetPlayerPawn(this, 0));
	const FVector Feet = GetActorLocation();
	const P9NPCLoco::FFloorProbe Probe = P9NPCLoco::ProbeFloor(GetWorld(), Feet, GetActorForwardVector(), Feet.Z, Params, StairTread);
	if (Probe.bOnStairs) { StairDir = Probe.StairDir; StairOffTime = 0.0f; StairRampZ = Probe.RampZ; }
	else if ((StairOffTime += DeltaSeconds) > 0.2f) StairDir = 0;
	UAnimSequence* Clip = WalkAnim;
	if (StairDir > 0 && StairsUpAnim) Clip = StairsUpAnim;
	else if (StairDir < 0 && StairsDownAnim) Clip = StairsDownAnim;
	Play(Clip);
	if (AnimInst)
	{
		if (Clip != WalkAnim)
		{
			// one cycle = two treads; the clip time follows her height on the flight (a foot on every tread, no sliding)
			const float Len = FMath::Max(0.1f, Clip->GetPlayLength());
			const float Horizontal = Speed / FMath::Sqrt(1.0f + FMath::Square(StairRise / StairTread));
			AnimInst->SetRate(FMath::Clamp(Horizontal / (2.0f * StairTread / Len), 0.3f, 3.0f));
			if (StairOffTime <= 0.0f && AnimInst->GetClip() == Clip) AnimInst->SetTime(P9NPCLoco::StairPhase(StairRampZ, StairRise, StairDir) * Len);
		}
		else
		{
			AnimInst->SetRate(FMath::Clamp(Speed / (WalkAnimSpeed > 0.0f ? WalkAnimSpeed : WalkClipSpeed), 0.5f, 2.6f));     // Stage 5C: no foot sliding
		}
	}
	// her trail is the player's feet (a smooth line over the steps): the mesh settles onto the stair ramp / the floor
	const float FloorZ = !Probe.bHit ? Feet.Z : (Probe.bOnStairs && Clip != WalkAnim ? Probe.RampZ : Probe.CentreZ);
	MeshZTarget = FMath::Clamp(FloorZ - Feet.Z, -30.0f, 30.0f);
}

void AP9SilhouetteStalker::PlayStand(UAnimSequence* Anim)
{
	Play(Anim);
	if (AnimInst) AnimInst->SetRate(1.0f);
	StairDir = 0; StairOffTime = 1.0f;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(P9SilhouetteFloor), false, this);
	Params.AddIgnoredActor(UGameplayStatics::GetPlayerPawn(this, 0));
	const FVector Feet = GetActorLocation();
	const P9NPCLoco::FFloorProbe Probe = P9NPCLoco::ProbeFloor(GetWorld(), Feet, GetActorForwardVector(), Feet.Z, Params, StairTread);
	MeshZTarget = Probe.bHit ? FMath::Clamp(Probe.CentreZ - Feet.Z, -30.0f, 30.0f) : 0.0f;    // standing: on the tread under her
}

void AP9SilhouetteStalker::ResetBodyOffset()
{
	MeshZ = MeshZTarget = 0.0f; SideOffset = FVector::ZeroVector; StairDir = 0; StairOffTime = 1.0f;
	Body->SetRelativeLocation(BodyBaseRel);
}

void AP9SilhouetteStalker::UpdateBodyOffset(float DeltaSeconds)
{
	MeshZ = FMath::FInterpTo(MeshZ, MeshZTarget, DeltaSeconds, 20.0f);
	// side-step a friend's body (she walks the player's path; the friends steer round her too)
	TArray<P9NPCLoco::FBody> Bodies;
	P9NPCLoco::GatherBodies(GetWorld(), this, Bodies);
	const FVector Feet = GetActorLocation() + FVector(SideOffset.X, SideOffset.Y, 0.0f);
	FVector Want = FVector::ZeroVector;
	for (const P9NPCLoco::FBody& B : Bodies)
	{
		const FVector BF = B.Actor->GetActorLocation() - FVector(0.0f, 0.0f, B.FeetOffset);
		if (FMath::Abs(BF.Z - Feet.Z) > 120.0f) continue;
		FVector Away(GetActorLocation().X - BF.X, GetActorLocation().Y - BF.Y, 0.0f);
		const float D = Away.Size();
		const float Need = BodyRadius + B.Radius + 6.0f - D;
		if (Need <= 0.0f) continue;
		Away = D > 1.0f ? Away / D : GetActorRightVector();
		Want += Away * Need;
	}
	Want = Want.GetClampedToMaxSize(45.0f);
	if (!Want.IsNearlyZero())
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(P9SilhouetteSide), false, this);
		Params.AddIgnoredActor(UGameplayStatics::GetPlayerPawn(this, 0));
		const FVector To = GetActorLocation() + Want;
		const P9NPCLoco::FFloorProbe Probe = P9NPCLoco::ProbeFloor(GetWorld(), To, Want, GetActorLocation().Z, Params, StairTread);
		if (!Probe.bHit || FMath::Abs(Probe.CentreZ - GetActorLocation().Z) > 25.0f || P9NPCLoco::MoveHitsWorld(GetWorld(), GetActorLocation(), To, BodyRadius, Params))
		{
			Want = FVector::ZeroVector;                                               // no room: she keeps to the path
		}
	}
	SideOffset = FMath::VInterpTo(SideOffset, Want, DeltaSeconds, 4.0f);
	const FVector LocalSide = GetActorRotation().UnrotateVector(FVector(SideOffset.X, SideOffset.Y, 0.0f));
	Body->SetRelativeLocation(BodyBaseRel + LocalSide + FVector(0.0f, 0.0f, MeshZ));
}

void AP9SilhouetteStalker::Play(UAnimSequence* Anim)
{
	if (!Anim || Anim == Playing) return;
	// appearing (nothing playing yet): no blend; otherwise a short crossfade between her poses
	const float Blend = Playing ? 0.3f : 0.0f;
	Playing = Anim;
	if (AnimInst) { AnimInst->PlayClip(Anim, true, Blend); AnimInst->SetRate(1.0f); }
	else Body->PlayAnimation(Anim, true);
}

bool AP9SilhouetteStalker::DayAllows()
{
	if (bForceActive) return true;
	if (!Director) return false;
	if (Director->GetPhase() == EP9DayPhase::Epilogue) return EpilogueAllows();    // Stage 5B
	if (Director->GetPhase() != EP9DayPhase::Exploring) return false;
	if (bHideUntilCampaignComplete) return false;                                  // Stage 5B: locked on Days 1-10
	if (bOnlyOnAnomalousDays && Director->GetNumActiveAnomalies() <= 0) return false;
	const int32 Key = Director->GetRunSeed() * 31 + Director->GetDay();
	if (Key != DayKey)
	{
		DayKey = Key; AppearancesToday = 0;
		FRandomStream R(Key ^ 0x5AD1); bDayActive = R.FRand() < DayChance;    // seeded: the same run/day always decides the same
		UE_LOG(LogP9, Log, TEXT("Silhouette: day %d %s"), Director->GetDay(), bDayActive ? TEXT("ACTIVE") : TEXT("quiet"));
	}
	return bDayActive && AppearancesToday < MaxAppearancesPerDay;
}

bool AP9SilhouetteStalker::PlayerAboardTrain() const
{
	return Director && ((Director->Train && Director->Train->IsPlayerAboard()) || (Director->OpeningTrain && Director->OpeningTrain->IsPlayerAboard()));
}

bool AP9SilhouetteStalker::EpilogueAllows()
{
	// Stage 5B: the playable epilogue (on foot between the Dadar local and the Vashi local); never during the scripted rides
	if (!bAppearInEpilogue || !Director || PlayerAboardTrain()) return false;
	// the epilogue's own seeded key (per run), separate from the RunSeed*31+Day keys of Days 1-10
	const int32 Key = static_cast<int32>((HashCombine(GetTypeHash(Director->GetRunSeed()), 0xE9110u) & 0x3fffffffu) | 0x40000000u);
	if (Key != DayKey)
	{
		DayKey = Key; AppearancesToday = 0;
		FRandomStream R(Key ^ 0x5AD1); bDayActive = R.FRand() < EpilogueChance;
		UE_LOG(LogP9, Log, TEXT("Silhouette: epilogue %s"), bDayActive ? TEXT("ACTIVE") : TEXT("quiet"));
	}
	return bDayActive && AppearancesToday < MaxAppearancesPerDay;
}

bool AP9SilhouetteStalker::PhaseAllows() const
{
	if (bForceActive) return true;
	if (!Director) return false;
	const EP9DayPhase Phase = Director->GetPhase();
	if (Phase == EP9DayPhase::Exploring) return !bHideUntilCampaignComplete;
	return Phase == EP9DayPhase::Epilogue && bAppearInEpilogue && !PlayerAboardTrain();
}

bool AP9SilhouetteStalker::InZone(const FVector& P) const
{
	if (bForceActive && !Director) return true;
	if (!Director) return false;
	for (const FBox& B : Director->AnomalyZones) if (B.ExpandBy(50.0f).IsInside(P)) return true;
	return false;
}

bool AP9SilhouetteStalker::PlayerSees(float MinDot) const
{
	const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!Cam) return false;
	const FVector Eye = Cam->GetCameraLocation();
	const FVector Head = GetActorLocation() + FVector(0, 0, 150);
	const FVector To = Head - Eye;
	if (To.Size() > NoticeRange || FVector::DotProduct(To.GetSafeNormal(), Cam->GetCameraRotation().Vector()) < MinDot) return false;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(P9SilhouetteSee), false, this);
	Params.AddIgnoredActor(UGameplayStatics::GetPlayerPawn(this, 0));
	FHitResult Hit;
	return !GetWorld()->LineTraceSingleByChannel(Hit, Eye, Head, ECC_Visibility, Params);
}

// ---------------------------------------------------------------------------------------------------------------------
// Stage 6B: trail navigation - she walks where the player walked
// ---------------------------------------------------------------------------------------------------------------------
namespace P9SilhouetteNav
{
	constexpr float TrailSpacing = 50.0f;      // cm between recorded footsteps
	constexpr int32 TrailMax = 140;            // ~70 m of path
	constexpr float TrailBreak = 400.0f;       // a jump longer than this (teleport, loop, ride) starts a new trail
	constexpr float JoinRange = 2500.0f;       // she looks for a straight way onto the trail within this range
	constexpr float StepHeight = 55.0f;        // floor height change allowed between probes on a straight walk
	constexpr float BodyRadius = 28.0f, BodyHalfHeight = 55.0f;
}

FVector AP9SilhouetteStalker::FeetOf(const APawn* Player)
{
	if (const ACharacter* C = Cast<ACharacter>(Player))
	{
		return C->GetActorLocation() - FVector(0.0f, 0.0f, C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	}
	return Player->GetActorLocation() - FVector(0.0f, 0.0f, 90.0f);
}

void AP9SilhouetteStalker::RecordTrail(const APawn* Player)
{
	using namespace P9SilhouetteNav;
	const ACharacter* C = Cast<ACharacter>(Player);
	if (PlayerAboardTrain() || (C && C->GetCharacterMovement() && C->GetCharacterMovement()->IsFalling()))
	{
		if (PlayerAboardTrain()) { Trail.Reset(); TrailCursor = INDEX_NONE; }   // a moving carriage is no path
		return;                                                                   // mid-jump points float: skip them
	}
	const FVector F = FeetOf(Player);
	if (Trail.Num() && FVector::Dist(Trail.Last(), F) > TrailBreak)
	{
		Trail.Reset(); TrailCursor = INDEX_NONE;                                   // teleported / looped: the old path leads nowhere
	}
	if (Trail.IsEmpty() || FVector::Dist(Trail.Last(), F) >= TrailSpacing)
	{
		Trail.Add(F);
	}
	while (Trail.Num() > TrailMax)
	{
		Trail.RemoveAt(0);
		if (TrailCursor != INDEX_NONE) TrailCursor = FMath::Max(0, TrailCursor - 1);
	}
}

bool AP9SilhouetteStalker::CanWalkStraight(const FVector& From, const FVector& To) const
{
	using namespace P9SilhouetteNav;
	const FVector Flat = To - From;
	const float Len = Flat.Size2D();
	if (FMath::Abs(Flat.Z) > StepHeight + Len * 0.6f)                             // steeper than any stair here
	{
		return false;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(P9SilhouetteWalk), false, this);
	Params.AddIgnoredActor(UGameplayStatics::GetPlayerPawn(this, 0));
	const FVector Lift(0.0f, 0.0f, BodyHalfHeight + StepHeight + 5.0f);          // body clear of the steps, not of walls
	FHitResult Hit;
	if (Len > 1.0f && GetWorld()->SweepSingleByChannel(Hit, From + Lift, To + Lift, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(BodyRadius, BodyHalfHeight), Params))
	{
		return false;                                                             // a wall, parapet, closure or bench in the way
	}
	// floor all the way (no stepping off the footbridge onto the tracks), never rising or falling more than a step between probes
	const int32 Probes = FMath::Max(1, FMath::CeilToInt(Len / 60.0f));
	float LastZ = From.Z;
	for (int32 i = 1; i <= Probes; ++i)
	{
		const FVector S = FMath::Lerp(From, To, i / static_cast<float>(Probes));
		if (!GetWorld()->LineTraceSingleByChannel(Hit, S + FVector(0, 0, 80), S - FVector(0, 0, 120), ECC_Visibility, Params) || FMath::Abs(Hit.ImpactPoint.Z - LastZ) > StepHeight)
		{
			return false;
		}
		LastZ = Hit.ImpactPoint.Z;
	}
	return true;
}

bool AP9SilhouetteStalker::JoinTrail()
{
	using namespace P9SilhouetteNav;
	// newest first: the closest place to the player she can walk to in a straight line (at most a dozen sweeps per try)
	const FVector Here = GetActorLocation();
	int32 Tries = 0;
	for (int32 i = Trail.Num() - 1; i >= 0 && Tries < 12; --i)
	{
		if (FVector::Dist(Here, Trail[i]) > JoinRange) continue;
		++Tries;
		if (CanWalkStraight(Here, Trail[i]))
		{
			TrailCursor = i; ++TrailJoins;
			UE_LOG(LogP9, Log, TEXT("Silhouette: joins the player's trail at point %d of %d (%.0f m away)"), i, Trail.Num(), FVector::Dist(Here, Trail[i]) / 100.0f);
			return true;
		}
		i -= 2;                                                                   // sample every third point
	}
	return false;
}

float AP9SilhouetteStalker::TrailDistanceToPlayer(const APawn* Player) const
{
	const FVector F = FeetOf(Player);
	if (!Trail.IsValidIndex(TrailCursor))
	{
		return FVector::Dist(GetActorLocation(), F);
	}
	float D = FVector::Dist(GetActorLocation(), Trail[TrailCursor]);
	for (int32 i = TrailCursor + 1; i < Trail.Num(); ++i) D += FVector::Dist(Trail[i - 1], Trail[i]);
	return D + FVector::Dist(Trail.Last(), F);
}

bool AP9SilhouetteStalker::TrailSpotBehind(const APawn* Player, bool bRequireZone, bool bRequireSightLine, FVector& Out, int32& OutIndex) const
{
	// walk back along the player's path until it is FollowDistance long; the spot must be out of the camera's view now
	// (she appears unseen) and - for the random appearances - in plain sight once the player turns round
	const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!Cam || Trail.Num() < 2) return false;
	const FVector Eye = Cam->GetCameraLocation(), Fwd = Cam->GetCameraRotation().Vector();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(P9SilhouetteSpot), false, this);
	Params.AddIgnoredActor(Player);
	float Along = FVector::Dist(Trail.Last(), FeetOf(Player));
	for (int32 i = Trail.Num() - 1; i > 0; --i)
	{
		Along += FVector::Dist(Trail[i], Trail[i - 1]);
		if (Along < FollowDistance * 0.85f) continue;
		if (Along > FollowDistance * 1.6f) break;
		const FVector Spot = Trail[i - 1];
		const FVector Head = Spot + FVector(0, 0, 150);
		if (FVector::DotProduct((Head - Eye).GetSafeNormal(), Fwd) > 0.25f) continue;           // would pop up in view
		if (bRequireZone && !InZone(Spot + FVector(0, 0, 90))) continue;
		FHitResult Block;
		if (bRequireSightLine && GetWorld()->LineTraceSingleByChannel(Block, Eye, Head, ECC_Visibility, Params)) continue;
		Out = Spot; OutIndex = i;                                                 // she then walks on from the next point
		return true;
	}
	return false;
}

bool AP9SilhouetteStalker::PlaceOnTrailBehind(const APawn* Player, bool bRequireZone)
{
	FVector Spot; int32 Index = INDEX_NONE;
	if (!TrailSpotBehind(Player, bRequireZone, false, Spot, Index)) return false;
	SetActorLocation(Spot);
	TrailCursor = Index; ++TrailCatchUps;
	UE_LOG(LogP9, Log, TEXT("Silhouette: catches up unseen on the player's trail (%.0f m of path behind)"), TrailDistanceToPlayer(Player) / 100.0f);
	return true;
}

bool AP9SilhouetteStalker::MoveAlongTrail(const APawn* Player, float Speed, float DeltaSeconds, float StopDistance)
{
	if (!Trail.IsValidIndex(TrailCursor) || TrailDistanceToPlayer(Player) <= StopDistance) return false;
	FVector Pos = GetActorLocation();
	const FVector Start = Pos;
	float Budget = Speed * DeltaSeconds;
	while (Budget > 0.0f && Trail.IsValidIndex(TrailCursor))
	{
		const FVector Target = Trail[TrailCursor];
		const float D = FVector::Dist(Pos, Target);
		if (D <= Budget)
		{
			Pos = Target; Budget -= D;
			if (TrailCursor == Trail.Num() - 1) break;                           // at the player's last footstep
			++TrailCursor;
		}
		else
		{
			Pos += (Target - Pos) / D * Budget; Budget = 0.0f;
		}
	}
	const FVector Step = Pos - Start;
	SetActorLocation(Pos);
	if (Step.SizeSquared2D() > 1.0f) SetActorRotation(FRotator(0.0f, Step.Rotation().Yaw, 0.0f));   // she faces the way she walks
	return Step.SizeSquared() > 0.01f;
}

bool AP9SilhouetteStalker::FindSpotBehind(FVector& Out) const
{
	const APawn* P = UGameplayStatics::GetPlayerPawn(this, 0);
	const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!P || !Cam) return false;
	int32 Index = INDEX_NONE;
	if (TrailSpotBehind(P, true, true, Out, Index)) return true;                  // Stage 6B: a place the player actually walked
	// fallback (no trail yet): straight behind the camera on the same floor
	const FVector Back = -Cam->GetCameraRotation().Vector().GetSafeNormal2D();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(P9SilhouetteSpot), false, this);
	Params.AddIgnoredActor(P);
	for (int32 Try = 0; Try < 6; ++Try)
	{
		const float Side = (Try % 2 ? 1.0f : -1.0f) * 70.0f * (Try / 2);
		const float Dist = FollowDistance * (1.0f - 0.12f * (Try / 2));
		const FVector Probe = P->GetActorLocation() + Back * Dist + FVector::CrossProduct(Back, FVector::UpVector) * Side;
		FHitResult Floor;
		if (!GetWorld()->LineTraceSingleByChannel(Floor, Probe + FVector(0, 0, 120), Probe - FVector(0, 0, 300), ECC_Visibility, Params)) continue;
		const FVector Spot = Floor.ImpactPoint;
		if (FMath::Abs(Spot.Z - (P->GetActorLocation().Z - 90.0f)) > 60.0f || !InZone(Spot + FVector(0, 0, 90))) continue;   // same floor, inside the zone
		FHitResult Block;                                                                     // she must be visible when the player turns
		if (GetWorld()->LineTraceSingleByChannel(Block, Cam->GetCameraLocation(), Spot + FVector(0, 0, 150), ECC_Visibility, Params)) continue;
		Out = Spot;
		return true;
	}
	return false;
}

void AP9SilhouetteStalker::Show(const FVector& Spot)
{
	SetActorLocation(Spot);
	if (const APawn* P = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		SetActorRotation(FRotator(0.0f, (P->GetActorLocation() - Spot).GetSafeNormal2D().Rotation().Yaw, 0.0f));
	}
	SetActorHiddenInGame(false);
	Body->SetComponentTickEnabled(true);
	ResetBodyOffset();
	Playing = nullptr; Play(WalkAnim);
	State = EState::Following; StateTime = 0.0f; SeenTime = 0.0f;
	TrailCursor = INDEX_NONE; JoinCheckTime = 0.0f;
	JoinTrail();                                                                      // Stage 6B: she stands on the player's path
	++AppearancesToday;
	UE_LOG(LogP9, Log, TEXT("Silhouette: appears behind the player at %s (%d today)"), *Spot.ToCompactString(), AppearancesToday);
}

void AP9SilhouetteStalker::Hide()
{
	SetActorHiddenInGame(true);
	Body->SetComponentTickEnabled(false);     // no animation work while hidden
	Voice->Stop();
	State = EState::Hidden; StateTime = 0.0f; MovingTime = 0.0f;
	NextAllowed = Now + Cooldown;
}

void AP9SilhouetteStalker::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Now += DeltaSeconds; StateTime += DeltaSeconds;
	if (!IsHidden()) UpdateBodyOffset(DeltaSeconds);                                 // Stage 7B: last tick's targets
	const APawn* P = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!P) return;
	const float Speed = P->GetVelocity().Size2D();
	RecordTrail(P);                                                                   // Stage 6B: always, so a trail exists when she appears
	if (bCampaignSighting)
	{
		TickCampaign(DeltaSeconds, P, Speed);                                       // Stage 6A: her scheduled day
		return;
	}
	switch (State)
	{
	case EState::Hidden:
	{
		MovingTime = Speed >= MinPlayerSpeed ? MovingTime + DeltaSeconds : 0.0f;
		if (Now < NextAllowed || MovingTime < MovingSeconds || !InZone(P->GetActorLocation()) || !DayAllows()) return;
		FVector Spot;
		if (FindSpotBehind(Spot) && !PlayerSees(0.2f)) Show(Spot);
		return;
	}
	case EState::Following:
	{
		if (!PhaseAllows()) { Hide(); return; }                                   // Stage 5B: Exploring (unchanged) or the epilogue walk
		// noticed: speak at once
		SeenTime = PlayerSees(NoticeDot) ? SeenTime + DeltaSeconds : 0.0f;
		if (SeenTime > 0.08f && Lines.Num())
		{
			SpeakLeft = SpeakNextLine();
			State = EState::Speaking; StateTime = 0.0f;
			return;
		}
		// keep her distance behind the player along the player's own path (Stage 6B); stand still (menace) when the player stops
		if (!Trail.IsValidIndex(TrailCursor) && (JoinCheckTime -= DeltaSeconds) <= 0.0f) { JoinCheckTime = 0.3f; JoinTrail(); }
		const float Behind = TrailDistanceToPlayer(P);
		if (Behind > FollowDistance * 1.8f || !Trail.IsValidIndex(TrailCursor))
		{
			// fell too far behind (or cannot reach the path): re-place behind the player while unseen
			if (Behind > FollowDistance * 1.8f && !PlayerSees(0.2f) && PlaceOnTrailBehind(P, true)) return;
			if (StateTime > 25.0f) { Hide(); return; }
		}
		bool bMoved = false;
		if (Speed > 30.0f && Behind > FollowDistance * 0.8f)
		{
			const float MoveSpeed = FMath::Min(Speed, 300.0f);
			bMoved = MoveAlongTrail(P, MoveSpeed, DeltaSeconds, FollowDistance * 0.8f);
			if (bMoved) PlayMove(MoveSpeed, DeltaSeconds);                          // Stage 7B: walk / stairs up / stairs down
		}
		if (!bMoved)
		{
			SetActorRotation(FRotator(0.0f, (P->GetActorLocation() - GetActorLocation()).GetSafeNormal2D().Rotation().Yaw, 0.0f));
			PlayStand(MenaceAnim ? MenaceAnim.Get() : IdleAnim.Get());
		}
		if (StateTime > 60.0f) Hide();
		return;
	}
	case EState::Speaking:
		if (const APawn* PP = P) SetActorRotation(FRotator(0.0f, (PP->GetActorLocation() - GetActorLocation()).GetSafeNormal2D().Rotation().Yaw, 0.0f));
		if ((SpeakLeft -= DeltaSeconds) <= 0.0f) { Play(MenaceAnim ? MenaceAnim.Get() : IdleAnim.Get()); State = EState::Lingering; StateTime = 0.0f; }
		return;
	case EState::Lingering:
		// gone the moment the player looks away (or after MaxLinger)
		if (!PlayerSees(0.5f) || StateTime > MaxLinger)
		{
			UE_LOG(LogP9, Log, TEXT("Silhouette: vanishes"));
			Hide();
		}
		return;
	}
}

float AP9SilhouetteStalker::SpeakNextLine()
{
	if (Lines.IsEmpty())
	{
		return 3.0f;
	}
	if (LineBag.IsEmpty()) { for (int32 i = 0; i < Lines.Num(); ++i) if (i != LastLine || Lines.Num() == 1) LineBag.Add(i); }
	const int32 Pick = LineBag[FMath::RandRange(0, LineBag.Num() - 1)]; LineBag.Remove(Pick); LastLine = Pick;
	USoundBase* S = Lines[Pick].LoadSynchronous();
	const float Duration = S ? FMath::Max(1.0f, S->GetDuration()) : 3.0f;
	if (S) { Voice->SetSound(S); Voice->SetVolumeMultiplier(VoiceVolume); Voice->Play(); P9Captions::NotifySound(this, S); }   // Stage 9B: captions
	Play(TalkAnim ? TalkAnim.Get() : MenaceAnim.Get());
	UE_LOG(LogP9, Log, TEXT("Silhouette: speaks line %d (%.1fs)"), Pick + 1, Duration);
	return Duration;
}

// ---------------------------------------------------------------------------------------------------------------------
// Stage 6A: the director's scheduled "Kanchana" day
// ---------------------------------------------------------------------------------------------------------------------
bool AP9SilhouetteStalker::BeginCampaignSighting(const FVector& Spot, float Yaw)
{
	if (!Body || !Body->GetSkeletalMeshAsset())
	{
		return false;
	}
	Voice->Stop();
	bCampaignSighting = true;
	SetActorLocationAndRotation(Spot, FRotator(0.0f, Yaw, 0.0f));
	SetActorHiddenInGame(false);
	Body->SetComponentTickEnabled(true);
	ResetBodyOffset();
	Playing = nullptr;
	Play(IdleAnim ? IdleAnim.Get() : MenaceAnim.Get());
	State = EState::Waiting; StateTime = 0.0f; SeenTime = 0.0f; NextLineAt = 0.0f; CampaignLines = 0;
	TrailCursor = INDEX_NONE; JoinCheckTime = 0.0f; TrailJoins = 0; TrailCatchUps = 0;
	UE_LOG(LogP9, Log, TEXT("Silhouette: campaign sighting - Kanchana waits at %s"), *Spot.ToCompactString());
	return true;
}

void AP9SilhouetteStalker::EndCampaignSighting()
{
	if (!bCampaignSighting)
	{
		return;
	}
	bCampaignSighting = false;
	Hide();
	UE_LOG(LogP9, Log, TEXT("Silhouette: campaign sighting over (%d lines spoken)"), CampaignLines);
}

void AP9SilhouetteStalker::TickCampaign(float DeltaSeconds, const APawn* Player, float PlayerSpeed)
{
	const FVector To = Player->GetActorLocation() - GetActorLocation();
	if (State == EState::Waiting)
	{
		// she watches the way the player comes; the moment she is clearly seen she starts talking and comes after them
		if (To.Size2D() < NoticeRange * 1.5f) SetActorRotation(FRotator(0.0f, To.Rotation().Yaw, 0.0f));
		SeenTime = PlayerSees(NoticeDot) ? SeenTime + DeltaSeconds : 0.0f;
		if (SeenTime > 0.25f)
		{
			NextLineAt = Now + SpeakNextLine() + CampaignLineInterval;
			++CampaignLines;
			State = EState::Pursuing; StateTime = 0.0f;
			UE_LOG(LogP9, Log, TEXT("Silhouette: Kanchana was seen (%.0f m) - she follows"), To.Size() / 100.0f);
		}
		return;
	}
	bool bTalking = Voice->IsPlaying();
	if (!bTalking && Now >= NextLineAt && To.Size() < CampaignTalkRange)
	{
		NextLineAt = Now + SpeakNextLine() + CampaignLineInterval;
		++CampaignLines;
		bTalking = true;
	}
	FollowStep(Player, PlayerSpeed, DeltaSeconds, bTalking);
}

void AP9SilhouetteStalker::FollowStep(const APawn* Player, float PlayerSpeed, float DeltaSeconds, bool bTalking)
{
	// Stage 6B: she walks the player's own path. Off it, she takes only a straight way proven clear; otherwise she watches,
	// and once the player is far and not looking she is suddenly on the path behind them.
	if (!Trail.IsValidIndex(TrailCursor) && (JoinCheckTime -= DeltaSeconds) <= 0.0f)
	{
		JoinCheckTime = 0.3f;
		JoinTrail();
	}
	const float Behind = TrailDistanceToPlayer(Player);
	if ((Behind > FollowDistance * 1.8f || (!Trail.IsValidIndex(TrailCursor) && Behind > FollowDistance * 1.2f)) && !PlayerSees(0.2f))
	{
		if (PlaceOnTrailBehind(Player, false)) return;
	}
	bool bMoved = false;
	if (Behind > FollowDistance * 0.4f)
	{
		// close in at her own pace, or keep up with the player (never faster than a hurried walk)
		const float MoveSpeed = FMath::Clamp(FMath::Max(PlayerSpeed, WalkClipSpeed), WalkClipSpeed, 320.0f);
		bMoved = MoveAlongTrail(Player, MoveSpeed, DeltaSeconds, FollowDistance * 0.4f);
		if (bMoved) PlayMove(MoveSpeed, DeltaSeconds);                              // Stage 7B: walk / stairs up / stairs down
	}
	if (!bMoved)
	{
		SetActorRotation(FRotator(0.0f, (Player->GetActorLocation() - GetActorLocation()).GetSafeNormal2D().Rotation().Yaw, 0.0f));
		PlayStand(bTalking && TalkAnim ? TalkAnim.Get() : (MenaceAnim ? MenaceAnim.Get() : IdleAnim.Get()));
	}
}

void AP9SilhouetteStalker::LogStatus() const
{
	static const TCHAR* S[] = {TEXT("Hidden"), TEXT("Following"), TEXT("Speaking"), TEXT("Lingering"), TEXT("Waiting"), TEXT("Pursuing")};
	UE_LOG(LogP9, Log, TEXT("Silhouette: %s t=%.1f force=%d dayActive=%d today=%d/%d next=%.0f loc=%s lines=%d director=%s lock=%d epilogue=%d phaseAllows=%d campaignSighting=%d spoken=%d trail=%d cursor=%d joins=%d catchUps=%d"), S[(int32)State], StateTime,
		bForceActive, bDayActive, AppearancesToday, MaxAppearancesPerDay, FMath::Max(0.0f, NextAllowed - Now), *GetActorLocation().ToCompactString(), Lines.Num(),
		Director ? TEXT("yes") : TEXT("no"), bHideUntilCampaignComplete ? 1 : 0, bAppearInEpilogue ? 1 : 0, PhaseAllows() ? 1 : 0, bCampaignSighting ? 1 : 0, CampaignLines,
		Trail.Num(), TrailCursor, TrailJoins, TrailCatchUps);
}
