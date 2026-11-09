#include "P9TrainAudio.h"
#include "P9Captions.h"
#include "ThanePlatform9.h"
#include "P9Train.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

namespace P9TrainAudioPack
{
	TSoftObjectPtr<USoundBase> S(const FString& Name)
	{
		return TSoftObjectPtr<USoundBase>(FSoftObjectPath(FString::Printf(TEXT("/Game/P9/Audio/OriginalAudioPack9A/Train/%s.%s"), *Name, *Name)));
	}
	void Attenuate(UAudioComponent* A, float Inner, float Falloff)
	{
		A->bOverrideAttenuation = true;
		A->AttenuationOverrides.bAttenuate = true;
		A->AttenuationOverrides.bSpatialize = true;
		A->AttenuationOverrides.AttenuationShape = EAttenuationShape::Sphere;
		A->AttenuationOverrides.AttenuationShapeExtents = FVector(Inner, 0.0f, 0.0f);
		A->AttenuationOverrides.FalloffDistance = Falloff;
	}
	static FAutoConsoleCommandWithWorld StatusCommand(TEXT("p9.TrainAudio.Status"), TEXT("p9.TrainAudio.Status - the 9A train sound layers per train"),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			for (TActorIterator<AP9TrainAudio> It(World); It; ++It) UE_LOG(LogP9, Log, TEXT("p9.TrainAudio.Status: %s"), *It->GetStatus());
		}));
}

AP9TrainAudio::AP9TrainAudio()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;          // after the trains (TG_PrePhysics) have moved this frame
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetHidden(true);
	using namespace P9TrainAudioPack;
	RollLoop = S(TEXT("SFX_Train9A_Roll_Loop"));
	TractionLoop = S(TEXT("SFX_Train9A_Traction_Loop"));
	IdleLoop = S(TEXT("SFX_Train9A_Idle_Loop"));
	BrakeFrictionLoop = S(TEXT("SFX_Train9A_BrakeFriction_Loop"));
	BrakeApply = S(TEXT("SFX_Train9A_BrakeApply"));
	StopRelease = S(TEXT("SFX_Train9A_StopRelease"));
	for (int32 i = 1; i <= 5; ++i) Clatter.Add(S(FString::Printf(TEXT("SFX_Train9A_Clatter_%02d"), i)));
	ApproachHorn = S(TEXT("SFX_Train9A_Horn_Double"));
	ApproachHornDistant = S(TEXT("SFX_Train9A_Horn_Distant"));
	AxleOffsets = {-875.0f, -585.0f, 585.0f, 875.0f};
}

UAudioComponent* AP9TrainAudio::MakeComp(AP9Train* Train, USoundBase* Sound, float Inner, float Falloff, const TCHAR* Name)
{
	UAudioComponent* A = NewObject<UAudioComponent>(Train, MakeUniqueObjectName(Train, UAudioComponent::StaticClass(), Name), RF_Transient);
	A->SetupAttachment(Train->GetRootComponent());
	A->bAutoActivate = false;
	P9TrainAudioPack::Attenuate(A, Inner, Falloff);
	A->SetSound(Sound);
	A->SetVolumeMultiplier(0.0f);
	A->RegisterComponent();
	Owned.Add(A);
	return A;
}

void AP9TrainAudio::BeginPlay()
{
	Super::BeginPlay();
	Rng.Initialize(0x9A7A);
	USoundBase* Roll = RollLoop.LoadSynchronous();
	USoundBase* Traction = TractionLoop.LoadSynchronous();
	USoundBase* Idle = IdleLoop.LoadSynchronous();
	USoundBase* Brake = BrakeFrictionLoop.LoadSynchronous();
	for (TActorIterator<AP9Train> It(GetWorld()); It; ++It)
	{
		FTrainSound T;
		T.Train = *It;
		T.Roll = Roll ? MakeComp(*It, Roll, 500.0f, 6500.0f, TEXT("P9A_Roll")) : nullptr;
		T.Traction = Traction ? MakeComp(*It, Traction, 400.0f, 5000.0f, TEXT("P9A_Traction")) : nullptr;
		T.Idle = Idle ? MakeComp(*It, Idle, 300.0f, 2600.0f, TEXT("P9A_Idle")) : nullptr;
		T.Brake = Brake ? MakeComp(*It, Brake, 400.0f, 5000.0f, TEXT("P9A_Brake")) : nullptr;
		T.OneShot = MakeComp(*It, nullptr, 500.0f, 9000.0f, TEXT("P9A_OneShot"));
		T.LastState = static_cast<uint8>(It->GetTrainState());
		T.LastDistance = It->GetRouteDistance();
		Trains.Add(T);
	}
	UE_LOG(LogP9, Log, TEXT("TrainAudio: %d trains, roll %s traction %s idle %s brake %s clatter %d"), Trains.Num(), Roll ? TEXT("ok") : TEXT("MISSING"),
		Traction ? TEXT("ok") : TEXT("MISSING"), Idle ? TEXT("ok") : TEXT("MISSING"), Brake ? TEXT("ok") : TEXT("MISSING"), Clatter.Num());
}

void AP9TrainAudio::EndPlay(const EEndPlayReason::Type Reason)
{
	for (UAudioComponent* A : Owned) if (IsValid(A)) A->Stop();
	Super::EndPlay(Reason);
}

void AP9TrainAudio::PlayAt(USoundBase* Sound, const FVector& Where, float Vol, float Pitch, float Falloff)
{
	if (!Sound) return;
	if (UAudioComponent* A = UGameplayStatics::SpawnSoundAtLocation(this, Sound, Where, FRotator::ZeroRotator, Vol * Volume, Pitch, 0.0f, nullptr, nullptr, true))
	{
		P9TrainAudioPack::Attenuate(A, 300.0f, Falloff);
	}
}

static void DriveLoop(UAudioComponent* A, float& Level, float Target, float Rate, float DeltaSeconds, float Pitch, float Master)
{
	if (!A) return;
	Level = FMath::FInterpConstantTo(Level, Target, DeltaSeconds, Rate);
	if (Level > 0.002f)
	{
		if (!A->IsPlaying()) A->Play(FMath::FRandRange(0.0f, 4.0f));
		A->SetVolumeMultiplier(Level * Master);
		A->SetPitchMultiplier(Pitch);
	}
	else if (A->IsPlaying())
	{
		A->Stop();
	}
}

void AP9TrainAudio::TickTrain(FTrainSound& T, float DeltaSeconds)
{
	AP9Train* Train = T.Train.Get();
	if (!Train) return;
	const bool bTrainHidden = Train->IsHidden();
	const EP9TrainState State = Train->GetTrainState();
	const float Speed = bTrainHidden ? 0.0f : Train->GetSpeed();
	const float Ratio = FMath::Clamp(Speed / FMath::Max(1.0f, Train->MaxSpeed), 0.0f, 1.0f);
	const bool bMoving = Speed > 1.0f;
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);

	// ---- state changes: brake application, standstill vent, approach horn
	const uint8 S = static_cast<uint8>(State);
	if (S != T.LastState && !bTrainHidden)
	{
		const EP9TrainState Old = static_cast<EP9TrainState>(T.LastState);
		if (State == EP9TrainState::Braking)
		{
			PlayAt(BrakeApply.LoadSynchronous(), Train->GetActorLocation(), 0.7f, Rng.FRandRange(0.96f, 1.04f), 4500.0f);
			const float Dist = Player ? FVector::Dist(Player->GetActorLocation(), Train->GetActorLocation()) : 1.0e9f;
			if (!Train->IsPlayerAboard() && Dist < ApproachHornRange && T.OneShot)
			{
				USoundBase* Horn = (Dist > 4500.0f ? ApproachHornDistant : ApproachHorn).LoadSynchronous();
				if (Horn && !T.OneShot->IsPlaying())
				{
					T.OneShot->SetSound(Horn);
					T.OneShot->SetVolumeMultiplier(0.9f * Volume);
					T.OneShot->Play();
					P9Captions::NotifySound(this, Horn);                  // Stage 9B
					++T.Horns;
					UE_LOG(LogP9, Log, TEXT("TrainAudio: %s approach horn (%s, %.0f m)"), *Train->GetName(), *Horn->GetName(), Dist / 100.0f);
				}
			}
		}
		if (State == EP9TrainState::StoppedAtPlatform && (Old == EP9TrainState::Arriving || Old == EP9TrainState::Braking))
		{
			PlayAt(StopRelease.LoadSynchronous(), Train->GetActorLocation(), 0.6f, Rng.FRandRange(0.97f, 1.03f), 3500.0f);
		}
		T.LastState = S;
	}

	// ---- continuous layers (levels follow the actual speed and state)
	const bool bBraking = State == EP9TrainState::Braking || State == EP9TrainState::Arriving;
	const float RollT = bMoving ? FMath::Pow(Ratio, 0.7f) * 0.9f + 0.05f : 0.0f;
	float TracT = 0.0f;
	if (State == EP9TrainState::Accelerating) TracT = 0.85f;
	else if (State == EP9TrainState::Cruising) TracT = 0.4f;
	else if (bBraking && bMoving) TracT = 0.3f * Ratio + 0.05f;                       // regenerative whine winding down
	const float BrakeT = bBraking && Speed > 4.0f ? (0.2f + 0.8f * (1.0f - Ratio)) * FMath::Clamp(Speed / 60.0f, 0.0f, 1.0f) : 0.0f;
	const float IdleT = bTrainHidden ? 0.0f : (bMoving ? 0.15f : 0.45f);
	DriveLoop(T.Roll, T.RollLevel, bTrainHidden ? 0.0f : RollT, 1.2f, DeltaSeconds, 0.62f + 0.48f * Ratio, Volume);
	DriveLoop(T.Traction, T.TractionLevel, bTrainHidden ? 0.0f : TracT, 0.6f, DeltaSeconds, 0.35f + 0.85f * Ratio, Volume);
	DriveLoop(T.Brake, T.BrakeLevel, bTrainHidden ? 0.0f : BrakeT, 1.5f, DeltaSeconds, 0.92f + 0.12f * Ratio, Volume * 0.8f);
	DriveLoop(T.Idle, T.IdleLevel, IdleT, 0.4f, DeltaSeconds, 1.0f, Volume * 0.7f);

	// ---- clatter: each axle over each rail joint, at the axle (the rhythm is the train's real speed)
	const float D = Train->GetRouteDistance();
	if (T.LastDistance >= 0.0f && bMoving && !bTrainHidden && Clatter.Num() > 0)
	{
		const float Step = D - T.LastDistance;
		if (Step > 0.0f && Step < 400.0f)
		{
			for (const float CarSign : {-1.0f, 1.0f})
			{
				for (const float A : AxleOffsets)
				{
					const float Off = CarSign * CarCentre + A;
					if (FMath::FloorToInt((T.LastDistance + Off) / RailLength) != FMath::FloorToInt((D + Off) / RailLength))
					{
						FVector Where; float Yaw;
						Train->EvaluateRoute(D + Off, Where, Yaw);
						int32 i = Rng.RandRange(0, Clatter.Num() - 1);
						if (i == LastClack) i = (i + 1) % Clatter.Num();
						LastClack = i;
						PlayAt(Clatter[i].LoadSynchronous(), Where + FVector(0.0f, 0.0f, 30.0f), 0.25f + 0.65f * Ratio, Rng.FRandRange(0.93f, 1.05f) * (0.85f + 0.2f * Ratio), 4000.0f);
						++T.Clacks;
					}
				}
			}
		}
	}
	T.LastDistance = D;
	T.LastSpeed = Speed;
}

void AP9TrainAudio::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	for (FTrainSound& T : Trains) TickTrain(T, DeltaSeconds);
}

FString AP9TrainAudio::GetStatus() const
{
	FString Out;
	for (const FTrainSound& T : Trains)
	{
		const AP9Train* Tr = T.Train.Get();
		if (!Tr) continue;
		Out += FString::Printf(TEXT("[%s state %d speed %.0f hidden %d roll %.2f traction %.2f brake %.2f idle %.2f clacks %d horns %d] "), *Tr->GetName(), (int32)Tr->GetTrainState(),
			Tr->GetSpeed(), Tr->IsHidden() ? 1 : 0, T.RollLevel, T.TractionLevel, T.BrakeLevel, T.IdleLevel, T.Clacks, T.Horns);
	}
	return Out;
}
