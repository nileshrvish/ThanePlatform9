#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "P9CampaignTypes.h"
#include "P9CampaignDirector.generated.h"

class AP9AnomalySpawnPoint;
class AP9Train;
class AP9StationAnnouncer;
class ACharacter;
class APlayerController;
class UCanvas;
class USoundAttenuation;
class UStaticMesh;
class SWidget;
class UPointLightComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FP9CampaignDaySignature, int32, Day);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FP9CampaignDecisionSignature, int32, Day, EP9Decision, Decision, bool, bCorrect);

/**
 * Stage 4A: the ten-day anomaly campaign (Exit 8-style).
 *
 * Each day the director generates a plan from (RunSeed, Day, Attempt, History): normal or anomalous, then for an anomalous day
 * a weighted pick of 1..3 anomalies from the catalogue (Anomalies), each placed at a free compatible spawn point with one
 * controller per target, tag exclusions and a repetition penalty for anomalies seen on the run's recent days. A failed
 * activation is not counted; if an anomalous day ends up with no working anomaly the selection is retried (bounded) and
 * finally falls back to a normal day.
 *
 * The route is the answer: reaching Platform 1 (decision zone) means "something was wrong" and is correct only when at least
 * one anomaly is active; riding the local train when it departs means "nothing was wrong" and is correct only on a normal day.
 * Correct -> next day; wrong -> a new run from Day 1 with a new seed; Day 10 correct -> the ending (never auto-resets).
 * Every change an anomaly makes is recorded (FP9BaselineRecord) and restored when the day ends.
 *
 * Stage 4B story layer (separate phases from the campaign day state):
 *  - Opening: the player rides the Dadar -> Thane local (OpeningTrain) into Thane Platform 1, hears the Marathi arrival
 *    announcement and gets off. Day 1 begins only when the player climbs onto the footbridge (CampaignStartZone).
 *  - The connecting Vashi local (Train) waits out of sight at its hold stop and is called in when the player approaches
 *    Platform 9; boarding it and riding it out is the "normal day" decision. Arrival/departure announcements are tied to it.
 *  - Randomised anomalies only use spawn points inside AnomalyZones (footbridge route + Platform 9) and never inside
 *    NormalZones (Platforms 1-8); Platform 1 stays the decision destination.
 *  - Epilogue: after Day 10 the chaiwala wakes the player on the Dadar train at Platform 1; the player walks to Platform 9,
 *    boards the Vashi local and a short scripted sequence ends on the final screen. The completed campaign is never reset.
 */
UCLASS()
class THANEPLATFORM9_API AP9CampaignDirector : public AActor
{
	GENERATED_BODY()

public:
	AP9CampaignDirector();

	// ---------------- configuration ----------------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Campaign") int32 NumDays = 10;
	/** Non-zero: every run is reproducible (the first run uses this seed, resets derive theirs from it) and the save is not loaded. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Campaign") int32 DebugSeed = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Campaign") bool bUseSaveGame = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Campaign") FString SaveSlotName = TEXT("P9Campaign");
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Campaign", meta = (ClampMin = "0.0", ClampMax = "1.0")) float AnomalyDayChance = 0.6f;
	/** Relative weights of 1, 2, 3 ... anomalies on an anomalous day. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Campaign") TArray<float> AnomalyCountWeights = {0.62f, 0.28f, 0.10f};
	/** Relative weights of day difficulty 1 (subtle), 2, 3 (obvious). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Campaign") TArray<float> DifficultyWeights = {0.30f, 0.35f, 0.23f, 0.12f};   // Stage 4D tiers: subtle, moderate, hard, extreme (rolled per (seed, day), never fixed to a day number)
	/** Weight multiplier for an anomaly seen 1, 2, 3 ... days ago in this run. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Campaign") TArray<float> RepetitionPenalty = {0.2f, 0.5f, 0.8f};
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Campaign", meta = (ClampMin = "0")) int32 MaxGenerationRetries = 4;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Campaign") TArray<FP9AnomalyDef> Anomalies;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Campaign|World") TArray<TObjectPtr<AP9AnomalySpawnPoint>> SpawnPoints;
	/** The connecting Vashi local at Platform 9: boarding it as it departs is the normal-day decision. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Campaign|World") TObjectPtr<AP9Train> Train;
	/** Its stop indices: the Platform 9 stop and the hold stop (out of sight) where it waits to be called in. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Campaign|World") int32 ConnectingPlatformStop = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Campaign|World") int32 ConnectingHoldStop = 0;
	/** Seconds the player must stay aboard (doors open) before the Vashi local is sent on its way. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Campaign|World") float BoardedDepartDelay = 5.0f;
	/** During the campaign the Vashi local leaves without the player after this long at the platform (0 = never). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Campaign|World") float ConnectingMaxDwell = 120.0f;
	/** Out of sight (route distance past this, unoccupied) it runs faster and hidden back to the hold stop. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Campaign|World") float ConnectingFastFromDistance = 26000.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Campaign|World") float ConnectingShowBeforeHold = 9000.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Campaign|World") float ConnectingFastSpeed = 4000.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Campaign|World") float ConnectingFastAccel = 600.0f;
	/** Stage 4C: the Vashi local is hidden at its hold stop; once called in it appears from this route distance (start of track 9). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Campaign|World") float ConnectingVisibleFrom = 0.0f;
	/** The Dadar -> Thane local of the opening (and the epilogue wake-up); stops: approach, Platform 1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Campaign|World") TObjectPtr<AP9Train> OpeningTrain;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Campaign|World") int32 OpeningArrivalStop = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Campaign|World") TObjectPtr<AP9StationAnnouncer> Announcer;
	/** Play the Dadar -> Thane opening when there is no save to resume. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Campaign|World") bool bPlayOpening = true;
	/** Randomised anomalies may only use points inside one of these (footbridge route, Platform 9). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Campaign|Zones") TArray<FBox> AnomalyZones;
	/** ...and never points inside these (Platforms 1-8). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Campaign|Zones") TArray<FBox> NormalZones;
	/** Day 1 begins when the player's feet enter this (the footbridge) after the opening. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Campaign|Zones") FBox CampaignStartZone = FBox(FVector(-750.0f, -1760.0f, 540.0f), FVector(750.0f, 7960.0f, 1300.0f));
	/** The Vashi local is called in when the player is here (north footbridge, Platform 9 stairs and platform). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Campaign|Zones") FBox Platform9ApproachZone = FBox(FVector(-3200.0f, 6100.0f, -160.0f), FVector(3200.0f, 7960.0f, 1300.0f));
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Campaign|Story") FP9StoryConfig Story;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Campaign|Story") TSoftObjectPtr<UStaticMesh> ChaiwalaMesh;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Campaign|Story") TSoftObjectPtr<USoundBase> ChaiwalaWakeLine;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Campaign|Story") TSoftObjectPtr<USoundBase> ChaiwalaCallLine;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Campaign|Story") TSoftObjectPtr<USoundBase> TrainRunAmbience;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Campaign|World") FVector StartLocation = FVector(0.0f, -300.0f, 700.0f);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Campaign|World") float StartYaw = 90.0f;
	/** Platform 1 decision zone, tested against the player's feet. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Campaign|World") FBox Platform1Zone = FBox(FVector(-2950.0f, -1690.0f, -15.0f), FVector(2950.0f, -900.0f, 60.0f));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Campaign|Assets") TSoftObjectPtr<UStaticMesh> FigureMesh;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Campaign|Assets") TSoftObjectPtr<USoundBase> ReflectionSting;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Campaign|Assets") TSoftObjectPtr<USoundBase> ApparitionSting;
	/** Hard references saved with the level so everything the campaign (and the train's soft defaults) loads gets cooked. */
	UPROPERTY(EditAnywhere, Category = "Campaign|Assets") TArray<TObjectPtr<UObject>> PreloadAssets;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Campaign|Jumpscare", meta = (ClampMin = "0.0")) float JumpscareCooldown = 45.0f;
	/** Stage 4D: volume of the scare stings, drones and rushes (accessibility; 0 mutes them). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign|Jumpscare", meta = (ClampMin = "0.0", ClampMax = "1.5")) float ScareVolume = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Campaign|Jumpscare") TSoftObjectPtr<USoundBase> StingClose;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Campaign|Jumpscare") TSoftObjectPtr<USoundBase> StingCarriage;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Campaign|Jumpscare") TSoftObjectPtr<USoundBase> StingFootbridge;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Campaign|Jumpscare") TSoftObjectPtr<USoundBase> ScareDrone;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Campaign|Jumpscare") TSoftObjectPtr<USoundBase> ScareRush;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Campaign|Jumpscare") TSoftObjectPtr<UStaticMesh> HeroMesh;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Campaign|Jumpscare") TSoftObjectPtr<UStaticMesh> TallMesh;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Campaign|Jumpscare") TSoftObjectPtr<UStaticMesh> PassengerMesh;

	UPROPERTY(BlueprintAssignable, Category = "Campaign") FP9CampaignDaySignature OnDayStarted;
	UPROPERTY(BlueprintAssignable, Category = "Campaign") FP9CampaignDecisionSignature OnDecision;
	UPROPERTY(BlueprintAssignable, Category = "Campaign") FP9CampaignDaySignature OnCampaignCompleted;

	// ---------------- API ----------------
	/** Pure: the plan for (seed, day, attempt) given the run's history. Same inputs -> same plan. */
	FP9DayPlan GenerateDayPlan(int32 InRunSeed, int32 InDay, int32 Attempt, const TArray<FString>& InHistory) const;
	/** New run from Day 1 (on the footbridge; the opening is not replayed). Seed 0 = derive (DebugSeed) or random. */
	void StartNewRun(int32 Seed = 0);
	/** Stage 4B debug/test entry points. */
	void DebugStartOpening();
	void DebugStartEpilogue();
	/** Point allowed for randomised anomalies? (zone rules; Why = reason when not) */
	bool IsPointAllowed(const AP9AnomalySpawnPoint* Point, FString* Why = nullptr) const;
	bool IsInAnomalyZone(const FVector& Location) const;
	/** Story consistency checks (trains, boards, announcer, zones); returns the number of problems (logged). */
	int32 ValidateStory() const;
	FString GetStoryStatus() const;
	/** Re-generates and restarts the current day (debug). */
	void RestartDay();
	/** Debug: the next day start uses this plan instead of generating one. */
	void SetForcedPlan(const FP9DayPlan& Plan) { ForcedPlan = Plan; bHasForcedPlan = true; }
	void SetDay(int32 NewDay);
	/** Routes a decision through the same validation as gameplay (debug). */
	void SubmitDecision(EP9Decision Decision) { ResolveDecision(Decision, DayToken); }
	/** Restores every anomaly change of the current day without starting a new one (debug / tests). */
	void CleanupDay();
	/** Compares the targets with the snapshot taken at BeginPlay; returns the number of differences (logged). */
	int32 VerifyBaseline(bool bLog = true) const;
	bool SaveProgress() const;
	bool LoadProgress();
	bool DeleteSave() const;
	/** Debug: fires a jumpscare now (ignores cooldown). */
	void DebugJumpscare(EP9JumpscareType Type);
	FString GetStatusString() const;
	int32 FindAnomalyDef(FName Id) const;
	int32 FindSpawnPoint(FName Id) const;

	UFUNCTION(BlueprintPure, Category = "Campaign") int32 GetDay() const { return Day; }
	UFUNCTION(BlueprintPure, Category = "Campaign") int32 GetRunSeed() const { return RunSeed; }
	UFUNCTION(BlueprintPure, Category = "Campaign") int32 GetRunId() const { return RunId; }
	UFUNCTION(BlueprintPure, Category = "Campaign") EP9DayPhase GetPhase() const { return Phase; }
	UFUNCTION(BlueprintPure, Category = "Campaign") EP9CampaignStatus GetStatus() const { return Status; }
	UFUNCTION(BlueprintPure, Category = "Campaign") int32 GetNumActiveAnomalies() const { return ActiveAnomalies.Num(); }
	const FP9DayPlan& GetCurrentPlan() const { return CurrentPlan; }
	const TArray<FString>& GetHistory() const { return History; }

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void BuildDefaultCatalogue();
	void StartDay(bool bTeleport = true);
	void StartOpening();
	void TickOpening(float DeltaSeconds);
	void StartEpilogue();
	void TickEpilogue(float DeltaSeconds);
	void TickConnectingTrain(float DeltaSeconds);
	void SetConnectingFast(bool bFast);
	bool SitPlayerInTrain(AP9Train* InTrain, bool bPlatformSide);
	void ShowHint(const FText& Text, float Duration);
	void SetOpeningTrainShown(bool bShown);
	void HideConnectingAtHold();
	/** Stage 5B: at a new day the Vashi local starts hidden and idle at its hold stop, with no call-in state left over. */
	void ResetConnectingTrainForNewDay(bool bPlayerMoved);
	void PlaceChaiwala(const TArray<FVector>& Path, float Dist, float Yaw, float Pitch);
	bool ActivateAnomaly(const FP9PlannedAnomaly& Plan);
	bool ActivateMaterialSwap(FP9ActiveAnomaly& A, const FP9AnomalyDef& Def, const FP9AnomalyVariant& V, AP9AnomalySpawnPoint* Point);
	bool ActivateLampFault(FP9ActiveAnomaly& A, const FP9AnomalyVariant& V, AP9AnomalySpawnPoint* Point);
	bool ActivateHideActor(AP9AnomalySpawnPoint* Point);
	bool ActivateExtraProp(const FP9AnomalyVariant& V, AP9AnomalySpawnPoint* Point);
	bool ActivateTrainDestination(FP9ActiveAnomaly& A, const FP9AnomalyVariant& V);
	bool ActivateTrainSeatTint(FP9ActiveAnomaly& A, const FP9AnomalyVariant& V);
	bool ActivateTrainLights(FP9ActiveAnomaly& A, const FP9AnomalyVariant& V, AP9AnomalySpawnPoint* Point);
	bool ActivateFigure(FP9ActiveAnomaly& A, const FP9AnomalyDef& Def, const FP9AnomalyVariant& V, AP9AnomalySpawnPoint* Point);
	void TickAnomaly(FP9ActiveAnomaly& A, float DeltaSeconds);
	void TickDecisionZone(float DeltaSeconds);
	void TickPhase(float DeltaSeconds);
	void TickPresentation(float DeltaSeconds);
	void ResolveDecision(EP9Decision Decision, int32 Token);
	void AfterResolution();
	void EnterEnding();
	void SaveEpilogueStage(int32 Stage);

	void RecordMesh(UMeshComponent* Mesh);
	void RecordLight(ULightComponent* Light);
	void RecordActor(AActor* Actor);
	void RecordSpawned(AActor* Actor);
	void RestoreBaseline();
	void TakeInitialSnapshot();
	TMap<FString, FString> CaptureSnapshot() const;

	AActor* SpawnFigure(const FVector& Location, float Yaw, UStaticMesh* Mesh = nullptr, const FVector& Scale = FVector::OneVector, UMaterialInterface* OverrideAll = nullptr);
	bool ActivateStaticProp(FP9ActiveAnomaly& A, const FP9AnomalyVariant& V, AP9AnomalySpawnPoint* Point);
	bool ScareAvailable() const { return WorldTimeNow - LastJumpscareTime >= JumpscareCooldown; }
	void ScareLightAt(const FVector& Where, float Intensity, float Duration);
	void PlayScare2D(const TSoftObjectPtr<USoundBase>& Sound, float Volume);
	FVector ScareSpotInFront(float Distance, float& OutYaw) const;
	void TriggerJumpscare(EP9JumpscareType Type, FP9ActiveAnomaly* Source);
	bool IsInView(const FVector& Location, float MinDot, float MaxDistance, const AActor* IgnoreActor, bool bTrace) const;
	void PlaySoundAt(USoundBase* Sound, const FVector& Location, float Volume, float Pitch, float Falloff);
	void PlaySound2D(USoundBase* Sound, float Volume, float Pitch);
	void StopCampaignSounds();
	ACharacter* PlayerCharacter() const;
	APlayerController* PlayerController() const;
	void PlacePlayerAtStart();
	FVector PlayerFeet() const;
	int32 NewRandomSeed() const;
	FString TodayHistoryEntry() const;

	UFUNCTION() void HandleTrainDeparted(int32 StopIndex);
	UFUNCTION() void HandleTrainArrived(int32 StopIndex);
	UFUNCTION() void HandleOpeningArrived(int32 StopIndex);
	UFUNCTION() void DrawDestination(UCanvas* Canvas, int32 Width, int32 Height);

	// HUD (Slate, above the mobile controls; hit-test invisible except the ending button)
	void EnsureHud();
	void RemoveHud();
	void ShowTitle(const FText& Title, const FText& Subtitle, float Duration);
	void SetEndingInput(bool bEnable);

	UPROPERTY(Transient) TArray<FP9ActiveAnomaly> ActiveAnomalies;
	UPROPERTY(Transient) TArray<FP9BaselineRecord> Baseline;
	UPROPERTY(Transient) TArray<TObjectPtr<UAudioComponent>> CampaignSounds;
	UPROPERTY(Transient) TObjectPtr<USoundAttenuation> SpatialAttenuation;
	UPROPERTY(Transient) TObjectPtr<AActor> ScareFigure;
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> ScareLight;
	float ScareLightTime = 0.0f;
	UPROPERTY(Transient) TObjectPtr<AActor> Chaiwala;
	UPROPERTY(Transient) TObjectPtr<AActor> EpilogueFigure;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> AmbienceAudio;
	UPROPERTY(Transient) TArray<TObjectPtr<ULightComponent>> EpilogueLights;

	FP9DayPlan CurrentPlan;
	FP9DayPlan ForcedPlan;
	bool bHasForcedPlan = false;
	TArray<FString> History;
	TMap<FString, FString> InitialSnapshot;
	FRandomStream PresentationStream;
	FString DestinationTextNow;

	int32 Day = 1;
	int32 RunSeed = 0;
	int32 RunId = 1;
	int32 DayToken = 0;
	EP9CampaignStatus Status = EP9CampaignStatus::InProgress;
	EP9DayPhase Phase = EP9DayPhase::Inactive;
	bool bDecisionLocked = false;
	bool bLastCorrect = false;
	EP9Decision LastDecision = EP9Decision::ReturnToPlatform1;
	bool bZoneArmed = false;
	float ZonePollTime = 0.0f;
	float DayTime = 0.0f;
	float PhaseTime = 0.0f;
	// Stage 4B story state
	bool bOpeningDone = true;
	int32 EpilogueStage = 0;
	int32 EpilogueStep = 0;
	float StoryTime = 0.0f;
	float RideTime = -1.0f;
	int32 StoryCue = 0;
	int32 ConnectingCalls = 0;
	bool bConnectingCalled = false;
	bool bConnectingFast = false;
	float ConnectingOpenAt = -1.0f;
	float ConnectingDwell = 0.0f;
	float ConnectingAboardTime = 0.0f;
	bool bConnectingDepartRequested = false;
	float SavedMaxSpeed = 0.0f;
	float SavedAccel = 0.0f;
	float SavedDecel = 0.0f;
	float OpeningDoorsAt = -1.0f;
	bool bOpeningDoorsClosed = false;
	// chaiwala walk (train-local cm): approach start, standing spot, door inside, platform outside
	TArray<FVector> ChaiwalaIn;
	TArray<FVector> ChaiwalaOut;
	float ChaiwalaFaceYaw = 0.0f;
	// final scare: authored spot in the player's carriage (car-local), camera turn towards it in the dark
	int32 ScareCar = 0;
	FVector ScareSpotLocal = FVector::ZeroVector;
	FRotator ScareCamFrom = FRotator::ZeroRotator;
	UPROPERTY(Transient) TObjectPtr<AActor> EpilogueScare;
	FText HintText;
	float HintTime = 0.0f;
	float HintDuration = 0.0f;
	FText DayLabelOverride;
	bool bFinalEnding = false;
	float LastJumpscareTime = -1000.0f;
	float WorldTimeNow = 0.0f;

	// presentation state read by the HUD
	float FadeAlpha = 1.0f;
	float FadeTarget = 0.0f;
	float FadeSpeed = 1.0f;
	float FlashAlpha = 0.0f;
	float TitleTime = 0.0f;
	float TitleDuration = 0.0f;
	FText TitleText;
	FText SubtitleText;
	float ShakeTime = 0.0f;
	FRotator ShakeOffset = FRotator::ZeroRotator;
	float ScareFigureTime = 0.0f;
	bool bEndingInput = false;
	TSharedPtr<SWidget> HudWidget;
};
