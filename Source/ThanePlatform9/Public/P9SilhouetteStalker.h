#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "P9SilhouetteStalker.generated.h"

class AP9CampaignDirector;
class UAnimSequence;
class UAudioComponent;
class UCapsuleComponent;
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
	/**
	 * Stage 7B: speed (cm/s) WalkAnim covers at rate 1, used only for its play rate (0 = WalkClipSpeed). WalkClipSpeed stays her
	 * own pace when she closes in (chase behaviour unchanged).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Silhouette|Assets", meta = (ClampMin = "0.0")) float WalkAnimSpeed = 0.0f;
	/** Stage 7B: stair clips fitted to the station stairs (stairs_npc3d_7b.py); the clip time follows her height on the flight. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Silhouette|Assets") TObjectPtr<UAnimSequence> StairsUpAnim;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Silhouette|Assets") TObjectPtr<UAnimSequence> StairsDownAnim;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Silhouette|Assets", meta = (ClampMin = "5.0")) float StairRise = 16.6667f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Silhouette|Assets", meta = (ClampMin = "10.0")) float StairTread = 30.0f;
	/**
	 * Stage 7B body: capsule (query-only Pawn, overlaps pawns only - never blocks the player or anything else) and the size the
	 * station NPCs keep clear of. Her navigation is the player's trail as before; only her MESH settles onto the steps (stair
	 * ramp) and side-steps a friend's body (at most 45 cm, never into a wall or off a floor).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Silhouette|Body", meta = (ClampMin = "10.0", ClampMax = "60.0")) float BodyRadius = 25.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Silhouette|Body", meta = (ClampMin = "40.0", ClampMax = "100.0")) float BodyHalfHeight = 82.0f;
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

	/**
	 * Stage 6A campaign sighting (the director's "Kanchana" anomaly, one seeded day of days 3-5): she stands at Spot until the
	 * player sees her, then follows the player for the rest of the day, speaking a line every CampaignLineInterval seconds.
	 * Independent of the random appearances and of bHideUntilCampaignComplete. Ended by the director when the day ends.
	 */
	bool BeginCampaignSighting(const FVector& Spot, float Yaw);
	void EndCampaignSighting();
	bool IsCampaignSighting() const { return bCampaignSighting; }
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Silhouette|Campaign", meta = (ClampMin = "3.0")) float CampaignLineInterval = 10.0f;
	/** She keeps talking while the player is within this range (cm); farther away she only follows. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Silhouette|Campaign", meta = (ClampMin = "300.0")) float CampaignTalkRange = 1800.0f;

	/** Console: p9.Silhouette.Status / p9.Silhouette.Force <0|1>. */
	void LogStatus() const;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Silhouette") TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, Category = "Silhouette") TObjectPtr<USkeletalMeshComponent> Body;
	UPROPERTY(VisibleAnywhere, Category = "Silhouette") TObjectPtr<UAudioComponent> Voice;
	UPROPERTY(VisibleAnywhere, Category = "Silhouette") TObjectPtr<UCapsuleComponent> BodyCapsule;   // Stage 7B

private:
	enum class EState : uint8 { Hidden, Following, Speaking, Lingering, Waiting, Pursuing };
	void TickCampaign(float DeltaSeconds, const APawn* Player, float PlayerSpeed);
	float SpeakNextLine();
	void FollowStep(const APawn* Player, float PlayerSpeed, float DeltaSeconds, bool bTalking);
	/**
	 * Stage 6B navigation (no navmesh): she follows the player's own footsteps. The trail is the player's feet sampled every
	 * TrailSpacing cm (reset by teleports, loops and train rides); she joins it only by a straight walk that a capsule sweep
	 * and floor probes prove clear, then walks it point by point - round corners, through doorways, up and down stairs.
	 */
	void RecordTrail(const APawn* Player);
	bool JoinTrail();
	bool TrailSpotBehind(const APawn* Player, bool bRequireZone, bool bRequireSightLine, FVector& Out, int32& OutIndex) const;
	bool PlaceOnTrailBehind(const APawn* Player, bool bRequireZone);
	float TrailDistanceToPlayer(const APawn* Player) const;
	bool MoveAlongTrail(const APawn* Player, float Speed, float DeltaSeconds, float StopDistance);
	bool CanWalkStraight(const FVector& From, const FVector& To) const;
	static FVector FeetOf(const APawn* Player);
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
	// Stage 7B: walk / stairs clip for a step along the trail (Speed cm/s); standing pose on the floor; mesh offsets
	void PlayMove(float Speed, float DeltaSeconds);
	void PlayStand(UAnimSequence* Anim);
	void UpdateBodyOffset(float DeltaSeconds);
	void ResetBodyOffset();

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
	// Stage 6A campaign sighting
	bool bCampaignSighting = false;
	float NextLineAt = 0.0f;
	int32 CampaignLines = 0;
	// Stage 6B trail navigation
	TArray<FVector> Trail;
	int32 TrailCursor = INDEX_NONE;          // the trail point she walks towards (INDEX_NONE: not on the trail)
	float JoinCheckTime = 0.0f;
	int32 TrailJoins = 0;
	int32 TrailCatchUps = 0;
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> Playing;
	// Stage 7B
	FVector BodyBaseRel = FVector::ZeroVector;
	float MeshZ = 0.0f, MeshZTarget = 0.0f;
	FVector SideOffset = FVector::ZeroVector;
	int32 StairDir = 0;
	float StairRampZ = 0.0f;
	float StairOffTime = 1.0f;
	UPROPERTY(Transient) TObjectPtr<class UP9NPCAnimInstance> AnimInst;   // Stage 5C crossfades
};
