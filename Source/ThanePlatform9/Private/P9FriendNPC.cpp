#include "P9FriendNPC.h"

#include "P9CampaignDirector.h"
#include "P9NPCAnimInstance.h"
#include "P9SeatComponent.h"
#include "P9Train.h"
#include "ThanePlatform9.h"
#include "TimerManager.h"
#include "Animation/AnimSequence.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
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

	// Stage 5B: -1 = follow the campaign (default), 0 = keep locked NPCs hidden, 1 = show them (test)
	static TAutoConsoleVariable<int32> CVarForceUnlock(TEXT("p9.Friends.ForceUnlock"), -1,
		TEXT("p9.Friends.ForceUnlock -1|0|1 - NPCs hidden until the campaign is completed: -1 follow the campaign, 0 force hidden, 1 force available (test)"));
	constexpr float AvailabilityPollSeconds = 0.5f;

	static bool IsLoopClip(EP9NPCClip C) { return C != EP9NPCClip::SitDown && C != EP9NPCClip::StandUp; }
	static bool IsWalkClip(EP9NPCClip C) { return C == EP9NPCClip::Walk || C == EP9NPCClip::DrunkWalk || C == EP9NPCClip::StairsUp; }
	// Stage 5C crossfade times: sit transitions start/end in the neighbouring clip's pose, so they need only a short blend
	static float BlendFor(EP9NPCClip From, EP9NPCClip To)
	{
		if (From == EP9NPCClip::None) return 0.0f;
		if (From == EP9NPCClip::SitDown || From == EP9NPCClip::StandUp || To == EP9NPCClip::SitDown || To == EP9NPCClip::StandUp) return 0.15f;
		if (IsWalkClip(From) || IsWalkClip(To)) return 0.3f;
		return 0.35f;
	}
	// Stage 5C walking
	constexpr float WalkAccel = 220.0f;      // cm/s^2 when setting off
	constexpr float WalkDecel = 170.0f;      // cm/s^2 when arriving
	constexpr float MinArriveSpeed = 25.0f;
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
	Capsule->SetCollisionObjectType(ECC_Pawn);
	Capsule->SetCollisionResponseToAllChannels(ECR_Ignore);
	if (bBlockPlayer)
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	}
	else
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
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
	default: return IdleAnim;
	}
}

EP9NPCClip AP9FriendNPC::MoveClip() const
{
	// Stage 5G: the walking clip for the moment - stair climb where the path rises, drunk on drunk legs, else the walk
	if (bClimbing && StairsUpAnim) return EP9NPCClip::StairsUp;
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
	// Stage 5C: eases into the final facing instead of stopping dead at full turn speed
	const float Step = TurnDegreesPerSecond * DeltaSeconds * FMath::Clamp(FMath::Abs(Delta) / 35.0f, 0.3f, 1.0f);
	if (FMath::Abs(Delta) <= Step)
	{
		SetActorRotation(FRotator(0.0f, Yaw, 0.0f));
		return true;
	}
	SetActorRotation(FRotator(0.0f, Current + FMath::Sign(Delta) * Step, 0.0f));
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
	// Stage 5C: heading looks ahead round the corner at a path point; speed eases in, slows for sharp turns and eases out
	FVector Dir = Dist > KINDA_SMALL_NUMBER ? To / Dist : GetActorForwardVector();
	if (Path.Num() > 1 && Dist < 80.0f)
	{
		const FVector NextDir = (Path[1] - Path[0]).GetSafeNormal2D();
		if (!NextDir.IsNearlyZero()) Dir = FMath::Lerp(NextDir, Dir, Dist / 80.0f).GetSafeNormal2D();
	}
	const float YawNow = GetActorRotation().Yaw;
	const float YawDelta = FRotator::NormalizeAxis(Dir.Rotation().Yaw - YawNow);
	{
		// Stage 5G: stairs up = the next path point is clearly higher than the feet over a short horizontal run
		const float Rise = Path[0].Z - (Loc.Z - P9Friends::CapsuleHalfHeight);
		ClimbSlope = Dist > 1.0f ? Rise / Dist : 0.0f;
		bClimbing = !bExplicitZ && Rise > 40.0f && ClimbSlope > 0.25f;
	}
	const float BaseSpeed = bClimbing && StairsUpAnim ? StairsUpSpeed : (bDrunkLeg && DrunkWalkAnim && Activity == EP9FriendActivity::Walk ? DrunkWalkSpeed : WalkSpeed);
	float Target = BaseSpeed * PlayRate * SpeedScale * FMath::Lerp(1.0f, 0.55f, FMath::Clamp(FMath::Abs(YawDelta) / 90.0f, 0.0f, 1.0f));
	if (Path.Num() == 1) Target = FMath::Min(Target, P9Friends::MinArriveSpeed + FMath::Sqrt(2.0f * P9Friends::WalkDecel * Dist));
	CurSpeed = CurSpeed < Target ? FMath::Min(Target, CurSpeed + P9Friends::WalkAccel * DeltaSeconds) : FMath::Max(Target, CurSpeed - P9Friends::WalkDecel * 1.5f * DeltaSeconds);
	const float Step = CurSpeed * DeltaSeconds;
	FVector Next = Dist <= Step ? FVector(Path[0].X, Path[0].Y, Loc.Z) : Loc + To / Dist * Step;
	if (bExplicitZ)
	{
		Next.Z = FMath::FInterpConstantTo(Loc.Z, Path[0].Z + P9Friends::CapsuleHalfHeight, DeltaSeconds, 150.0f);   // train floor / door step
	}
	else
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(P9FriendFloor), false, this);
		FHitResult Floor;
		if (GetWorld()->LineTraceSingleByChannel(Floor, Next + FVector(0, 0, 60), Next - FVector(0, 0, 250), ECC_Visibility, Params))
		{
			// follows stairs and slopes, but never climbs onto furniture (a bench seat right at a sit stop is ~45 cm up)
			if (Floor.ImpactPoint.Z - (Loc.Z - P9Friends::CapsuleHalfHeight) < 35.0f)
			{
				Next.Z = Floor.ImpactPoint.Z + P9Friends::CapsuleHalfHeight;
			}
		}
	}
	SetActorLocation(Next);
	if (Dist > 1.0f)
	{
		const float MaxStep = TurnDegreesPerSecond * 1.6f * DeltaSeconds;
		const float Eased = YawDelta * (1.0f - FMath::Exp(-7.0f * DeltaSeconds));
		SetActorRotation(FRotator(0.0f, YawNow + FMath::Clamp(Eased, -MaxStep, MaxStep), 0.0f));
	}
	if (Dist <= Step)
	{
		if (PathIdx.Num()) { if (PathIdx[0] != INDEX_NONE) RouteIndex = PathIdx[0]; PathIdx.RemoveAt(0); }
		Path.RemoveAt(0);
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
		SitPhase = 0;
		Activity = EP9FriendActivity::Sit;    // turn first (below), then sit
		SetClip(FMath::Abs(FRotator::NormalizeAxis(TargetYaw - GetActorRotation().Yaw)) > 25.0f ? EP9NPCClip::Turn : EP9NPCClip::Idle);
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
	if (Activity == EP9FriendActivity::Sit && SitPhase >= 1)
	{
		bMeetAfterStand = true;                      // stand up first; Tick continues the meet
		SitPhase = 3; SetClip(EP9NPCClip::StandUp);
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
		else if (CurrentClip == EP9NPCClip::StairsUp) Rate = ClipSpeed * FMath::Sqrt(1.0f + ClimbSlope * ClimbSlope) / FMath::Max(10.0f, StairsUpClipSpeed);
		AnimInst->SetRate(P9Friends::IsWalkClip(CurrentClip) ? FMath::Clamp(Rate, 0.3f, 1.8f) : Rate);
	}
	TickDrunkLines();
	if (!P9Friends::IsWalkClip(CurrentClip)) CurSpeed = 0.0f;               // paused (yielding, turning, standing): set off again from rest
	if ((TrainCheck -= DeltaSeconds) <= 0.0f)
	{
		TrainCheck = 0.5f;
		TryBoardEpilogueTrain();
	}

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
			if (PlayerBlocksWay() && (YieldTime += DeltaSeconds) < 6.0f) { SetClip(EP9NPCClip::Idle); return; }
			SetClip(EP9NPCClip::Walk);
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
			if ((YieldTime += DeltaSeconds) < 2.5f) { SetClip(EP9NPCClip::Idle); return; }      // a short pause to let the player pass
			if (const APawn* PP = UGameplayStatics::GetPlayerPawn(this, 0); PP && YieldTime < 100.0f)
			{
				// still in the way: step round the player (side away from the player, back onto the path)
				const FVector Dir = (Path[0] - GetActorLocation()).GetSafeNormal2D();
				FVector Side = FVector::CrossProduct(Dir, FVector::UpVector);
				if (FVector::DotProduct(Side, PP->GetActorLocation() - GetActorLocation()) > 0.0f) Side = -Side;
				Path.Insert(PP->GetActorLocation() + Side * 95.0f + Dir * 40.0f - FVector(0, 0, 90), 0); PathIdx.Insert(INDEX_NONE, 0);
				YieldTime = 100.0f;                                                                    // no further yielding until clear
			}
		}
		else YieldTime = 0.0f;
		SetClip(EP9NPCClip::Walk);
		if (P9Friends::IsWalkClip(CurrentClip) || !WalkAnim)
		{
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
		case 0:   // turn to the bench's facing, then sit down
			if (TurnTowardsYaw(TargetYaw, DeltaSeconds)) { SitPhase = 1; SetClip(EP9NPCClip::SitDown); }
			break;
		case 1:
			if (CurrentClip == EP9NPCClip::SitDown && OneShotDone()) { SitPhase = 2; SetClip(EP9NPCClip::SitIdle); }
			break;
		case 2:
			if ((DwellLeft -= DeltaSeconds) <= 0.0f) { SitPhase = 3; SetClip(EP9NPCClip::StandUp); }
			break;
		case 3:
			if (CurrentClip == EP9NPCClip::StandUp && OneShotDone())
			{
				SitPhase = 0; DwellLeft = 0.0f;
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
		|| Activity == EP9FriendActivity::Turn || Activity == EP9FriendActivity::Walk || (Activity == EP9FriendActivity::Sit && SitPhase == 0);
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
			SetClip(TurnAnim ? EP9NPCClip::Turn : EP9NPCClip::Idle);
		}
		return;
	case 2:   // turn to face away from the seat back
		if (TurnTowardsYaw(SeatYaw, DeltaSeconds)) { TrainPhase = 3; SetClip(EP9NPCClip::SitDown); }
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
		SetClip(EP9NPCClip::Walk);
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
