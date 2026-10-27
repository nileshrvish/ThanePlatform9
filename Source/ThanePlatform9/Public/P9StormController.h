#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "P9StormController.generated.h"

class ADirectionalLight;
class ASkyLight;
class UMaterialInstanceDynamic;

/**
 * Optional storm driver for the night station: random lightning flashes on the sky dome, a brief
 * moonlight boost, and an occasional distant bolt silhouette.
 *
 * Nothing in the level depends on it: without this actor the sky dome simply stays still and dark.
 * It finds its targets by actor tag so no references need wiring:
 *  - SkyDomeTag   : static mesh actor using the storm sky material (engine M_Sky_Panning_Clouds2 instance)
 *  - MoonLightTag : the directional "moon" light
 *  - SkyLightTag  : the sky light (only used when bFlashSkyLight is on)
 *  - actors whose first tag starts with BoltTagPrefix form bolt groups (one group per distinct tag)
 * Mobile cost: a few vector parameter writes and one or two light intensity changes per frame, only while a flash lasts.
 */
UCLASS(Blueprintable)
class THANEPLATFORM9_API AP9StormController : public AActor
{
	GENERATED_BODY()

public:
	AP9StormController();

	/** Master switch for random flashes (manual TriggerFlash still works when off). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Timing")
	bool bEnableLightning = true;

	/** Delay before the first flash after BeginPlay (s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Timing", meta = (ClampMin = "0.0"))
	float FirstFlashDelay = 6.0f;

	/** Random gap between flashes (s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Timing", meta = (ClampMin = "0.5"))
	float MinFlashInterval = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Timing", meta = (ClampMin = "0.5"))
	float MaxFlashInterval = 40.0f;

	/** Total length of one flash, including the decay of the second pulse (s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Timing", meta = (ClampMin = "0.05"))
	float FlashDuration = 0.45f;

	/** Probability that a flash flickers twice. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Timing", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DoubleFlashChance = 0.55f;

	/** Gap before the second pulse (s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Timing", meta = (ClampMin = "0.02"))
	float DoubleFlashGap = 0.13f;

	/** 0..1 scale of how far the sky colours move toward the flash colours at the peak. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Look", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SkyFlashStrength = 0.85f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Look")
	FLinearColor FlashCloudColor = FLinearColor(0.55f, 0.6f, 0.75f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Look")
	FLinearColor FlashHorizonColor = FLinearColor(0.35f, 0.36f, 0.45f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Look")
	FLinearColor FlashZenithColor = FLinearColor(0.12f, 0.13f, 0.2f);

	/** Briefly raise the moon light during a flash (same light, no extra light). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Look")
	bool bFlashMoonLight = true;

	/** Intensity added to the moon light at the flash peak (light units, lux for directional lights). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Look", meta = (ClampMin = "0.0"))
	float MoonFlashIntensity = 12.0f;

	/**
	 * Briefly raise the sky light during a flash. Its ambient also reaches covered areas (canopies, footbridge)
	 * that the moon cannot. Intensity change only (no recapture).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Look")
	bool bFlashSkyLight = false;

	/** Intensity added to the sky light at the flash peak. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Look", meta = (ClampMin = "0.0"))
	float SkyLightFlashIntensity = 4.0f;

	/** Keep the sky material's moon disk ("Light direction") aligned with the moon light. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Look")
	bool bSyncMoonDirection = true;

	/** Probability that a flash also shows one bolt group. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Bolt", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BoltChance = 0.5f;

	/** How long a bolt group stays visible (s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Bolt", meta = (ClampMin = "0.02"))
	float BoltVisibleTime = 0.14f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Storm|Targets")
	FName SkyDomeTag = TEXT("P9SkyDome");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Storm|Targets")
	FName MoonLightTag = TEXT("P9MoonLight");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Storm|Targets")
	FName SkyLightTag = TEXT("P9SkyLight");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Storm|Targets")
	FString BoltTagPrefix = TEXT("P9Bolt");

	/** Log each flash (development aid). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Debug")
	bool bLogFlashes = false;

	/** Starts a flash now. HoldSeconds > 0 keeps it at peak brightness that long (for screenshots). */
	UFUNCTION(BlueprintCallable, Category = "Storm")
	void TriggerFlash(bool bForceBolt = false, float HoldSeconds = 0.0f);

	/** One-line status for logs and test commands. */
	FString DescribeState() const;

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void ResolveTargets();
	void ScheduleNextFlash(float ExtraDelay = 0.0f);
	float Envelope(float T) const;
	void ApplyFlash(float Amount);
	void SetBoltGroupVisible(int32 Group, bool bVisible);

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SkyMID;

	UPROPERTY(Transient)
	TObjectPtr<ADirectionalLight> MoonLight;

	UPROPERTY(Transient)
	TObjectPtr<ASkyLight> SkyLight;

	TArray<TArray<TWeakObjectPtr<AActor>>> BoltGroups;

	FLinearColor BaseCloudColor = FLinearColor::Black;
	FLinearColor BaseHorizonColor = FLinearColor::Black;
	FLinearColor BaseZenithColor = FLinearColor::Black;
	float BaseMoonIntensity = 0.0f;
	float BaseSkyLightIntensity = 0.0f;

	float Countdown = 0.0f;
	float FlashTime = -1.0f;     // < 0 when idle
	float HoldRemaining = 0.0f;
	bool bDoubleFlash = false;
	int32 ActiveBolt = INDEX_NONE;
	int32 FlashCount = 0;
};
