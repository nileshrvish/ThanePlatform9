#include "P9FriendNPC.h"

#include "P9CampaignDirector.h"
#include "P9NPCAnimInstance.h"
#include "P9NPCLocomotion.h"
#include "P9SeatComponent.h"
#include "P9Train.h"
#include "ThanePlatform9.h"
#include "TimerManager.h"
#include "Animation/AnimSequence.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"
#include "Sound/SoundBase.h"

TMap<FName, TWeakObjectPtr<AP9FriendNPC>> AP9FriendNPC::Speakers;
TArray<TWeakObjectPtr<UP9SeatComponent>> AP9FriendNPC::ClaimedSeats;

namespace P9Friends
{
	constexpr float CapsuleRadius = 32.0f;
	constexpr float CapsuleHalfHeight = 90.0f;

	static FAutoConsoleCommandWithWorld StatusCommand(
		TEXT("p9.Friends.Status"),
		TEXT("p9.Friends.Status - log every station NPC: role, activity, clip, location"),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			int32 Count = 0;
			for (TActorIterator<AP9FriendNPC> It(World); It; ++It)
			{
				It->LogStatus();
				++Count;
			}
			UE_LOG(LogP9, Log, TEXT("p9.Friends.Status: %d NPCs"), Count);
		}));

	// Stage 9C: test helper - an NPC with a Sit stop goes and sits there now (or stands up if seated)
	static FAutoConsoleCommandWithWorldAndArgs SitCommand(
		TEXT("p9.Friends.Sit"),
		TEXT("p9.Friends.Sit <name|key> - that NPC walks to its route's Sit stop and sits (stands up if already seated); test helper"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			for (TActorIterator<AP9FriendNPC> It(World); It; ++It)
			{
				if (Args.IsEmpty() || It->FriendName.Equals(Args[0], ESearchCase::IgnoreCase) || It->FriendKey.ToString().Equals(Args[0], ESearchCase::IgnoreCase)) It->DebugSit();
			}
		}));
	// Stage 9C: live tuning of the bench seat depth (pelvis behind the seat front, cm); -1 = each NPC's SitSeatInset
	static TAutoConsoleVariable<float> CVarSitInset(TEXT("p9.Friends.SitInset"), -1.0f,
		TEXT("p9.Friends.SitInset <cm> - pelvis distance behind the seat front edge for bench sitting (-1 = per-NPC SitSeatInset)"));

	// Stage 8A: box-overlap check of every visible NPC body and the player (test helper; logs only)
	static FAutoConsoleCommandWithWorld OverlapCommand(
		TEXT("p9.Friends.Overlaps"),
		TEXT("p9.Friends.Overlaps - how many visible NPC bodies (and the player) overlap as boxes right now, and the deepest overlap"),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			float Depth = 0.0f; FString Pairs;
			const int32 N = P9NPCLoco::CountBoxOverlaps(World, Depth, &Pairs);
			UE_LOG(LogP9, Log, TEXT("p9.Friends.Overlaps: %d overlapping pairs, deepest %.1f cm%s"), N, Depth, *Pairs);
		}));

	// Stage 5B: -1 = follow the campaign (default), 0 = keep locked NPCs hidden, 1 = show them (test)
	static TAutoConsoleVariable<int32> CVarForceUnlock(TEXT("p9.Friends.ForceUnlock"), -1,
		TEXT("p9.Friends.ForceUnlock -1|0|1 - NPCs hidden until the campaign is completed: -1 follow the campaign, 0 force hidden, 1 force available (test)"));
	constexpr float AvailabilityPollSeconds = 0.5f;

	static bool IsLoopClip(EP9NPCClip C) { return C != EP9NPCClip::SitDown && C != EP9NPCClip::StandUp; }
	static bool IsWalkClip(EP9NPCClip C) { return C == EP9NPCClip::Walk || C == EP9NPCClip::DrunkWalk || C == EP9NPCClip::StairsUp || C == EP9NPCClip::StairsDown; }
	static bool IsStairClip(EP9NPCClip C) { return C == EP9NPCClip::StairsUp || C == EP9NPCClip::StairsDown; }
	// Stage 7B separation
	constexpr float BodyGap = 10.0f;          // cm kept between two bodies when passing
	constexpr float StandGap = 4.0f;          // cm between standing bodies
	constexpr float StandPushSpeed = 40.0f;   // cm/s at which standing NPCs ease apart
	constexpr float HoldSeconds = 2.5f;       // a walker waits this long behind a body before squeezing past slowly
	// Stage 5C crossfade times: sit transitions start/end in the neighbouring clip's pose, so they need only a short blend
	static float BlendFor(EP9NPCClip From, EP9NPCClip To)
	{
		if (From == EP9NPCClip::None) return 0.0f;
		// Stage 9C: sitting down starts from a settled stand (0.3 s), lands in the seated loop (0.25 s); standing up leaves the
		// seated loop (0.25 s) and settles into the stand (0.35 s) before anything else (was 0.15 s everywhere - a visible pop)
		if (To == EP9NPCClip::SitDown) return 0.3f;
		if (From == EP9NPCClip::SitDown) return 0.25f;
		if (To == EP9NPCClip::StandUp) return 0.25f;
		if (From == EP9NPCClip::StandUp) return 0.35f;
		// Stage 9A: stopping (walk -> standing clip) settles over 0.45 s while the last stride slows (SetOutgoingRateDecay);
		// setting off 0.35 s while the speed ramps up; walk <-> stairs stays quick (same gait family)
		if (IsWalkClip(From) && IsWalkClip(To)) return 0.25f;
		if (IsWalkClip(From)) return 0.45f;
		if (IsWalkClip(To)) return 0.35f;
		return 0.35f;
	}
	// Stage 5C walking (9A: arrivals ease down to a near stop; the clip rate follows, so the feet do not slide)
	constexpr float WalkAccel = 200.0f;      // cm/s^2 when setting off
	constexpr float WalkDecel = 170.0f;      // cm/s^2 when arriving
	constexpr float MinArriveSpeed = 12.0f;
	// Stage 9A on-the-spot turns: angular acceleration and the turn clip's rate range
	constexpr float TurnAccel = 320.0f;      // deg/s^2
	constexpr float MinMoveClipRate = 0.12f;
	// AP9Train layout (car-local, cm), as used by the campaign story for the same train
	constexpr float TrainFloor = 110.0f;
	constexpr float CarCentreX = 1066.5f;
	constexpr float DoorX[3] = {-690.0f, 0.0f, 690.0f};
	static bool WaitsForLoopEnd(EP9NPCClip C)
	{
		return C == EP9NPCClip::Look || C == EP9NPCClip::Chat || C == EP9NPCClip::Gesture || C == EP9NPCClip::Pour;
	}
}

AP9FriendNPC::AP9FriendNPC()
{
	PrimaryActorTick.bCanEverTick = true;
	Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	Capsule->InitCapsuleSize(P9Friends::CapsuleRadius, P9Friends::CapsuleHalfHeight);
	Capsule->SetCanEverAffectNavigation(false);
	Capsule->CanCharacterStepUpOn = ECB_No;
	SetRootComponent(Capsule);

	BodyBox = CreateDefaultSubobject<UBoxComponent>(TEXT("BodyBox"));             // Stage 8A hybrid body
	BodyBox->SetupAttachment(Capsule);
	BodyBox->SetCanEverAffectNavigation(false);
	BodyBox->CanCharacterStepUpOn = ECB_No;
	BodyBox->SetHiddenInGame(true);

	Body = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Capsule);
	Body->SetRelativeLocation(FVector(0.0f, 0.0f, -P9Friends::CapsuleHalfHeight));
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetGenerateOverlapEvents(false);
	Body->SetCanEverAffectNavigation(false);
	Body->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	Body->bReceivesDecals = false;
	Body->bComponentUseFixedSkelBounds = true;     // the mesh's import bounds (the clips stay within them); cheaper than physics-asset bounds
	Body->bEnableUpdateRateOptimizations = true;   // fewer pose updates when small on screen (mobile)

	Voice = CreateDefaultSubobject<UAudioComponent>(TEXT("Voice"));
	Voice->SetupAttachment(Capsule);
	Voice->SetRelativeLocation(FVector(0.0f, 0.0f, 70.0f));
	Voice->bAutoActivate = false;
	Voice->bOverrideAttenuation = true;
	Voice->AttenuationOverrides.bAttenuate = true;
	Voice->AttenuationOverrides.bSpatialize = true;
	Voice->AttenuationOverrides.AttenuationShape = EAttenuationShape::Sphere;
	Voice->AttenuationOverrides.AttenuationShapeExtents = FVector(250.0f, 0.0f, 0.0f);
	Voice->AttenuationOverrides.FalloffDistance = 1800.0f;
	DisableDebugDrawing();
	ApplyCollision();
}

void AP9FriendNPC::ApplyHeldProps()
{
	// Stage 5C: hand props are created at runtime only for NPCs that carry one (an empty static mesh component on every
	// friend would be flagged by Map Check); no collision, no shadow from tiny objects on mobile
	auto Fit = [this](TObjectPtr<UStaticMeshComponent>& C, UStaticMesh* M, FName Socket, const FTransform& Off, const TCHAR* Name)
	{
		if (!M || !Body->DoesSocketExist(Socket))
		{
			if (C) { C->DestroyComponent(); C = nullptr; }
			return;
		}
		if (!C)
		{
			C = NewObject<UStaticMeshComponent>(this, Name, RF_Transient);
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			C->SetGenerateOverlapEvents(false);
			C->SetCanEverAffectNavigation(false);
			C->SetCastShadow(false);
			C->bReceivesDecals = false;
			C->SetupAttachment(Body, Socket);
			C->RegisterComponent();
		}
		C->SetStaticMesh(M);
		C->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket);
		C->SetRelativeTransform(Off);
	};
	Fit(HeldRight, HeldMeshRight, HeldSocketRight, HeldOffsetRight, TEXT("HeldRight"));
	Fit(HeldLeft, HeldMeshLeft, HeldSocketLeft, HeldOffsetLeft, TEXT("HeldLeft"));
}

void AP9FriendNPC::DisableDebugDrawing()
{
	// bones / skeleton overlays must never appear in play (editor "Display Bones" flag and the runtime debug-skeleton flag)
#if WITH_EDITORONLY_DATA
	Body->bDisplayBones = false;
#endif
#if UE_ENABLE_DEBUG_DRAWING
	Body->SetDrawDebugSkeleton(false);
#endif
	Body->bDisplayDebugUpdateRateOptimizations = false;
}

void AP9FriendNPC::ApplyCollision()
{
	// Stage 7B: a body-sized capsule (the actor origin stays 90 cm above the feet). Query-only Pawn that OVERLAPS pawns only:
	// it never blocks the player, trains, doors or stairs, ignores Visibility/Camera traces (anomaly checks, scares, ledge
	// climbing) and generates no overlap events (campaign triggers never see it). NPC-to-NPC separation is done in code.
	Capsule->SetCapsuleSize(BodyRadius, FMath::Max(BodyRadius, BodyHalfHeight));
	Capsule->SetCollisionObjectType(ECC_Pawn);
	Capsule->SetCollisionResponseToAllChannels(ECR_Ignore);
	Capsule->SetGenerateOverlapEvents(false);
	Capsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Capsule->SetCollisionResponseToChannel(ECC_Pawn, bBlockPlayer ? ECR_Block : ECR_Overlap);
	// Stage 8A: the body-aligned box (same rules as the capsule: query-only, overlaps pawns only, no events)
	if (BodyBox)
	{
		BodyBox->SetBoxExtent(FVector(BodyRadius * BoxDepthScale, BodyRadius * BoxWidthScale, FMath::Max(BodyRadius, BodyHalfHeight)));
		BodyBox->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
		BodyBox->SetCollisionObjectType(ECC_Pawn);
		BodyBox->SetCollisionResponseToAllChannels(ECR_Ignore);
		BodyBox->SetGenerateOverlapEvents(false);
		BodyBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		BodyBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	}
}

void AP9FriendNPC::ApplyAssets()
{
	if (FriendMesh && Body->GetSkeletalMeshAsset() != FriendMesh)
	{
		Body->SetSkeletalMeshAsset(FriendMesh);
	}
	Body->SetAnimationMode(EAnimationMode::AnimationSingleNode);
}

void AP9FriendNPC::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyCollision();
	ApplyAssets();
	DisableDebugDrawing();
	if (IdleAnim)
	{
		Body->SetAnimation(IdleAnim);   // the editor shows the relaxed first frame, not the bind pose
		Body->SetPosition(0.0f, false);
	}
}

void AP9FriendNPC::BeginPlay()
{
	Super::BeginPlay();
	ApplyCollision();
	ApplyAssets();
	DisableDebugDrawing();
	InitAnimInstance();
	ApplyHeldProps();
	Rng.Initialize(Seed * 7919 + GetTypeHash(GroupId));
	Home = GetActorLocation();
	HomeYaw = GetActorRotation().Yaw;
	TargetYaw = HomeYaw;
	PlayRate = FMath::Clamp(PlayRate * Rng.FRandRange(0.94f, 1.06f), 0.5f, 1.5f);
	ValidateRouteTargets();                                                         // Stage 9A
	StartRoutine();
	UE_LOG(LogP9, Log, TEXT("NPC %s (%s, role %d): ready at %s, mesh %s, route %d points"), *FriendName, *FriendKey.ToString(), (int32)NPCRole,
		*GetActorLocation().ToCompactString(), FriendMesh ? *FriendMesh->GetName() : TEXT("none"), Route.Num());

	// Stage 5B: read-only link to the campaign director (day starts for the vendor's daily line, status for availability)
	for (TActorIterator<AP9CampaignDirector> It(GetWorld()); It; ++It) { Director = *It; break; }
	if (Director)
	{
		Director->OnDayStarted.AddDynamic(this, &AP9FriendNPC::HandleDayStarted);
	}
	UpdateAvailability();
	GetWorldTimerManager().SetTimer(AvailabilityTimer, this, &AP9FriendNPC::UpdateAvailability, P9Friends::AvailabilityPollSeconds, true);
	P9NPCLoco::RegisterBody(this, BodyRadius, BodyHalfHeight, P9Friends::CapsuleHalfHeight, BodyRadius * BoxDepthScale, BodyRadius * BoxWidthScale);   // Stage 7B / 8A box
}

void AP9FriendNPC::InitAnimInstance()
{
	// Stage 5C: crossfading clip player instead of the single-node PlayAnimation switch
	Body->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	Body->SetAnimInstanceClass(UP9NPCAnimInstance::StaticClass());
	AnimInst = Cast<UP9NPCAnimInstance>(Body->GetAnimInstance());
	if (!AnimInst)
	{
		UE_LOG(LogP9, Warning, TEXT("NPC %s: crossfade anim instance missing - falling back to single-node clips"), *FriendName);
		Body->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	}
}

void AP9FriendNPC::StartRoutine()
{
	// fresh routine from the start point (BeginPlay, and when a hidden NPC becomes available again)
	if (Activity == EP9FriendActivity::Train || RideSeat.IsValid())
	{
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		ClaimedSeats.Remove(RideSeat);
	}
	RideSeat = nullptr; RideTrain = nullptr; TrainPhase = 0; TrainExit.Reset(); bExplicitZ = false; CurSpeed = 0.0f; SpeedScale = 1.0f;
	if (Activity == EP9FriendActivity::Scripted) DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	WakeProxy = nullptr; ScriptPhase = 0;
	Path.Reset();
	PathIdx.Reset();
	bStrictPath = false; bEscaping = false; PivotPhase = 0; WatchTime = 0.0f;   // Stage 9A
	TargetIndex = INDEX_NONE;
	YieldTime = 0.0f;
	DwellLeft = 0.0f;
	bReturning = false;
	MeetRouteIndex = INDEX_NONE;
	bMeetAfterStand = false;
	SitPhase = 0;
	bLeaveAfterTurn = false;
	bMeetArrived = false;
	EndMeetDelay = -1.0f;
	SpeakLeft = 0.0f;
	bTurnedToSpeaker = false;
	Activity = EP9FriendActivity::Idle;
	CurrentClip = EP9NPCClip::None;
	PendingClip = EP9NPCClip::None;
	// Stage 7B: a construction rerun (editor property edits while playing) puts the body back in single-node mode; make sure
	// the crossfade clip player is the live one again before the routine restarts
	if (!AnimInst || Body->GetAnimInstance() != AnimInst) InitAnimInstance();
	SetActorRotation(FRotator(0.0f, HomeYaw, 0.0f));
	TargetYaw = HomeYaw;

	RouteIndex = Route.IsValidIndex(StartIndex) ? StartIndex : 0;
	if (Route.IsValidIndex(RouteIndex))
	{
		const FVector P = Route[RouteIndex].Location;
		SetActorLocation(FVector(P.X, P.Y, P.Z + P9Friends::CapsuleHalfHeight));
		Home = GetActorLocation();
	}
	SetClip(EP9NPCClip::Idle);
	UpdateClip(0.0f);
	if (IdleAnim)
	{
		ClipClock = Rng.FRandRange(0.0f, IdleAnim->GetPlayLength());          // not in lock-step
		if (AnimInst) AnimInst->SetTime(ClipClock); else Body->SetPosition(ClipClock, false);
	}
	NextSpeechTime = WorldTime + Rng.FRandRange(5.0f, 15.0f);
	if (Route.Num() > 1)
	{
		CurrentStop = Route[RouteIndex].Stop;
		DwellLeft = Rng.FRandRange(2.0f, 8.0f);
		ArriveAtStop();
	}
	else
	{
		StartActivity(EP9FriendActivity::Idle, Rng.FRandRange(1.0f, MinActivitySeconds));
	}
}

// ---------------------------------------------------------------------------------------------------------------- availability / days (Stage 5B)
void AP9FriendNPC::UpdateAvailability()
{
	bool bAvailable = true;
	if (bHideUntilCampaignComplete)
	{
		const int32 Force = P9Friends::CVarForceUnlock.GetValueOnGameThread();
		if (Force >= 0)
		{
			bAvailable = Force != 0;
		}
		else if (Director)
		{
			// Status becomes Completed in the first statement block of StartEpilogue (same frame the phase becomes Epilogue);
			// a new run (StartNewRun) sets it back to InProgress
			const EP9DayPhase Phase = Director->GetPhase();
			bAvailable = Director->GetStatus() == EP9CampaignStatus::Completed || Phase == EP9DayPhase::Epilogue || Phase == EP9DayPhase::Ending;
		}
	}
	SetNPCAvailable(bAvailable);
}

void AP9FriendNPC::SetNPCAvailable(bool bAvailable)
{
	if (bAvailable == bAvailableNow)
	{
		return;
	}
	bAvailableNow = bAvailable;
	SetActorHiddenInGame(!bAvailable);
	SetActorTickEnabled(bAvailable);
	Body->SetComponentTickEnabled(bAvailable);           // no pose / animation work while hidden
	if (!bAvailable)
	{
		Voice->Stop();
		if (TWeakObjectPtr<AP9FriendNPC>* S = Speakers.Find(GroupId); S && S->Get() == this)
		{
			Speakers.Remove(GroupId);
		}
	}
	else
	{
		StartRoutine();
	}
	UE_LOG(LogP9, Log, TEXT("NPC %s: %s (director day %d phase %d status %d)"), *FriendName, bAvailable ? TEXT("AVAILABLE") : TEXT("hidden until the campaign is completed"),
		Director ? Director->GetDay() : 0, Director ? (int32)Director->GetPhase() : -1, Director ? (int32)Director->GetStatus() : -1);
}

void AP9FriendNPC::HandleDayStarted(int32 Day)
{
	// Stage 9A: a day starting while the vendor still plays the epilogue scene (a new run / a restarted day) - the day change is a
	// fade and teleport, so he is put back behind his counter unseen instead of being left wherever the scene stopped
	if (Activity == EP9FriendActivity::Scripted && !EpilogueReturnPath.IsEmpty())
	{
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		const FVector Stall = EpilogueReturnPath.Last();
		SetActorLocationAndRotation(FVector(Stall.X, Stall.Y, Stall.Z + P9Friends::CapsuleHalfHeight), FRotator(0.0f, HomeYaw, 0.0f));
		Path.Reset(); PathIdx.Reset(); ScriptPhase = 0; WakeProxy = nullptr; CurSpeed = 0.0f; bExplicitZ = false;
		Home = GetActorLocation(); TargetYaw = HomeYaw;
		StartActivity(EP9FriendActivity::Idle, Rng.FRandRange(2.0f, 4.0f));
		UE_LOG(LogP9, Log, TEXT("NPC %s: day %d started during the epilogue scene - back behind his counter"), *FriendName, Day);
	}
	if (!bOncePerCampaignDay)
	{
		return;
	}
	const bool bWasSpeaking = Activity == EP9FriendActivity::Speak;
	bSpokeToday = false;
	NextSpeechTime = WorldTime;                          // the cooldown does not carry over into the new day
	if (bWasSpeaking)
	{
		Voice->Stop();                                   // scene change (fade + teleport): like the station announcements
		StartActivity(EP9FriendActivity::Idle, Rng.FRandRange(2.0f, 4.0f));
	}
	UE_LOG(LogP9, Log, TEXT("NPC %s: day %d started - daily line available again%s"), *FriendName, Day, bWasSpeaking ? TEXT(" (speech cut by the day change)") : TEXT(""));
}

void AP9FriendNPC::EndPlay(const EEndPlayReason::Type Reason)
{
	GetWorldTimerManager().ClearTimer(AvailabilityTimer);
	P9NPCLoco::UnregisterBody(this);
	if (Director)
	{
		Director->OnDayStarted.RemoveDynamic(this, &AP9FriendNPC::HandleDayStarted);
	}
	if (TWeakObjectPtr<AP9FriendNPC>* S = Speakers.Find(GroupId); S && S->Get() == this)
	{
		Speakers.Remove(GroupId);
	}
	Super::EndPlay(Reason);
}

// ---------------------------------------------------------------------------------------------------------------- clips
UAnimSequence* AP9FriendNPC::ClipAnim(EP9NPCClip Clip) const
{
	switch (Clip)
	{
	case EP9NPCClip::Walk: return WalkAnim;
	case EP9NPCClip::Turn: return TurnAnim ? TurnAnim.Get() : IdleAnim.Get();
	case EP9NPCClip::Look: return LookAroundAnim ? LookAroundAnim.Get() : IdleAnim.Get();
	case EP9NPCClip::Chat: return ChatAnim ? ChatAnim.Get() : IdleAnim.Get();
	case EP9NPCClip::SitDown: return SitDownAnim;
	case EP9NPCClip::SitIdle: return SitIdleAnim;
	case EP9NPCClip::StandUp: return StandUpAnim;
	case EP9NPCClip::Gesture: return GestureAnim ? GestureAnim.Get() : ChatAnim.Get();
	case EP9NPCClip::Pour: return PourAnim ? PourAnim.Get() : IdleAnim.Get();
	case EP9NPCClip::DrunkWalk: return DrunkWalkAnim ? DrunkWalkAnim.Get() : WalkAnim.Get();
	case EP9NPCClip::StairsUp: return StairsUpAnim ? StairsUpAnim.Get() : WalkAnim.Get();
	case EP9NPCClip::StairsDown: return StairsDownAnim ? StairsDownAnim.Get() : WalkAnim.Get();
	default: return IdleAnim;
	}
}

EP9NPCClip AP9FriendNPC::MoveClip() const
{
	// Stage 5G/7B: the walking clip for the moment - stairs up/down where the floor rises/falls, drunk on drunk legs, else the walk
	if (StairDir > 0 && StairsUpAnim) return EP9NPCClip::StairsUp;
	if (StairDir < 0 && StairsDownAnim) return EP9NPCClip::StairsDown;
	if (bDrunkLeg && DrunkWalkAnim && Activity == EP9FriendActivity::Walk) return EP9NPCClip::DrunkWalk;
	return EP9NPCClip::Walk;
}

void AP9FriendNPC::SetClip(EP9NPCClip Clip)
{
	if (Clip == EP9NPCClip::Walk) Clip = MoveClip();
	PendingClip = Clip;
}

bool AP9FriendNPC::OneShotDone() const
{
	const UAnimSequence* A = ClipAnim(CurrentClip);
	return !A || ClipClock >= A->GetPlayLength() - 0.02f;
}

void AP9FriendNPC::UpdateClip(float DeltaSeconds)
{
	// clip timing runs on our own clock (pose ticking stops for NPCs that are not rendered, the behaviour must not)
	bool bWrapped = false;
	if (const UAnimSequence* A = ClipAnim(CurrentClip))
	{
		ClipClock += DeltaSeconds * PlayRate;
		const float Len = FMath::Max(0.05f, A->GetPlayLength());
		if (P9Friends::IsLoopClip(CurrentClip) && ClipClock >= Len) { ClipClock = FMath::Fmod(ClipClock, Len); bWrapped = true; }
	}
	if (PendingClip != EP9NPCClip::None && PendingClip != CurrentClip)
	{
		bool bCanSwitch = CurrentClip == EP9NPCClip::None;
		if (!bCanSwitch)
		{
			if (!P9Friends::IsLoopClip(CurrentClip)) bCanSwitch = OneShotDone();                 // sit transitions play to the end
			else if (P9Friends::WaitsForLoopEnd(CurrentClip)) bCanSwitch = bWrapped;               // gestures change at their loop end
			else bCanSwitch = true;                                                                    // idle/walk/turn/sit-idle: near the rest pose
		}
		UAnimSequence* Anim = ClipAnim(PendingClip);
		if (bCanSwitch && Anim)
		{
			const EP9NPCClip From = CurrentClip;
			CurrentClip = PendingClip;
			if (AnimInst)
			{
				AnimInst->PlayClip(Anim, P9Friends::IsLoopClip(CurrentClip), P9Friends::BlendFor(From, CurrentClip));
				AnimInst->SetRate(PlayRate);
				// Stage 9A: a walker stopping - the outgoing stride slows to rest during the blend instead of running on
				if (P9Friends::IsWalkClip(From) && !P9Friends::IsWalkClip(CurrentClip)) AnimInst->SetOutgoingRateDecay(LastClipRate);
			}
			else
			{
				Body->PlayAnimation(Anim, P9Friends::IsLoopClip(CurrentClip));
				Body->SetPlayRate(PlayRate);
			}
			ClipClock = 0.0f;
		}
		else if (!Anim) PendingClip = CurrentClip;   // missing clip: keep what plays
	}
}

// ---------------------------------------------------------------------------------------------------------------- helpers
TArray<AP9FriendNPC*> AP9FriendNPC::GroupMembers() const
{
	TArray<AP9FriendNPC*> Out;
	for (TActorIterator<AP9FriendNPC> It(GetWorld()); It; ++It)
	{
		if (It->GroupId == GroupId) Out.Add(*It);
	}
	return Out;
}

FVector AP9FriendNPC::GetGroupCenter() const
{
	const TArray<AP9FriendNPC*> Members = GroupMembers();
	FVector Sum = FVector::ZeroVector;
	for (const AP9FriendNPC* M : Members) Sum += M->Home;
	return Members.Num() ? Sum / Members.Num() : GetActorLocation();
}

AP9FriendNPC* AP9FriendNPC::GroupSpeaker() const
{
	const TWeakObjectPtr<AP9FriendNPC>* S = Speakers.Find(GroupId);
	return S ? S->Get() : nullptr;
}

float AP9FriendNPC::YawTo(const FVector& Point) const
{
	return (Point - GetActorLocation()).GetSafeNormal2D().Rotation().Yaw;
}

bool AP9FriendNPC::CameraNear(float Distance) const
{
	const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0);
	return Cam && FVector::Dist(Cam->GetCameraLocation(), GetActorLocation()) < Distance;
}

bool AP9FriendNPC::PlayerBlocksWay() const
{
	const APawn* P = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!P || Path.IsEmpty()) return false;
	const FVector To = P->GetActorLocation() - GetActorLocation();
	const FVector Dir = (Path[0] - GetActorLocation()).GetSafeNormal2D();
	return FMath::Abs(To.Z) < 150.0f && To.Size2D() < 140.0f && FVector::DotProduct(To.GetSafeNormal2D(), Dir) > 0.5f;
}

bool AP9FriendNPC::TurnTowardsYaw(float Yaw, float DeltaSeconds)
{
	const float Current = GetActorRotation().Yaw;
	const float Delta = FRotator::NormalizeAxis(Yaw - Current);
	// Stage 9A: a turn starts from rest, accelerates to TurnDegreesPerSecond and decelerates into the final facing
	// (v <= sqrt(2 a d)): no snap at the start, no stop dead at the end, and no instant 180-degree flips
	if (WorldTime - LastTurnCall > 0.15f) TurnRate = 0.0f;
	LastTurnCall = WorldTime;
	const float A = P9Friends::TurnAccel;
	const float Dir = FMath::Sign(Delta);
	if (TurnRate * Dir < 0.0f) TurnRate = 0.0f;                                   // the target swapped sides: turn the other way from rest
	const float VMax = FMath::Min(TurnDegreesPerSecond, FMath::Sqrt(2.0f * A * FMath::Abs(Delta)) + 8.0f);
	const float V = FMath::Min(FMath::Abs(TurnRate) + A * DeltaSeconds, VMax);
	const float Step = V * DeltaSeconds;
	if (FMath::Abs(Delta) <= FMath::Max(Step, 0.3f))
	{
		SetActorRotation(FRotator(0.0f, Yaw, 0.0f));
		TurnRate = 0.0f;
		return true;
	}
	TurnRate = Dir * V;
	SetActorRotation(FRotator(0.0f, Current + Dir * Step, 0.0f));
	return false;
}

int32 AP9FriendNPC::RouteIndexNear(const FVector& Center, float MaxDistance) const
{
	int32 Best = INDEX_NONE;
	float BestD = MaxDistance;
	for (int32 i = 0; i < Route.Num(); ++i)
	{
		const float D = FVector::Dist(Route[i].Location, Center);
		if (D < BestD) { BestD = D; Best = i; }
	}
	return Best;
}

float AP9FriendNPC::RouteDistanceTo(const FVector& Point) const
{
	const int32 Near = RouteIndexNear(Point, 1.0e7f);
	if (!Route.IsValidIndex(Near) || !Route.IsValidIndex(RouteIndex)) return FVector::Dist(GetActorLocation(), Point);
	float D = FVector::Dist(GetActorLocation(), Route[RouteIndex].Location + FVector(0, 0, 90));
	const int32 Step = Near >= RouteIndex ? 1 : -1;
	for (int32 i = RouteIndex; i != Near; i += Step) D += FVector::Dist(Route[i].Location, Route[i + Step].Location);
	return D + FVector::Dist(Route[Near].Location, Point);
}

TArray<FVector> AP9FriendNPC::RoutePathTo(int32 Target) const
{
	// along the polyline, index by index: stairs and corridors between points are respected
	TArray<FVector> Out;
	PathIdx.Reset();
	if (!Route.IsValidIndex(Target)) return Out;
	const int32 Step = Target >= RouteIndex ? 1 : -1;
	for (int32 i = RouteIndex + Step; Step > 0 ? i <= Target : i >= Target; i += Step)
	{
		Out.Add(Route[i].Location);
		PathIdx.Add(i);
	}
	return Out;
}

void AP9FriendNPC::WalkPath(const TArray<FVector>& Points)
{
	if (Activity != EP9FriendActivity::Walk) CurSpeed = 0.0f;        // sets off from standing
	Path = Points;
	YieldTime = 0.0f;
	if (Path.IsEmpty())
	{
		ArriveAtStop();
		return;
	}
	// a turn on the spot first if the way out is behind
	TargetYaw = YawTo(Path[0]);
	if (FMath::Abs(FRotator::NormalizeAxis(TargetYaw - GetActorRotation().Yaw)) > 60.0f && TurnAnim)
	{
		bLeaveAfterTurn = true;
		StartActivity(EP9FriendActivity::Turn, 3.0f);
		return;
	}
	StartActivity(EP9FriendActivity::Walk, 120.0f);
}

bool AP9FriendNPC::StepAlongPath(float DeltaSeconds)
{
	if (Path.IsEmpty()) return true;
	const FVector Loc = GetActorLocation();
	FVector To = Path[0] - Loc; To.Z = 0.0f;
	const float Dist = To.Size();
	if (Dist <= 1.0f && !bExplicitZ)                                              // Stage 9A: already there - arrive before any floor / body checks
	{
		if (PathIdx.Num()) { if (PathIdx[0] != INDEX_NONE) RouteIndex = PathIdx[0]; PathIdx.RemoveAt(0); }
		Path.RemoveAt(0);
		bStrictPath = false; bEscaping = false;
		return Path.IsEmpty();
	}
	// Stage 7B natural locomotion: pure pursuit - the NPC aims at a point ahead along its path (further at walking pace, short
	// on stairs), so it rounds path corners in smooth arcs instead of turning sharply at each point. A corner cut that would
	// clip a wall (stair heads, closures) falls back to the straight segment.
	const FVector SegDir = Dist > KINDA_SMALL_NUMBER ? To / Dist : GetActorForwardVector();
	const float Look = StairDir != 0 ? 25.0f : FMath::Clamp(CurSpeed * 0.55f, 35.0f, 100.0f);
	FVector Dir = SegDir;
	if (!bExplicitZ && !bStrictPath)                                             // Stage 9A: recovery legs follow the straight segment
	{
		FVector Aim = PursuitPoint(Loc, Look) - Loc; Aim.Z = 0.0f;
		if (Aim.SizeSquared() > 1.0f) Dir = Aim.GetSafeNormal();
		if (Path.Num() > 1 && FVector::DotProduct(Dir, SegDir) < 0.985f)
		{
			FCollisionQueryParams CP(SCENE_QUERY_STAT(P9FriendCorner), false, this);
			const FVector F0 = Loc - FVector(0, 0, P9Friends::CapsuleHalfHeight);
			if (P9NPCLoco::MoveHitsWorld(GetWorld(), F0, F0 + Dir * (BodyRadius + 30.0f), BodyRadius, CP)) Dir = SegDir;
		}
	}
	// Stage 7B: steer round / slow behind other NPC bodies (not inside the train)
	float AvoidScale = 1.0f;
	FVector MoveDir = Dir;
	if (bAvoidOthers && !bExplicitZ && Activity != EP9FriendActivity::Train)
	{
		const FVector Steer = AvoidanceSteer(Dir, AvoidScale, DeltaSeconds);
		SteerSmooth = FMath::VInterpTo(SteerSmooth, Steer, DeltaSeconds, 5.0f);   // eased: passing is a curve, not a twitch
		if (!SteerSmooth.IsNearlyZero(0.02f)) MoveDir = (Dir + SteerSmooth).GetSafeNormal2D();
		if (AvoidScale < 0.1f) BlockedTime += DeltaSeconds;
		if (BlockedTime > P9Friends::HoldSeconds) AvoidScale = FMath::Max(AvoidScale, 0.45f);   // squeeze past slowly, never stuck
	}
	const float YawNow = GetActorRotation().Yaw;
	const float YawDelta = FRotator::NormalizeAxis(Dir.Rotation().Yaw - YawNow);
	if (bExplicitZ) StairDir = 0;
	bClimbing = StairDir > 0;                                                     // Stage 5G flag, now from the floor probe
	ClimbSlope = StairDir != 0 ? StairRise / FMath::Max(1.0f, StairTread) : 0.0f;
	float BaseSpeed = WalkSpeed;
	if (StairDir > 0 && StairsUpAnim) BaseSpeed = StairsUpSpeed;
	else if (StairDir < 0 && StairsDownAnim) BaseSpeed = StairsDownSpeed;
	else if (bDrunkLeg && DrunkWalkAnim && Activity == EP9FriendActivity::Walk) BaseSpeed = DrunkWalkSpeed;
	// each person's pace drifts slowly (+-5 %); the clip rate follows the speed, so the feet stay planted
	PacePhase += DeltaSeconds;
	const float Pace = 1.0f + 0.035f * FMath::Sin(PacePhase * 0.37f + Seed * 1.7f) + 0.015f * FMath::Sin(PacePhase * 1.13f + Seed * 0.9f);
	float Target = BaseSpeed * PlayRate * SpeedScale * AvoidScale * (StairDir != 0 ? 1.0f : Pace) * FMath::Lerp(1.0f, 0.55f, FMath::Clamp(FMath::Abs(YawDelta) / 90.0f, 0.0f, 1.0f));
	if (Path.Num() == 1) Target = FMath::Min(Target, P9Friends::MinArriveSpeed + FMath::Sqrt(2.0f * P9Friends::WalkDecel * Dist));
	CurSpeed = CurSpeed < Target ? FMath::Min(Target, CurSpeed + P9Friends::WalkAccel * DeltaSeconds) : FMath::Max(Target, CurSpeed - P9Friends::WalkDecel * 1.5f * DeltaSeconds);
	const float Step = CurSpeed * DeltaSeconds;
	const float FeetZ = Loc.Z - P9Friends::CapsuleHalfHeight;
	const FVector FeetOff(0.0f, 0.0f, P9Friends::CapsuleHalfHeight);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(P9FriendFloor), false, this);
	auto NextFor = [&](const FVector& MD, bool bOnPath)
	{
		if (bOnPath && Dist <= Step && Path.Num() == 1) return FVector(Path[0].X, Path[0].Y, Loc.Z);      // exact arrival at a stop
		return Loc + (bOnPath && Path.Num() == 1 && Dist < Look ? To / FMath::Max(Dist, KINDA_SMALL_NUMBER) : MD) * Step;
	};
	bool bOnPath = MoveDir.Equals(Dir, 1.0e-3f);
	FVector Next = NextFor(MoveDir, bOnPath);
	P9NPCLoco::FFloorProbe Probe;
	if (!bExplicitZ)
	{
		Probe = P9NPCLoco::ProbeFloor(GetWorld(), Next - FeetOff, MoveDir, FeetZ, Params, StairTread);
		// a steering detour must stay on this floor and clear of walls / parapets; otherwise keep to the path
		if (!bOnPath && (!Probe.bCentreFloor || FMath::Abs(Probe.CentreZ - FeetZ) > 40.0f || P9NPCLoco::MoveHitsWorld(GetWorld(), Loc - FeetOff, Next - FeetOff, BodyRadius, Params)))
		{
			MoveDir = Dir; bOnPath = true; SteerSmooth = FVector::ZeroVector; Next = NextFor(Dir, true);
			Probe = P9NPCLoco::ProbeFloor(GetWorld(), Next - FeetOff, MoveDir, FeetZ, Params, StairTread);
		}
	}
	if (bExplicitZ)
	{
		Next.Z = FMath::FInterpConstantTo(Loc.Z, Path[0].Z + P9Friends::CapsuleHalfHeight, DeltaSeconds, 150.0f);   // train floor / door step
	}
	else if (Probe.bHit)
	{
		// Stage 7B: on stairs the root follows the ramp through the tread tops (no riser-high jump at every nosing); floor more
		// than 35 cm up is ignored as before (a bench seat right at a sit stop)
		Next.Z = (bStairClipsFitted && Probe.bOnStairs ? Probe.RampZ : Probe.CentreZ) + P9Friends::CapsuleHalfHeight;
	}
	if (!bExplicitZ && Probe.bOnStairs)
	{
		StairDir = Probe.StairDir; StairOffTime = 0.0f; StairRampZ = Probe.RampZ;
	}
	else if ((StairOffTime += DeltaSeconds) > 0.2f)
	{
		StairDir = 0;
	}
	// Stage 7B hard separation: a step that would overlap another NPC body is pushed out to contact distance (only onto floor
	// at this level, never into a wall); the steering above normally keeps bodies apart before this is needed
	if (bAvoidOthers && !bExplicitZ && Activity != EP9FriendActivity::Train && Probe.bHit)
	{
		TArray<P9NPCLoco::FBody> Others;
		P9NPCLoco::GatherBodies(GetWorld(), this, Others);
		if (APawn* PP = UGameplayStatics::GetPlayerPawn(this, 0))
		{
			P9NPCLoco::FBody PB; PB.Actor = PP; PB.Radius = 34.0f; PB.HalfDepth = 30.0f; PB.HalfWidth = 30.0f;
			PB.FeetOffset = Cast<ACharacter>(PP) ? Cast<ACharacter>(PP)->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.0f;
			Others.Add(PB);                                                    // a walker never steps into the player either
		}
		// Stage 8A: the bodies are boxes aligned with each person (shoulders wider than chest-to-back); the step is tested box
		// against box. Pushed out when it would overlap; if that is impossible (wall / edge / another body) the step is held
		// instead of walking into the other body - only a walker stuck for PassThroughAfter seconds passes (deadlock guard)
		const FVector NF = Next - FeetOff;
		const float MyYaw = GetActorRotation().Yaw, MyD = BodyRadius * BoxDepthScale, MyW = BodyRadius * BoxWidthScale;
		auto Overlap = [&](const FVector& At, FVector& OutPush)
		{
			OutPush = FVector::ZeroVector;
			bool bAny = false;
			for (const P9NPCLoco::FBody& B : Others)
			{
				const FVector BF = B.Actor->GetActorLocation() - FVector(0.0f, 0.0f, B.FeetOffset);
				if (FMath::Abs(BF.Z - At.Z) > 120.0f) continue;
				FVector P;
				if (P9NPCLoco::BoxOverlap2D(At, MyYaw, MyD, MyW, BF, B.Actor->GetActorRotation().Yaw, B.HalfDepth, B.HalfWidth, 2.0f, P))
				{
					OutPush += P; bAny = true;
				}
			}
			return bAny;
		};
		FVector Push;
		if (Overlap(NF, Push))
		{
			const FVector Cand = NF + Push.GetClampedToMaxSize(12.0f);
			FVector Still;
			const P9NPCLoco::FFloorProbe P2 = P9NPCLoco::ProbeFloor(GetWorld(), Cand, MoveDir, FeetZ, Params, StairTread);
			if (P2.bCentreFloor && FMath::Abs(P2.CentreZ - FeetZ) < 40.0f && !P9NPCLoco::MoveHitsWorld(GetWorld(), Loc - FeetOff, Cand, BodyRadius, Params) && !Overlap(Cand, Still))
			{
				Next.X = Cand.X; Next.Y = Cand.Y;
			}
			else if (BlockedTime < PassThroughAfter)
			{
				FVector Now;
				if (!Overlap(Loc - FeetOff, Now))                                    // never walk into a body: wait where we are
				{
					BlockedTime += DeltaSeconds;
					CurSpeed = 0.0f;
					return false;
				}
			}
			else if (!bLoggedPassThrough)
			{
				bLoggedPassThrough = true;
				UE_LOG(LogP9, Warning, TEXT("NPC %s: held %.1f s by a body it cannot get round - passing (deadlock guard)"), *FriendName, BlockedTime);
			}
		}
		else
		{
			bLoggedPassThrough = false;
		}
	}
	// Stage 7B edge guard: no NPC step ever drops more than 45 cm (stairs drop 17 cm per tread): a path leading off a platform,
	// stair or footbridge edge is refused; a temporary off-route point (stepping round the player) is given up after a second
	if (!bExplicitZ && !bEscaping && (!Probe.bHit || Probe.CentreZ < FeetZ - 45.0f))
	{
		if ((EdgeHoldTime += DeltaSeconds) > 1.0f && PathIdx.Num() && PathIdx[0] == INDEX_NONE && Path.Num() > 1)
		{
			UE_LOG(LogP9, Log, TEXT("NPC %s: path point %s leads off the floor - skipped"), *FriendName, *Path[0].ToCompactString());
			Path.RemoveAt(0); PathIdx.RemoveAt(0); EdgeHoldTime = 0.0f;
		}
		else if (EdgeHoldTime > 1.5f)
		{
			// Stage 9A: the step ahead has no floor at this level - furniture (a bench seat, a counter) or an edge. The 7B guard
			// held such a walker for ever (Specs at his bench stop, the chaiwala in the concourse bench); now it recovers.
			EdgeHoldTime = 0.0f;
			RecoverBlocked(TEXT("no floor ahead (furniture or an edge)"));
		}
		CurSpeed = 0.0f;
		return false;
	}
	EdgeHoldTime = 0.0f;
	if (!bExplicitZ && Probe.bCentreFloor) { LastGoodFeet = Next - FeetOff; bHaveGoodFeet = true; }   // Stage 9A
	SetActorLocation(Next);
	if (Dist > 1.0f)
	{
		const float FaceDelta = FRotator::NormalizeAxis(MoveDir.Rotation().Yaw - YawNow);
		const float MaxStep = TurnDegreesPerSecond * 1.6f * DeltaSeconds;
		const float Eased = FaceDelta * (1.0f - FMath::Exp(-7.0f * DeltaSeconds));
		SetActorRotation(FRotator(0.0f, YawNow + FMath::Clamp(Eased, -MaxStep, MaxStep), 0.0f));
	}
	// a walker steering round a body counts as arrived once within its own radius of a stop (or 40 cm of a pass-through point):
	// two NPCs sharing a destination would otherwise circle it for ever; standing separation then eases them apart
	// pass-through points are passed once the pursuit point is beyond them; a walker held up next to an occupied stop for over a
	// second counts as arrived (another NPC or the player stands on it) - no endless circling or pushing
	const bool bReached = (bOnPath && Dist <= Step) || (Path.Num() > 1 && Dist < FMath::Max(Step, FMath::Min(Look * 0.6f, 45.0f)))
		|| (!bOnPath && Dist < (Path.Num() == 1 ? FMath::Max(BodyRadius, 25.0f) : 40.0f))
		|| (Path.Num() == 1 && BlockedTime > 1.0f && Dist < BodyRadius * 2.0f + 40.0f);
	if (bReached || Dist <= 2.0f)
	{
		if (PathIdx.Num()) { if (PathIdx[0] != INDEX_NONE) RouteIndex = PathIdx[0]; PathIdx.RemoveAt(0); }
		Path.RemoveAt(0);
		bStrictPath = false;
		bEscaping = false;
	}
	return Path.IsEmpty();
}

// ---------------------------------------------------------------------------------------------------------------- routine
void AP9FriendNPC::StartActivity(EP9FriendActivity Next, float Seconds)
{
	Activity = Next;
	ActivityLeft = Seconds;
	switch (Next)
	{
	case EP9FriendActivity::Chat:
		if (NPCRole == EP9NPCRole::Group) Speakers.Add(GroupId, this);
		SetClip(NPCRole == EP9NPCRole::Vendor && PourAnim ? EP9NPCClip::Pour : EP9NPCClip::Chat);
		break;
	case EP9FriendActivity::LookAround: SetClip(EP9NPCClip::Look); break;
	case EP9FriendActivity::Turn: SetClip(EP9NPCClip::Turn); break;
	case EP9FriendActivity::Walk: SetClip(EP9NPCClip::Walk); break;
	case EP9FriendActivity::Sit: break;
	case EP9FriendActivity::Meet: break;
	case EP9FriendActivity::Speak: break;
	default: SetClip(EP9NPCClip::Idle); break;
	}
}

void AP9FriendNPC::ArriveAtStop()
{
	CurrentStop = Route.IsValidIndex(RouteIndex) ? Route[RouteIndex].Stop : EP9NPCStop::Stand;
	if (Route.IsValidIndex(RouteIndex) && DwellLeft <= 0.0f)
	{
		DwellLeft = Rng.FRandRange(Route[RouteIndex].MinWait, Route[RouteIndex].MaxWait);
	}
	const bool bFace = Route.IsValidIndex(RouteIndex) && Route[RouteIndex].bUseFaceYaw;
	TargetYaw = bFace ? Route[RouteIndex].FaceYaw : GetActorRotation().Yaw;
	bLeaveAfterTurn = false;
	if (CurrentStop == EP9NPCStop::Stall)
	{
		for (TActorIterator<AP9FriendNPC> It(GetWorld()); It; ++It)
		{
			if (It->NPCRole == EP9NPCRole::Vendor && FVector::Dist(It->GetActorLocation(), GetActorLocation()) < 450.0f) It->OnCustomer(this);
		}
	}
	if (CurrentStop == EP9NPCStop::Sit && SitDownAnim && SitIdleAnim && StandUpAnim)
	{
		Activity = EP9FriendActivity::Sit;    // Stage 9C: line up with the seat, turn (shuffling onto the spot), settle, then sit
		BeginSitAlign();
		return;
	}
	if (FMath::Abs(FRotator::NormalizeAxis(TargetYaw - GetActorRotation().Yaw)) > 25.0f && TurnAnim)
	{
		StartActivity(EP9FriendActivity::Turn, 3.0f);
		return;
	}
	ChooseNext();
}

void AP9FriendNPC::ChooseRoutineStop()
{
	TArray<int32> Stops;
	for (int32 i = 0; i < Route.Num(); ++i)
	{
		if (Route[i].Stop != EP9NPCStop::Pass && i != RouteIndex) Stops.Add(i);
	}
	if (Stops.IsEmpty()) { StartActivity(EP9FriendActivity::Idle, 5.0f); return; }
	int32 Target = Stops[Rng.RandRange(0, Stops.Num() - 1)];
	auto FirstOf = [&](EP9NPCStop Kind) -> int32 { for (int32 i : Stops) if (Route[i].Stop == Kind) return i; return INDEX_NONE; };
	switch (NPCRole)
	{
	case EP9NPCRole::Commuter:
		// end to end; the ends are Wait stops
		if (RouteIndex <= 0) CommuterDir = 1; else if (RouteIndex >= Route.Num() - 1) CommuterDir = -1;
		Target = CommuterDir > 0 ? Route.Num() - 1 : 0;
		break;
	case EP9NPCRole::BenchSitter:
		if (const int32 S = FirstOf(EP9NPCStop::Sit); S != INDEX_NONE && Rng.FRand() < 0.65f) Target = S;
		break;
	case EP9NPCRole::StallVisitor:
		if (const int32 S = FirstOf(EP9NPCStop::Stall); S != INDEX_NONE && CurrentStop != EP9NPCStop::Stall && Rng.FRand() < 0.55f) Target = S;
		break;
	default: break;
	}
	TargetIndex = Target;
	DwellLeft = 0.0f;
	bDrunkLeg = DrunkWalkAnim && DrunkChance > 0.0f && Rng.FRand() < DrunkChance;     // Stage 5G
	WalkPath(RoutePathTo(Target));
}

void AP9FriendNPC::ChooseNext()
{
	if (Activity == EP9FriendActivity::Chat && GroupSpeaker() == this) Speakers.Remove(GroupId);
	const float Seconds = Rng.FRandRange(MinActivitySeconds, MaxActivitySeconds);
	// routes: dwell at the stop with small idle/look-around cycles, then walk on
	if (Route.Num() > 1)
	{
		if (DwellLeft <= 0.0f) { ChooseRoutineStop(); return; }
		const bool bStall = CurrentStop == EP9NPCStop::Stall;
		const float Pick = Rng.FRand();
		const float Chunk = FMath::Min(DwellLeft, Rng.FRandRange(3.0f, 6.0f));
		StartActivity(bStall && Pick < 0.35f ? EP9FriendActivity::Chat : Pick < 0.65f ? EP9FriendActivity::Idle : EP9FriendActivity::LookAround, Chunk);
		return;
	}
	// group / vendor (v1 behaviour): one speaker at a time, listeners sometimes face the speaker
	AP9FriendNPC* Speaker = NPCRole == EP9NPCRole::Group ? GroupSpeaker() : nullptr;
	if (Speaker && Speaker != this && !bTurnedToSpeaker && TurnAnim && Rng.FRand() < FaceSpeakerChance)
	{
		const float Yaw = YawTo(Speaker->GetActorLocation());
		if (FMath::Abs(FRotator::NormalizeAxis(Yaw - GetActorRotation().Yaw)) > 12.0f)
		{
			bTurnedToSpeaker = true; TargetYaw = Yaw; StartActivity(EP9FriendActivity::Turn, 3.0f); return;
		}
	}
	if (bTurnedToSpeaker && (!Speaker || Speaker == this))
	{
		bTurnedToSpeaker = false; TargetYaw = HomeYaw; StartActivity(EP9FriendActivity::Turn, 3.0f); return;
	}
	const bool bCanChat = (ChatAnim || PourAnim) && (!Speaker || Speaker == this);
	const float W[3] = {IdleAnim ? IdleWeight : 0.0f, bCanChat ? ChatWeight : 0.0f, LookAroundAnim ? LookAroundWeight : 0.0f};
	float Pick = Rng.FRand() * FMath::Max(W[0] + W[1] + W[2], KINDA_SMALL_NUMBER);
	int32 Choice = 0;
	for (; Choice < 2 && Pick > W[Choice]; ++Choice) Pick -= W[Choice];
	if (FMath::Abs(FRotator::NormalizeAxis(HomeYaw - GetActorRotation().Yaw)) > 20.0f && !bTurnedToSpeaker && TurnAnim)
	{
		TargetYaw = HomeYaw; StartActivity(EP9FriendActivity::Turn, 3.0f); return;
	}
	StartActivity(Choice == 1 ? EP9FriendActivity::Chat : Choice == 2 ? EP9FriendActivity::LookAround : EP9FriendActivity::Idle, Seconds);
}

// ---------------------------------------------------------------------------------------------------------------- meets (AP9StationLife)
bool AP9FriendNPC::IsAvailableForMeet() const
{
	return bAvailableNow && Route.Num() > 1 && NPCRole != EP9NPCRole::Vendor && Activity != EP9FriendActivity::Meet && Activity != EP9FriendActivity::Speak
		&& Activity != EP9FriendActivity::Train
		&& EndMeetDelay < 0.0f && !bReturning;
}

void AP9FriendNPC::BeginMeet(const FVector& Slot, const FVector& Center)
{
	MeetSlot = Slot; MeetCenter = Center; bMeetArrived = false; EndMeetDelay = -1.0f;
	MeetRouteIndex = RouteIndexNear(Center, 100000.0f);
	if (Activity == EP9FriendActivity::Sit && (SitPhase == 1 || SitPhase == 2 || SitPhase == 3))
	{
		bMeetAfterStand = true;                      // stand up first; Tick continues the meet
		if (SitPhase != 3) { SitPhase = 3; SetClip(EP9NPCClip::StandUp); }
		return;
	}
	TArray<FVector> P = RoutePathTo(MeetRouteIndex);
	P.Add(Slot); PathIdx.Add(INDEX_NONE);
	bDrunkLeg = false;
	Path = P; YieldTime = 0.0f;
	if (Activity != EP9FriendActivity::Walk) CurSpeed = 0.0f;
	Activity = EP9FriendActivity::Meet;
	SetClip(EP9NPCClip::Walk);
}

void AP9FriendNPC::FaceTowards(const FVector& Point)
{
	TargetYaw = YawTo(Point);
}

float AP9FriendNPC::Say(USoundBase* Line)
{
	if (!Line) return 0.0f;
	const float Duration = FMath::Max(0.5f, Line->GetDuration());
	if (CameraNear(AudibleDistance))
	{
		Voice->SetSound(Line);
		Voice->SetVolumeMultiplier(VoiceVolume);
		Voice->Play();
	}
	const bool bVendor = NPCRole == EP9NPCRole::Vendor;
	SetClip(bVendor ? EP9NPCClip::Gesture : EP9NPCClip::Chat);
	if (bVendor)
	{
		// the rally speech starts on the first word (Stage 5C: crossfaded rather than cut)
		if (AnimInst && GestureAnim) { AnimInst->PlayClip(GestureAnim, true, 0.25f); AnimInst->SetRate(PlayRate); CurrentClip = PendingClip = EP9NPCClip::Gesture; ClipClock = 0.0f; }
		else { CurrentClip = EP9NPCClip::None; UpdateClip(0.0f); }
	}
	SpeakLeft = Duration;
	if (Activity != EP9FriendActivity::Speak) PreSpeakActivity = Activity;
	Activity = EP9FriendActivity::Speak;
	return Duration;
}

void AP9FriendNPC::EndMeet(float Delay)
{
	EndMeetDelay = FMath::Max(0.01f, Delay);
}

void AP9FriendNPC::OnCustomer(AP9FriendNPC* Customer)
{
	if (NPCRole != EP9NPCRole::Vendor || Activity == EP9FriendActivity::Speak || !Customer) return;
	TargetYaw = YawTo(Customer->GetActorLocation());
	StartActivity(EP9FriendActivity::Chat, Rng.FRandRange(5.0f, 8.0f));     // pours a glass for the customer
}

void AP9FriendNPC::TickVendor(float DeltaSeconds)
{
	if (Activity == EP9FriendActivity::Speak || ProximityLine.IsNull() || WorldTime < NextSpeechTime) return;
	if (bOncePerCampaignDay && bSpokeToday) return;      // Stage 5B: once per campaign day (reset by HandleDayStarted)
	const APawn* P = UGameplayStatics::GetPlayerPawn(this, 0);
	const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!P || !Cam) return;
	const float Dist = FVector::Dist(P->GetActorLocation(), GetActorLocation());
	if (Dist > SpeechRadius || FMath::Abs(P->GetActorLocation().Z - GetActorLocation().Z) > 200.0f) return;
	const FVector ToMe = (GetActorLocation() + FVector(0, 0, 60) - Cam->GetCameraLocation()).GetSafeNormal();
	const bool bLooking = FVector::DotProduct(ToMe, Cam->GetCameraRotation().Vector()) > 0.55f;
	if (!bLooking && Dist > 300.0f) return;
	TargetYaw = YawTo(P->GetActorLocation());
	const float Len = Say(ProximityLine.LoadSynchronous());
	NextSpeechTime = WorldTime + Len + SpeechCooldown;
	bSpokeToday = true;
	if (bOncePerCampaignDay)
	{
		UE_LOG(LogP9, Log, TEXT("NPC %s: speaks to the player (%.1fs; daily line used - next on the next campaign day)"), *FriendName, Len);
	}
	else
	{
		UE_LOG(LogP9, Log, TEXT("NPC %s: speaks to the player (%.1fs, next in %.0fs)"), *FriendName, Len, Len + SpeechCooldown);
	}
}

// ---------------------------------------------------------------------------------------------------------------- tick
void AP9FriendNPC::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	WorldTime += DeltaSeconds;
	UpdateClip(DeltaSeconds);
	if ((RateCheck -= DeltaSeconds) <= 0.0f)
	{
		RateCheck = 1.0f;
		const bool bNear = CameraNear(FullRateDistance);
		SetActorTickInterval(bNear ? 0.0f : 0.2f);
		Body->VisibilityBasedAnimTickOption = bNear ? EVisibilityBasedAnimTickOption::AlwaysTickPose : EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	}
	if (NPCRole == EP9NPCRole::Vendor && Activity != EP9FriendActivity::Scripted) TickVendor(DeltaSeconds);
	// Stage 5C: the walk clip is authored for WalkSpeed; its rate follows the eased speed so the feet stay planted
	const float ClipSpeed = (Activity == EP9FriendActivity::Scripted && ScriptPhase == 1) ? ProxySpeed : CurSpeed;
	if (AnimInst)
	{
		// each moving clip plays at the rate that matches the actual speed (feet planted)
		float Rate = PlayRate;
		if (CurrentClip == EP9NPCClip::Walk) Rate = ClipSpeed / FMath::Max(10.0f, WalkClipSpeed > 0.0f ? WalkClipSpeed : WalkSpeed);
		else if (CurrentClip == EP9NPCClip::DrunkWalk) Rate = ClipSpeed / FMath::Max(10.0f, DrunkWalkSpeed);
		else if (P9Friends::IsStairClip(CurrentClip) && bStairClipsFitted)
		{
			// Stage 7B: fitted stair clips - one cycle covers two treads; the clip time follows the height on the flight, so
			// every plant lands on a tread whatever the speed (no sliding)
			const UAnimSequence* A = ClipAnim(CurrentClip);
			const float Len = A ? FMath::Max(0.1f, A->GetPlayLength()) : 1.0f;
			Rate = ClipSpeed / FMath::Max(10.0f, 2.0f * StairTread / Len);
			if (StairDir != 0 && StairOffTime <= 0.0f && AnimInst->GetClip() == A)
			{
				AnimInst->SetTime(P9NPCLoco::StairPhase(StairRampZ, StairRise, CurrentClip == EP9NPCClip::StairsUp ? 1 : -1) * Len);
			}
		}
		else if (CurrentClip == EP9NPCClip::StairsUp) Rate = ClipSpeed * FMath::Sqrt(1.0f + ClimbSlope * ClimbSlope) / FMath::Max(10.0f, StairsUpClipSpeed);
		else if (CurrentClip == EP9NPCClip::Turn && Activity != EP9FriendActivity::Scripted)
		{
			// Stage 9A: the turn-in-place clip steps as fast as the body actually turns (no feet shuffling on a still body)
			Rate = PlayRate * FMath::Clamp(FMath::Abs(TurnRate) / FMath::Max(10.0f, TurnDegreesPerSecond), 0.35f, 1.15f);
		}
		Rate = P9Friends::IsWalkClip(CurrentClip) ? FMath::Clamp(Rate, P9Friends::MinMoveClipRate, 1.8f) : Rate;
		if (P9Friends::IsWalkClip(CurrentClip)) LastClipRate = Rate;
		AnimInst->SetRate(Rate);
	}
	TickDrunkLines();
	if (!P9Friends::IsWalkClip(CurrentClip)) CurSpeed = 0.0f;               // paused (yielding, turning, standing): set off again from rest
	if ((TrainCheck -= DeltaSeconds) <= 0.0f)
	{
		TrainCheck = 0.5f;
		TryBoardEpilogueTrain();
	}
	if (bAvoidOthers) SeparateStanding(DeltaSeconds);                          // Stage 7B

	switch (Activity)
	{
	case EP9FriendActivity::Train:
		TickTrain(DeltaSeconds);
		return;
	case EP9FriendActivity::Scripted:
		TickScripted(DeltaSeconds);
		return;
	case EP9FriendActivity::Speak:
		TurnTowardsYaw(TargetYaw, DeltaSeconds);
		if ((SpeakLeft -= DeltaSeconds) <= 0.0f)
		{
			if (PreSpeakActivity == EP9FriendActivity::Meet) { Activity = EP9FriendActivity::Meet; SetClip(EP9NPCClip::Idle); }
			else StartActivity(EP9FriendActivity::Idle, Rng.FRandRange(2.0f, 4.0f));
		}
		return;
	case EP9FriendActivity::Meet:
		if (EndMeetDelay >= 0.0f)
		{
			if ((EndMeetDelay -= DeltaSeconds) <= 0.0f)
			{
				// back to the route point the meet was reached from, then on with the routine
				EndMeetDelay = -1.0f; bMeetArrived = false; bReturning = true;
				TArray<FVector> Back;
				if (Route.IsValidIndex(MeetRouteIndex)) { Back.Add(Route[MeetRouteIndex].Location); PathIdx.Reset(); PathIdx.Add(MeetRouteIndex); }
				DwellLeft = Rng.FRandRange(2.0f, 5.0f);
				WalkPath(Back);
			}
			return;
		}
		if (!bMeetArrived)
		{
			if (PlayerBlocksWay() && (YieldTime += DeltaSeconds) < 6.0f) { if (!DecelerateToStop(DeltaSeconds)) SetClip(EP9NPCClip::Idle); return; }
			if (HandleReversal(DeltaSeconds)) return;                                       // Stage 9A
			SetClip(EP9NPCClip::Walk);
			TickProgressWatchdog(DeltaSeconds);
			if (StepAlongPath(DeltaSeconds)) { bMeetArrived = true; TargetYaw = YawTo(MeetCenter); SetClip(EP9NPCClip::Idle); }
			return;
		}
		TurnTowardsYaw(TargetYaw, DeltaSeconds);
		return;
	case EP9FriendActivity::Turn:
		if (CurrentClip == EP9NPCClip::Turn || !TurnAnim)
		{
			if (TurnTowardsYaw(TargetYaw, DeltaSeconds) || (ActivityLeft -= DeltaSeconds) <= 0.0f)
			{
				if (bLeaveAfterTurn) { bLeaveAfterTurn = false; StartActivity(EP9FriendActivity::Walk, 120.0f); }
				else ChooseNext();
			}
		}
		return;
	case EP9FriendActivity::Walk:
		if (PlayerBlocksWay())
		{
			// a short pause to let the player pass (Stage 9A: the walker slows to a stop first instead of freezing mid-stride)
			if ((YieldTime += DeltaSeconds) < 2.5f) { if (!DecelerateToStop(DeltaSeconds)) SetClip(EP9NPCClip::Idle); return; }
			if (const APawn* PP = UGameplayStatics::GetPlayerPawn(this, 0); PP && YieldTime < 100.0f)
			{
				// still in the way: step round the player (side away from the player, back onto the path). Stage 7B: the side
				// point must be floor at this level with a clear way to it (it used to land beyond stair walls); else the other
				// side; else no detour - the NPC walks on (it has no collision, so it passes the player)
				const FVector Dir = (Path[0] - GetActorLocation()).GetSafeNormal2D();
				FVector Side = FVector::CrossProduct(Dir, FVector::UpVector);
				if (FVector::DotProduct(Side, PP->GetActorLocation() - GetActorLocation()) > 0.0f) Side = -Side;
				const FVector Feet = GetActorLocation() - FVector(0, 0, P9Friends::CapsuleHalfHeight);
				FCollisionQueryParams Params(SCENE_QUERY_STAT(P9FriendStepRound), false, this);
				Params.AddIgnoredActor(PP);
				for (const float S : {1.0f, -1.0f})
				{
					FVector Pt = PP->GetActorLocation() + Side * S * 95.0f + Dir * 40.0f; Pt.Z = Feet.Z;
					const P9NPCLoco::FFloorProbe Pr = P9NPCLoco::ProbeFloor(GetWorld(), Pt, Dir, Feet.Z, Params, StairTread);
					if (Pr.bCentreFloor && FMath::Abs(Pr.CentreZ - Feet.Z) < 40.0f && !P9NPCLoco::MoveHitsWorld(GetWorld(), Feet, Pt, BodyRadius, Params))
					{
						Path.Insert(FVector(Pt.X, Pt.Y, Pr.CentreZ), 0); PathIdx.Insert(INDEX_NONE, 0);
						break;
					}
				}
				YieldTime = 100.0f;                                                                    // no further yielding until clear
			}
		}
		else YieldTime = 0.0f;
		if (bAvoidOthers && !Path.IsEmpty() && BlockedTime <= P9Friends::HoldSeconds)
		{
			// Stage 7B: a body right in front that cannot be stepped round yet - wait (idle) a moment
			float Scale = 1.0f;
			AvoidanceSteer((Path[0] - GetActorLocation()).GetSafeNormal2D(), Scale, DeltaSeconds);
			bHoldingForBody = Scale < (bHoldingForBody ? 0.3f : 0.05f);              // hysteresis: no idle/walk flicker
			if (bHoldingForBody) { BlockedTime += DeltaSeconds; CurSpeed = 0.0f; SetClip(EP9NPCClip::Idle); return; }
		}
		if (HandleReversal(DeltaSeconds)) return;                                           // Stage 9A
		SetClip(EP9NPCClip::Walk);
		if (P9Friends::IsWalkClip(CurrentClip) || !WalkAnim)
		{
			TickProgressWatchdog(DeltaSeconds);
			if (StepAlongPath(DeltaSeconds))
			{
				bReturning = false;
				ArriveAtStop();
			}
		}
		return;
	case EP9FriendActivity::Sit:
		switch (SitPhase)
		{
		case 5:   // Stage 9C: a few slow steps onto the stand point in front of the seat (too far to shuffle while turning)
		{
			const FVector To = SitAlignTarget - GetActorLocation();
			const float D = To.Size2D();
			if (D < 12.0f) { SitPhase = 0; CurSpeed = 0.0f; SetClip(FMath::Abs(FRotator::NormalizeAxis(TargetYaw - GetActorRotation().Yaw)) > 20.0f && TurnAnim ? EP9NPCClip::Turn : EP9NPCClip::Idle); break; }
			TurnTowardsYaw(To.Rotation().Yaw, DeltaSeconds);
			const float Facing = FVector::DotProduct(GetActorRotation().Vector().GetSafeNormal2D(), To.GetSafeNormal2D());
			if (Facing < 0.6f) { CurSpeed = 0.0f; SetClip(TurnAnim ? EP9NPCClip::Turn : EP9NPCClip::Idle); break; }
			SetClip(EP9NPCClip::Walk);
			if (!P9Friends::IsWalkClip(CurrentClip)) break;
			const float VMax = FMath::Min(55.0f, FMath::Sqrt(2.0f * P9Friends::WalkDecel * D) + 8.0f);
			CurSpeed = FMath::Min(CurSpeed + P9Friends::WalkAccel * DeltaSeconds, VMax);
			SetActorLocation(GetActorLocation() + To.GetSafeNormal2D() * FMath::Min(D, CurSpeed * DeltaSeconds));
			break;
		}
		case 0:   // turn to the bench's facing (Stage 9C: shuffling the last few cm onto the stand point with the turn steps), then sit
		{
			const bool bTurned = TurnTowardsYaw(TargetYaw, DeltaSeconds);
			FVector To = SitAlignTarget - GetActorLocation(); To.Z = 0.0f;
			const float D = To.Size2D();
			if (!bSitAligned && D > 0.5f)
			{
				// slow drift while the feet step round (the turn clip); finishes with the turn or at most ~0.5 s after it
				const float Rate = bTurned ? 30.0f : 22.0f;
				SetActorLocation(GetActorLocation() + To.GetSafeNormal2D() * FMath::Min(D, Rate * DeltaSeconds));
				if (bTurned && CurrentClip != EP9NPCClip::Turn && TurnAnim) SetClip(EP9NPCClip::Turn);
			}
			else bSitAligned = true;
			if (bTurned && (bSitAligned || D <= 0.5f))
			{
				bSitAligned = true;
				SitPhase = 4; SitSettleLeft = SitSettleTime; SetClip(EP9NPCClip::Idle);
			}
			break;
		}
		case 4:   // Stage 9C: settled stand, then sit down
			if (CurrentClip == EP9NPCClip::Idle && (SitSettleLeft -= DeltaSeconds) <= 0.0f) { SitPhase = 1; SetClip(EP9NPCClip::SitDown); }
			break;
		case 1:
			if (CurrentClip == EP9NPCClip::SitDown && OneShotDone()) { SitPhase = 2; SeatedTime = 0.0f; SetClip(EP9NPCClip::SitIdle); }
			break;
		case 2:
			SeatedTime += DeltaSeconds;
			if (SeatedTime > 0.6f && SeatedTime - DeltaSeconds <= 0.6f) MeasureSeated();
			if ((DwellLeft -= DeltaSeconds) <= 0.0f) { SitPhase = 3; SetClip(EP9NPCClip::StandUp); }
			break;
		case 3:
			if (CurrentClip == EP9NPCClip::StandUp && OneShotDone())
			{
				DwellLeft = 0.0f;
				SitPhase = 6; SitSettleLeft = StandSettleTime; SetClip(EP9NPCClip::Idle);     // Stage 9C: settle on the feet first
			}
			break;
		case 6:
			if (CurrentClip == EP9NPCClip::Idle && (SitSettleLeft -= DeltaSeconds) <= 0.0f)
			{
				SitPhase = 0; bSitAligned = true;
				if (bMeetAfterStand) { bMeetAfterStand = false; Activity = EP9FriendActivity::Idle; BeginMeet(MeetSlot, MeetCenter); }
				else ChooseRoutineStop();
			}
			break;
		}
		return;
	default:
		if (Route.Num() > 1) DwellLeft -= DeltaSeconds;
		if (Route.Num() <= 1 && FMath::Abs(FRotator::NormalizeAxis(TargetYaw - GetActorRotation().Yaw)) > 1.0f && NPCRole == EP9NPCRole::Vendor)
		{
			TurnTowardsYaw(TargetYaw, DeltaSeconds);     // the vendor keeps facing whoever he talks to
		}
		if ((ActivityLeft -= DeltaSeconds) <= 0.0f) ChooseNext();
		return;
	}
}

void AP9FriendNPC::LogStatus() const
{
	static const TCHAR* Acts[] = {TEXT("Idle"), TEXT("Chat"), TEXT("Look"), TEXT("Turn"), TEXT("Walk"), TEXT("Sit"), TEXT("Meet"), TEXT("Speak"), TEXT("Train"), TEXT("Script")};
	const UAnimSequence* A = ClipAnim(CurrentClip);
	UE_LOG(LogP9, Log, TEXT("NPC %-9s %-3s %-5s clip=%s pos=%.2f loc=%s yaw=%.0f route=%d/%d dwell=%.1f path=%d sit=%d meet=%d avail=%d hidden=%d tick=%d lock=%d daily=%d spoke=%d"), *FriendName, *FriendKey.ToString(),
		Acts[(int32)Activity], A ? *A->GetName() : TEXT("none"), AnimInst ? AnimInst->GetTime() : Body->GetPosition(), *GetActorLocation().ToCompactString(), GetActorRotation().Yaw,
		RouteIndex, Route.Num(), DwellLeft, Path.Num(), SitPhase, bMeetArrived ? 1 : 0, bAvailableNow ? 1 : 0, IsHidden() ? 1 : 0, IsActorTickEnabled() ? 1 : 0,
		bHideUntilCampaignComplete ? 1 : 0, bOncePerCampaignDay ? 1 : 0, bSpokeToday ? 1 : 0);
}

// ---------------------------------------------------------------------------------------------------------------- epilogue train (Stage 5C)
// The Dadar local, the campaign director and the seats are only READ here (state, doors, transforms, occupancy). The NPC has
// no collision, so it never blocks the doors; it attaches itself to its seat while seated so it would ride with the car.
float AP9FriendNPC::SitStandOffset() const
{
	// the sit clips put the hips 22 cm (scaled by height / 1.75 m) behind the stand point
	const float H = FriendMesh ? FriendMesh->GetBounds().BoxExtent.Z * 2.0f : 175.0f;
	return 22.0f * FMath::Clamp(H / 175.0f, 0.85f, 1.15f);
}

void AP9FriendNPC::TryBoardEpilogueTrain()
{
	if (!Director || Director->GetPhase() != EP9DayPhase::Epilogue)
	{
		bRodeThisEpilogue = false;                       // a later epilogue (new run) may board again
		return;
	}
	if (!bRideEpilogueTrain || bRodeThisEpilogue || !bAvailableNow || NPCRole == EP9NPCRole::Vendor || Route.Num() < 2) return;
	const bool bFree = Activity == EP9FriendActivity::Idle || Activity == EP9FriendActivity::Chat || Activity == EP9FriendActivity::LookAround
		|| Activity == EP9FriendActivity::Turn || Activity == EP9FriendActivity::Walk
		|| (Activity == EP9FriendActivity::Sit && (SitPhase == 0 || SitPhase == 4 || SitPhase == 5 || SitPhase == 6));
	if (!bFree) return;
	AP9Train* T = Director->OpeningTrain;
	if (!T || T->IsHidden() || T->IsMoving() || T->GetDoorState() != EP9DoorState::Open || !T->IsPlayerAboard()) return;
	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!PlayerPawn || !UP9SeatComponent::GetSeatOf(PlayerPawn)) return;          // only while the player is still seated
	int32 Riders = 0;
	for (TActorIterator<AP9FriendNPC> It(GetWorld()); It; ++It) if (It->IsOnTrain()) ++Riders;
	if (Riders >= MaxEpilogueRiders) return;

	const FTransform TX = T->GetActorTransform();
	const float Side = T->PlatformSideWorldY >= 0 ? 1.0f : -1.0f;
	const float PlatformZ = GetActorLocation().Z - P9Friends::CapsuleHalfHeight;
	if (FMath::Abs(PlatformZ - (TX.GetLocation().Z + 75.0f)) > 120.0f) return;     // must be on the platform level
	const float FloorZ = TX.GetLocation().Z + P9Friends::TrainFloor;
	auto Local = [&TX](float X, float Y, float Z) { FVector P = TX.TransformPosition(FVector(X, Y, 0.0f)); P.Z = Z; return P; };

	// a free seat near the player (not the player's own seat), in the player's car
	ClaimedSeats.RemoveAll([](const TWeakObjectPtr<UP9SeatComponent>& S) { return !S.IsValid(); });
	TArray<UP9SeatComponent*> Seats;
	T->GetComponents<UP9SeatComponent>(Seats);
	const UP9SeatComponent* PlayerSeat = UP9SeatComponent::GetSeatOf(PlayerPawn);
	const FVector PlayerLocal = TX.InverseTransformPosition(PlayerPawn->GetActorLocation());
	UP9SeatComponent* Best = nullptr;
	float BestScore = TNumericLimits<float>::Max();
	for (UP9SeatComponent* S : Seats)
	{
		if (!S || S == PlayerSeat || S->IsOccupied() || ClaimedSeats.Contains(S)) continue;
		const FVector SL = TX.InverseTransformPosition(S->GetComponentLocation());
		if (FMath::Sign(SL.X) != FMath::Sign(PlayerLocal.X)) continue;                // same car as the player
		const float D = FVector::Dist2D(SL, PlayerLocal);
		if (D < 100.0f) continue;                                                     // not shoulder to shoulder with the player (camera)
		const float Score = D + Rng.FRandRange(0.0f, 150.0f);
		if (Score < BestScore) { BestScore = Score; Best = S; }
	}
	if (!Best) return;
	const FVector SeatLocal = TX.InverseTransformPosition(Best->GetComponentLocation());
	const float Car = SeatLocal.X < 0.0f ? -P9Friends::CarCentreX : P9Friends::CarCentreX;
	float DoorLX = Car + P9Friends::DoorX[0];
	for (float DX : P9Friends::DoorX) if (FMath::Abs(Car + DX - SeatLocal.X) < FMath::Abs(DoorLX - SeatLocal.X)) DoorLX = Car + DX;
	const FVector Outside = Local(DoorLX, Side * 330.0f, PlatformZ);
	if (FVector::Dist2D(Outside, GetActorLocation()) > BoardRadius) return;          // too far away along the platform

	const FVector Threshold = Local(DoorLX, Side * 125.0f, FloorZ);
	const FVector AisleDoor = Local(DoorLX, 0.0f, FloorZ);
	FVector AisleSeat = Best->GetComponentTransform().TransformPosition(Best->StandOffset); AisleSeat.Z = FloorZ;
	const FVector SeatFwd = Best->GetForwardVector().GetSafeNormal2D();
	FVector SitSpot = Best->GetComponentLocation() + SeatFwd * SitStandOffset(); SitSpot.Z = FloorZ;

	if (Activity == EP9FriendActivity::Chat && GroupSpeaker() == this) Speakers.Remove(GroupId);
	ClaimedSeats.Add(Best);
	RideTrain = T; RideSeat = Best; SeatYaw = SeatFwd.Rotation().Yaw;
	TrainExit = {AisleSeat, AisleDoor, Threshold, Outside};
	Path = {Outside, Threshold, AisleDoor, AisleSeat, SitSpot};
	PathIdx.Init(INDEX_NONE, Path.Num());
	if (Activity != EP9FriendActivity::Walk) CurSpeed = 0.0f;
	bDrunkLeg = false;
	Activity = EP9FriendActivity::Train; TrainPhase = 1; TrainTime = 0.0f; bExplicitZ = true; bTrappedLogged = false; YieldTime = 0.0f;
	SetClip(EP9NPCClip::Walk);
	UE_LOG(LogP9, Log, TEXT("NPC %s: boards the Dadar local (door x %.0f, seat %s, %d riders)"), *FriendName, DoorLX, *Best->GetName(), Riders + 1);
}

void AP9FriendNPC::LeaveTrainSeat()
{
	TrainPhase = 5;
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	SetClip(EP9NPCClip::StandUp);
}

void AP9FriendNPC::TickTrain(float DeltaSeconds)
{
	AP9Train* T = RideTrain.Get();
	UP9SeatComponent* Seat = RideSeat.Get();
	if (!T || !Seat) { FinishTrainRide(); return; }
	TrainTime += DeltaSeconds;
	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	// leave as soon as the player stands up or gets off (the director shuts the doors once the player is 15 m away)
	const bool bPlayerGoing = !PlayerPawn || !T->IsPlayerAboard() || !UP9SeatComponent::GetSeatOf(PlayerPawn);
	const bool bDoorsOpen = T->GetDoorState() == EP9DoorState::Open && !T->IsMoving();
	switch (TrainPhase)
	{
	case 1:   // walk in through the door to the seat
		if (bPlayerGoing && Path.Num() >= 4) { FinishTrainRide(); return; }            // not inside yet: give up, back to the routine
		SetClip(EP9NPCClip::Walk);
		if ((P9Friends::IsWalkClip(CurrentClip) || !WalkAnim) && StepAlongPath(DeltaSeconds))
		{
			TrainPhase = 2;
			SitSettleLeft = 1.0e6f;                                  // Stage 9C: turning first (case 2)
			SetClip(TurnAnim ? EP9NPCClip::Turn : EP9NPCClip::Idle);
		}
		return;
	case 2:   // turn to face away from the seat back (Stage 9C: then a short settled stand before sitting down)
		if (CurrentClip != EP9NPCClip::Idle || SitSettleLeft > SitSettleTime)
		{
			if (TurnTowardsYaw(SeatYaw, DeltaSeconds)) { SetClip(EP9NPCClip::Idle); SitSettleLeft = SitSettleTime * 0.7f; }
			return;
		}
		if ((SitSettleLeft -= DeltaSeconds) <= 0.0f) { TrainPhase = 3; SetClip(EP9NPCClip::SitDown); }
		return;
	case 3:   // sit down
		if (CurrentClip == EP9NPCClip::SitDown && OneShotDone())
		{
			TrainPhase = 4; TrainTime = 0.0f;
			SetClip(EP9NPCClip::SitIdle);
			AttachToComponent(Seat, FAttachmentTransformRules::KeepWorldTransform);
			UE_LOG(LogP9, Log, TEXT("NPC %s: seated in the Dadar local (%s)"), *FriendName, *Seat->GetName());
		}
		return;
	case 4:   // seated: gets up the moment the player stands up or leaves
		if ((bPlayerGoing && TrainTime > 0.8f) || TrainTime > 75.0f)
		{
			bool bUnseen = true;
			if (const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0))
			{
				const FVector To = GetActorLocation() - Cam->GetCameraLocation();
				bUnseen = To.Size() > 1800.0f || FVector::DotProduct(To.GetSafeNormal(), Cam->GetCameraRotation().Vector()) < 0.35f;
			}
			if (bDoorsOpen || bUnseen)
			{
				if (!bDoorsOpen) UE_LOG(LogP9, Log, TEXT("NPC %s: doors already shut - slips out unseen"), *FriendName);
				LeaveTrainSeat();
			}
			else if (!bTrappedLogged)
			{
				bTrappedLogged = true;
				UE_LOG(LogP9, Log, TEXT("NPC %s: doors shut and the player is watching - waits seated"), *FriendName);
			}
		}
		return;
	case 5:   // stand up
		if (CurrentClip == EP9NPCClip::StandUp && OneShotDone())
		{
			TrainPhase = 6;
			Path = TrainExit; PathIdx.Init(INDEX_NONE, Path.Num()); CurSpeed = 0.0f; SpeedScale = 1.3f;     // hurries off
			SetClip(EP9NPCClip::Walk);
		}
		return;
	case 6:   // walk out onto the platform
		SetClip(EP9NPCClip::Walk);
		if ((P9Friends::IsWalkClip(CurrentClip) || !WalkAnim) && StepAlongPath(DeltaSeconds)) FinishTrainRide();
		return;
	default:
		FinishTrainRide();
		return;
	}
}

void AP9FriendNPC::FinishTrainRide()
{
	const bool bWasAboard = TrainPhase >= 2;
	if (RideSeat.IsValid()) ClaimedSeats.Remove(RideSeat);
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	RideSeat = nullptr; RideTrain = nullptr; TrainPhase = 0; TrainExit.Reset();
	bExplicitZ = false; bRodeThisEpilogue = true; SpeedScale = 1.0f;
	Activity = EP9FriendActivity::Idle;
	SetActorRotation(FRotator(0.0f, GetActorRotation().Yaw, 0.0f));
	// back onto the routine at the nearest route point
	const int32 Near = RouteIndexNear(GetActorLocation(), 1.0e7f);
	UE_LOG(LogP9, Log, TEXT("NPC %s: %s - back to the routine (route point %d)"), *FriendName, bWasAboard ? TEXT("got off the Dadar local") : TEXT("did not board"), Near);
	if (Route.IsValidIndex(Near))
	{
		RouteIndex = Near; bReturning = true;
		DwellLeft = Rng.FRandRange(2.0f, 5.0f);
		TArray<FVector> Back = {Route[Near].Location};
		WalkPath(Back);
		PathIdx.Reset(); PathIdx.Add(Near);
	}
	else
	{
		StartActivity(EP9FriendActivity::Idle, 3.0f);
	}
}

// ---------------------------------------------------------------------------------------------------------------- epilogue wake-up (Stage 5C)
// The campaign director still owns the scene: it moves its (now hidden) chaiwala proxy along the train-local path and
// plays the lines; this vendor mirrors the proxy with real clips, then walks home along EpilogueReturnPath.
void AP9FriendNPC::BeginEpilogueWake(AActor* Proxy)
{
	if (!Proxy || !CanPlayEpilogueWake()) return;
	Voice->Stop();
	if (Activity == EP9FriendActivity::Chat && GroupSpeaker() == this) Speakers.Remove(GroupId);
	Path.Reset(); PathIdx.Reset();
	bDrunkLeg = false;
	WakeProxy = Proxy; ScriptPhase = 1; ProxySpeed = 0.0f;
	{
		const USceneComponent* Car = Proxy->GetRootComponent() ? Proxy->GetRootComponent()->GetAttachParent() : nullptr;
		LastProxyLoc = Car ? Car->GetComponentTransform().InverseTransformPosition(Proxy->GetActorLocation()) : Proxy->GetActorLocation();
	}
	Activity = EP9FriendActivity::Scripted;
	SetActorLocation(LastProxyLoc + FVector(0.0f, 0.0f, P9Friends::CapsuleHalfHeight));
	SetActorRotation(FRotator(0.0f, Proxy->GetActorRotation().Yaw, 0.0f));
	if (USceneComponent* Parent = Proxy->GetRootComponent() ? Proxy->GetRootComponent()->GetAttachParent() : nullptr)
	{
		AttachToComponent(Parent, FAttachmentTransformRules::KeepWorldTransform);       // rides with the train still pulling in
	}
	SetClip(EP9NPCClip::Walk);
	UE_LOG(LogP9, Log, TEXT("NPC %s: plays the epilogue wake-up in the Dadar local"), *FriendName);
}

void AP9FriendNPC::EndEpilogueWake()
{
	if (Activity != EP9FriendActivity::Scripted || ScriptPhase != 1) return;
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	SetActorRotation(FRotator(0.0f, GetActorRotation().Yaw, 0.0f));
	ScriptPhase = 2;
	Path = EpilogueReturnPath; PathIdx.Init(INDEX_NONE, Path.Num());
	CurSpeed = 0.0f; bExplicitZ = false;
	SetClip(EP9NPCClip::Walk);
	UE_LOG(LogP9, Log, TEXT("NPC %s: off the train - walks back to his stall (%d points)"), *FriendName, Path.Num());
}

void AP9FriendNPC::TickScripted(float DeltaSeconds)
{
	switch (ScriptPhase)
	{
	case 1:
	{
		AActor* P = WakeProxy.Get();
		if (!P) { EndEpilogueWake(); return; }
		const FVector L = P->GetActorLocation();
		// Stage 5F: walking speed measured in the CARRIAGE frame - the train is still pulling in during the wake-up and its
		// motion used to count as walking (walk clip played up to 1.8x too fast)
		const USceneComponent* Car = P->GetRootComponent() ? P->GetRootComponent()->GetAttachParent() : nullptr;
		const FVector Local = Car ? Car->GetComponentTransform().InverseTransformPosition(L) : L;
		const float Moved = FVector::Dist2D(Local, LastProxyLoc) / FMath::Max(DeltaSeconds, 1.0e-3f);
		ProxySpeed = FMath::FInterpTo(ProxySpeed, FMath::Min(Moved, 200.0f), DeltaSeconds, 10.0f);
		LastProxyLoc = Local;
		FVector Loc = L + FVector(0.0f, 0.0f, P9Friends::CapsuleHalfHeight);
		Loc.Z = FMath::FInterpTo(GetActorLocation().Z, Loc.Z, DeltaSeconds, 6.0f);         // no bob from the proxy's gait
		SetActorLocation(Loc);
		const float Yaw = FMath::FixedTurn(GetActorRotation().Yaw, P->GetActorRotation().Yaw, 300.0f * DeltaSeconds);
		SetActorRotation(FRotator(0.0f, Yaw, 0.0f));
		// walking while the proxy moves; talking to the player while it stands (he speaks both of his lines then)
		SetClip(ProxySpeed > 15.0f ? EP9NPCClip::Walk : (GestureAnim ? EP9NPCClip::Chat : EP9NPCClip::Idle));
		return;
	}
	case 2:
		if (HandleReversal(DeltaSeconds)) return;                                           // Stage 9A
		SetClip(EP9NPCClip::Walk);
		if (P9Friends::IsWalkClip(CurrentClip) || !WalkAnim) TickProgressWatchdog(DeltaSeconds);
		if ((P9Friends::IsWalkClip(CurrentClip) || !WalkAnim) && StepAlongPath(DeltaSeconds))
		{
			ScriptPhase = 3;
			SetClip(TurnAnim ? EP9NPCClip::Turn : EP9NPCClip::Idle);
		}
		return;
	case 3:
		if (TurnTowardsYaw(HomeYaw, DeltaSeconds))
		{
			ScriptPhase = 0; WakeProxy = nullptr;
			Home = GetActorLocation(); TargetYaw = HomeYaw;
			UE_LOG(LogP9, Log, TEXT("NPC %s: back at his stall"), *FriendName);
			StartActivity(EP9FriendActivity::Idle, Rng.FRandRange(2.0f, 4.0f));
		}
		return;
	default:
		StartActivity(EP9FriendActivity::Idle, 2.0f);
		return;
	}
}

// ---------------------------------------------------------------------------------------------------------------- separation (Stage 7B)
FVector AP9FriendNPC::AvoidanceSteer(const FVector& Dir, float& OutSpeedScale, float DeltaSeconds)
{
	// bodies inside the corridor ahead (own radius + theirs + a gap): steer to one side (away from the body; dead ahead both
	// keep left, so two NPCs meeting head-on pass each other), slow down when one is right in front
	OutSpeedScale = 1.0f;
	TArray<P9NPCLoco::FBody> Bodies;
	P9NPCLoco::GatherBodies(GetWorld(), this, Bodies);
	if (APawn* PP = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		// the player is steered round early too (the NPC never blocks the player and still never touches the camera);
		// holding for the player stays with PlayerBlocksWay
		P9NPCLoco::FBody PB; PB.Actor = PP; PB.Radius = 34.0f; PB.FeetOffset = 90.0f;
		if (const ACharacter* C = Cast<ACharacter>(PP)) PB.FeetOffset = C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		Bodies.Add(PB);
	}
	const FVector Feet = GetActorLocation() - FVector(0.0f, 0.0f, P9Friends::CapsuleHalfHeight);
	const FVector Left(-Dir.Y, Dir.X, 0.0f);
	FVector Steer = FVector::ZeroVector;
	bool bAny = false;
	for (const P9NPCLoco::FBody& B : Bodies)
	{
		const FVector BF = B.Actor->GetActorLocation() - FVector(0.0f, 0.0f, B.FeetOffset);
		if (FMath::Abs(BF.Z - Feet.Z) > 120.0f) continue;
		const FVector Rel(BF.X - Feet.X, BF.Y - Feet.Y, 0.0f);
		const float Along = FVector::DotProduct(Rel, Dir), Lat = FVector::DotProduct(Rel, Left);
		const float R = BodyRadius + B.Radius + P9Friends::BodyGap;
		const float Look = R + 250.0f;
		if (Along <= -5.0f || Along > Look || FMath::Abs(Lat) >= R) continue;
		bAny = true;
		if (AvoidSide == 0) AvoidSide = FMath::Abs(Lat) > 6.0f ? (Lat > 0.0f ? -1 : 1) : 1;
		const float W = (1.0f - FMath::Abs(Lat) / R) * FMath::Clamp((Look - Along) / FMath::Max(1.0f, Look - R * 0.5f), 0.0f, 1.0f);
		Steer += Left * static_cast<float>(AvoidSide) * W * 2.0f;
		if (Along < R) OutSpeedScale = FMath::Min(OutSpeedScale, FMath::Clamp((Along - R * 0.45f) / (R * 0.55f), 0.0f, 1.0f));
	}
	if (!bAny) { AvoidSide = 0; BlockedTime = FMath::Max(0.0f, BlockedTime - DeltaSeconds); }
	else if (OutSpeedScale > 0.5f) BlockedTime = FMath::Max(0.0f, BlockedTime - DeltaSeconds);
	return Steer;
}

FVector AP9FriendNPC::PursuitPoint(const FVector& Loc, float Look) const
{
	FVector From(Loc.X, Loc.Y, 0.0f);
	float Left = Look;
	for (const FVector& P : Path)
	{
		const FVector To(P.X, P.Y, 0.0f);
		const float D = FVector::Dist(From, To);
		if (D >= Left) return From + (To - From) / FMath::Max(D, KINDA_SMALL_NUMBER) * Left;
		Left -= D; From = To;
	}
	return From;
}

bool AP9FriendNPC::CanBeMovedBySeparation() const
{
	// seated, train-riding, scripted and vendor NPCs keep their exact spot; walkers steer instead
	if (!bAvailableNow || NPCRole == EP9NPCRole::Vendor) return false;
	switch (Activity)
	{
	case EP9FriendActivity::Walk: case EP9FriendActivity::Sit: case EP9FriendActivity::Train: case EP9FriendActivity::Scripted: return false;
	case EP9FriendActivity::Meet: return bMeetArrived;
	default: return true;
	}
}

void AP9FriendNPC::SeparateStanding(float DeltaSeconds)
{
	if (!CanBeMovedBySeparation()) return;
	TArray<P9NPCLoco::FBody> Bodies;
	P9NPCLoco::GatherBodies(GetWorld(), this, Bodies);
	const FVector Loc = GetActorLocation();
	const FVector Feet = Loc - FVector(0.0f, 0.0f, P9Friends::CapsuleHalfHeight);
	FVector Push = FVector::ZeroVector;
	for (const P9NPCLoco::FBody& B : Bodies)
	{
		const FVector BF = B.Actor->GetActorLocation() - FVector(0.0f, 0.0f, B.FeetOffset);
		if (FMath::Abs(BF.Z - Feet.Z) > 120.0f) continue;
		// Stage 8A: box against box (each body's own orientation); identical spots get a sideways nudge
		FVector P;
		if (P9NPCLoco::BoxOverlap2D(Feet, GetActorRotation().Yaw, BodyRadius * BoxDepthScale, BodyRadius * BoxWidthScale, BF, B.Actor->GetActorRotation().Yaw,
			B.HalfDepth, B.HalfWidth, P9Friends::StandGap, P))
		{
			Push += P.IsNearlyZero(0.1f) ? GetActorRightVector() * 2.0f : P;
		}
	}
	if (Push.IsNearlyZero(0.5f)) return;
	const FVector Step = Push.GetClampedToMaxSize(P9Friends::StandPushSpeed * DeltaSeconds);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(P9FriendSeparate), false, this);
	const FVector To = Feet + Step;
	const P9NPCLoco::FFloorProbe Probe = P9NPCLoco::ProbeFloor(GetWorld(), To, Step, Feet.Z, Params, StairTread);
	if (!Probe.bHit || FMath::Abs(Probe.CentreZ - Feet.Z) > 20.0f || P9NPCLoco::MoveHitsWorld(GetWorld(), Feet, To, BodyRadius, Params)) return;   // never off a platform edge or into a wall
	SetActorLocation(FVector(To.X, To.Y, Probe.CentreZ + P9Friends::CapsuleHalfHeight));
}

// ---------------------------------------------------------------------------------------------------------------- drunk lines (Stage 5G)
void AP9FriendNPC::TickDrunkLines()
{
	// mumbles while walking drunk (and while lingering at the stop after a drunk leg); only near the camera
	if (!bDrunkLeg || DrunkLines.IsEmpty() || WorldTime < NextDrunkLine || Voice->IsPlaying()) return;
	if (Activity != EP9FriendActivity::Walk && Activity != EP9FriendActivity::Idle && Activity != EP9FriendActivity::LookAround) return;
	if (!CameraNear(AudibleDistance))
	{
		NextDrunkLine = WorldTime + 1.0f;
		return;
	}
	int32 Pick = Rng.RandRange(0, DrunkLines.Num() - 1);
	if (DrunkLines.Num() > 1 && Pick == LastDrunkLine) Pick = (Pick + 1) % DrunkLines.Num();
	USoundBase* Line = DrunkLines[Pick].LoadSynchronous();
	if (!Line) { NextDrunkLine = WorldTime + 5.0f; return; }
	LastDrunkLine = Pick;
	Voice->SetSound(Line);
	Voice->SetVolumeMultiplier(VoiceVolume);
	Voice->Play();
	NextDrunkLine = WorldTime + Line->GetDuration() + Rng.FRandRange(DrunkLineGap.X, DrunkLineGap.Y);
	UE_LOG(LogP9, Log, TEXT("NPC %s: drunk line %d (%s)"), *FriendName, Pick + 1, *Line->GetName());
}

// ---------------------------------------------------------------------------------------------------------------- Stage 9A: targets, recovery, reversals
bool AP9FriendNPC::HasFloorUnder(const FVector& FeetPoint) const
{
	// floor at this level right under the body centre (a bench seat or counter top is furniture, not floor) and room for the
	// body at chest height (no wall / parapet / closure)
	FCollisionQueryParams Params(SCENE_QUERY_STAT(P9FriendTarget), false, this);
	const P9NPCLoco::FFloorProbe P = P9NPCLoco::ProbeFloor(GetWorld(), FeetPoint, FVector::ForwardVector, FeetPoint.Z, Params, StairTread);
	if (!P.bCentreFloor || FMath::Abs(P.CentreZ - FeetPoint.Z) > 40.0f) return false;
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	return !GetWorld()->OverlapAnyTestByObjectType(FVector(FeetPoint.X, FeetPoint.Y, P.CentreZ + 100.0f), FQuat::Identity, Objects,
		FCollisionShape::MakeSphere(BodyRadius * 0.8f), Params);
}

bool AP9FriendNPC::FindClearFloorNear(const FVector& FeetPoint, float MaxRadius, FVector& Out) const
{
	if (HasFloorUnder(FeetPoint)) { Out = FeetPoint; return true; }
	for (float R = 10.0f; R <= MaxRadius + 0.1f; R += 10.0f)
	{
		for (int32 k = 0; k < 16; ++k)
		{
			const float A = k * (2.0f * PI / 16.0f);
			const FVector P = FeetPoint + FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, 0.0f);
			if (HasFloorUnder(P)) { Out = P; return true; }
		}
	}
	return false;
}

void AP9FriendNPC::ValidateRouteTargets()
{
	// a movement target inside furniture can never be reached (the floor probe refuses to step onto a seat or counter): move it
	// to the nearest clear floor and say so. Legs that cross furniture are reported (the walker recovers round them).
	int32 Moved = 0, Bad = 0;
	auto Fix = [&](FVector& P, const FString& What)
	{
		if (HasFloorUnder(P)) return;
		FVector Clear;
		if (FindClearFloorNear(P, 80.0f, Clear))
		{
			UE_LOG(LogP9, Warning, TEXT("NPC %s: %s %s stands inside furniture or a wall - moved %.0f cm to %s"), *FriendName, *What, *P.ToCompactString(),
				FVector::Dist2D(P, Clear), *Clear.ToCompactString());
			P = FVector(Clear.X, Clear.Y, P.Z);
			++Moved;
		}
		else
		{
			UE_LOG(LogP9, Warning, TEXT("NPC %s: %s %s has no clear floor within 80 cm"), *FriendName, *What, *P.ToCompactString());
			++Bad;
		}
	};
	for (int32 i = 0; i < Route.Num(); ++i) Fix(Route[i].Location, FString::Printf(TEXT("route point %d"), i));
	for (int32 i = 0; i < EpilogueReturnPath.Num(); ++i) Fix(EpilogueReturnPath[i], FString::Printf(TEXT("return-path point %d"), i));
	auto CheckSeg = [&](const FVector& A, const FVector& B, const FString& What)
	{
		if (FMath::Abs(A.Z - B.Z) > 30.0f) return;                                    // stairs: checked by the walker itself
		const int32 N = FMath::Max(1, FMath::CeilToInt(FVector::Dist2D(A, B) / 25.0f));
		for (int32 s = 1; s < N; ++s)
		{
			const FVector P = FMath::Lerp(A, B, static_cast<float>(s) / N);
			if (!HasFloorUnder(P))
			{
				UE_LOG(LogP9, Warning, TEXT("NPC %s: %s crosses furniture or a wall near %s"), *FriendName, *What, *P.ToCompactString());
				++Bad;
				return;
			}
		}
	};
	for (int32 i = 0; i + 1 < Route.Num(); ++i) CheckSeg(Route[i].Location, Route[i + 1].Location, FString::Printf(TEXT("route leg %d-%d"), i, i + 1));
	for (int32 i = 0; i + 1 < EpilogueReturnPath.Num(); ++i) CheckSeg(EpilogueReturnPath[i], EpilogueReturnPath[i + 1], FString::Printf(TEXT("return-path leg %d-%d"), i, i + 1));
	UE_LOG(LogP9, Log, TEXT("NPC %s: route check - %d points, %d return-path points, %d moved, %d warnings"), *FriendName, Route.Num(), EpilogueReturnPath.Num(), Moved, Bad);
}

void AP9FriendNPC::RecoverBlocked(const TCHAR* Reason)
{
	if (Path.IsEmpty()) return;
	++Recoveries;
	const FVector Loc = GetActorLocation();
	const FVector Feet = Loc - FVector(0.0f, 0.0f, P9Friends::CapsuleHalfHeight);
	const float Dist = FVector::Dist2D(Feet, Path[0]);
	// 1) the destination is (nearly) here: it is occupied or inside furniture - arrive where the walker stands
	if (Path.Num() == 1 && Dist < 60.0f && HasFloorUnder(Feet))
	{
		UE_LOG(LogP9, Log, TEXT("NPC %s: %s - target %s %.0f cm away cannot be reached; arrives here"), *FriendName, Reason, *Path[0].ToCompactString(), Dist);
		Path[0] = FVector(Feet.X, Feet.Y, Path[0].Z);
		return;
	}
	// 2) the body centre itself is inside furniture: walk back out to clear floor (no teleport)
	if (!HasFloorUnder(Feet))
	{
		FVector Out = bHaveGoodFeet ? LastGoodFeet : Feet;
		FindClearFloorNear(Feet, 80.0f, Out);
		if (FVector::Dist2D(Out, Feet) > 2.0f)
		{
			UE_LOG(LogP9, Log, TEXT("NPC %s: %s - inside furniture at %s, backs out to %s"), *FriendName, Reason, *Feet.ToCompactString(), *Out.ToCompactString());
			Path.Insert(Out, 0); PathIdx.Insert(INDEX_NONE, 0); bStrictPath = true; bEscaping = true;
			return;
		}
	}
	// 3) a detour beside the blocked line: a clear spot 70-150 cm to the side with clear floor to it and on to the target
	const FVector Dir = (Path[0] - Feet).GetSafeNormal2D();
	const FVector Side(-Dir.Y, Dir.X, 0.0f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(P9FriendDetour), false, this);
	const FVector Goal(Path[0].X, Path[0].Y, Feet.Z);
	for (const float D : {70.0f, 110.0f, 150.0f})
	{
		for (const float S : {1.0f, -1.0f})
		{
			const FVector P = Feet + Side * (S * D) + Dir * 30.0f;
			if (!HasFloorUnder(P) || P9NPCLoco::MoveHitsWorld(GetWorld(), Feet, P, BodyRadius, Params) || P9NPCLoco::MoveHitsWorld(GetWorld(), P, Goal, BodyRadius, Params)) continue;
			bool bClear = true;
			for (int32 s = 1; s < 6 && bClear; ++s)
			{
				bClear = HasFloorUnder(FMath::Lerp(Feet, P, s / 6.0f)) && (FMath::Abs(Path[0].Z - Feet.Z) > 30.0f || HasFloorUnder(FMath::Lerp(P, Goal, s / 6.0f)));
			}
			if (!bClear) continue;
			UE_LOG(LogP9, Log, TEXT("NPC %s: %s - detours via %s"), *FriendName, Reason, *P.ToCompactString());
			Path.Insert(P, 0); PathIdx.Insert(INDEX_NONE, 0); bStrictPath = true;
			return;
		}
	}
	// 4) give up on this point: a pass-through point is skipped, a final point becomes the current spot
	if (Path.Num() > 1)
	{
		UE_LOG(LogP9, Warning, TEXT("NPC %s: %s - no way to %s; point skipped"), *FriendName, Reason, *Path[0].ToCompactString());
		if (PathIdx.Num()) { if (PathIdx[0] != INDEX_NONE) RouteIndex = PathIdx[0]; PathIdx.RemoveAt(0); }
		Path.RemoveAt(0);
	}
	else
	{
		UE_LOG(LogP9, Warning, TEXT("NPC %s: %s - no way to %s; stops here instead"), *FriendName, Reason, *Path[0].ToCompactString());
		Path[0] = FVector(Feet.X, Feet.Y, Path[0].Z);
	}
	bStrictPath = true;
}

void AP9FriendNPC::TickProgressWatchdog(float DeltaSeconds)
{
	// no walker stays put for ever: if the distance to the current path point has not shrunk by 8 cm in 4 s (and it is not
	// yielding to the player or waiting behind a body), recover
	if (Path.IsEmpty() || bExplicitZ) { WatchTime = 0.0f; return; }
	const float D = FVector::Dist2D(GetActorLocation(), Path[0]);
	if (!Path[0].Equals(WatchTarget, 1.0f) || D < WatchBestDist - 8.0f || bHoldingForBody || YieldTime > 0.0f)
	{
		WatchTarget = Path[0]; WatchBestDist = D; WatchTime = 0.0f;
		return;
	}
	if ((WatchTime += DeltaSeconds) > 4.0f)
	{
		WatchTime = 0.0f; WatchBestDist = D;
		RecoverBlocked(TEXT("no progress for 4 s"));
	}
}

bool AP9FriendNPC::DecelerateToStop(float DeltaSeconds)
{
	// keep the walk clip and slow down to rest along the current facing (the clip rate follows the speed): a stop, not a freeze
	if (CurSpeed <= 15.0f || !P9Friends::IsWalkClip(CurrentClip) || bExplicitZ) { CurSpeed = 0.0f; return false; }
	CurSpeed = FMath::Max(0.0f, CurSpeed - P9Friends::WalkDecel * 1.6f * DeltaSeconds);
	const FVector Loc = GetActorLocation();
	const FVector Fwd = GetActorForwardVector().GetSafeNormal2D();
	const FVector FeetOff(0.0f, 0.0f, P9Friends::CapsuleHalfHeight);
	const FVector Next = Loc + Fwd * CurSpeed * DeltaSeconds;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(P9FriendBrake), false, this);
	const P9NPCLoco::FFloorProbe P = P9NPCLoco::ProbeFloor(GetWorld(), Next - FeetOff, Fwd, Loc.Z - FeetOff.Z, Params, StairTread);
	if (!P.bCentreFloor || FMath::Abs(P.CentreZ - (Loc.Z - FeetOff.Z)) > 25.0f || P9NPCLoco::MoveHitsWorld(GetWorld(), Loc - FeetOff, Next - FeetOff, BodyRadius, Params))
	{
		CurSpeed = 0.0f;
		return false;
	}
	SetActorLocation(FVector(Next.X, Next.Y, (bStairClipsFitted && P.bOnStairs ? P.RampZ : P.CentreZ) + FeetOff.Z));
	SetClip(EP9NPCClip::Walk);
	return CurSpeed > 15.0f;
}

bool AP9FriendNPC::HandleReversal(float DeltaSeconds)
{
	// the path turns back behind a walker (a meet called from behind, a route change): it slows to a stop along its facing,
	// turns on the spot (eased), then walks off - never a 180-degree flip or a backwards slide
	if (bExplicitZ || Path.IsEmpty()) { PivotPhase = 0; return false; }
	const FVector Loc = GetActorLocation();
	FVector Aim = PursuitPoint(Loc, 60.0f) - Loc; Aim.Z = 0.0f;
	if (Aim.SizeSquared() < 1.0f) { PivotPhase = 0; return false; }
	const float AimYaw = Aim.Rotation().Yaw;
	const float Delta = FRotator::NormalizeAxis(AimYaw - GetActorRotation().Yaw);
	if (PivotPhase == 0)
	{
		if (FMath::Abs(Delta) < 110.0f || FVector::Dist2D(Loc, Path.Last()) < 50.0f) return false;
		PivotPhase = CurSpeed > 15.0f ? 1 : 2;
	}
	if (PivotPhase == 1)
	{
		if (DecelerateToStop(DeltaSeconds)) return true;
		PivotPhase = 2;
	}
	CurSpeed = 0.0f;
	SetClip(TurnAnim ? EP9NPCClip::Turn : EP9NPCClip::Idle);
	if (TurnTowardsYaw(AimYaw, DeltaSeconds) || FMath::Abs(Delta) < 25.0f)
	{
		PivotPhase = 0;
		SetClip(EP9NPCClip::Walk);
	}
	return true;
}

// ---------------------------------------------------------------------------------------------------------------- Stage 9C bench sitting
float AP9FriendNPC::HipShift() const
{
	// the clip build's 22 cm x height / 1.75 m (measured in PIE for Specs: 20.4 cm vs 21.2 - within 1 cm); the seated
	// measurement is only logged, not fed back (a pose caught mid-blend or at a reduced tick rate gave 11 cm once)
	return SitHipShift > 0.0f ? SitHipShift : SitStandOffset();
}

bool AP9FriendNPC::ProbeSeat(const FVector& StandFeet, const FVector& Fwd, float& OutFrontDist, float& OutSeatZ) const
{
	// walk a vertical probe backwards (against the sit facing) from the stand point: the first hit 30..62 cm above the floor
	// is the seat; its edge is refined to 1 cm
	FCollisionQueryParams Params(SCENE_QUERY_STAT(P9FriendSeat), true, this);
	auto SeatAt = [&](float Dist, float& Z) -> bool
	{
		const FVector P = StandFeet - Fwd * Dist;
		FHitResult H;
		if (!GetWorld()->LineTraceSingleByChannel(H, P + FVector(0.0f, 0.0f, 70.0f), P + FVector(0.0f, 0.0f, 20.0f), ECC_Visibility, Params)) return false;
		const float Rel = H.ImpactPoint.Z - StandFeet.Z;
		if (Rel < 30.0f || Rel > 62.0f) return false;
		Z = H.ImpactPoint.Z;
		return true;
	};
	float Z = 0.0f, Prev = -12.0f;
	for (float D = -12.0f; D <= 80.0f; D += 4.0f)
	{
		if (SeatAt(D, Z))
		{
			float Lo = Prev, Hi = D;
			for (int32 i = 0; i < 4; ++i) { const float M = 0.5f * (Lo + Hi); float Zm; if (SeatAt(M, Zm)) Hi = M; else Lo = M; }
			OutFrontDist = Hi;
			OutSeatZ = Z;
			return true;
		}
		Prev = D;
	}
	return false;
}

void AP9FriendNPC::BeginSitAlign()
{
	// the stand point that puts the pelvis SitSeatInset behind the seat's front edge once the sit clip has moved it back HipShift
	const FVector Fwd = FRotator(0.0f, TargetYaw, 0.0f).Vector();
	const FVector Feet = GetActorLocation() - FVector(0.0f, 0.0f, P9Friends::CapsuleHalfHeight);
	const float Inset = P9Friends::CVarSitInset.GetValueOnGameThread() >= 0.0f ? P9Friends::CVarSitInset.GetValueOnGameThread() : SitSeatInset;
	float Front = 0.0f, SeatZ = 0.0f;
	SitAlignTarget = GetActorLocation();
	SitSeatFront = -1.0f;
	if (ProbeSeat(Feet, Fwd, Front, SeatZ))
	{
		const float Back = Front + Inset - HipShift();            // >0: the stand point is further back (towards the seat)
		SitAlignTarget = GetActorLocation() - Fwd * FMath::Clamp(Back, -40.0f, 40.0f);
		SitSeatFront = Front - FMath::Clamp(Back, -40.0f, 40.0f);
		SitSeatZ = SeatZ;
		UE_LOG(LogP9, Log, TEXT("NPC %s: sit - seat front %.1f cm behind the stop, seat %.1f cm high, pelvis target %.0f cm in, hip shift %.1f -> stand point moves %.1f cm"),
			*FriendName, Front, SeatZ - Feet.Z, Inset, HipShift(), Back);
	}
	else UE_LOG(LogP9, Log, TEXT("NPC %s: sit - no seat found behind the stop (sits where it stands)"), *FriendName);
	const float D = FVector::Dist2D(SitAlignTarget, GetActorLocation());
	bSitAligned = D < 0.5f;
	const float Turn = FMath::Abs(FRotator::NormalizeAxis(TargetYaw - GetActorRotation().Yaw));
	if (D > 22.0f) { SitPhase = 5; CurSpeed = 0.0f; SetClip(EP9NPCClip::Idle); }
	else { SitPhase = 0; SetClip((Turn > 20.0f || !bSitAligned) && TurnAnim ? EP9NPCClip::Turn : EP9NPCClip::Idle); }
}

void AP9FriendNPC::MeasureSeated()
{
	// once seated: where did the pelvis land (along the facing, behind the stand point) - logged for checking
	const FName Bone = Body->DoesSocketExist(TEXT("Hips")) ? FName(TEXT("Hips")) : (Body->DoesSocketExist(TEXT("pelvis")) ? FName(TEXT("pelvis")) : NAME_None);
	if (Bone.IsNone()) return;
	// an NPC nobody sees keeps a stale pose (bones only refresh when rendered): measure only what is actually on screen
	if (!Body->WasRecentlyRendered(0.25f))
	{
		UE_LOG(LogP9, Verbose, TEXT("NPC %s: seated - not on screen, pose not measured"), *FriendName);
		return;
	}
	const FVector Fwd = GetActorRotation().Vector().GetSafeNormal2D();
	const FVector Hips = Body->GetSocketLocation(Bone);
	const FVector Feet = GetActorLocation() - FVector(0.0f, 0.0f, P9Friends::CapsuleHalfHeight);
	const float Shift = FVector::DotProduct(GetActorLocation() - Hips, Fwd);
	FString Feet2;
	for (const TCHAR* F : {TEXT("LeftFoot"), TEXT("RightFoot")})
	{
		if (Body->DoesSocketExist(F)) Feet2 += FString::Printf(TEXT(" %s %.0f cm ahead / %.0f cm up"), F, FVector::DotProduct(Body->GetSocketLocation(F) - GetActorLocation(), Fwd),
			Body->GetSocketLocation(F).Z - Feet.Z);
	}
	UE_LOG(LogP9, Log, TEXT("NPC %s: seated - pelvis %.1f cm behind the stand point (%.1f behind the seat front), %.1f cm above the seat;%s"),
		*FriendName, Shift, SitSeatFront >= 0.0f ? Shift - SitSeatFront : -1.0f, Hips.Z - (SitSeatFront >= 0.0f ? SitSeatZ : Feet.Z + 45.0f), *Feet2);
	MeasuredHipShift = Shift; bSitMeasured = true;                                  // test readout only
}

void AP9FriendNPC::DebugSit()
{
	if (Activity == EP9FriendActivity::Train || Activity == EP9FriendActivity::Scripted || !bAvailableNow) return;
	if (Activity == EP9FriendActivity::Sit && SitPhase == 2) { DwellLeft = 0.0f; return; }
	for (int32 i = 0; i < Route.Num(); ++i)
	{
		if (Route[i].Stop != EP9NPCStop::Sit) continue;
		if (Activity == EP9FriendActivity::Sit) { SitPhase = 0; }
		TargetIndex = i;
		DwellLeft = 0.0f;
		bDrunkLeg = false;
		UE_LOG(LogP9, Log, TEXT("NPC %s: p9.Friends.Sit - going to sit stop %d"), *FriendName, i);
		WalkPath(RoutePathTo(i));
		return;
	}
}
