#include "P9StormController.h"

#include "ThanePlatform9.h"
#include "Components/LightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace P9Storm
{
	const FName CloudColor(TEXT("Cloud color"));
	const FName HorizonColor(TEXT("Horizon color"));
	const FName ZenithColor(TEXT("Zenith Color"));
	const FName LightDirection(TEXT("Light direction"));
	constexpr float RiseTime = 0.03f;
	constexpr float DecayTime = 0.09f;
}

AP9StormController::AP9StormController()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
}

void AP9StormController::BeginPlay()
{
	Super::BeginPlay();
	ResolveTargets();
	ScheduleNextFlash(FirstFlashDelay);
}

void AP9StormController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (FlashTime >= 0.0f)
	{
		ApplyFlash(0.0f);
		SetBoltGroupVisible(ActiveBolt, false);
		FlashTime = -1.0f;
	}
	Super::EndPlay(EndPlayReason);
}

void AP9StormController::ResolveTargets()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TMap<FName, int32> GroupIndex;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!SkyMID && Actor->ActorHasTag(SkyDomeTag))
		{
			if (UStaticMeshComponent* Mesh = Actor->FindComponentByClass<UStaticMeshComponent>())
			{
				SkyMID = Mesh->CreateDynamicMaterialInstance(0);
			}
		}
		if (!MoonLight && Actor->ActorHasTag(MoonLightTag))
		{
			MoonLight = Cast<ADirectionalLight>(Actor);
		}
		if (!SkyLight && Actor->ActorHasTag(SkyLightTag))
		{
			SkyLight = Cast<ASkyLight>(Actor);
		}
		if (Actor->Tags.Num() > 0 && Actor->Tags[0].ToString().StartsWith(BoltTagPrefix))
		{
			const int32* Found = GroupIndex.Find(Actor->Tags[0]);
			const int32 Index = Found ? *Found : GroupIndex.Add(Actor->Tags[0], BoltGroups.AddDefaulted());
			BoltGroups[Index].Add(Actor);
			Actor->SetActorHiddenInGame(true);
		}
	}

	if (SkyMID)
	{
		SkyMID->GetVectorParameterValue(FHashedMaterialParameterInfo(P9Storm::CloudColor), BaseCloudColor);
		SkyMID->GetVectorParameterValue(FHashedMaterialParameterInfo(P9Storm::HorizonColor), BaseHorizonColor);
		SkyMID->GetVectorParameterValue(FHashedMaterialParameterInfo(P9Storm::ZenithColor), BaseZenithColor);
	}
	if (MoonLight && MoonLight->GetLightComponent())
	{
		BaseMoonIntensity = MoonLight->GetLightComponent()->Intensity;
		if (SkyMID && bSyncMoonDirection)
		{
			const FVector Dir = MoonLight->GetActorForwardVector();
			SkyMID->SetVectorParameterValue(P9Storm::LightDirection, FLinearColor(Dir.X, Dir.Y, Dir.Z, 1.0f));
		}
	}

	if (SkyLight && SkyLight->GetLightComponent())
	{
		BaseSkyLightIntensity = SkyLight->GetLightComponent()->Intensity;
	}

	UE_LOG(LogP9, Log, TEXT("Storm: sky=%s moon=%s skyLight=%s(flash %s) boltGroups=%d lightning=%s interval=%.1f..%.1fs"),
		SkyMID ? TEXT("found") : TEXT("MISSING"), MoonLight ? TEXT("found") : TEXT("MISSING"),
		SkyLight ? TEXT("found") : TEXT("MISSING"), bFlashSkyLight ? TEXT("on") : TEXT("off"),
		BoltGroups.Num(), bEnableLightning ? TEXT("on") : TEXT("off"), MinFlashInterval, MaxFlashInterval);
}

void AP9StormController::ScheduleNextFlash(float ExtraDelay)
{
	const float Lo = FMath::Min(MinFlashInterval, MaxFlashInterval);
	const float Hi = FMath::Max(MinFlashInterval, MaxFlashInterval);
	Countdown = ExtraDelay > 0.0f ? ExtraDelay : FMath::FRandRange(Lo, Hi);
}

void AP9StormController::TriggerFlash(bool bForceBolt, float HoldSeconds)
{
	if (FlashTime >= 0.0f)
	{
		SetBoltGroupVisible(ActiveBolt, false);
	}
	FlashTime = 0.0f;
	HoldRemaining = FMath::Max(0.0f, HoldSeconds);
	bDoubleFlash = FMath::FRand() < DoubleFlashChance;
	ActiveBolt = (BoltGroups.Num() > 0 && (bForceBolt || FMath::FRand() < BoltChance)) ? FMath::RandRange(0, BoltGroups.Num() - 1) : INDEX_NONE;
	SetBoltGroupVisible(ActiveBolt, true);
	++FlashCount;
	if (bLogFlashes)
	{
		UE_LOG(LogP9, Log, TEXT("Storm: flash #%d double=%s bolt=%d hold=%.2fs"), FlashCount, bDoubleFlash ? TEXT("yes") : TEXT("no"), ActiveBolt, HoldRemaining);
	}
}

float AP9StormController::Envelope(float T) const
{
	auto Pulse = [](float Local) -> float
	{
		if (Local < 0.0f) return 0.0f;
		if (Local < P9Storm::RiseTime) return Local / P9Storm::RiseTime;
		return FMath::Exp(-(Local - P9Storm::RiseTime) / P9Storm::DecayTime);
	};
	float Value = Pulse(T);
	if (bDoubleFlash)
	{
		Value = FMath::Max(Value, 0.8f * Pulse(T - DoubleFlashGap));
	}
	return FMath::Clamp(Value, 0.0f, 1.0f);
}

void AP9StormController::ApplyFlash(float Amount)
{
	if (SkyMID)
	{
		const float A = Amount * SkyFlashStrength;
		SkyMID->SetVectorParameterValue(P9Storm::CloudColor, FMath::Lerp(BaseCloudColor, FlashCloudColor, A));
		SkyMID->SetVectorParameterValue(P9Storm::HorizonColor, FMath::Lerp(BaseHorizonColor, FlashHorizonColor, A));
		SkyMID->SetVectorParameterValue(P9Storm::ZenithColor, FMath::Lerp(BaseZenithColor, FlashZenithColor, A));
	}
	if (bFlashMoonLight && MoonLight && MoonLight->GetLightComponent())
	{
		MoonLight->GetLightComponent()->SetIntensity(BaseMoonIntensity + Amount * MoonFlashIntensity);
	}
	// Movable sky light: SetIntensity only updates the light colour on the render thread, no recapture.
	if (bFlashSkyLight && SkyLight && SkyLight->GetLightComponent())
	{
		SkyLight->GetLightComponent()->SetIntensity(BaseSkyLightIntensity + Amount * SkyLightFlashIntensity);
	}
}

void AP9StormController::SetBoltGroupVisible(int32 Group, bool bVisible)
{
	if (!BoltGroups.IsValidIndex(Group))
	{
		return;
	}
	for (const TWeakObjectPtr<AActor>& Segment : BoltGroups[Group])
	{
		if (AActor* Actor = Segment.Get())
		{
			Actor->SetActorHiddenInGame(!bVisible);
		}
	}
}

void AP9StormController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (FlashTime < 0.0f)
	{
		if (!bEnableLightning)
		{
			return;
		}
		Countdown -= DeltaSeconds;
		if (Countdown <= 0.0f)
		{
			TriggerFlash();
		}
		return;
	}

	if (HoldRemaining > 0.0f)
	{
		HoldRemaining -= DeltaSeconds;
		ApplyFlash(1.0f);
		return;
	}

	FlashTime += DeltaSeconds;
	if (ActiveBolt != INDEX_NONE && FlashTime > BoltVisibleTime)
	{
		SetBoltGroupVisible(ActiveBolt, false);
		ActiveBolt = INDEX_NONE;
	}
	if (FlashTime >= FlashDuration)
	{
		ApplyFlash(0.0f);   // restores the exact base values
		SetBoltGroupVisible(ActiveBolt, false);
		ActiveBolt = INDEX_NONE;
		FlashTime = -1.0f;
		ScheduleNextFlash();
		if (bLogFlashes)
		{
			UE_LOG(LogP9, Log, TEXT("Storm: flash #%d ended, next in %.1fs"), FlashCount, Countdown);
		}
		return;
	}
	ApplyFlash(Envelope(FlashTime));
}

FString AP9StormController::DescribeState() const
{
	FLinearColor Cloud = FLinearColor::Black;
	if (SkyMID)
	{
		SkyMID->GetVectorParameterValue(FHashedMaterialParameterInfo(P9Storm::CloudColor), Cloud);
	}
	FLinearColor Horizon = FLinearColor::Black;
	FLinearColor Zenith = FLinearColor::Black;
	if (SkyMID)
	{
		SkyMID->GetVectorParameterValue(FHashedMaterialParameterInfo(P9Storm::HorizonColor), Horizon);
		SkyMID->GetVectorParameterValue(FHashedMaterialParameterInfo(P9Storm::ZenithColor), Zenith);
	}
	const float Moon = (MoonLight && MoonLight->GetLightComponent()) ? MoonLight->GetLightComponent()->Intensity : -1.0f;
	const float Sky = (SkyLight && SkyLight->GetLightComponent()) ? SkyLight->GetLightComponent()->Intensity : -1.0f;
	return FString::Printf(TEXT("lightning=%s flashing=%s flashes=%d nextIn=%.1fs cloud=%s baseCloud=%s horizon=%s baseHorizon=%s zenith=%s baseZenith=%s moon=%.4f baseMoon=%.4f skyLight=%.4f baseSkyLight=%.4f skyLightFlash=%s boltGroups=%d activeBolt=%d"),
		bEnableLightning ? TEXT("on") : TEXT("off"), FlashTime >= 0.0f ? TEXT("yes") : TEXT("no"), FlashCount, Countdown,
		*Cloud.ToString(), *BaseCloudColor.ToString(), *Horizon.ToString(), *BaseHorizonColor.ToString(),
		*Zenith.ToString(), *BaseZenithColor.ToString(), Moon, BaseMoonIntensity, Sky, BaseSkyLightIntensity,
		bFlashSkyLight ? TEXT("on") : TEXT("off"), BoltGroups.Num(), ActiveBolt);
}

#if !UE_BUILD_SHIPPING
namespace P9Storm
{
	void ForEachController(UWorld* World, TFunctionRef<void(AP9StormController&)> Fn)
	{
		if (!World) return;
		for (TActorIterator<AP9StormController> It(World); It; ++It) Fn(**It);
	}
}

static FAutoConsoleCommandWithWorldAndArgs GP9StormFlash(
	TEXT("p9.Storm.Flash"),
	TEXT("p9.Storm.Flash [Bolt 0|1] [HoldSeconds] - trigger a lightning flash now"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		const bool bBolt = Args.IsValidIndex(0) && FCString::Atoi(*Args[0]) != 0;
		const float Hold = Args.IsValidIndex(1) ? FCString::Atof(*Args[1]) : 0.0f;
		int32 Count = 0;
		P9Storm::ForEachController(World, [&](AP9StormController& C) { C.TriggerFlash(bBolt, Hold); ++Count; });
		UE_LOG(LogP9, Log, TEXT("p9.Storm.Flash: %d controller(s), bolt=%d hold=%.2f"), Count, bBolt ? 1 : 0, Hold);
	}));

static FAutoConsoleCommandWithWorldAndArgs GP9StormState(
	TEXT("p9.Storm.State"),
	TEXT("p9.Storm.State - log storm controller state"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		int32 Count = 0;
		P9Storm::ForEachController(World, [&](AP9StormController& C) { UE_LOG(LogP9, Log, TEXT("p9.Storm.State: %s"), *C.DescribeState()); ++Count; });
		if (Count == 0) UE_LOG(LogP9, Log, TEXT("p9.Storm.State: no storm controller in this world"));
	}));
#endif // !UE_BUILD_SHIPPING
