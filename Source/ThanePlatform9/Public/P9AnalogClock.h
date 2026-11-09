#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "P9AnalogClock.generated.h"

class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EP9AnalogClockMode : uint8
{
	Normal,            // all hands clockwise, showing the station time
	ReverseArmed,      // the anomaly is active and waits for the player to look at a dial before it turns
	Reversing          // the displayed time runs backwards (all hands counterclockwise)
};

/**
 * Stage 6C: the double-faced railway analog clock hanging in the footbridge concourse.
 * One clock state (station time in seconds) drives all six hands (hour/minute/second on the +Y and -Y faces), so the
 * faces always agree. Each campaign day starts at StartTime and runs at TimeScale; the campaign director turns the
 * "Reverse Analog Clock" anomaly on (BeginReverse) and off (EndReverse, on every day cleanup).
 * Hand meshes point at 12 (+Z) with their pivot at the clock centre; they turn about the local Y axis.
 * The tick only advances two numbers unless the clock was rendered recently.
 */
UCLASS()
class THANEPLATFORM9_API AP9AnalogClock : public AActor
{
	GENERATED_BODY()

public:
	AP9AnalogClock();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Clock") TObjectPtr<USceneComponent> Root;
	/** Housing, both dials, centre pins, pole and ceiling flange (the only part with collision). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Clock") TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Clock") TObjectPtr<UStaticMeshComponent> Glass;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Clock") TObjectPtr<UStaticMeshComponent> HourFront;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Clock") TObjectPtr<UStaticMeshComponent> MinuteFront;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Clock") TObjectPtr<UStaticMeshComponent> SecondFront;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Clock") TObjectPtr<UStaticMeshComponent> HourBack;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Clock") TObjectPtr<UStaticMeshComponent> MinuteBack;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Clock") TObjectPtr<UStaticMeshComponent> SecondBack;

	/** Station time at the start of every campaign day, "HH:MM[:SS]" (the platform's digital clock reads 23:47). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clock") FString StartTime = TEXT("23:47:00");
	/** Clock seconds per real second (1 = real time). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clock", meta = (ClampMin = "0.0")) float TimeScale = 1.0f;
	/** Dial surface distance from the centre plane (cm) and each hand's height above the dial (X hour, Y minute, Z second). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clock") float DialOffset = 6.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clock") FVector HandLayers = FVector(0.6f, 1.1f, 1.6f);
	/** Reverse anomaly: the player counts as looking at a dial within this distance (cm) and view cone (cos). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clock|Reverse") float SeenDistance = 2600.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clock|Reverse") float SeenMinDot = 0.86f;
	/** Seconds the clock keeps running normally after it is first seen, before the second hand stops. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clock|Reverse") float TriggerDelay = 0.8f;
	/** Ramp from the slow reverse to the full reverse rate (s). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clock|Reverse") float RampSeconds = 3.0f;

	/** Back to StartTime (the director calls this through OnDayStarted). The reverse mode is not touched. */
	UFUNCTION(BlueprintCallable, Category = "Clock") void ResetToStartTime();
	/** Sets the station time (both the true and the displayed time). */
	UFUNCTION(BlueprintCallable, Category = "Clock") void SetStationTime(double Seconds);
	/**
	 * Starts the anomaly. bWaitUntilSeen: the clock runs normally until the player looks at a dial, keeps going for
	 * TriggerDelay, the second hand stops and twitches for PauseSeconds, then runs backwards at real speed for SlowSeconds
	 * (the second hand visibly first) before all hands ramp to ReverseRate x backwards. Otherwise it runs backwards at
	 * ReverseRate from now on, without the pause.
	 */
	UFUNCTION(BlueprintCallable, Category = "Clock") void BeginReverse(float ReverseRate, float PauseSeconds, float SlowSeconds, bool bWaitUntilSeen);
	/** Ends the anomaly: clockwise again and showing the correct station time. */
	UFUNCTION(BlueprintCallable, Category = "Clock") void EndReverse();

	bool IsReversed() const { return Mode != EP9AnalogClockMode::Normal; }
	EP9AnalogClockMode GetMode() const { return Mode; }
	double GetDisplaySeconds() const { return DisplaySeconds; }
	double GetTrueSeconds() const { return TrueSeconds; }
	float GetRate() const { return Rate; }
	/** All seven meshes are assigned (for the anomaly audit). */
	bool HasAllParts() const;
	/** Current hand angles in degrees clockwise from 12 (hour, minute, second), as drawn. */
	FVector GetHandAngles() const;
	FString GetStatus() const;

	static double ParseTime(const FString& Text);
	static FString FormatTime(double Seconds);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnConstruction(const FTransform& Transform) override;

	UFUNCTION() void HandleDayStarted(int32 Day);

private:
	void LayoutHands();
	void ApplyHands();
	bool IsSeenByPlayer() const;

	double TrueSeconds = 0.0;
	double DisplaySeconds = 0.0;
	EP9AnalogClockMode Mode = EP9AnalogClockMode::Normal;
	float Rate = 1.0f;
	float SecondTwitch = 0.0f;           // visual only, degrees on the second hand during the pause
	float SequenceTime = 0.0f;           // since the reverse was triggered
	float SeenCheckTimer = 0.0f;
	float ReverseRateTarget = 1.0f;
	float PauseTime = 1.5f;
	float SlowTime = 6.0f;
	bool bWaitForSight = true;
	bool bTriggered = false;
};
