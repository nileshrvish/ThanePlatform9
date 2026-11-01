#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "P9SilhouetteStalker.generated.h"

class AP9CampaignDirector;
class UAnimSequence;
class UAudioComponent;
class USkeletalMesh;
class USkeletalMeshComponent;
class USoundBase;

/**
 * Stage 5A horror character "the silhouette" (woman in a red saree). Separate from ordinary commuters and from the anomaly
 * director's own scares: it only READS the director (phase, number of active anomalies, day, seed) and changes nothing there.
 *
 *  - Active only on anomalous days, during the Exploring phase, inside the director's AnomalyZones, and only on a seeded
 *    fraction of those days (DayChance) - normal days stay normal, so she is never a false cue.
 *  - Appears behind the player, out of view, while the player walks or runs; follows at a distance, stops when the player
 *    stops (hunched menace pose).
 *  - The moment the player turns around and sees her (look direction + line of sight + range) she speaks one line at once
 *    (the next unused variation; never the same line twice in a row, at most MaxAppearancesPerDay per day, cooldown between),
 *    then vanishes the next time the player looks away (or after MaxLinger).
 *  - No collision; Visibility/Camera traces ignore her; one voice component; no ticking work while hidden.
 */
UCLASS(Blueprintable)
class THANEPLATFORM9_API AP9SilhouetteStalker : public AActor
{
	GENERATED_BODY()

public:
	AP9SilhouetteStalker();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Silhouette|Assets") TObjectPtr<USkeletalMesh> StalkerMesh;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Silhouette|Assets") TObjectPtr<UAnimSequence> IdleAnim;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Silhouette|Assets") TObjectPtr<UAnimSequence> WalkAnim;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Silhouette|Assets") TObjectPtr<UAnimSequence> MenaceAnim;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Silhouette|Assets") TObjectPtr<UAnimSequence> TalkAnim;
	/** Stage 5C: speed (cm/s) the walk clip is authored for; its play rate follows her actual follow speed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Silhouette|Assets", meta = (ClampMin = "10.0")) float WalkClipSpeed = 105.0f;
	/** Dialogue variations, independently replaceable. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Silhouette|Voice") TArray<TSoftObjectPtr<USoundBase>> Lines;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Silhouette|Voice", meta = (ClampMin = "0.0", ClampMax = "2.0")) float VoiceVolume = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Silhouette|Rules") bool bOnlyOnAnomalousDays = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Silhouette|Rules", meta = (ClampMin = "0.0", ClampMax = "1.0")) float DayChance = 0.35f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Silhouette|Rules", meta = (ClampMin = "0")) int32 MaxAppearancesPerDay = 2;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Silhouette|Rules", meta = (ClampMin = "0.0")) float Cooldown = 50.0f;
	/** Player must be moving at least this fast (walk/run) for this long before she appears. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Silhouette|Rules", meta = (ClampMin = "0.0")) float MinPlayerSpeed = 120.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Silhouette|Rules", meta = (ClampMin = "0.0")) float MovingSeconds = 2.5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Silhouette|Rules", meta = (ClampMin = "200.0")) float FollowDistance = 950.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Silhouette|Rules", meta = (ClampMin = "300.0")) float NoticeRange = 2200.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Silhouette|Rules", meta = (ClampMin = "0.5", ClampMax = "1.0")) float NoticeDot = 0.82f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Silhouette|Rules", meta = (ClampMin = "1.0")) float MaxLinger = 9.0f;
	/**
	 * Stage 5B: never during Days 1-10 (the Exploring days) while set. Off (default): the existing anomaly-day gating applies
	 * on Days 1-10 unchanged. Independent of the epilogue override below.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Silhouette|Rules") bool bHideUntilCampaignComplete = false;
	/**
	 * Stage 5B: may also appear during the playable epilogue (director phase Epilogue, the player on foot - not aboard the
	 * Dadar or Vashi local), inside the same AnomalyZones, with the same cooldown and MaxAppearancesPerDay. The epilogue is
	 * decided once per run with EpilogueChance (seeded from the run seed, separate from the day rolls).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Silhouette|Rules") bool bAppearInEpilogue = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Silhouette|Rules", meta = (ClampMin = "0.0", ClampMax = "1.0")) float EpilogueChance = 1.0f;
	/** Testing: ignore the day/anomaly rules (console p9.Silhouette.Force 1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Silhouette|Rules") bool bForceActive = false;

	/** Console: p9.Silhouette.Status / p9.Silhouette.Force <0|1>. */
	void LogStatus() const;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Silhouette") TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, Category = "Silhouette") TObjectPtr<USkeletalMeshComponent> Body;
	UPROPERTY(VisibleAnywhere, Category = "Silhouette") TObjectPtr<UAudioComponent> Voice;

private:
	enum class EState : uint8 { Hidden, Following, Speaking, Lingering };
	bool DayAllows();
	bool EpilogueAllows();
	bool PhaseAllows() const;
	bool PlayerAboardTrain() const;
	bool InZone(const FVector& P) const;
	bool PlayerSees(float MinDot) const;
	bool FindSpotBehind(FVector& Out) const;
	void Show(const FVector& Spot);
	void Hide();
	void Play(UAnimSequence* Anim);

	UPROPERTY(Transient) TObjectPtr<AP9CampaignDirector> Director;
	EState State = EState::Hidden;
	float MovingTime = 0.0f;
	float NextAllowed = 0.0f;
	float StateTime = 0.0f;
	float SpeakLeft = 0.0f;
	float SeenTime = 0.0f;
	float Now = 0.0f;
	int32 DayKey = INT32_MIN;
	bool bDayActive = false;
	int32 AppearancesToday = 0;
	int32 LastLine = INDEX_NONE;
	TArray<int32> LineBag;
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> Playing;
	UPROPERTY(Transient) TObjectPtr<class UP9NPCAnimInstance> AnimInst;   // Stage 5C crossfades
};
