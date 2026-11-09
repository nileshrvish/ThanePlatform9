#include "P9AudioDirector.h"
#include "P9Captions.h"
#include "ThanePlatform9.h"
#include "P9CampaignDirector.h"
#include "P9SilhouetteStalker.h"
#include "P9Train.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Components/LightComponent.h"
#include "Engine/DirectionalLight.h"
#include "P9StormController.h"
#include "P9ReverseEscalator.h"
#include "Sound/SoundAttenuation.h"

namespace P9AudioPack
{
	TSoftObjectPtr<USoundBase> S(const TCHAR* Folder, const TCHAR* Name)
	{
		return TSoftObjectPtr<USoundBase>(FSoftObjectPath(FString::Printf(TEXT("/Game/P9/Audio/OriginalAudioPack/%s/%s.%s"), Folder, Name, Name)));
	}
	// Stage 9A additions live in their own folder (nothing of the 8A pack is replaced on disk)
	TSoftObjectPtr<USoundBase> S9(const TCHAR* Folder, const FString& Name)
	{
		return TSoftObjectPtr<USoundBase>(FSoftObjectPath(FString::Printf(TEXT("/Game/P9/Audio/OriginalAudioPack9A/%s/%s.%s"), Folder, *Name, *Name)));
	}
	static FAutoConsoleCommandWithWorldAndArgs ThunderCommand(TEXT("p9.Audio.Thunder"), TEXT("p9.Audio.Thunder [0 distant|1 strong|2 close] [delay s] - queue a thunder (test)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			for (TActorIterator<AP9AudioDirector> It(World); It; ++It)
			{
				It->QueueThunder(Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) : 1, Args.IsValidIndex(1) ? FCString::Atof(*Args[1]) : 0.0f);
			}
		}));
	static FAutoConsoleCommandWithWorld StatusCommand(TEXT("p9.Audio.Status"), TEXT("p9.Audio.Status - the original audio pack's layers and volumes"),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			for (TActorIterator<AP9AudioDirector> It(World); It; ++It) UE_LOG(LogP9, Log, TEXT("p9.Audio.Status: %s"), *It->GetStatus());
		}));
}

AP9AudioDirector::AP9AudioDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetHidden(true);
	using namespace P9AudioPack;
	MainAmbience = S(TEXT("Music"), TEXT("BGM_Main_Station_Ambience"));
	Platform9Horror = S(TEXT("Music"), TEXT("BGM_Platform9_Horror"));
	AnomalyAmbience = S(TEXT("Music"), TEXT("BGM_Anomaly_Ambience"));
	EpilogueMusic = S(TEXT("Music"), TEXT("BGM_Final_Epilogue"));
	PlatformWind = S9(TEXT("Ambience"), TEXT("AMB_Platform_Wind9A_Loop"));      // Stage 9A: slower, deeper, softer breeze (8A loop kept on disk)
	FootbridgeBed = S(TEXT("Ambience"), TEXT("AMB_Footbridge_Loop"));
	for (int32 i = 1; i <= 4; ++i)
	{
		StepsPlatform.Add(S(TEXT("SFX"), *FString::Printf(TEXT("SFX_Footstep_Platform_%02d"), i)));
		StepsStairs.Add(S(TEXT("SFX"), *FString::Printf(TEXT("SFX_Footstep_Stairs_%02d"), i)));
		StepsFootbridge.Add(S(TEXT("SFX"), *FString::Printf(TEXT("SFX_Footstep_Footbridge_%02d"), i)));
	}
	// Stage 9C footsteps: /Game/P9/Audio/OriginalAudioPack9C/Player/SFX_Step9C_<Surface>_<Walk|Run>_<L|R><n>, ..._Scuff<n>
	auto Fill9C = [](FP9StepSet& Set, const TCHAR* Surface)
	{
		auto P = [Surface](const FString& Rest)
		{
			const FString N = FString::Printf(TEXT("SFX_Step9C_%s_%s"), Surface, *Rest);
			return TSoftObjectPtr<USoundBase>(FSoftObjectPath(FString::Printf(TEXT("/Game/P9/Audio/OriginalAudioPack9C/Player/%s.%s"), *N, *N)));
		};
		for (int32 i = 1; i <= 4; ++i) { Set.WalkL.Add(P(FString::Printf(TEXT("Walk_L%d"), i))); Set.WalkR.Add(P(FString::Printf(TEXT("Walk_R%d"), i))); }
		for (int32 i = 1; i <= 3; ++i) { Set.RunL.Add(P(FString::Printf(TEXT("Run_L%d"), i))); Set.RunR.Add(P(FString::Printf(TEXT("Run_R%d"), i))); }
		for (int32 i = 1; i <= 2; ++i) Set.Scuff.Add(P(FString::Printf(TEXT("Scuff%d"), i)));
	};
	Fill9C(Steps9C_Platform, TEXT("Platform")); Fill9C(Steps9C_Footbridge, TEXT("Footbridge")); Fill9C(Steps9C_Concourse, TEXT("Concourse"));
	Fill9C(Steps9C_Stairs, TEXT("Stairs")); Fill9C(Steps9C_Metal, TEXT("Metal")); Fill9C(Steps9C_Train, TEXT("Train"));
	// Stage 9A footsteps: SFX_Step9A_<Surface>_<Walk|Run>_<L|R><n>
	auto Fill = [](TArray<TSoftObjectPtr<USoundBase>>& Set, const TCHAR* Surface, const TCHAR* Gait, const TCHAR* Foot, int32 Count)
	{
		for (int32 i = 1; i <= Count; ++i) Set.Add(S9(TEXT("Player"), FString::Printf(TEXT("SFX_Step9A_%s_%s_%s%d"), Surface, Gait, Foot, i)));
	};
	Fill(Steps9A_Platform_Walk_L, TEXT("Platform"), TEXT("Walk"), TEXT("L"), 3); Fill(Steps9A_Platform_Walk_R, TEXT("Platform"), TEXT("Walk"), TEXT("R"), 3);
	Fill(Steps9A_Platform_Run_L, TEXT("Platform"), TEXT("Run"), TEXT("L"), 2); Fill(Steps9A_Platform_Run_R, TEXT("Platform"), TEXT("Run"), TEXT("R"), 2);
	Fill(Steps9A_Footbridge_Walk_L, TEXT("Footbridge"), TEXT("Walk"), TEXT("L"), 3); Fill(Steps9A_Footbridge_Walk_R, TEXT("Footbridge"), TEXT("Walk"), TEXT("R"), 3);
	Fill(Steps9A_Footbridge_Run_L, TEXT("Footbridge"), TEXT("Run"), TEXT("L"), 2); Fill(Steps9A_Footbridge_Run_R, TEXT("Footbridge"), TEXT("Run"), TEXT("R"), 2);
	Fill(Steps9A_Stairs_Walk_L, TEXT("Stairs"), TEXT("Walk"), TEXT("L"), 3); Fill(Steps9A_Stairs_Walk_R, TEXT("Stairs"), TEXT("Walk"), TEXT("R"), 3);
	Fill(Steps9A_Stairs_Run_L, TEXT("Stairs"), TEXT("Run"), TEXT("L"), 2); Fill(Steps9A_Stairs_Run_R, TEXT("Stairs"), TEXT("Run"), TEXT("R"), 2);
	for (int32 i = 1; i <= 4; ++i) ThunderDistant.Add(S9(TEXT("Weather"), FString::Printf(TEXT("SFX_Thunder9A_DistantRoll_%02d"), i)));
	for (int32 i = 1; i <= 3; ++i) ThunderStrong.Add(S9(TEXT("Weather"), FString::Printf(TEXT("SFX_Thunder9A_Strong_%02d"), i)));
	for (int32 i = 1; i <= 2; ++i) ThunderClose.Add(S9(TEXT("Weather"), FString::Printf(TEXT("SFX_Thunder9A_Close_%02d"), i)));
	JumpSound = S(TEXT("SFX"), TEXT("SFX_Jump"));
	LandSound = S(TEXT("SFX"), TEXT("SFX_Land"));
	KanchanaBreathing = S(TEXT("Creatures"), TEXT("SFX_Kanchana_Breathing_Loop"));
	KanchanaPresence = S(TEXT("Creatures"), TEXT("SFX_Kanchana_Presence_Loop"));
	for (int32 i = 1; i <= 3; ++i)
	{
		KanchanaStepsClose.Add(S(TEXT("Creatures"), *FString::Printf(TEXT("SFX_Kanchana_Footstep_Close_%02d"), i)));
		KanchanaStepsDistant.Add(S(TEXT("Creatures"), *FString::Printf(TEXT("SFX_Kanchana_Footstep_Distant_%02d"), i)));
	}
}

UAudioComponent* AP9AudioDirector::MakeLoop(USoundBase* Sound, const TCHAR* Name)
{
	if (!Sound) return nullptr;
	UAudioComponent* A = NewObject<UAudioComponent>(this, Name, RF_Transient);
	A->SetupAttachment(RootComponent);
	A->bAutoActivate = false;
	A->bAllowSpatialization = false;
	A->bIsUISound = false;
	A->SetSound(Sound);
	A->SetVolumeMultiplier(0.0f);
	A->RegisterComponent();
	Owned.Add(A);
	return A;
}

void AP9AudioDirector::BeginPlay()
{
	Super::BeginPlay();
	Rng.Initialize(GetTypeHash(GetName()) ^ 0x8A);
	for (TActorIterator<AP9CampaignDirector> It(GetWorld()); It; ++It) { Director = *It; break; }
	for (TActorIterator<AP9SilhouetteStalker> It(GetWorld()); It; ++It) { Stalker = *It; break; }
	for (TActorIterator<AP9StormController> It(GetWorld()); It; ++It) { Storm = *It; break; }
	if (Storm)
	{
		for (TActorIterator<ADirectionalLight> It(GetWorld()); It; ++It) { if (It->ActorHasTag(Storm->MoonLightTag)) { Moon = *It; break; } }
	}
	Main.Comp = MakeLoop(MainAmbience.LoadSynchronous(), TEXT("Main"));
	P9.Comp = MakeLoop(Platform9Horror.LoadSynchronous(), TEXT("Platform9"));
	Anomaly.Comp = MakeLoop(AnomalyAmbience.LoadSynchronous(), TEXT("Anomaly"));
	Epilogue.Comp = MakeLoop(EpilogueMusic.LoadSynchronous(), TEXT("Epilogue"));
	Wind.Comp = MakeLoop(PlatformWind.LoadSynchronous(), TEXT("Wind"));
	Bridge.Comp = MakeLoop(FootbridgeBed.LoadSynchronous(), TEXT("Bridge"));
	Presence.Comp = MakeLoop(KanchanaPresence.LoadSynchronous(), TEXT("Presence"));
	Epilogue.bLoop = false;
	// Stage 9C: near-field attenuation for the footsteps - full level within 4 m, 3D at the foot but blended mostly to the
	// centre when this close (the feet are ~1.6 m below the ears), so left / right steps lean gently instead of hard-panning
	StepAttenuation = FootstepAttenuation;
	if (!StepAttenuation)
	{
		StepAttenuation = NewObject<USoundAttenuation>(this, TEXT("StepAttenuation9C"), RF_Transient);
		FSoundAttenuationSettings& A = StepAttenuation->Attenuation;
		A.bAttenuate = true;
		A.bSpatialize = true;
		A.DistanceAlgorithm = EAttenuationDistanceModel::Linear;
		A.AttenuationShape = EAttenuationShape::Sphere;
		A.AttenuationShapeExtents = FVector(400.0f, 0.0f, 0.0f);
		A.FalloffDistance = 1600.0f;
		A.NonSpatializedRadiusStart = 260.0f;
		A.NonSpatializedRadiusEnd = 90.0f;
		A.NonSpatializedRadiusMode = ENonSpatializedRadiusSpeakerMapMode::OmniDirectional;
		A.bEnableOcclusion = false;
	}
	{
		FHitResult H;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(P9StepConcourse), false);
		if (GetWorld()->LineTraceSingleByChannel(H, ConcourseProbe, ConcourseProbe - FVector(0.0f, 0.0f, 250.0f), ECC_Visibility, Q)) ConcourseFloor = H.GetActor();
	}
	Main.FadeRate = 0.12f; P9.FadeRate = 0.10f; Anomaly.FadeRate = 0.06f; Epilogue.FadeRate = 0.08f; Wind.FadeRate = 0.08f; Bridge.FadeRate = 0.2f; Presence.FadeRate = 0.15f;
	UE_LOG(LogP9, Log, TEXT("Audio: original audio pack ready (director %s, Kanchana %s, layers %d)"), Director ? TEXT("found") : TEXT("missing"),
		Stalker ? TEXT("found") : TEXT("missing"), Owned.Num());
}

void AP9AudioDirector::DriveLayer(FLayer& L, float DeltaSeconds)
{
	if (!L.Comp) return;
	L.Volume = FMath::FInterpConstantTo(L.Volume, L.Target, DeltaSeconds, L.FadeRate);
	if (L.Volume > 0.001f)
	{
		if (!L.Comp->IsPlaying() && L.bLoop) L.Comp->Play(Rng.FRandRange(0.0f, 20.0f));   // loops start somewhere inside, never always at 0:00
		L.Comp->SetVolumeMultiplier(L.Volume);
	}
	else if (L.Comp->IsPlaying() && L.Target <= 0.0f)
	{
		L.Comp->Stop();
	}
}

void AP9AudioDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const ACharacter* C = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
	if (!Director || !C || !bEnabled)
	{
		for (FLayer* L : {&Main, &P9, &Anomaly, &Epilogue, &Wind, &Bridge, &Presence}) { L->Target = 0.0f; DriveLayer(*L, DeltaSeconds); }
		return;
	}
	const FVector Feet = C->GetActorLocation() - FVector(0.0f, 0.0f, C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	const EP9DayPhase Phase = Director->GetPhase();
	const bool bAboard = (Director->Train && Director->Train->IsPlayerAboard()) || (Director->OpeningTrain && Director->OpeningTrain->IsPlayerAboard());
	const bool bOnBridge = Feet.Z > 540.0f;
	// closeness to Platform 9: the north footbridge (from the Platform 8 stairs on), the PF9/10 stairs and the island
	float Near9 = FMath::Clamp((Feet.Y - 5600.0f) / 1400.0f, 0.0f, 1.0f);
	if (Feet.Y > 6800.0f && Feet.Z < 400.0f) Near9 = 1.0f;
	const float Inside = bAboard ? 0.4f : 1.0f;
	Main.Target = Wind.Target = Bridge.Target = P9.Target = Anomaly.Target = Epilogue.Target = 0.0f;
	if (Phase == EP9DayPhase::Exploring || Phase == EP9DayPhase::Resolving || Phase == EP9DayPhase::Opening || Phase == EP9DayPhase::Inactive)
	{
		Main.Target = MainVolume * (1.0f - 0.45f * Near9) * (Phase == EP9DayPhase::Opening ? 0.7f : 1.0f) * Inside;
		P9.Target = Phase == EP9DayPhase::Opening ? 0.0f : Platform9Volume * Near9 * Inside;
		Wind.Target = !bOnBridge ? WindVolume * Inside : 0.0f;
		Bridge.Target = bOnBridge && !bAboard ? FootbridgeVolume : 0.0f;
		// only once the player has seen / met the day's anomaly (normal days: never)
		Anomaly.Target = Director->IsAnyAnomalyNoticed() ? AnomalyVolume : 0.0f;
		if (Phase == EP9DayPhase::Resolving) { Main.Target *= 0.4f; P9.Target = 0.0f; Anomaly.Target = 0.0f; }
	}
	else if (Phase == EP9DayPhase::Epilogue)
	{
		if (!bEpilogueStarted && Epilogue.Comp)
		{
			bEpilogueStarted = true;
			Epilogue.Comp->Play(0.0f);
			Epilogue.Volume = 0.01f;
		}
		// the last ride: everything fades out before the lights fail - the Greeting comes out of silence
		const bool bFadeOut = Director->GetEpilogueStep() >= 2 && Director->GetRideTime() >= 22.0f;
		Epilogue.Target = bFadeOut ? 0.0f : EpilogueVolume;
		Epilogue.FadeRate = bFadeOut ? 0.09f : 0.08f;
		Wind.Target = !bOnBridge && !bFadeOut ? WindVolume * 0.6f * Inside : 0.0f;
		Bridge.Target = bOnBridge && !bFadeOut && !bAboard ? FootbridgeVolume * 0.6f : 0.0f;
	}
	else if (Phase == EP9DayPhase::Ending)
	{
		if (!bEndingStarted && Epilogue.Comp)
		{
			bEndingStarted = true;
			Epilogue.Comp->Stop();
			Epilogue.Comp->Play(58.0f);                                      // the second pass and the dark ending under the final screen
			Epilogue.Volume = 0.01f;
		}
		Epilogue.Target = EpilogueVolume;
	}
	if (Phase != EP9DayPhase::Epilogue && Phase != EP9DayPhase::Ending) { bEpilogueStarted = false; bEndingStarted = false; }
	for (FLayer* L : {&Main, &P9, &Anomaly, &Epilogue, &Wind, &Bridge}) DriveLayer(*L, DeltaSeconds);
	TickFootsteps(DeltaSeconds);
	TickKanchana(DeltaSeconds);
	TickThunder(DeltaSeconds);
	DriveLayer(Presence, DeltaSeconds);
}

USoundBase* AP9AudioDirector::Pick(const TArray<TSoftObjectPtr<USoundBase>>& Set, int32& Last)
{
	if (Set.IsEmpty()) return nullptr;
	int32 i = Rng.RandRange(0, Set.Num() - 1);
	if (Set.Num() > 1 && i == Last) i = (i + 1) % Set.Num();
	Last = i;
	return Set[i].LoadSynchronous();
}

void AP9AudioDirector::TickFootsteps(float DeltaSeconds)
{
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const ACharacter* C = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
	const UCharacterMovementComponent* M = C ? C->GetCharacterMovement() : nullptr;
	if (!M) return;
	const FVector Feet = C->GetActorLocation() - FVector(0.0f, 0.0f, C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	const bool bFalling = M->IsFalling();
	const FVector Base = C->GetMovementBase() ? C->GetMovementBase()->GetComponentVelocity() : FVector::ZeroVector;
	if (bFalling && !bWasFalling && M->Velocity.Z > 100.0f)
	{
		if (USoundBase* S = JumpSound.LoadSynchronous()) UGameplayStatics::PlaySound2D(this, S, FootstepVolume * 1.1f, Rng.FRandRange(0.96f, 1.04f));
	}
	if (bFalling) FallTime += DeltaSeconds;
	if (!bFalling && bWasFalling)
	{
		if (FallTime > 0.25f)
		{
			if (USoundBase* S = LandSound.LoadSynchronous()) UGameplayStatics::PlaySound2D(this, S, FootstepVolume * FMath::Clamp(FallTime * 2.0f, 0.8f, 1.6f), Rng.FRandRange(0.95f, 1.03f));
		}
		FallTime = 0.0f;
		StrideLeft = StrideLength * 0.5f;
	}
	bWasFalling = bFalling;
	if (!M->IsMovingOnGround() || C->IsHidden())
	{
		bHaveLast = false;
		return;
	}
	if (bHaveLast)
	{
		// distance walked relative to what the player stands on (a moving train is not walking)
		FVector D = Feet - LastFeet - Base * DeltaSeconds;
		const float Rise = FMath::Abs(D.Z);
		D.Z = 0.0f;
		const float Walked = D.Size();
		const float Speed = (M->Velocity - Base).Size2D();
		if (Walked < 60.0f)                                                    // a teleport is not a stride
		{
			StrideLeft -= Walked;
			StrideRise += Rise;
			// Stage 9A cadence: steps per second grow with speed (about 2.2/s at 1.4 m/s, 4.3/s at a full 5.9 m/s run), so the
			// stride is speed / cadence - a slow walk takes short steps, a run long and quick ones
			const float Vms = FMath::Max(0.0f, Speed) / 100.0f;
			const float Cadence = FMath::Clamp(1.6f + 0.45f * Vms, 1.6f, 4.3f);
			const float Stride = FMath::Max(35.0f, FMath::Max(0.0f, Speed) / Cadence);
			if (StrideLeft <= 0.0f && Speed > 40.0f)
			{
				const bool bStairs = StrideRise > 9.0f;                        // about half a riser per stride or more
				const bool bBridge = !bStairs && Feet.Z > 540.0f;
				const bool bRun = Speed > RunSpeed;
				bLeftFoot = !bLeftFoot;                                         // left / right alternate
				// louder with speed (a slow walk is soft, a run heavy); stairs a touch harder; a small pitch spread per step
				const float V = FootstepVolume * FMath::Clamp(0.45f + 0.55f * Vms / 1.6f, 0.45f, 1.0f)
					* (bRun ? FMath::Clamp(1.05f + 0.1f * (Vms - 3.3f), 1.05f, 1.3f) : 1.0f) * (bStairs ? 1.08f : 1.0f);
				// Stage 9C: the surface under the feet picks the set; the step sounds at the stepping foot
				LastSurface = SurfaceUnder(C, Feet, StrideRise);
				float Gain = 1.0f;
				if (USoundBase* S9 = PickStep9C(LastSurface, bRun, bLeftFoot, false, Gain))
				{
					PlayStep(S9, C, Feet, bLeftFoot, V * Gain, Rng.FRandRange(0.975f, 1.025f));
				}
				else if (USoundBase* S = PickStep(bStairs, bBridge, bRun, bLeftFoot))
				{
					UGameplayStatics::PlaySound2D(this, S, V, Rng.FRandRange(0.965f, 1.035f));
				}
				StrideLeft = Stride;
				StrideRise = 0.0f;
				StepVolumeEase = 1.0f;
				++Steps;
			}
		}
		// Stage 9A: the player stops mid-stride - one soft settling step (the trailing foot comes down), then silence
		if (Speed < 25.0f && StepVolumeEase > 0.0f)
		{
			if (StrideLeft < 30.0f)
			{
				bLeftFoot = !bLeftFoot;
				float Gain = 1.0f;
				if (USoundBase* S9 = PickStep9C(SurfaceUnder(C, Feet, StrideRise), false, bLeftFoot, false, Gain)) PlayStep(S9, C, Feet, bLeftFoot, FootstepVolume * 0.36f * Gain, Rng.FRandRange(0.97f, 1.0f));
				else if (USoundBase* S = PickStep(StrideRise > 9.0f, Feet.Z > 540.0f, false, bLeftFoot)) UGameplayStatics::PlaySound2D(this, S, FootstepVolume * 0.38f, Rng.FRandRange(0.97f, 1.02f));
				++Steps;
			}
			StepVolumeEase = 0.0f;
		}
		if (Speed < 25.0f) { StrideLeft = 30.0f; StrideRise = 0.0f; }          // the first step after a stop comes quickly
		// Stage 9C: a hard stop from a run or a sharp pivot at speed drags the sole - one scuff (not on every stop)
		ScuffCooldown -= DeltaSeconds;
		const float VelYaw = (M->Velocity - Base).Size2D() > 50.0f ? (M->Velocity - Base).Rotation().Yaw : LastYaw;
		const float YawRate = FMath::Abs(FRotator::NormalizeAxis(VelYaw - LastYaw)) / FMath::Max(0.001f, DeltaSeconds);
		const float Decel = (LastSpeed - Speed) / FMath::Max(0.001f, DeltaSeconds);
		// only from a run (a walking stop is just the settling step) or a sharp pivot at running pace
		if (ScuffCooldown <= 0.0f && Walked < 60.0f && ((LastSpeed > RunSpeed && Decel > 1400.0f && Speed < LastSpeed * 0.6f) || (Speed > RunSpeed * 0.85f && YawRate > 320.0f)))
		{
			float Gain = 1.0f;
			if (USoundBase* S9 = PickStep9C(SurfaceUnder(C, Feet, 0.0f), false, bLeftFoot, true, Gain))
			{
				PlayStep(S9, C, Feet, !bLeftFoot, FootstepVolume * 0.55f * Gain * FMath::Clamp(LastSpeed / 450.0f, 0.6f, 1.1f), Rng.FRandRange(0.96f, 1.04f));
				++Scuffs;
				ScuffCooldown = 0.7f;
			}
		}
		LastSpeed = Speed;
		LastYaw = VelYaw;
	}
	LastFeet = Feet;
	bHaveLast = true;
}

USoundBase* AP9AudioDirector::PickStep(bool bStairs, bool bBridge, bool bRun, bool bLeft)
{
	const TArray<TSoftObjectPtr<USoundBase>>* Set = nullptr;
	if (bStairs) Set = bRun ? (bLeft ? &Steps9A_Stairs_Run_L : &Steps9A_Stairs_Run_R) : (bLeft ? &Steps9A_Stairs_Walk_L : &Steps9A_Stairs_Walk_R);
	else if (bBridge) Set = bRun ? (bLeft ? &Steps9A_Footbridge_Run_L : &Steps9A_Footbridge_Run_R) : (bLeft ? &Steps9A_Footbridge_Walk_L : &Steps9A_Footbridge_Walk_R);
	else Set = bRun ? (bLeft ? &Steps9A_Platform_Run_L : &Steps9A_Platform_Run_R) : (bLeft ? &Steps9A_Platform_Walk_L : &Steps9A_Platform_Walk_R);
	if (USoundBase* S = (Set && !Set->IsEmpty()) ? Pick(*Set, LastStep) : nullptr) { LastStepName = S->GetName(); return S; }   // 9B: shown in p9.Audio.Status
	return Pick(bStairs ? StepsStairs : (bBridge ? StepsFootbridge : StepsPlatform), LastStep);    // 8A fallback
}

// ---------------------------------------------------------------------------------------------------------------- Stage 9C footsteps
EP9StepSurface AP9AudioDirector::SurfaceUnder(const ACharacter* C, const FVector& Feet, float StrideRiseNow) const
{
	// the floor the movement component stands on: a train or the 8A escalator by its owner, the concourse deck found at play
	// start, otherwise by level (platforms at 0, the footbridge deck at 600, everything walkable between is a stair or landing)
	const UCharacterMovementComponent* M = C ? C->GetCharacterMovement() : nullptr;
	const AActor* Floor = M ? M->CurrentFloor.HitResult.GetActor() : nullptr;
	const AActor* BaseOwner = (C && C->GetMovementBase()) ? C->GetMovementBase()->GetOwner() : nullptr;
	if ((Floor && Floor->IsA<AP9Train>()) || (BaseOwner && BaseOwner->IsA<AP9Train>())) return EP9StepSurface::Train;
	if ((Floor && Floor->IsA<AP9ReverseEscalator>()) || (BaseOwner && BaseOwner->IsA<AP9ReverseEscalator>())) return EP9StepSurface::Metal;
	if (Floor && ConcourseFloor && Floor == ConcourseFloor) return EP9StepSurface::Concourse;
	if (Feet.Z >= 585.0f) return EP9StepSurface::Footbridge;
	if (StrideRiseNow > 9.0f || (Feet.Z > 8.0f && Feet.Z < 585.0f)) return EP9StepSurface::Stairs;
	return EP9StepSurface::Platform;
}

const FP9StepSet* AP9AudioDirector::StepSet(EP9StepSurface Surface) const
{
	switch (Surface)
	{
	case EP9StepSurface::Footbridge: return &Steps9C_Footbridge;
	case EP9StepSurface::Concourse: return &Steps9C_Concourse;
	case EP9StepSurface::Stairs: return &Steps9C_Stairs;
	case EP9StepSurface::Metal: return &Steps9C_Metal;
	case EP9StepSurface::Train: return &Steps9C_Train;
	default: return &Steps9C_Platform;
	}
}

USoundBase* AP9AudioDirector::PickStep9C(EP9StepSurface Surface, bool bRun, bool bLeft, bool bScuff, float& OutGain)
{
	const FP9StepSet* Set = StepSet(Surface);
	if (!Set) return nullptr;
	const TArray<TSoftObjectPtr<USoundBase>>& A = bScuff ? Set->Scuff : (bRun ? (bLeft ? Set->RunL : Set->RunR) : (bLeft ? Set->WalkL : Set->WalkR));
	if (A.IsEmpty()) return nullptr;
	USoundBase* S = Pick(A, bScuff ? LastScuff : LastStep);
	if (S) { LastStepName = S->GetName(); OutGain = Set->Gain; }
	return S;
}

void AP9AudioDirector::PlayStep(USoundBase* S, const ACharacter* C, const FVector& Feet, bool bLeft, float Volume, float Pitch)
{
	if (!S) return;
	if (!bSpatialFootsteps || !C || !StepAttenuation)
	{
		UGameplayStatics::PlaySound2D(this, S, Volume, Pitch);
		return;
	}
	// at the stepping foot: beside the body centre, a little ahead, just above the floor
	const FRotator Yaw(0.0f, C->GetActorRotation().Yaw, 0.0f);
	const FVector Loc = Feet + Yaw.RotateVector(FVector(8.0f, (bLeft ? -1.0f : 1.0f) * FootSpacing, 3.0f));
	UGameplayStatics::PlaySoundAtLocation(this, S, Loc, FRotator::ZeroRotator, Volume, Pitch, 0.0f, StepAttenuation);
}

void AP9AudioDirector::QueueThunder(int32 Kind, float Delay, float Distance)
{
	FThunder T;
	T.Kind = FMath::Clamp(Kind, 0, 2);
	T.Delay = FMath::Max(0.0f, Delay);
	T.Distance = Distance;
	T.Volume = ThunderVolume * (T.Kind == 2 ? 1.0f : (T.Kind == 1 ? 0.85f : 0.6f));
	PendingThunder.Add(T);
	UE_LOG(LogP9, Log, TEXT("Audio: thunder queued - %s, %.0f m, arrives in %.1f s"), T.Kind == 2 ? TEXT("close") : (T.Kind == 1 ? TEXT("strong") : TEXT("distant")), Distance, T.Delay);
}

void AP9AudioDirector::TickThunder(float DeltaSeconds)
{
	// lightning is seen at once; its thunder arrives after distance / 343 m/s. The flash is read from the moon light, which the
	// storm controller raises by MoonFlashIntensity at each flash (the controller itself is not touched)
	FlashCooldown -= DeltaSeconds;
	if (Moon && Storm && Moon->GetLightComponent())
	{
		const float I = Moon->GetLightComponent()->Intensity;
		if (MoonBase < 0.0f || I < MoonBase) MoonBase = I;                    // the resting level (lowest seen)
		if (FlashCooldown <= 0.0f && I > MoonBase + 0.3f * FMath::Max(0.1f, Storm->MoonFlashIntensity))
		{
			FlashCooldown = 1.2f;
			++Flashes;
			// a visible bolt is a strike that can be placed: its distance sets the delay and the character
			float BoltDist = -1.0f;
			const APawn* P = UGameplayStatics::GetPlayerPawn(this, 0);
			for (TActorIterator<AActor> It(GetWorld()); It && P; ++It)
			{
				if (It->Tags.Num() > 0 && It->Tags[0].ToString().StartsWith(Storm->BoltTagPrefix) && !It->IsHidden())
				{
					const float D = FVector::Dist2D(It->GetActorLocation(), P->GetActorLocation()) / 100.0f;
					BoltDist = BoltDist < 0.0f ? D : FMath::Min(BoltDist, D);
				}
			}
			if (BoltDist > 0.0f)
			{
				QueueThunder(BoltDist < 1500.0f ? 2 : 1, FMath::Clamp(BoltDist / 343.0f, 0.4f, 20.0f), BoltDist);
			}
			else if (Rng.FRand() < ThunderChanceNoBolt)
			{
				const float D = Rng.FRandRange(2500.0f, 9000.0f);                // cloud-to-cloud, kilometres away
				QueueThunder(D < 4000.0f ? 1 : 0, FMath::Min(D / 343.0f, 22.0f), D);
			}
			else
			{
				UE_LOG(LogP9, Log, TEXT("Audio: flash %d - too far, no thunder"), Flashes);
			}
		}
	}
	for (int32 i = PendingThunder.Num() - 1; i >= 0; --i)
	{
		FThunder& T = PendingThunder[i];
		if ((T.Delay -= DeltaSeconds) > 0.0f) continue;
		const TArray<TSoftObjectPtr<USoundBase>>& Set = T.Kind == 2 ? ThunderClose : (T.Kind == 1 ? ThunderStrong : ThunderDistant);
		if (USoundBase* S = Pick(Set, LastThunder[T.Kind]))
		{
			// quieter inside a train; two at once are each a little lower
			const bool bAboard = Director && ((Director->Train && Director->Train->IsPlayerAboard()) || (Director->OpeningTrain && Director->OpeningTrain->IsPlayerAboard()));
			UGameplayStatics::PlaySound2D(this, S, T.Volume * (bAboard ? 0.55f : 1.0f) * (PendingThunder.Num() > 1 ? 0.8f : 1.0f), Rng.FRandRange(0.93f, 1.04f));
			++Thunders;
			P9Captions::NotifySound(this, S);                                // Stage 9B: only strong / close thunder is captioned
			UE_LOG(LogP9, Log, TEXT("Audio: thunder %s (%.0f m)"), *S->GetName(), T.Distance);
		}
		PendingThunder.RemoveAt(i);
	}
}

void AP9AudioDirector::TickKanchana(float DeltaSeconds)
{
	Presence.Target = 0.0f;
	if (!Stalker || Stalker->IsHidden())
	{
		bHaveKanchana = false;
		if (KanchanaBreath && KanchanaBreath->IsPlaying()) KanchanaBreath->FadeOut(0.8f, 0.0f);
		return;
	}
	const APawn* P = UGameplayStatics::GetPlayerPawn(this, 0);
	const FVector K = Stalker->GetActorLocation();
	const float Dist = P ? FVector::Dist(P->GetActorLocation(), K) : 1.0e9f;
	// her breathing comes from her (positional); a low presence layer grows as she gets close
	if (!KanchanaBreath)
	{
		if (USoundBase* S = KanchanaBreathing.LoadSynchronous())
		{
			KanchanaBreath = UGameplayStatics::SpawnSoundAttached(S, Stalker->GetRootComponent(), NAME_None, FVector(0.0f, 0.0f, 150.0f), EAttachLocation::KeepRelativeOffset,
				false, 0.45f, 1.0f, 0.0f, nullptr, nullptr, false);
			if (KanchanaBreath)
			{
				KanchanaBreath->bOverrideAttenuation = true;
				KanchanaBreath->AttenuationOverrides.bAttenuate = true;
				KanchanaBreath->AttenuationOverrides.bSpatialize = true;
				KanchanaBreath->AttenuationOverrides.AttenuationShapeExtents = FVector(150.0f, 0.0f, 0.0f);
				KanchanaBreath->AttenuationOverrides.FalloffDistance = 900.0f;
			}
		}
	}
	if (KanchanaBreath && !KanchanaBreath->IsPlaying() && Dist < 1400.0f) KanchanaBreath->FadeIn(1.0f, 0.45f);
	Presence.Target = 0.22f * FMath::Clamp(1.0f - (Dist - 300.0f) / 1500.0f, 0.0f, 1.0f);
	// footsteps where she walks
	if (bHaveKanchana)
	{
		const float Moved = FVector::Dist2D(K, LastKanchana);
		if (Moved < 80.0f)
		{
			KStride -= Moved;
			if (KStride <= 0.0f)
			{
				KStride = 68.0f;
				const bool bClose = Dist < 700.0f;
				if (USoundBase* S = Pick(bClose ? KanchanaStepsClose : KanchanaStepsDistant, LastKStep))
				{
					UGameplayStatics::PlaySoundAtLocation(this, S, K, bClose ? 0.5f : 0.35f, Rng.FRandRange(0.95f, 1.05f));
				}
			}
		}
	}
	LastKanchana = K;
	bHaveKanchana = true;
}

FString AP9AudioDirector::GetStatus() const
{
	auto V = [](const FLayer& L) { return FString::Printf(TEXT("%.2f>%.2f%s"), L.Volume, L.Target, L.Comp && L.Comp->IsPlaying() ? TEXT("*") : TEXT("")); };
	return FString::Printf(TEXT("main %s p9 %s anomaly %s epilogue %s wind %s bridge %s presence %s | steps %d | noticed %d | flashes %d thunders %d pending %d moon %s | last step %s surface %d scuffs %d spatial %d"), *V(Main), *V(P9), *V(Anomaly), *V(Epilogue), *V(Wind),
		*V(Bridge), *V(Presence), Steps, Director && Director->IsAnyAnomalyNoticed() ? 1 : 0, Flashes, Thunders, PendingThunder.Num(), Moon ? TEXT("found") : TEXT("missing"), *LastStepName, (int32)LastSurface, Scuffs,
		bSpatialFootsteps && StepAttenuation ? 1 : 0);
}

void AP9AudioDirector::EndPlay(const EEndPlayReason::Type Reason)
{
	for (UAudioComponent* A : Owned) if (IsValid(A)) A->Stop();
	if (IsValid(KanchanaBreath)) KanchanaBreath->Stop();
	Super::EndPlay(Reason);
}
