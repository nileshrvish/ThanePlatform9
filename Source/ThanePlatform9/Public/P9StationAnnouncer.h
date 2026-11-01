#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "P9StationAnnouncer.generated.h"

class UAudioComponent;
class USoundAttenuation;
class USoundBase;

/** One station PA announcement: its audio and the platform it belongs to. Source text lives with the audio
 *  (P9_Assets/Announcements/Scripts/announcements_mr.json); Script here is the Roman reference for logs and checks. */
USTRUCT(BlueprintType)
struct FP9AnnouncementDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Announcement") FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Announcement") int32 Platform = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Announcement") TSoftObjectPtr<USoundBase> Sound;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Announcement", meta = (ClampMin = "0.0")) float Volume = 0.8f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Announcement") FString Script;
};

/**
 * Stage 4B station public-address system. One channel: a new announcement fades out the one playing, so two never overlap.
 * Every play carries an event key and a key is honoured only once, so repeated triggers of the same event cannot replay it.
 * Only platforms listed in AllowedPlatforms (1 and 9 in this stage) can announce. Station announcements play from the
 * platform's speaker nearest to the listener (spatialised, wide inner radius); bTrainPA plays without position (inside a train).
 */
UCLASS()
class THANEPLATFORM9_API AP9StationAnnouncer : public AActor
{
	GENERATED_BODY()

public:
	AP9StationAnnouncer();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Announcer") TArray<FP9AnnouncementDef> Announcements;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Announcer") TArray<int32> AllowedPlatforms = {1, 9};
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Announcer") TArray<FVector> Platform1Speakers;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Announcer") TArray<FVector> Platform9Speakers;
	/** Full volume within this distance of a speaker (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Announcer", meta = (ClampMin = "0.0")) float InnerRadius = 1800.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Announcer", meta = (ClampMin = "100.0")) float FalloffDistance = 6000.0f;

	/** Plays announcement Id for event EventKey. Returns false (and logs why) when refused or a duplicate. */
	bool Play(FName Id, const FString& EventKey, float Pitch = 1.0f, bool bTrainPA = false);
	/** Same rules for an arbitrary sound tagged with a platform (anomalous variants). */
	bool PlaySound(USoundBase* Sound, int32 Platform, FName LogId, const FString& EventKey, float Volume, float Pitch, bool bTrainPA);
	void StopAll(float FadeOutSeconds);
	bool IsPlaying() const;
	FName GetCurrentId() const { return CurrentId; }
	int32 GetPlayCount(FName Id) const { return PlayCounts.FindRef(Id); }
	const FP9AnnouncementDef* Find(FName Id) const;
	/** Data checks: unique ids, allowed platforms, loadable sounds. Returns the number of problems (logged). */
	int32 Validate(bool bLog) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> Current;
	UPROPERTY(Transient) TObjectPtr<USoundAttenuation> Attenuation;
	TSet<FString> PlayedKeys;
	TMap<FName, int32> PlayCounts;
	FName CurrentId;
};
