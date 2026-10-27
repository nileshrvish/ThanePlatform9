#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "P9Train.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UPointLightComponent;
class UAudioComponent;
class USoundBase;
class UMaterialInterface;
class UTexture;
class UCanvas;
class UCanvasRenderTarget2D;
class UP9InteractableComponent;
class UP9SeatComponent;
class ACharacter;

UENUM(BlueprintType)
enum class EP9TrainState : uint8
{
	StoppedAtPlatform,
	DoorsOpening,
	DoorsOpen,
	DoorsClosing,
	ReadyToDepart,
	Accelerating,
	Cruising,
	Braking,
	Arriving
};

UENUM(BlueprintType)
enum class EP9DoorState : uint8
{
	Closed,
	Opening,
	Open,
	Warning,
	Closing
};

/** One piece of the train's route: a straight (Radius 0) or a circular arc. Arcs turn by AngleDegrees (negative = yaw decreasing). */
USTRUCT(BlueprintType)
struct FP9RouteSegment
{
	GENERATED_BODY()

	/** Straight length in cm (ignored for arcs). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Route") float Length = 0.0f;
	/** Arc radius in cm; 0 makes this a straight. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Route") float Radius = 0.0f;
	/** Arc turn in degrees; negative turns towards decreasing yaw. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Route") float AngleDegrees = 0.0f;
};

USTRUCT()
struct FP9DoorLeaf
{
	GENERATED_BODY()

	UPROPERTY() TObjectPtr<UStaticMeshComponent> Comp = nullptr;
	UPROPERTY() int32 Car = 0;
	UPROPERTY() int32 Door = 0;
	/** Car-local side of the body (+1 = local +Y). */
	UPROPERTY() int32 Side = 1;
	/** Slide direction along the car (-1 = towards local -X). */
	UPROPERTY() int32 Dir = 1;
	UPROPERTY() FVector ClosedLocation = FVector::ZeroVector;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FP9TrainStateSignature, EP9TrainState, OldState, EP9TrainState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FP9DoorStateSignature, EP9DoorState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FP9TrainStopSignature, int32, StopIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FP9TrainBlockedSignature, const FString&, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FP9TrainPlayerSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FP9TrainImpactSignature, ACharacter*, Character, FVector, Impulse);

/**
 * Two coupled Indian suburban EMU driving cars (Stage 3E.2/3E.3) that run round a closed route.
 *
 * Route: an analytic chain of straights and circular arcs (RouteSegments) starting at RouteStart/RouteStartYaw. The train
 * position is a distance along the route that only increases; each car is placed on the chord between its two bogie points,
 * so it sits inside curves like a real car, and every bogie (with its wheelsets) follows the route tangent.
 * Stops are distances along the route (StopDistances); each departure runs to the next stop, round the loop and back.
 *
 * Riding uses CharacterMovement's moving-base support (train ticks in TG_PrePhysics). While aboard, the player's capsule
 * ignores the invisible station blockers found along the route at BeginPlay, so the occupant is never scraped off by them;
 * they apply again when the player leaves the train. Seated players are attached to their seat.
 *
 * Impacts: a character outside the train that the moving train reaches is launched clear (travel direction, side and up,
 * bounded) once per cooldown; the train never stops or changes state because of it.
 */
UCLASS(Blueprintable)
class THANEPLATFORM9_API AP9Train : public AActor
{
	GENERATED_BODY()

public:
	AP9Train();

	// ---------------- assets ----------------
	UPROPERTY(EditAnywhere, Category = "Train|Assets") TSoftObjectPtr<UStaticMesh> ShellMesh;
	UPROPERTY(EditAnywhere, Category = "Train|Assets") TSoftObjectPtr<UStaticMesh> InteriorMesh;
	UPROPERTY(EditAnywhere, Category = "Train|Assets") TSoftObjectPtr<UStaticMesh> BogieFrameMesh;
	UPROPERTY(EditAnywhere, Category = "Train|Assets") TSoftObjectPtr<UStaticMesh> WheelsetMesh;
	UPROPERTY(EditAnywhere, Category = "Train|Assets") TSoftObjectPtr<UStaticMesh> DoorLeafMesh;
	UPROPERTY(EditAnywhere, Category = "Train|Assets") TArray<TSoftObjectPtr<UMaterialInterface>> NumberMaterials;
	UPROPERTY(EditAnywhere, Category = "Train|Assets") TSoftObjectPtr<UTexture> LightsAtlas;

	// ---------------- route ----------------
	/** World position of the route's first segment start (rail-top level, track centre). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Train|Route") FVector RouteStart = FVector(-6000.0f, -240.0f, -75.0f);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Train|Route") float RouteStartYaw = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Train|Route") TArray<FP9RouteSegment> RouteSegments;
	/** Distances (cm) along the route where the train-centre stops; doors open only there. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Train|Route") TArray<float> StopDistances;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Train|Route") int32 StartStopIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train|Motion", meta = (ClampMin = "10.0")) float MaxSpeed = 1100.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train|Motion", meta = (ClampMin = "5.0")) float Acceleration = 70.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train|Motion", meta = (ClampMin = "5.0")) float ServiceDeceleration = 70.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train|Motion", meta = (ClampMin = "1.0")) float ArrivalSpeed = 25.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train|Motion", meta = (ClampMin = "1.0")) float ArrivalDistance = 40.0f;

	// ---------------- doors & cycle ----------------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train|Doors", meta = (ClampMin = "0.2")) float DoorOpenTime = 2.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train|Doors", meta = (ClampMin = "0.2")) float DoorCloseTime = 3.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train|Doors", meta = (ClampMin = "0.0")) float DoorWarningTime = 1.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train|Doors", meta = (ClampMin = "1.0")) float DwellTime = 25.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train|Doors", meta = (ClampMin = "0.0")) float AutoOpenDelay = 2.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train|Doors", meta = (ClampMin = "0.5")) float ObstructionHoldTime = 2.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train|Doors", meta = (ClampMin = "0.0")) float DepartChimeTime = 1.8f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train|Doors") bool bAutoCycle = true;
	/** World-space side (+1 = +Y) of the platform at the stops; only that side's doors ever open. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Train|Doors") int32 PlatformSideWorldY = 1;

	// ---------------- impact ----------------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train|Impact") bool bEnableImpacts = true;
	/** Horizontal launch speed (cm/s) = ImpactBaseSpeed + train speed * ImpactSpeedScale, clamped to ImpactMaxSpeed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train|Impact", meta = (ClampMin = "0.0")) float ImpactBaseSpeed = 450.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train|Impact", meta = (ClampMin = "0.0")) float ImpactSpeedScale = 0.8f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train|Impact", meta = (ClampMin = "0.0")) float ImpactMaxSpeed = 1100.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train|Impact", meta = (ClampMin = "0.0")) float ImpactVerticalSpeed = 380.0f;
	/** Lateral share of the launch for a hit by the leading end (the rest follows the travel direction). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train|Impact", meta = (ClampMin = "0.0", ClampMax = "2.0")) float FrontHitSideBias = 0.7f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train|Impact", meta = (ClampMin = "0.05")) float ImpactCooldown = 1.0f;
	/** Movement input is ignored until the character lands, or for at most this long. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train|Impact", meta = (ClampMin = "0.1")) float ImpactRecoveryTimeout = 2.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train|Impact") bool bBlockMoveInputDuringKnockback = true;

	// ---------------- presentation ----------------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Train|Display") FString DestinationText = TEXT("PALSHET  S");
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Train|Lights") float InteriorLightIntensity = 90.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Train|Lights") float InteriorLightRadius = 700.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Train|Audio") TSoftObjectPtr<USoundBase> DoorOpenSound;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Train|Audio") TSoftObjectPtr<USoundBase> DoorCloseSound;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Train|Audio") TSoftObjectPtr<USoundBase> DoorWarningSound;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Train|Audio") TSoftObjectPtr<USoundBase> DepartChimeSound;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Train|Audio") TSoftObjectPtr<USoundBase> RunLoopSound;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Train|Audio") TSoftObjectPtr<USoundBase> BrakeSound;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Train|Audio") TSoftObjectPtr<USoundBase> HornSound;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train|Audio") bool bHornOnDeparture = true;

	// ---------------- seats ----------------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Train|Seats") bool bCreateSeats = true;

	// ---------------- events ----------------
	UPROPERTY(BlueprintAssignable, Category = "Train|Events") FP9TrainStateSignature OnTrainStateChanged;
	UPROPERTY(BlueprintAssignable, Category = "Train|Events") FP9DoorStateSignature OnDoorStateChanged;
	UPROPERTY(BlueprintAssignable, Category = "Train|Events") FP9TrainStopSignature OnArrived;
	UPROPERTY(BlueprintAssignable, Category = "Train|Events") FP9TrainStopSignature OnDeparted;
	UPROPERTY(BlueprintAssignable, Category = "Train|Events") FP9TrainBlockedSignature OnDepartureBlocked;
	UPROPERTY(BlueprintAssignable, Category = "Train|Events") FP9TrainPlayerSignature OnPlayerBoarded;
	UPROPERTY(BlueprintAssignable, Category = "Train|Events") FP9TrainPlayerSignature OnPlayerExited;
	UPROPERTY(BlueprintAssignable, Category = "Train|Events") FP9TrainImpactSignature OnCharacterHit;

	// ---------------- control API ----------------
	UFUNCTION(BlueprintCallable, Category = "Train") bool RequestOpenDoors();
	UFUNCTION(BlueprintCallable, Category = "Train") bool RequestCloseDoors();
	/** Close (if open) and depart to the next stop once the doors are closed and clear. */
	UFUNCTION(BlueprintCallable, Category = "Train") bool RequestDeparture();
	UFUNCTION(BlueprintCallable, Category = "Train") void SetDestinationText(const FString& NewText);

	UFUNCTION(BlueprintPure, Category = "Train") EP9TrainState GetTrainState() const { return TrainState; }
	UFUNCTION(BlueprintPure, Category = "Train") EP9DoorState GetDoorState() const { return DoorState; }
	UFUNCTION(BlueprintPure, Category = "Train") float GetSpeed() const { return Speed; }
	UFUNCTION(BlueprintPure, Category = "Train") float GetRouteDistance() const { return Distance; }
	UFUNCTION(BlueprintPure, Category = "Train") float GetRouteLength() const { return RouteLength; }
	UFUNCTION(BlueprintPure, Category = "Train") bool IsPlayerAboard() const { return bPlayerAboard; }
	UFUNCTION(BlueprintPure, Category = "Train") bool IsMoving() const;
	UFUNCTION(BlueprintPure, Category = "Train") FString GetStatusString() const;
	UFUNCTION(BlueprintPure, Category = "Train") FString GetDepartureBlockReason() const;

	/** Route evaluation (world space, rail-top level). Distance wraps round a closed route. */
	void EvaluateRoute(float InDistance, FVector& OutLocation, float& OutYaw) const;
	bool IsRouteClosed() const { return bRouteClosed; }
	/** Lap gap check: distance between the route end and start (cm) and heading difference (deg). */
	void GetRouteClosure(float& OutGap, float& OutYawError) const { OutGap = ClosureGap; OutYawError = ClosureYawError; }

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	static constexpr int32 NumCars = 2;
	void ApplyLayout();
	void BuildRoute();
	void PlaceTrain(float TrainDistance);
	void CreateSeats();
	void FindRouteBlockers();
	void SetPlayerIgnoresBlockers(bool bIgnore);
	void SetState(EP9TrainState NewState);
	void SetDoorState(EP9DoorState NewState);
	void UpdateDoors(float DeltaSeconds);
	void UpdateMotion(float DeltaSeconds);
	void UpdateImpacts(float DeltaSeconds);
	void UpdatePlayerAndPrompts();
	void ApplyDoorAlpha();
	void BeginDeparture();
	bool AtStop(int32 StopIndex) const;
	int32 NextStopIndex() const;
	float StopDistance(int32 Index) const;
	void PlayOneShot(UAudioComponent* Audio, const TSoftObjectPtr<USoundBase>& Sound);
	APawn* PlayerPawn() const;
	bool IsPlayerInDoorway() const;
	bool ComputePlayerAboard() const;
	int32 PlatformLocalSide(int32 Car) const;
	void SetupDestinationBoard();
	UFUNCTION() void DrawDestinationBoard(UCanvas* Canvas, int32 Width, int32 Height);
	UFUNCTION() void HandleDoorInteract(UP9InteractableComponent* Source, APawn* InteractingPawn);

	UPROPERTY(VisibleAnywhere, Category = "Train") TObjectPtr<USceneComponent> TrainRoot;
	UPROPERTY(VisibleAnywhere, Category = "Train") TArray<TObjectPtr<USceneComponent>> CarRoots;
	UPROPERTY(VisibleAnywhere, Category = "Train") TArray<TObjectPtr<UStaticMeshComponent>> Shells;
	UPROPERTY(VisibleAnywhere, Category = "Train") TArray<TObjectPtr<UStaticMeshComponent>> Interiors;
	UPROPERTY(VisibleAnywhere, Category = "Train") TArray<TObjectPtr<UStaticMeshComponent>> BogieFrames;
	UPROPERTY(VisibleAnywhere, Category = "Train") TArray<TObjectPtr<UStaticMeshComponent>> Wheelsets;
	UPROPERTY(VisibleAnywhere, Category = "Train") TArray<TObjectPtr<UPointLightComponent>> InteriorLights;
	UPROPERTY(VisibleAnywhere, Category = "Train") TArray<TObjectPtr<UP9InteractableComponent>> DoorInteractables;
	UPROPERTY(VisibleAnywhere, Category = "Train") TArray<TObjectPtr<UAudioComponent>> DoorAudio;
	UPROPERTY(VisibleAnywhere, Category = "Train") TObjectPtr<UAudioComponent> ChimeAudio;
	UPROPERTY(VisibleAnywhere, Category = "Train") TObjectPtr<UAudioComponent> RunAudio;
	UPROPERTY(VisibleAnywhere, Category = "Train") TObjectPtr<UAudioComponent> BrakeAudio;
	UPROPERTY(VisibleAnywhere, Category = "Train") TObjectPtr<UAudioComponent> HornAudio;
	UPROPERTY() TArray<FP9DoorLeaf> Leaves;
	UPROPERTY(Transient) TArray<TObjectPtr<UP9SeatComponent>> Seats;
	UPROPERTY(Transient) TObjectPtr<UCanvasRenderTarget2D> DestinationTarget;
	UPROPERTY(Transient) TArray<TWeakObjectPtr<AActor>> RouteBlockers;

	struct FRouteSeg { FVector Start; float StartYaw; float StartDist; float Length; float Radius; float Sign; FVector Center; };
	TArray<FRouteSeg> Segs;
	float RouteLength = 0.0f;
	bool bRouteClosed = false;
	float ClosureGap = 0.0f;
	float ClosureYawError = 0.0f;

	struct FKnockback { TWeakObjectPtr<ACharacter> Character; float Cooldown = 0.0f; float Recovery = 0.0f; bool bInputBlocked = false; };
	TArray<FKnockback> Knockbacks;

	EP9TrainState TrainState = EP9TrainState::StoppedAtPlatform;
	EP9DoorState DoorState = EP9DoorState::Closed;
	float Distance = 0.0f;           // train-centre distance along the route (kept in [0, RouteLength) between journeys)
	float TargetDistance = 0.0f;     // absolute (unwrapped) distance of the stop being approached
	float Speed = 0.0f;
	float DoorAlpha = 0.0f;
	float StateTime = 0.0f;
	float DoorPhaseTime = 0.0f;
	float WheelAngle[NumCars] = {0.0f, 0.0f};
	int32 CurrentStopIndex = 0;
	int32 TargetStopIndex = 0;
	bool bDeparturePending = false;
	bool bPlayerAboard = false;
	bool bIgnoringBlockers = false;
	bool bArrivalOpenPending = false;
	bool bChimePlayed = false;
	FString LastBlockReason;
	float PromptTimer = 0.0f;
};
