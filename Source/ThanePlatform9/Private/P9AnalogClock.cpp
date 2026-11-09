// Stage 6C: double-faced analog railway clock (footbridge concourse) and its reverse mode for the "Reverse Analog Clock" anomaly.

#include "P9AnalogClock.h"
#include "ThanePlatform9.h"
#include "P9CampaignDirector.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

namespace P9AnalogClockLocal
{
	constexpr double Day = 24.0 * 3600.0;
	constexpr TCHAR MeshFolder[] = TEXT("/Game/P9/Anomalies6C/");

	UStaticMesh* FindMesh(const TCHAR* Name)
	{
		const FString Path = FString(MeshFolder) + Name + TEXT(".") + Name;
		ConstructorHelpers::FObjectFinderOptional<UStaticMesh> Finder(*Path);
		return Finder.Get();
	}

	float SmoothStep01(float X)
	{
		X = FMath::Clamp(X, 0.0f, 1.0f);
		return X * X * (3.0f - 2.0f * X);
	}
}

AP9AnalogClock::AP9AnalogClock()
{
	using namespace P9AnalogClockLocal;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Root->SetMobility(EComponentMobility::Static);                  // the clock never moves; only the hands are movable

	auto Part = [this](const TCHAR* Name, const TCHAR* Mesh, bool bCollide) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetupAttachment(Root);
		C->SetStaticMesh(FindMesh(Mesh));
		C->SetMobility(EComponentMobility::Movable);
		C->SetCastShadow(false);                                    // station lamps are unshadowed; no shadow cost on mobile
		C->bReceivesDecals = false;
		if (bCollide)
		{
			C->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		}
		else
		{
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			C->SetGenerateOverlapEvents(false);
			C->SetCanEverAffectNavigation(false);
		}
		return C;
	};
	Body = Part(TEXT("Body"), TEXT("SM_P9_AnalogClock_Body"), true);
	Glass = Part(TEXT("Glass"), TEXT("SM_P9_AnalogClock_Glass"), false);
	HourFront = Part(TEXT("HourFront"), TEXT("SM_P9_AnalogClock_HourHand"), false);
	MinuteFront = Part(TEXT("MinuteFront"), TEXT("SM_P9_AnalogClock_MinuteHand"), false);
	SecondFront = Part(TEXT("SecondFront"), TEXT("SM_P9_AnalogClock_SecondHand"), false);
	HourBack = Part(TEXT("HourBack"), TEXT("SM_P9_AnalogClock_HourHand"), false);
	MinuteBack = Part(TEXT("MinuteBack"), TEXT("SM_P9_AnalogClock_MinuteHand"), false);
	SecondBack = Part(TEXT("SecondBack"), TEXT("SM_P9_AnalogClock_SecondHand"), false);
	Body->SetMobility(EComponentMobility::Static);
	Glass->SetMobility(EComponentMobility::Static);
	Body->SetCanEverAffectNavigation(false);                       // hangs above head height; never a nav obstacle
}

double AP9AnalogClock::ParseTime(const FString& Text)
{
	TArray<FString> Parts;
	Text.TrimStartAndEnd().ParseIntoArray(Parts, TEXT(":"));
	double S = 0.0;
	for (int32 i = 0; i < 3; ++i)
	{
		S = S * 60.0 + (Parts.IsValidIndex(i) ? FCString::Atod(*Parts[i]) : 0.0);
	}
	return FMath::Fmod(FMath::Fmod(S, P9AnalogClockLocal::Day) + P9AnalogClockLocal::Day, P9AnalogClockLocal::Day);
}

FString AP9AnalogClock::FormatTime(double Seconds)
{
	const int64 S = static_cast<int64>(FMath::FloorToDouble(FMath::Fmod(FMath::Fmod(Seconds, P9AnalogClockLocal::Day) + P9AnalogClockLocal::Day, P9AnalogClockLocal::Day)));
	return FString::Printf(TEXT("%02lld:%02lld:%02lld"), S / 3600, (S / 60) % 60, S % 60);
}

void AP9AnalogClock::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	TrueSeconds = DisplaySeconds = ParseTime(StartTime);
	LayoutHands();
	ApplyHands();                                                   // the editor shows the start time
}

void AP9AnalogClock::BeginPlay()
{
	Super::BeginPlay();
	ResetToStartTime();
	for (TActorIterator<AP9CampaignDirector> It(GetWorld()); It; ++It)
	{
		It->OnDayStarted.AddUniqueDynamic(this, &AP9AnalogClock::HandleDayStarted);
		break;
	}
}

void AP9AnalogClock::HandleDayStarted(int32 InDay)
{
	ResetToStartTime();
}

void AP9AnalogClock::ResetToStartTime()
{
	SetStationTime(ParseTime(StartTime));
}

void AP9AnalogClock::SetStationTime(double Seconds)
{
	TrueSeconds = DisplaySeconds = Seconds;
	ApplyHands();
}

void AP9AnalogClock::BeginReverse(float ReverseRate, float PauseSeconds, float SlowSeconds, bool bWaitUntilSeen)
{
	ReverseRateTarget = FMath::Max(1.0f, ReverseRate);
	PauseTime = FMath::Max(0.0f, PauseSeconds);
	SlowTime = FMath::Max(0.0f, SlowSeconds);
	bWaitForSight = bWaitUntilSeen;
	SequenceTime = 0.0f;
	SecondTwitch = 0.0f;
	bTriggered = !bWaitUntilSeen;
	Mode = bWaitUntilSeen ? EP9AnalogClockMode::ReverseArmed : EP9AnalogClockMode::Reversing;
	Rate = bWaitUntilSeen ? 1.0f : -ReverseRateTarget;
	UE_LOG(LogP9, Log, TEXT("AnalogClock %s: reverse armed (rate x%.1f, pause %.1fs, slow %.1fs, %s) at %s"), *GetName(), ReverseRateTarget, PauseTime, SlowTime,
		bWaitUntilSeen ? TEXT("waits for the player's eyes") : TEXT("backwards from now"), *FormatTime(DisplaySeconds));
}

void AP9AnalogClock::EndReverse()
{
	const bool bWas = IsReversed();
	Mode = EP9AnalogClockMode::Normal;
	Rate = 1.0f;
	SecondTwitch = 0.0f;
	SequenceTime = 0.0f;
	bTriggered = false;
	DisplaySeconds = TrueSeconds;                                   // the correct station time again
	ApplyHands();
	if (bWas)
	{
		UE_LOG(LogP9, Log, TEXT("AnalogClock %s: normal again at %s"), *GetName(), *FormatTime(DisplaySeconds));
	}
}

bool AP9AnalogClock::IsSeenByPlayer() const
{
	const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!Cam)
	{
		return false;
	}
	const FVector Eye = Cam->GetCameraLocation();
	const FVector Centre = GetActorLocation();
	const FVector ToClock = Centre - Eye;
	const float Dist = ToClock.Size();
	if (Dist > SeenDistance || Dist < 1.0f)
	{
		return false;
	}
	const FVector Dir = ToClock / Dist;
	if (FVector::DotProduct(Cam->GetCameraRotation().Vector(), Dir) < SeenMinDot)
	{
		return false;
	}
	if (FMath::Abs(FVector::DotProduct(Dir, GetActorRightVector())) < 0.35f)
	{
		return false;                                               // edge-on: no dial is readable
	}
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(P9AnalogClockSight), false, this);
	Params.AddIgnoredActor(Cam->GetViewTarget());
	return !GetWorld()->LineTraceSingleByChannel(Hit, Eye, Centre, ECC_Visibility, Params);
}

void AP9AnalogClock::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Dt = DeltaSeconds * TimeScale;
	TrueSeconds += Dt;
	if (Mode == EP9AnalogClockMode::Normal)
	{
		Rate = 1.0f;
		DisplaySeconds = TrueSeconds;
	}
	else
	{
		if (Mode == EP9AnalogClockMode::ReverseArmed && (SeenCheckTimer -= DeltaSeconds) <= 0.0f)
		{
			SeenCheckTimer = 0.2f;
			if (IsSeenByPlayer())
			{
				Mode = EP9AnalogClockMode::Reversing;
				bTriggered = true;
				SequenceTime = 0.0f;
				UE_LOG(LogP9, Log, TEXT("AnalogClock %s: seen by the player at %s - reversing"), *GetName(), *FormatTime(DisplaySeconds));
			}
		}
		if (Mode == EP9AnalogClockMode::Reversing && bWaitForSight)
		{
			// seen -> keeps running -> the second hand stops and twitches back -> backwards at real speed -> all hands race backwards
			SequenceTime += DeltaSeconds;
			const float T0 = TriggerDelay, T1 = T0 + PauseTime, T2 = T1 + 0.5f, T3 = T2 + SlowTime, T4 = T3 + RampSeconds;
			const float T = SequenceTime;
			if (T < T0) Rate = 1.0f;
			else if (T < T1) Rate = 0.0f;
			else if (T < T2) Rate = -P9AnalogClockLocal::SmoothStep01((T - T1) / 0.5f);
			else if (T < T3) Rate = -1.0f;
			else if (T < T4) Rate = -FMath::Lerp(1.0f, ReverseRateTarget, P9AnalogClockLocal::SmoothStep01((T - T3) / FMath::Max(0.01f, RampSeconds)));
			else Rate = -ReverseRateTarget;
			const float P = T - T0;
			SecondTwitch = (T >= T0 && T < T1 && P < 1.2f) ? -5.0f * FMath::Max(0.0f, FMath::Sin(2.0f * PI * P / 0.6f)) : 0.0f;
		}
		else if (Mode == EP9AnalogClockMode::Reversing)
		{
			Rate = -ReverseRateTarget;
		}
		else
		{
			Rate = 1.0f;
		}
		DisplaySeconds += Dt * Rate;
	}
	if (Body && Body->WasRecentlyRendered(0.25f))
	{
		ApplyHands();                                               // nobody looking: only the two numbers above advance
	}
}

void AP9AnalogClock::LayoutHands()
{
	const float Base = DialOffset;
	const TPair<UStaticMeshComponent*, float> Front[] = {{HourFront, HandLayers.X}, {MinuteFront, HandLayers.Y}, {SecondFront, HandLayers.Z}};
	const TPair<UStaticMeshComponent*, float> Back[] = {{HourBack, HandLayers.X}, {MinuteBack, HandLayers.Y}, {SecondBack, HandLayers.Z}};
	for (const auto& P : Front) if (P.Key) P.Key->SetRelativeLocation(FVector(0.0f, Base + P.Value, 0.0f));
	for (const auto& P : Back) if (P.Key) P.Key->SetRelativeLocation(FVector(0.0f, -(Base + P.Value), 0.0f));
}

FVector AP9AnalogClock::GetHandAngles() const
{
	const double S = FMath::Fmod(FMath::Fmod(DisplaySeconds, 43200.0) + 43200.0, 43200.0);
	return FVector(S / 120.0, FMath::Fmod(S, 3600.0) / 10.0, FMath::Fmod(S, 60.0) * 6.0 + SecondTwitch);
}

void AP9AnalogClock::ApplyHands()
{
	// +Y face: seen from +Y, +X is on the viewer's right, so turning +Z towards +X (about +Y) is clockwise; -Y face mirrored
	const FVector A = GetHandAngles();
	auto Set = [](UStaticMeshComponent* C, double Deg, double Sign)
	{
		if (C) C->SetRelativeRotation(FQuat(FVector::YAxisVector, FMath::DegreesToRadians(Deg * Sign)));
	};
	Set(HourFront, A.X, 1.0); Set(MinuteFront, A.Y, 1.0); Set(SecondFront, A.Z, 1.0);
	Set(HourBack, A.X, -1.0); Set(MinuteBack, A.Y, -1.0); Set(SecondBack, A.Z, -1.0);
}

bool AP9AnalogClock::HasAllParts() const
{
	for (const UStaticMeshComponent* C : {Body.Get(), Glass.Get(), HourFront.Get(), MinuteFront.Get(), SecondFront.Get(), HourBack.Get(), MinuteBack.Get(), SecondBack.Get()})
	{
		if (!C || !C->GetStaticMesh()) return false;
	}
	return true;
}

FString AP9AnalogClock::GetStatus() const
{
	static const TCHAR* Modes[] = {TEXT("Normal"), TEXT("ReverseArmed"), TEXT("Reversing")};
	const FVector A = GetHandAngles();
	return FString::Printf(TEXT("%s mode=%s shows %s (true %s) rate %+.2f seq %.1fs hands h%.1f m%.1f s%.1f deg rendered=%s parts=%s"), *GetName(), Modes[static_cast<int32>(Mode)],
		*FormatTime(DisplaySeconds), *FormatTime(TrueSeconds), Rate, SequenceTime, A.X, A.Y, A.Z,
		Body && Body->WasRecentlyRendered(0.25f) ? TEXT("yes") : TEXT("no"), HasAllParts() ? TEXT("ok") : TEXT("MISSING"));
}

#if !UE_BUILD_SHIPPING
namespace P9AnalogClockDebug
{
	void ForEachClock(UWorld* World, TFunctionRef<void(AP9AnalogClock*)> Fn)
	{
		int32 N = 0;
		for (TActorIterator<AP9AnalogClock> It(World); It; ++It, ++N) Fn(*It);
		if (N == 0) UE_LOG(LogP9, Warning, TEXT("p9.Clock: no analog clock in this world"));
	}

	static FAutoConsoleCommandWithWorldAndArgs StatusCmd(TEXT("p9.Clock.Status"), TEXT("p9.Clock.Status - analog clock time, mode and hand angles"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			ForEachClock(World, [](AP9AnalogClock* C) { UE_LOG(LogP9, Log, TEXT("p9.Clock.Status: %s"), *C->GetStatus()); });
		}));
	static FAutoConsoleCommandWithWorldAndArgs SetCmd(TEXT("p9.Clock.Set"), TEXT("p9.Clock.Set HH:MM[:SS] - set the analog clock's station time (test)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const double S = AP9AnalogClock::ParseTime(Args.IsValidIndex(0) ? Args[0] : TEXT("23:47:00"));
			ForEachClock(World, [S](AP9AnalogClock* C) { C->SetStationTime(S); UE_LOG(LogP9, Log, TEXT("p9.Clock.Set: %s"), *C->GetStatus()); });
		}));
}
#endif
