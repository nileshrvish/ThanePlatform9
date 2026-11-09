#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "P9TrainAudio.generated.h"

class AP9Train;
class UAudioComponent;
class USoundBase;

/**
 * Stage 9A: the running sound of every AP9Train in the level - an isolated LISTENER. It reads each train's public state
 * (GetTrainState, GetSpeed, GetRouteDistance, EvaluateRoute, IsPlayerAboard) and plays sound on the train; it never changes the
 * train's movement, doors, boarding, seating, impacts or timing.
 *
 *  - Roll: wheel-rail roar + body rumble; level and pitch follow the speed.
 *  - Traction: the EMU motor / gear whine; its pitch follows the speed, louder while accelerating, fading while coasting / braking.
 *  - Clatter: one wheelset-over-rail-joint sound per axle per joint (13 m rails), placed at the axle - the rhythm is the real
 *    speed of the train (da-dum ... da-dum from each bogie).
 *  - Braking: the air-brake application when braking begins, the brake-block friction (rising as the train slows, soft squeal
 *    near the stop), the brake-cylinder vent at standstill.
 *  - Idle: compressor / blower hum while a train stands.
 *  - Horn: on approach (the train starts braking towards a stop with the player not aboard and within ApproachHornRange).
 *    The departure horn stays the train's own (its HornSound property points at the 9A horn).
 * The trains' own run loop and brake sound are set to none on the level's trains (so nothing plays twice).
 * Console: p9.TrainAudio.Status.
 */
UCLASS()
class THANEPLATFORM9_API AP9TrainAudio : public AActor
{
	GENERATED_BODY()

public:
	AP9TrainAudio();

	UPROPERTY(EditAnywhere, Category = "TrainAudio") TSoftObjectPtr<USoundBase> RollLoop;
	UPROPERTY(EditAnywhere, Category = "TrainAudio") TSoftObjectPtr<USoundBase> TractionLoop;
	UPROPERTY(EditAnywhere, Category = "TrainAudio") TSoftObjectPtr<USoundBase> IdleLoop;
	UPROPERTY(EditAnywhere, Category = "TrainAudio") TSoftObjectPtr<USoundBase> BrakeFrictionLoop;
	UPROPERTY(EditAnywhere, Category = "TrainAudio") TSoftObjectPtr<USoundBase> BrakeApply;
	UPROPERTY(EditAnywhere, Category = "TrainAudio") TSoftObjectPtr<USoundBase> StopRelease;
	UPROPERTY(EditAnywhere, Category = "TrainAudio") TArray<TSoftObjectPtr<USoundBase>> Clatter;
	UPROPERTY(EditAnywhere, Category = "TrainAudio") TSoftObjectPtr<USoundBase> ApproachHorn;
	UPROPERTY(EditAnywhere, Category = "TrainAudio") TSoftObjectPtr<USoundBase> ApproachHornDistant;

	/** Rail length (cm) between joints, and the axle positions along each car (cm from the car centre). */
	UPROPERTY(EditAnywhere, Category = "TrainAudio") float RailLength = 1300.0f;
	UPROPERTY(EditAnywhere, Category = "TrainAudio") TArray<float> AxleOffsets = {-860.0f, -650.0f, 650.0f, 860.0f};
	UPROPERTY(EditAnywhere, Category = "TrainAudio") float CarCentre = 1066.5f;
	/** Approach horn only for trains braking within this distance of the player (cm), not when the player is aboard. */
	UPROPERTY(EditAnywhere, Category = "TrainAudio") float ApproachHornRange = 9000.0f;
	UPROPERTY(EditAnywhere, Category = "TrainAudio") float Volume = 1.0f;

	virtual void Tick(float DeltaSeconds) override;
	FString GetStatus() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	struct FTrainSound
	{
		TWeakObjectPtr<AP9Train> Train;
		TObjectPtr<UAudioComponent> Roll = nullptr;
		TObjectPtr<UAudioComponent> Traction = nullptr;
		TObjectPtr<UAudioComponent> Idle = nullptr;
		TObjectPtr<UAudioComponent> Brake = nullptr;
		TObjectPtr<UAudioComponent> OneShot = nullptr;
		uint8 LastState = 0;
		float LastSpeed = 0.0f;
		float LastDistance = -1.0f;
		float BrakeLevel = 0.0f;
		float TractionLevel = 0.0f;
		float RollLevel = 0.0f;
		float IdleLevel = 0.0f;
		int32 Clacks = 0;
		int32 Horns = 0;
	};
	UAudioComponent* MakeComp(AP9Train* Train, USoundBase* Sound, float Inner, float Falloff, const TCHAR* Name);
	void TickTrain(FTrainSound& T, float DeltaSeconds);
	void PlayAt(USoundBase* Sound, const FVector& Where, float Vol, float Pitch, float Falloff);

	TArray<FTrainSound> Trains;
	UPROPERTY(Transient) TArray<TObjectPtr<UAudioComponent>> Owned;
	FRandomStream Rng;
	int32 LastClack = -1;
};
