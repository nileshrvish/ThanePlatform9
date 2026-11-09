#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "P9CampaignTypes.h"
#include "P9AnomalySpawnPoint.generated.h"

/**
 * A place where the campaign director may put an anomaly. Placed in the level and listed on the director (stable references,
 * no world scans at runtime). The point's transform is where spawned things appear; TargetActor is the existing actor an
 * anomaly alters (sign, lamp, bench, train...). Invisible in game.
 */
UCLASS()
class THANEPLATFORM9_API AP9AnomalySpawnPoint : public AActor
{
	GENERATED_BODY()

public:
	AP9AnomalySpawnPoint();

	/** Stable id used in day plans, logs and saves. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Anomaly") FName PointId;
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Anomaly") EP9AnomalyLocation LocationType = EP9AnomalyLocation::PlatformSign;
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Anomaly") TObjectPtr<AActor> TargetActor;
	/** Optional second actor that belongs to the same target (e.g. a lamp's tube mesh). */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Anomaly") TObjectPtr<AActor> SecondaryActor;
	/** Further actors that belong to the target (e.g. a lamp's fill light); changed together and covered by baseline checks. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Anomaly") TArray<TObjectPtr<AActor>> ExtraActors;
	/** Narrows the target to a part of TargetActor (e.g. one train car); also part of the one-controller-per-target key. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Anomaly") FName TargetComponent;
	/**
	 * Stage 6A loop anomalies (endless footbridge / stair): when the player's feet enter this world box while moving along
	 * LoopDirection, the player is moved by LoopOffset (facing and speed kept), so the stretch ahead never ends.
	 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Anomaly|Loop") FBox LoopTrigger = FBox(ForceInit);
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Anomaly|Loop") FVector LoopOffset = FVector::ZeroVector;
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Anomaly|Loop") FVector LoopDirection = FVector::ForwardVector;

	/** Two points with the same key are never used on the same day. */
	FString GetTargetKey() const;

private:
	UPROPERTY(VisibleAnywhere, Category = "Anomaly") TObjectPtr<USceneComponent> Root;
#if WITH_EDITORONLY_DATA
	UPROPERTY() TObjectPtr<class UArrowComponent> Arrow;
#endif
};
