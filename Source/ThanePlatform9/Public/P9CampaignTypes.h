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
	DistantView        // Stage 4D: far sight-lines (seen from the zone, never on Platforms 1-8)
};

/** The behaviour that realises an anomaly; each has one implementation in AP9CampaignDirector. */
UENUM(BlueprintType)
enum class EP9AnomalyHandler : uint8
{
	MaterialSwap,      // the target mesh slot matching MaterialSlotMatch shows the variant material
	LampFault,         // platform lamp (light + tube): off / wrong colour / flicker
	HideActor,         // the target is gone (hidden, no collision)
	ExtraProp,         // a copy of the target's mesh appears at the point (overlap-validated)
	TrainDestination,  // the train's destination board shows the variant text
	TrainSeatTint,     // seat upholstery tinted
	TrainLights,       // interior lights of one car: colour / flicker / off
	PAAnnouncement,    // the station announcements are replaced by the variant
	FollowFootsteps,   // footsteps behind the player
	PhantomHorn,       // a train horn from where no train can be
	DistantFigure,     // a figure at a platform end, gone when approached
	WatchingFigure,    // a figure that is gone once the player looks away from it
	WindowFigure,      // a figure inside the train (Param 0 far end of the carriage, 1 in the doorway, 2 facing the window)
	StaticProp         // Stage 4D: a mesh appears at the point (wall silhouette, impossible shadow, a train that should not exist)
};

UENUM(BlueprintType)
enum class EP9JumpscareType : uint8
{
	None,
	Reflection,   // figure seen in the train window: looked at -> sting, flash, gone
	Apparition,   // A: the distant figure vanishes, silence, then it stands right in front of the player
	Carriage,     // B: a passenger at the far end of the carriage; the lights fail; it is much closer when they return
	Footbridge    // C: the approaching figure on the footbridge lunges once
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
	/** Stage 4D: MaterialSwap only - the target looks normal until the player has seen it and looked away. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Anomaly") bool bChangeAfterLookAway = false;
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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Anomaly|Jumpscare") EP9JumpscareType Jumpscare = EP9JumpscareType::None;
	/** Chance (decided when the day is generated) that this anomaly carries its jumpscare. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Anomaly|Jumpscare", meta = (ClampMin = "0.0", ClampMax = "1.0")) float JumpscareChance = 0.0f;
};

/** One anomaly chosen for a day. */
USTRUCT(BlueprintType)
struct FP9PlannedAnomaly
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Anomaly") int32 DefIndex = INDEX_NONE;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Anomaly") int32 VariantIndex = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Anomaly") int32 PointIndex = INDEX_NONE;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Anomaly") bool bJumpscare = false;
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
			S += FString::Printf(TEXT(" [%s:%s@%s%s]"), *A.DefId.ToString(), *A.VariantId.ToString(), *A.PointId.ToString(), A.bJumpscare ? TEXT(" scare") : TEXT(""));
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
	UPROPERTY() TObjectPtr<AActor> Figure = nullptr;
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> MIDs;
	UPROPERTY() TArray<TObjectPtr<ULightComponent>> Lights;
	UPROPERTY() TObjectPtr<UCanvasRenderTarget2D> Target = nullptr;
	/** Stage 4D: a material swap waiting for the player to look away. */
	UPROPERTY() TObjectPtr<UMeshComponent> PendingMesh = nullptr;
	UPROPERTY() TObjectPtr<UMaterialInterface> PendingMaterial = nullptr;
	int32 PendingSlot = INDEX_NONE;
	/** Stage 4D: staged jumpscare state and the approaching figure's steps / the figure's carriage. */
	int32 ScareStage = 0;
	float ScareTime = 0.0f;
	int32 Steps = 0;
	int32 CarIndex = 0;
	FVector ScareFrom = FVector::ZeroVector;
	FVector ScareTo = FVector::ZeroVector;
	TArray<float> BaseIntensity;
	float Time = 0.0f;
	float NextEvent = 0.0f;
	float SeenTime = 0.0f;
	float AwayTime = 0.0f;
	bool bSeen = false;
	bool bGone = false;
	bool bScareDone = false;
};
