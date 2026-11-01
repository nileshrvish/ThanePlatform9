#include "P9SilhouetteStalker.h"
#include "P9NPCAnimInstance.h"

#include "P9CampaignDirector.h"
#include "P9Train.h"
#include "ThanePlatform9.h"
#include "Animation/AnimSequence.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
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
	Hide();
	NextAllowed = 10.0f;
	UE_LOG(LogP9, Log, TEXT("Silhouette: ready (%d lines, director %s, day chance %.2f)"), Lines.Num(), Director ? TEXT("found") : TEXT("missing"), DayChance);
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

bool AP9SilhouetteStalker::FindSpotBehind(FVector& Out) const
{
	const APawn* P = UGameplayStatics::GetPlayerPawn(this, 0);
	const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!P || !Cam) return false;
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
	Playing = nullptr; Play(WalkAnim);
	State = EState::Following; StateTime = 0.0f; SeenTime = 0.0f;
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
	const APawn* P = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!P) return;
	const float Speed = P->GetVelocity().Size2D();
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
			if (LineBag.IsEmpty()) { for (int32 i = 0; i < Lines.Num(); ++i) if (i != LastLine || Lines.Num() == 1) LineBag.Add(i); }
			const int32 Pick = LineBag[FMath::RandRange(0, LineBag.Num() - 1)]; LineBag.Remove(Pick); LastLine = Pick;
			USoundBase* S = Lines[Pick].LoadSynchronous();
			SpeakLeft = S ? FMath::Max(1.0f, S->GetDuration()) : 3.0f;
			if (S) { Voice->SetSound(S); Voice->SetVolumeMultiplier(VoiceVolume); Voice->Play(); }
			Play(TalkAnim ? TalkAnim.Get() : MenaceAnim.Get());
			State = EState::Speaking; StateTime = 0.0f;
			UE_LOG(LogP9, Log, TEXT("Silhouette: noticed - line %d (%.1fs)"), Pick + 1, SpeakLeft);
			return;
		}
		// keep her distance behind the player; stand still (menace) when the player stops
		FVector To = P->GetActorLocation() - GetActorLocation(); To.Z = 0.0f;
		SetActorRotation(FRotator(0.0f, To.Rotation().Yaw, 0.0f));
		if (To.Size() > FollowDistance * 1.8f)
		{
			FVector Spot;                         // fell too far behind: re-place behind the player while unseen
			if (!PlayerSees(0.2f) && FindSpotBehind(Spot)) SetActorLocation(Spot); else if (StateTime > 25.0f) Hide();
			return;
		}
		if (Speed > 30.0f && To.Size() > FollowDistance * 0.8f)
		{
			Play(WalkAnim);
			const float MoveSpeed = FMath::Min(Speed, 300.0f);
			if (AnimInst) AnimInst->SetRate(FMath::Clamp(MoveSpeed / WalkClipSpeed, 0.5f, 2.6f));     // Stage 5C: no foot sliding
			FVector Next = GetActorLocation() + To.GetSafeNormal() * MoveSpeed * DeltaSeconds;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(P9SilhouetteFloor), false, this);
			FHitResult Floor;
			if (GetWorld()->LineTraceSingleByChannel(Floor, Next + FVector(0, 0, 80), Next - FVector(0, 0, 250), ECC_Visibility, Params)) Next.Z = Floor.ImpactPoint.Z;
			SetActorLocation(Next);
		}
		else { Play(MenaceAnim ? MenaceAnim.Get() : IdleAnim.Get()); if (AnimInst) AnimInst->SetRate(1.0f); }
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

void AP9SilhouetteStalker::LogStatus() const
{
	static const TCHAR* S[] = {TEXT("Hidden"), TEXT("Following"), TEXT("Speaking"), TEXT("Lingering")};
	UE_LOG(LogP9, Log, TEXT("Silhouette: %s t=%.1f force=%d dayActive=%d today=%d/%d next=%.0f loc=%s lines=%d director=%s lock=%d epilogue=%d phaseAllows=%d"), S[(int32)State], StateTime,
		bForceActive, bDayActive, AppearancesToday, MaxAppearancesPerDay, FMath::Max(0.0f, NextAllowed - Now), *GetActorLocation().ToCompactString(), Lines.Num(),
		Director ? TEXT("yes") : TEXT("no"), bHideUntilCampaignComplete ? 1 : 0, bAppearInEpilogue ? 1 : 0, PhaseAllows() ? 1 : 0);
}
