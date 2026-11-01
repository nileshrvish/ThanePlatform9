#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "P9CampaignTypes.h"
#include "P9CampaignSave.generated.h"

/**
 * Campaign progress. Only the run identity and position are stored; the current day's anomalies are regenerated on load from
 * (RunSeed, Day, History), which gives the identical plan the day had when it was first generated.
 */
UCLASS()
class THANEPLATFORM9_API UP9CampaignSave : public USaveGame
{
	GENERATED_BODY()

public:
	/** 2 = Stage 4B (adds bOpeningDone, EpilogueStage). Always written explicitly on save; save games only store values that
	 *  differ from the class default, so the default 0 identifies a legacy Stage 4A save (its version 1 was never written). */
	static constexpr int32 CurrentVersion = 2;

	UPROPERTY() int32 Version = 0;
	UPROPERTY() int32 Day = 1;
	UPROPERTY() EP9CampaignStatus Status = EP9CampaignStatus::InProgress;
	UPROPERTY() int32 RunSeed = 0;
	UPROPERTY() int32 RunId = 1;
	/** Anomaly ids of the run's completed days (oldest first, "A|B" per day, "" for a normal day); drives the repetition penalty. */
	UPROPERTY() TArray<FString> History;
	UPROPERTY() FString SavedAt;
	/** The Dadar -> Thane opening has been played (a resumed run never replays it). */
	UPROPERTY() bool bOpeningDone = true;
	/** 0 = not reached, 1 = started (Day 10 completed; the epilogue replays from the wake-up on load), 2 = finished. */
	UPROPERTY() int32 EpilogueStage = 0;
};
