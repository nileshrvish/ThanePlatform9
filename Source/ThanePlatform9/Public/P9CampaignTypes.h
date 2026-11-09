#pragma once

#include "CoreMinimal.h"
#include "P9CampaignTypes.generated.h"

class AActor;
class UAudioComponent;
class UCanvasRenderTarget2D;
class ULightComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UMeshComponent;
class USoundBase;
class UStaticMesh;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EP9AnomalyCategory : uint8
{
	Signage,
	Environment,
	Train,
	Audio,
	Supernatural,
	Other
};

/** What a spawn point offers; an anomaly definition lists the location types it can use. */
UENUM(BlueprintType)
enum class EP9AnomalyLocation : uint8
{
	PlatformSign,
	FOBSign,
	Indicator,
	Clock,
	Lamp,
	Bench,
	BenchSlot,
	TrainDestination,
	TrainSeats,
	TrainLights,
	TrainWindow,
	PlatformEnd,
	Footbridge,
	PASystem,
	PlayerFollow,
	HornSource,
	FOBSignPF9,        // Stage 4B: the footbridge sign for platforms 9/10
	Poster,            // Stage 4D: platform/footbridge advertisement stands
	WallRelief,        // Stage 4D: wall faces where something can press through
	FloorShadow,       // Stage 4D: floor spots under a lamp
	DistantView,       // Stage 4D: far sight-lines (seen from the zone, never on Platforms 1-8)
	TicketCounter,     // Stage 6A: the concourse booking office front
	ClosedEntry,       // Stage 6A: a closed stair head on the footbridge (gate / scaffold / barricade)
	Track,             // Stage 6A: a track panel seen from Platform 9/10
	FOBLoop,           // Stage 6A: the footbridge stretch that loops (endless footbridge)
	StairLoop,         // Stage 6A: the Platform 9/10 stair flight that loops (endless stair)
	StalkerSpot,       // Stage 6A: a place where Kanchana waits on her day
	AnalogClock,       // Stage 6C: the footbridge concourse's analog clock (AP9AnalogClock)
	PlatformLamp,      // Stage 8A: a Platform 9/10 lamp (spot light target, tube mesh secondary, fill light extra)
	LanternSpot        // Stage 8A: a dark Platform 9 spot where the Lantern Man stands (target = the lamp beside it that flickers)
};

/** The behaviour that realises an anomaly; each has one implementation in AP9CampaignDirector. */
UENUM(BlueprintType)
enum class EP9AnomalyHandler : uint8
{
	MaterialSwap,      // the target mesh slot matching MaterialSlotMatch shows the variant material
	HideActor,         // the target and its ExtraActors are gone (hidden, no collision); Param >= 1 also hides SecondaryActor
	PAAnnouncement,    // the Platform 9 arrival announcement is replaced by the variant (played when the Vashi local is called in)
	DynamicClock,      // Stage 6A: the clock face is redrawn at runtime (Text = start time, Param 0 fixed / 1 backwards / 2 racing)
	MeshSwap,          // Stage 6A: the target is hidden and the variant mesh stands in its place with its materials
	LoopTeleport,      // Stage 6A: the point's LoopTrigger sends the player back by LoopOffset (endless footbridge / stair)
	PosterEyes,        // Stage 6A: the poster changes to the variant material and its eyes follow the player
	Stalker,           // Stage 6A: Kanchana waits at the point; once seen she follows the player and speaks
	AnalogClock,       // Stage 6C: the target AP9AnalogClock runs backwards (Param = reverse rate x, Scale.X pause s, Scale.Y slow s, Scale.Z 1 = waits until seen)
	LightTint,         // Stage 8A: the target lamp's lights take the variant Color (Param = intensity x, Scale.X pulse depth, Scale.Y pulse period s); tube glows the colour
	TrainMaterial,     // Stage 8A: every mesh slot of the target train matching MaterialSlotMatch shows the variant material (both cars)
	TrainLightsOff,    // Stage 8A: the point's car (TargetComponent "Car0"/"Car1") goes dark (Param = residual light 0..1; Scale.Z 1 = dies when the player comes near)
	LanternFigure,     // Stage 8A: the Lantern Man stands motionless at the point; the target lamp beside him flickers
	EndlessBridge,     // Stage 8A: the footbridge's north end recedes as the player walks on (AP9EndlessFootbridge) - no teleport
	ReverseEscalator   // Stage 8A: the Platform 9/10 lower flight runs upwards like an escalator against the player (AP9ReverseEscalator)
};

UENUM(BlueprintType)
enum class EP9CampaignStatus : uint8
{
	InProgress,
	Completed
};

UENUM(BlueprintType)
enum class EP9DayPhase : uint8
{
	Inactive,
	Exploring,
	Resolving,
	Ending,
	Opening,           // Stage 4B: the Dadar -> Thane arrival before Day 1 (no campaign state)
	Epilogue           // Stage 4B: after Day 10 - the chaiwala wakes the player, the journey to Platform 9 and the last ride
};

/** Stage 4B story facts that signs, indicators, trains and announcements must agree on (checked by p9.Story.Validate). */
USTRUCT(BlueprintType)
struct FP9StoryConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story") FString OriginStation = TEXT("Dadar");
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story") FString ArrivalStation = TEXT("Thane");
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story") int32 ArrivalPlatform = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story") int32 ConnectingPlatform = 9;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story") FString ConnectingDestination = TEXT("Vashi");
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story") FString IntendedStop = TEXT("Airoli");
	/** Destination boards (AP9Train::DestinationText) - must contain these words. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story") FString OpeningTrainBoard = TEXT("THANE");
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story") FString ConnectingTrainBoard = TEXT("VASHI");
	/** Indicator material instances and the texture each must show (real names). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story") TMap<FString, FString> IndicatorTextures;
};

UENUM(BlueprintType)
enum class EP9Decision : uint8
{
	ReturnToPlatform1,
	BoardTrain
};

USTRUCT(BlueprintType)
struct FP9AnomalyVariant
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Anomaly") FName Id;
	/** 1 = subtle, 2 = noticeable, 3 = obvious. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Anomaly", meta = (ClampMin = "1", ClampMax = "3")) int32 Difficulty = 2;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Anomaly", meta = (ClampMin = "0.0")) float Weight = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Anomaly") TSoftObjectPtr<UMaterialInterface> Material;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Anomaly") TSoftObjectPtr<USoundBase> Sound;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Anomaly") FString Text;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Anomaly") FLinearColor Color = FLinearColor::White;
	/** Handler-specific value (light mode, pitch, yaw offset...). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Anomaly") float Param = 0.0f;
	/** Stage 4D: mesh for figures / props (empty = handler default) and its scale. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Anomaly") TSoftObjectPtr<UStaticMesh> Mesh;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Anomaly") FVector Scale = FVector::OneVector;
	/** Stage 4D: MaterialSwap / PosterEyes - the target looks normal until the player has seen it and looked away. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Anomaly") bool bChangeAfterLookAway = false;
	/** Stage 6A: only usable at spawn points whose PointId contains this text (empty = any point of the definition). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Anomaly") FString PointMatch;
};

USTRUCT(BlueprintType)
struct FP9AnomalyDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Anomaly") FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Anomaly") FString DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Anomaly") bool bEnabled = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Anomaly") EP9AnomalyCategory Category = EP9AnomalyCategory::Environment;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Anomaly") EP9AnomalyHandler Handler = EP9AnomalyHandler::MaterialSwap;
	/** Relative selection weight (rarity). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Anomaly", meta = (ClampMin = "0.0")) float Weight = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Anomaly") TArray<EP9AnomalyLocation> LocationTypes;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Anomaly") TArray<FName> Tags;
	/** Never on the same day as an anomaly carrying one of these tags. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Anomaly") TArray<FName> ExcludeTags;
	/** MaterialSwap: substring of the current material name that identifies the slot to replace. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Anomaly") FString MaterialSlotMatch;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Anomaly") TArray<FP9AnomalyVariant> Variants;
	/**
	 * Stage 6A: never picked at random. It is the day's only anomaly on exactly one day of each run, chosen from
	 * ScheduleFirstDay..ScheduleLastDay by the run seed (Kanchana: one of days 3-5).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Anomaly|Schedule") bool bScheduled = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Anomaly|Schedule", meta = (ClampMin = "1")) int32 ScheduleFirstDay = 3;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Anomaly|Schedule", meta = (ClampMin = "1")) int32 ScheduleLastDay = 5;
};

/** One anomaly chosen for a day. */
USTRUCT(BlueprintType)
struct FP9PlannedAnomaly
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Anomaly") int32 DefIndex = INDEX_NONE;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Anomaly") int32 VariantIndex = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Anomaly") int32 PointIndex = INDEX_NONE;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Anomaly") FName DefId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Anomaly") FName VariantId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Anomaly") FName PointId;
};

/** A generated day: a pure function of (run seed, day, attempt, history of the run's previous days). */
USTRUCT(BlueprintType)
struct FP9DayPlan
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Campaign") int32 Day = 1;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Campaign") int32 Attempt = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Campaign") int32 Seed = 0;
	/** 1..3: biases variant choice for the day (not tied to the day number). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Campaign") int32 Difficulty = 2;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Campaign") bool bAnomalous = false;
	/** An anomalous day whose anomalies all failed to activate after every retry, played as a normal day. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Campaign") bool bFallbackNormal = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Campaign") TArray<FP9PlannedAnomaly> Anomalies;

	FString ToString() const
	{
		FString S = FString::Printf(TEXT("day=%d attempt=%d seed=%d difficulty=%d anomalous=%s fallback=%s count=%d"),
			Day, Attempt, Seed, Difficulty, bAnomalous ? TEXT("yes") : TEXT("no"), bFallbackNormal ? TEXT("yes") : TEXT("no"), Anomalies.Num());
		for (const FP9PlannedAnomaly& A : Anomalies)
		{
			S += FString::Printf(TEXT(" [%s:%s@%s]"), *A.DefId.ToString(), *A.VariantId.ToString(), *A.PointId.ToString());
		}
		return S;
	}
};

/** Something an anomaly changed, kept so cleanup can put it back exactly. */
USTRUCT()
struct FP9BaselineRecord
{
	GENERATED_BODY()

	UPROPERTY() TObjectPtr<UMeshComponent> Mesh = nullptr;
	UPROPERTY() TArray<TObjectPtr<UMaterialInterface>> Materials;
	/** The epilogue's failing carriage lights. */
	UPROPERTY() TObjectPtr<ULightComponent> Light = nullptr;
	float Intensity = 0.0f;
	FColor LightColor = FColor::White;
	bool bLightVisible = true;
	UPROPERTY() TObjectPtr<AActor> Actor = nullptr;
	bool bHidden = false;
	bool bCollision = true;
	UPROPERTY() TObjectPtr<AActor> Spawned = nullptr;
};

/** Runtime state of one activated anomaly. */
USTRUCT()
struct FP9ActiveAnomaly
{
	GENERATED_BODY()

	UPROPERTY() FP9PlannedAnomaly Plan;
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> MIDs;
	UPROPERTY() TObjectPtr<UCanvasRenderTarget2D> Target = nullptr;
	/** Stage 6A: actors spawned for the anomaly (poster pupils, the stand-in track) - destroyed with the baseline. */
	UPROPERTY() TArray<TObjectPtr<AActor>> Extras;
	/** Stage 4D: a material swap waiting for the player to look away. */
	UPROPERTY() TObjectPtr<UMeshComponent> PendingMesh = nullptr;
	UPROPERTY() TObjectPtr<UMaterialInterface> PendingMaterial = nullptr;
	int32 PendingSlot = INDEX_NONE;
	float Time = 0.0f;
	float NextEvent = 0.0f;
	float SeenTime = 0.0f;
	float AwayTime = 0.0f;
	bool bSeen = false;
	/** Stage 6A: dynamic clock minutes since midnight and time since the last step; loop count and re-arm time. */
	int32 ClockMinutes = 0;
	float ClockAccum = 0.0f;
	int32 Loops = 0;
	float LoopCooldown = 0.0f;
	/** Stage 6A: poster pupils' current offsets (smoothed). */
	FVector2D EyeOffset = FVector2D::ZeroVector;
	/** Stage 8A: the lights the anomaly drives and their original intensities (light tint pulse, coach lights, lamp flicker). */
	UPROPERTY() TArray<TObjectPtr<ULightComponent>> Lights;
	TArray<float> LightBase;
	/** Stage 8A: 0 waiting, 1 flickering, 2 settled (coach lights that die on approach). */
	int32 Stage = 0;
	float StageTime = 0.0f;
	/** Stage 8A: the player has seen / met this anomaly (drives the "after it's seen" anomaly ambience only - never a cue before). */
	bool bNoticed = false;
	float NoticeTime = 0.0f;
};
