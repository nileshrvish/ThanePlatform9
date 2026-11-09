#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "P9FriendNPC.generated.h"

class AP9CampaignDirector;
class AP9StationLife;
class AP9Train;
class UP9NPCAnimInstance;
class UP9SeatComponent;
class UAnimSequence;
class UAudioComponent;
class UCapsuleComponent;
class USkeletalMesh;
class USkeletalMeshComponent;
class USoundBase;
class UStaticMesh;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EP9FriendActivity : uint8
{
	Idle,
	Chat,
	LookAround,
	Turn,
	Walk,
	Sit,
	Meet,
	Speak,
	Train,    // Stage 5C: boards the epilogue Dadar local, sits, gets off again
	Scripted  // Stage 5C: the vendor plays the epilogue wake-up, then walks back to his stall
};

/** What a friend's routine does at a route point. */
UENUM(BlueprintType)
enum class EP9NPCStop : uint8
{
	Pass,     // walk through
	Stand,    // stand, look around, idle
	Sit,      // sit down facing FaceYaw (bench), stand up afterwards
	Stall,    // stand at a stall counter facing FaceYaw; the stall's vendor reacts
	Wait      // wait for a train (commuter end points)
};

UENUM(BlueprintType)
enum class EP9NPCRole : uint8
{
	Group,         // Stage 5A v1: stands with its group, chats and looks around
	Vendor,        // stands at a stall, pours, speaks to the player when the player comes close
	Commuter,      // walks the whole route end to end and waits at the ends
	BenchSitter,   // mostly sits at a Sit stop, now and then gets up and strolls
	StallVisitor,  // alternates between its stops and a Stall stop
	Wanderer       // drifts between its stops
};

UENUM()
enum class EP9NPCClip : uint8
{
	None, Idle, Walk, Turn, Look, Chat, SitDown, SitIdle, StandUp, Gesture, Pour,
	DrunkWalk, StairsUp,  // Stage 5G (Mixamo clips)
	StairsDown            // Stage 7B
};

USTRUCT(BlueprintType)
struct FP9NPCRoutePoint
{
	GENERATED_BODY()

	/** World position on the floor (the friend's feet). Consecutive points must have a clear straight walk between them. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Route") FVector Location = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Route") EP9NPCStop Stop = EP9NPCStop::Pass;
	/** Facing at the stop (bench, stall counter, track); used when bUseFaceYaw. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Route") bool bUseFaceYaw = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Route") float FaceYaw = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Route", meta = (ClampMin = "0.0")) float MinWait = 6.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Route", meta = (ClampMin = "0.0")) float MaxWait = 14.0f;
};

/**
 * Stage 5A station NPC (friends, chaiwala). Self-contained ambient character: it never touches the campaign, the anomaly
 * director, the trains or the player. Its capsule has no collision by default (it never blocks the player, doors, stairs or
 * the footbridge) and ignores Visibility/Camera traces, so anomaly visibility checks and scare placement never see it.
 * Walking NPCs pause when the player is right in front of them.
 *
 * Routine: a route of world points with stops (stand, sit on a bench, stall, wait for a train). Each role picks its next stop
 * differently; walking follows the route polyline so stairs and corridors are respected. AP9StationLife can pull friends off
 * their routines to meet at a spot and hold a short conversation (lines played from each friend's Voice component), after
 * which they walk back to their routes. Vendors (the chaiwala) speak their line when the player comes close and looks.
 *
 * Animation (Stage 5C): clips crossfade through UP9NPCAnimInstance. Big gesture clips change at loop ends; sit transitions
 * are one-shots. Walking eases in and out and the walk clip's rate follows the actual speed (the clip is authored for
 * WalkSpeed, so feet stay planted). Beyond FullRateDistance the NPC updates five times a second and poses only when rendered.
 *
 * Epilogue train (Stage 5C): while the Dadar local stands at Platform 1 with its doors open and the player is aboard,
 * up to MaxEpilogueRiders friends near it walk in through the nearest door, sit in free seats near the player and get off
 * again as soon as the player stands up or leaves (before the director closes the doors). Trains are only read
 * (state, doors, seats); nothing in the train or the campaign is changed.
 */
UCLASS(Blueprintable)
class THANEPLATFORM9_API AP9FriendNPC : public AActor
{
	GENERATED_BODY()

public:
	AP9FriendNPC();

	// ---------------- identity
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Friend") FString FriendName;
	/** Short key used by conversations (F1..F5, CW). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Friend") FName FriendKey;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Friend") FName GroupId = TEXT("Friends");
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Friend") int32 Seed = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Friend") EP9NPCRole NPCRole = EP9NPCRole::Group;

	// ---------------- assets
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Friend|Assets") TObjectPtr<USkeletalMesh> FriendMesh;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Friend|Assets") TObjectPtr<UAnimSequence> IdleAnim;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Friend|Assets") TObjectPtr<UAnimSequence> ChatAnim;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Friend|Assets") TObjectPtr<UAnimSequence> LookAroundAnim;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Friend|Assets") TObjectPtr<UAnimSequence> TurnAnim;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Friend|Assets") TObjectPtr<UAnimSequence> WalkAnim;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Friend|Assets") TObjectPtr<UAnimSequence> SitDownAnim;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Friend|Assets") TObjectPtr<UAnimSequence> SitIdleAnim;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Friend|Assets") TObjectPtr<UAnimSequence> StandUpAnim;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Friend|Assets") TObjectPtr<UAnimSequence> GestureAnim;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Friend|Assets") TObjectPtr<UAnimSequence> PourAnim;
	/** Stage 5G: optional drunk walk (used on drunk legs) and stair-climb clip (used where the path rises). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Friend|Assets") TObjectPtr<UAnimSequence> DrunkWalkAnim;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Friend|Assets") TObjectPtr<UAnimSequence> StairsUpAnim;
	/** Stage 7B: stair-descent clip (used where the floor falls along the walking direction). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Friend|Assets") TObjectPtr<UAnimSequence> StairsDownAnim;
	/**
	 * Stage 7B: the stair clips are fitted to the station stairs (one cycle = two treads; phase 0 = root over a tread centre,
	 * see stairs_npc3d_7b.py). The clip time then follows the NPC's height on the flight, so a foot lands on every tread at
	 * any speed. Off: the Stage 5G behaviour (clip rate from StairsUpClipSpeed).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Friend|Assets") bool bStairClipsFitted = false;

	// ---------------- routine
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Friend|Routine") TArray<FP9NPCRoutePoint> Route;
	/** Route point the friend starts at (its placed location should match it). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Friend|Routine", meta = (ClampMin = "0")) int32 StartIndex = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Friend|Routine", meta = (ClampMin = "10.0")) float WalkSpeed = 105.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Friend|Routine", meta = (ClampMin = "0.5", ClampMax = "1.5")) float PlayRate = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Friend|Routine", meta = (ClampMin = "10.0")) float TurnDegreesPerSecond = 140.0f;
	/** Stage 5G: speed (cm/s) the walk clip covers at rate 1 (0 = WalkSpeed); the clip rate is scaled so the feet stay planted. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Friend|Routine", meta = (ClampMin = "0.0")) float WalkClipSpeed = 0.0f;
	/** Stage 5G drunk legs: chance that a routine leg is walked drunk, its speed (= the drunk clip's own speed) and mumbled lines. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Friend|Drunk", meta = (ClampMin = "0.0", ClampMax = "1.0")) float DrunkChance = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Friend|Drunk", meta = (ClampMin = "10.0")) float DrunkWalkSpeed = 95.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Friend|Drunk") TArray<TSoftObjectPtr<USoundBase>> DrunkLines;
	/** Seconds of quiet between drunk lines (random in range); lines only play when the camera is within AudibleDistance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Friend|Drunk") FVector2D DrunkLineGap = FVector2D(6.0f, 14.0f);
	/** Stage 5G stairs: horizontal speed while climbing, and the stair clip's own speed along its slope (cm/s at rate 1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Friend|Routine", meta = (ClampMin = "10.0")) float StairsUpSpeed = 60.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Friend|Routine", meta = (ClampMin = "10.0")) float StairsUpClipSpeed = 45.0f;
	/** Stage 9C bench sitting: the pelvis lands this far (cm) behind the seat's front edge (found by probing the seat behind the
	 *  sit stop); the NPC stands at the point that puts it there and shuffles onto it while turning (p9.Friends.SitInset overrides). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Friend|Sit", meta = (ClampMin = "0.0", ClampMax = "40.0")) float SitSeatInset = 20.0f;
	/** How far (cm) the sit clips move the pelvis back from the stand point (0 = measured on the first sit; before that the
	 *  22 cm x height / 1.75 m of the clip build). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Friend|Sit", meta = (ClampMin = "0.0")) float SitHipShift = 0.0f;
	/** Stage 9C: a breath of standing still before sitting down and after standing up (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Friend|Sit", meta = (ClampMin = "0.0")) float SitSettleTime = 0.35f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Friend|Sit", meta = (ClampMin = "0.0")) float StandSettleTime = 0.5f;
	/** Stage 7B: horizontal speed while descending stairs (with StairsDownAnim; without it the walk speed as before). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Friend|Routine", meta = (ClampMin = "10.0")) float StairsDownSpeed = 66.7f;
	/** Stage 7B: the station stair the fitted clips are authored for (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Friend|Routine", meta = (ClampMin = "5.0")) float StairRise = 16.6667f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Friend|Routine", meta = (ClampMin = "10.0")) float StairTread = 30.0f;

	// ---------------- standing behaviour (group/vendor and at stops)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Friend|Behaviour", meta = (ClampMin = "0.0")) float IdleWeight = 0.40f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Friend|Behaviour", meta = (ClampMin = "0.0")) float ChatWeight = 0.35f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Friend|Behaviour", meta = (ClampMin = "0.0")) float LookAroundWeight = 0.20f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Friend|Behaviour", meta = (ClampMin = "0.0")) float WanderWeight = 0.05f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Friend|Behaviour", meta = (ClampMin = "0.0", ClampMax = "1.0")) float FaceSpeakerChance = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Friend|Behaviour", meta = (ClampMin = "0.5")) float MinActivitySeconds = 4.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Friend|Behaviour", meta = (ClampMin = "0.5")) float MaxActivitySeconds = 9.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Friend|Behaviour", meta = (ClampMin = "0.0")) float WanderRadius = 120.0f;

	// ---------------- voice
	/** Vendor only: spoken when the player comes within SpeechRadius and looks at the vendor (once per SpeechCooldown). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Friend|Voice") TSoftObjectPtr<USoundBase> ProximityLine;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Friend|Voice", meta = (ClampMin = "100.0")) float SpeechRadius = 700.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Friend|Voice", meta = (ClampMin = "0.0")) float SpeechCooldown = 150.0f;
	/** Lines are only played when the camera is this close (mobile audio budget); the conversation still runs silently. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Friend|Voice", meta = (ClampMin = "100.0")) float AudibleDistance = 2500.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Friend|Voice", meta = (ClampMin = "0.0", ClampMax = "2.0")) float VoiceVolume = 1.0f;
	/**
	 * Stage 5B (vendor): the proximity line is spoken at most once per campaign day. The flag and the cooldown are reset when
	 * the campaign director starts a day (OnDayStarted). Off: the plain SpeechCooldown behaviour.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Friend|Voice") bool bOncePerCampaignDay = false;

	// ---------------- availability (Stage 5B)
	/**
	 * Hidden and inactive (no rendering, ticking, animation, voice or meet-ups) while the campaign is in progress (Days 1-10).
	 * Available from the moment Day 10 is cleared and the epilogue begins (director phase Epilogue/Ending or status Completed),
	 * and hidden again when a new campaign starts. Console override: p9.Friends.ForceUnlock -1 (auto) | 0 | 1.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Friend|Availability") bool bHideUntilCampaignComplete = false;

	// ---------------- epilogue train (Stage 5C)
	/** Board the epilogue Dadar local at Platform 1 while the player is aboard (non-vendors with a route only). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Friend|Train") bool bRideEpilogueTrain = true;
	/** Only friends within this distance of a door (on the platform level) board. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Friend|Train", meta = (ClampMin = "100.0")) float BoardRadius = 2600.0f;

	// ---------------- held props (Stage 5C: the chaiwala's ketli and glass)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Friend|Props") TObjectPtr<UStaticMesh> HeldMeshRight;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Friend|Props") TObjectPtr<UStaticMesh> HeldMeshLeft;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Friend|Props") FName HeldSocketRight = TEXT("hand_r");
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Friend|Props") FName HeldSocketLeft = TEXT("hand_l");
	/** Prop transform in the hand bone's space. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Friend|Props") FTransform HeldOffsetRight;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Friend|Props") FTransform HeldOffsetLeft;

	// ---------------- epilogue wake-up (Stage 5C, vendor only)
	/**
	 * World floor points from the Dadar local's door back to the vendor's stall spot (last point = his place behind the
	 * counter). Used after the epilogue wake-up, which the campaign director lets this vendor play (BeginEpilogueWake).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Friend|Epilogue") TArray<FVector> EpilogueReturnPath;
	/** Director hook: mirror Proxy (the director's hidden epilogue chaiwala, moved along its train-local path) with real clips. */
	void BeginEpilogueWake(AActor* Proxy);
	/** Director hook: the wake-up walk ended at the door - walk back to the stall. Ignored unless mirroring. */
	void EndEpilogueWake();
	bool CanPlayEpilogueWake() const { return NPCRole == EP9NPCRole::Vendor && bAvailableNow && !EpilogueReturnPath.IsEmpty(); }
	UFUNCTION(BlueprintPure, Category = "Friend") bool IsNPCAvailable() const { return bAvailableNow; }

	// ---------------- collision / performance
	/** Off (default): the NPC never blocks anything. On: the player bumps into it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Friend|Collision") bool bBlockPlayer = false;
	/**
	 * Stage 7B body: capsule radius / half height (cm, sized per person). The capsule is a query-only Pawn that only OVERLAPS
	 * pawns (never blocks the player, trains, doors or traces; generates no overlap events). NPC-to-NPC separation uses these
	 * sizes in code (P9NPCLoco bodies): walkers steer round or slow behind other bodies, standing NPCs ease apart, seated and
	 * train-riding NPCs are never moved. A walker held up for long passes anyway (no permanent block).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Friend|Collision", meta = (ClampMin = "10.0", ClampMax = "60.0")) float BodyRadius = 28.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Friend|Collision", meta = (ClampMin = "40.0", ClampMax = "100.0")) float BodyHalfHeight = 88.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Friend|Collision") bool bAvoidOthers = true;
	/**
	 * Stage 8A hybrid body: a box aligned with the body (rotates with the NPC) - BodyRadius x BoxDepthScale deep, x BoxWidthScale
	 * wide, BodyHalfHeight tall. Like the capsule it is query-only and only overlaps pawns (never blocks the player, trains, doors,
	 * stairs or traces, no overlap events). The hard no-overlap rule between NPCs uses these boxes (separating-axis test): a step
	 * that would push one body into another is pushed out, or held, instead of passing through. Steering stays on the round body.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Friend|Collision", meta = (ClampMin = "0.3", ClampMax = "1.0")) float BoxDepthScale = 0.6f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Friend|Collision", meta = (ClampMin = "0.5", ClampMax = "1.2")) float BoxWidthScale = 0.95f;
	/** A walker held by a body it cannot get round passes it only after this long (deadlock guard). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Friend|Collision", meta = (ClampMin = "2.0")) float PassThroughAfter = 8.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Friend|Performance", meta = (ClampMin = "100.0")) float FullRateDistance = 3000.0f;

	UFUNCTION(BlueprintPure, Category = "Friend") EP9FriendActivity GetActivity() const { return Activity; }
	UFUNCTION(BlueprintPure, Category = "Friend") FVector GetGroupCenter() const;
	void LogStatus() const;
	/** Stage 9C test helper: go and sit on the route's Sit stop now (or stand up if seated). */
	void DebugSit();

	// ---------------- AP9StationLife interface
	bool IsAvailableForMeet() const;
	/** Closest route point to Center within MaxDistance (INDEX_NONE if none). */
	int32 RouteIndexNear(const FVector& Center, float MaxDistance) const;
	/** Walking distance (cm) from here to Point via the route (to the route point nearest Point, then straight). */
	float RouteDistanceTo(const FVector& Point) const;
	void BeginMeet(const FVector& Slot, const FVector& Center);
	bool HasArrivedAtMeet() const { return Activity == EP9FriendActivity::Meet && bMeetArrived; }
	bool IsInMeet() const { return Activity == EP9FriendActivity::Meet || Activity == EP9FriendActivity::Speak; }
	bool IsOnTrain() const { return Activity == EP9FriendActivity::Train; }
	/** Plays a line (audible only near the camera) with the talking clip; returns its length in seconds. */
	float Say(USoundBase* Line);
	void FaceTowards(const FVector& Point);
	void EndMeet(float Delay);
	/** A visitor arrived at this vendor's stall. */
	void OnCustomer(AP9FriendNPC* Customer);

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Friend") TObjectPtr<UCapsuleComponent> Capsule;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Friend") TObjectPtr<class UBoxComponent> BodyBox;     // Stage 8A
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Friend") TObjectPtr<USkeletalMeshComponent> Body;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Friend") TObjectPtr<UAudioComponent> Voice;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> HeldRight;     // runtime only (Stage 5C)
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> HeldLeft;

private:
	void ApplyAssets();
	void ApplyCollision();
	void StartRoutine();
	void UpdateAvailability();
	void SetNPCAvailable(bool bAvailable);
	UFUNCTION() void HandleDayStarted(int32 Day);
	void DisableDebugDrawing();
	UAnimSequence* ClipAnim(EP9NPCClip Clip) const;
	void SetClip(EP9NPCClip Clip);
	void UpdateClip(float DeltaSeconds);
	bool OneShotDone() const;
	void ChooseNext();
	void StartActivity(EP9FriendActivity Next, float Seconds);
	void ChooseRoutineStop();
	void WalkPath(const TArray<FVector>& Points);
	void ArriveAtStop();
	bool StepAlongPath(float DeltaSeconds);
	bool TurnTowardsYaw(float Yaw, float DeltaSeconds);
	float YawTo(const FVector& Point) const;
	bool PlayerBlocksWay() const;
	void TickVendor(float DeltaSeconds);
	bool CameraNear(float Distance) const;
	AP9FriendNPC* GroupSpeaker() const;
	TArray<AP9FriendNPC*> GroupMembers() const;
	TArray<FVector> RoutePathTo(int32 Target) const;
	// Stage 5C
	void InitAnimInstance();
	void TryBoardEpilogueTrain();
	void TickTrain(float DeltaSeconds);
	void LeaveTrainSeat();
	void FinishTrainRide();
	float SitStandOffset() const;
	void ApplyHeldProps();
	EP9NPCClip MoveClip() const;
	void TickDrunkLines();
	void TickScripted(float DeltaSeconds);
	// Stage 7B
	FVector AvoidanceSteer(const FVector& Dir, float& OutSpeedScale, float DeltaSeconds);
	/** Point Look cm ahead along Loc -> Path[0] -> Path[1] ... (pure pursuit: smooth curves round path corners). */
	FVector PursuitPoint(const FVector& Loc, float Look) const;
	void SeparateStanding(float DeltaSeconds);
	bool CanBeMovedBySeparation() const;
	// Stage 9A
	/** Route / return-path points that stand inside furniture (no floor under the body centre) are moved to the nearest clear floor. */
	void ValidateRouteTargets();
	bool FindClearFloorNear(const FVector& FeetPoint, float MaxRadius, FVector& Out) const;
	bool HasFloorUnder(const FVector& FeetPoint) const;
	// Stage 9C bench sitting
	/** Probes the seat behind Stand along -Fwd: distance to its front edge and the seat height. False = no seat found. */
	bool ProbeSeat(const FVector& StandFeet, const FVector& Fwd, float& OutFrontDist, float& OutSeatZ) const;
	void BeginSitAlign();
	float HipShift() const;
	void MeasureSeated();
	/** A walker that has made no progress for a while: arrive here / back out of furniture / detour / skip the point. */
	void RecoverBlocked(const TCHAR* Reason);
	/** A path that turns back behind a walker: it slows to a stop along its facing, turns on the spot, then walks on. */
	bool HandleReversal(float DeltaSeconds);
	/** Slows a walker to a stop along its facing (true while still moving). */
	bool DecelerateToStop(float DeltaSeconds);
	void TickProgressWatchdog(float DeltaSeconds);

	FRandomStream Rng;
	EP9FriendActivity Activity = EP9FriendActivity::Idle;
	float ActivityLeft = 0.0f;
	FVector Home = FVector::ZeroVector;
	float HomeYaw = 0.0f;
	float TargetYaw = 0.0f;
	bool bTurnedToSpeaker = false;
	// route state
	int32 RouteIndex = 0;
	int32 TargetIndex = INDEX_NONE;
	int32 CommuterDir = 1;
	TArray<FVector> Path;
	mutable TArray<int32> PathIdx;   // route index of each Path point (INDEX_NONE for off-route points)
	float YieldTime = 0.0f;
	float DwellLeft = 0.0f;
	EP9NPCStop CurrentStop = EP9NPCStop::Stand;
	bool bReturning = false;
	int32 MeetRouteIndex = INDEX_NONE;
	bool bMeetAfterStand = false;
	EP9FriendActivity PreSpeakActivity = EP9FriendActivity::Idle;
	// stop state
	int32 SitPhase = 0;          // 0 turn (+ shuffle onto the stand point), 1 sitting down, 2 seated, 3 standing up, 4 settle before
	                             // sitting, 5 short walk to the stand point, 6 settle after standing up (Stage 9C)
	FVector SitAlignTarget = FVector::ZeroVector;
	bool bSitAligned = true;
	float SitSettleLeft = 0.0f;
	float SitSeatFront = 0.0f;   // seat front edge: distance behind the aligned stand point (cm), -1 = unknown
	float SitSeatZ = 0.0f;
	float MeasuredHipShift = 0.0f;
	bool bSitMeasured = false;
	float SeatedTime = 0.0f;
	bool bLeaveAfterTurn = false;
	// meet state
	FVector MeetSlot = FVector::ZeroVector;
	FVector MeetCenter = FVector::ZeroVector;
	bool bMeetArrived = false;
	float EndMeetDelay = -1.0f;
	float SpeakLeft = 0.0f;
	// vendor state
	float NextSpeechTime = 0.0f;
	float WorldTime = 0.0f;
	bool bSpokeToday = false;
	// availability state (Stage 5B); the director is only read (status, phase) and its OnDayStarted is listened to
	UPROPERTY(Transient) TObjectPtr<AP9CampaignDirector> Director;
	FTimerHandle AvailabilityTimer;
	bool bAvailableNow = true;
	// clips
	EP9NPCClip CurrentClip = EP9NPCClip::None;
	EP9NPCClip PendingClip = EP9NPCClip::None;
	float ClipClock = 0.0f;      // own clip clock: clip changes do not depend on the pose being ticked (off-screen NPCs)
	float RateCheck = 0.0f;
	UPROPERTY(Transient) TObjectPtr<UP9NPCAnimInstance> AnimInst;
	// movement (Stage 5C)
	float CurSpeed = 0.0f;       // eased walking speed (cm/s)
	bool bExplicitZ = false;     // follow the path points' Z instead of tracing the floor (inside the train)
	float SpeedScale = 1.0f;     // > 1 when hurrying (getting off the train)
	bool bDrunkLeg = false;      // Stage 5G
	bool bLoggedPassThrough = false;   // Stage 8A
	bool bClimbing = false;      // Stage 5G: the current path segment rises (stairs up)
	float ClimbSlope = 0.0f;
	int32 StairDir = 0;          // Stage 7B: +1 on stairs going up, -1 going down, 0 flat (from the floor probe)
	float StairRampZ = 0.0f;     // Stage 7B: feet height on the stair ramp (phase source)
	float StairOffTime = 0.0f;   // Stage 7B: time since the last nosing under the body (short landing gaps keep the clip)
	float BlockedTime = 0.0f;    // Stage 7B: time held up by another body
	int32 AvoidSide = 0;         // Stage 7B: side chosen while passing a body (kept until clear)
	float EdgeHoldTime = 0.0f;   // Stage 7B: time a step was refused because it would leave the floor
	bool bHoldingForBody = false;              // Stage 7B: waiting (idle) behind another body
	FVector SteerSmooth = FVector::ZeroVector; // Stage 7B: eased avoidance steering (no twitching)
	float PacePhase = 0.0f;                    // Stage 7B: per-person slow pace variation
	float NextDrunkLine = 0.0f;
	int32 LastDrunkLine = INDEX_NONE;
	// Stage 9A transitions / recovery
	float TurnRate = 0.0f;                     // current angular speed of an on-the-spot turn (deg/s, signed)
	float LastTurnCall = -1.0f;                // WorldTime of the last TurnTowardsYaw (a new turn starts from rest)
	float LastClipRate = 1.0f;                 // rate of the moving clip last tick (the outgoing stride settles from it)
	int32 PivotPhase = 0;                      // 0 none, 1 slowing along the facing, 2 turning on the spot
	FVector LastGoodFeet = FVector::ZeroVector;
	bool bHaveGoodFeet = false;
	bool bStrictPath = false;                  // no corner cutting until the next path point (recovery legs)
	bool bEscaping = false;                    // backing out of furniture: the floor guard is lifted for this one leg
	float WatchTime = 0.0f;
	float WatchBestDist = 0.0f;
	FVector WatchTarget = FVector::ZeroVector;
	int32 Recoveries = 0;
	// epilogue wake-up (Stage 5C)
	TWeakObjectPtr<AActor> WakeProxy;
	int32 ScriptPhase = 0;       // 1 mirroring the proxy, 2 walking back to the stall, 3 turning to the counter
	FVector LastProxyLoc = FVector::ZeroVector;
	float ProxySpeed = 0.0f;
	// epilogue train (Stage 5C)
	TWeakObjectPtr<AP9Train> RideTrain;
	TWeakObjectPtr<UP9SeatComponent> RideSeat;
	int32 TrainPhase = 0;        // 1 walk in, 2 turn + sit down, 3 seated, 4 stand up, 5 walk out
	float TrainTime = 0.0f;
	float TrainCheck = 0.0f;
	float SeatYaw = 0.0f;
	TArray<FVector> TrainExit;   // world points from the seat back out to the platform
	bool bRodeThisEpilogue = false;
	bool bTrappedLogged = false;
	static TArray<TWeakObjectPtr<UP9SeatComponent>> ClaimedSeats;
	static constexpr int32 MaxEpilogueRiders = 3;
	static TMap<FName, TWeakObjectPtr<AP9FriendNPC>> Speakers;
};
