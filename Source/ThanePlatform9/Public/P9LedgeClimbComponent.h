#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "P9LedgeClimbComponent.generated.h"

class ACharacter;
class AController;
class APawn;
class APlayerController;
class UEnhancedInputComponent;
class UInputAction;
struct FInputActionValue;

UENUM(BlueprintType)
enum class EP9MantlePhase : uint8
{
	None,
	Rising,
	Forward
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FP9MantleStartedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FP9MantleFinishedSignature, bool, bCompleted);

/**
 * Jump handling plus automatic ledge grab and mantle for a first-person ACharacter.
 *
 * Binds JumpAction itself: press = Jump(), release = StopJumping(). While the character is airborne
 * after a jump and moving toward a wall, it looks for a reachable ledge and, if the character fits on
 * top and the path is clear, lifts it up and over (no climb button).
 *
 * Detection follows the ClimbingSystem project's approach (forward wall trace, then a height trace down
 * onto the ledge top) adapted for a mesh-less first-person capsule:
 *  - wall must be near-vertical and faced by the movement direction;
 *  - ledge top must be walkable and within [MinLedgeHeight, capsule height + MaxReachAboveHead] of the feet;
 *  - the capsule must fit on top, and the rise and forward paths are capsule-swept, so no ceiling or wall clipping.
 * Every mantle has a hard timeout and aborts to falling when blocked, so the character can't get stuck.
 */
UCLASS(ClassGroup = (P9), meta = (BlueprintSpawnableComponent))
class THANEPLATFORM9_API UP9LedgeClimbComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UP9LedgeClimbComponent();

	/** Jump input (IA_Jump). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ledge|Input")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ledge")
	bool bEnableLedgeClimb = true;

	/** Only grab ledges while airborne from a jump (not when walking off an edge). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ledge")
	bool bOnlyAfterJump = true;

	/** How far beyond the capsule surface to look for a wall (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ledge|Detection", meta = (ClampMin = "5.0"))
	float WallReach = 45.0f;

	/** Ledges lower than this above the feet are left to normal movement (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ledge|Detection", meta = (ClampMin = "0.0"))
	float MinLedgeHeight = 50.0f;

	/** How far above the top of the capsule a ledge can be and still be grabbed (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ledge|Detection", meta = (ClampMin = "0.0"))
	float MaxReachAboveHead = 40.0f;

	/** How far past the edge the capsule ends up standing (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ledge|Detection", meta = (ClampMin = "0.0"))
	float LedgeInset = 10.0f;

	/** Minimum cosine between movement direction and wall-facing direction (0.5 = within 60 degrees). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ledge|Detection", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinFacingDot = 0.5f;

	/** Don't grab while falling faster than this (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ledge|Detection", meta = (ClampMin = "0.0"))
	float MaxGrabFallSpeed = 900.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ledge|Motion", meta = (ClampMin = "10.0"))
	float RiseSpeed = 350.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ledge|Motion", meta = (ClampMin = "10.0"))
	float ForwardSpeed = 260.0f;

	/** Hard limit for a whole mantle; exceeding it aborts to falling (s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ledge|Motion", meta = (ClampMin = "0.2"))
	float MaxMantleDuration = 1.5f;

	/** Pause before trying again after an aborted mantle (s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ledge|Motion", meta = (ClampMin = "0.0"))
	float RetryCooldown = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ledge|Debug")
	bool bDrawDebug = false;

	UPROPERTY(BlueprintAssignable, Category = "Ledge")
	FP9MantleStartedSignature OnMantleStarted;

	UPROPERTY(BlueprintAssignable, Category = "Ledge")
	FP9MantleFinishedSignature OnMantleFinished;

	UFUNCTION(BlueprintPure, Category = "Ledge")
	bool IsMantling() const { return Phase != EP9MantlePhase::None; }

	/** Aborts a mantle in progress; the character drops back into falling. */
	UFUNCTION(BlueprintCallable, Category = "Ledge")
	void CancelMantle();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	struct FLedgeCandidate
	{
		FVector WallPoint = FVector::ZeroVector;
		FVector LedgePoint = FVector::ZeroVector;
		FVector RiseLocation = FVector::ZeroVector;
		FVector EndLocation = FVector::ZeroVector;
	};

	UFUNCTION()
	void HandleControllerChanged(APawn* Pawn, AController* OldController, AController* NewController);

	void OnJumpStarted(const FInputActionValue& Value);
	void OnJumpCompleted(const FInputActionValue& Value);

	/** Horizontal direction the player is trying to move in; zero when there is no intent. */
	FVector GetApproachDirection() const;
	bool FindLedge(const FVector& Direction, FLedgeCandidate& OutCandidate, FString& OutRejectReason) const;
	bool IsCapsulePathClear(const FVector& From, const FVector& To) const;
	bool IsCapsuleSpotFree(const FVector& Location) const;

	void StartMantle(const FLedgeCandidate& Candidate);
	void UpdateMantle(float DeltaTime);
	void FinishMantle(bool bCompleted, const TCHAR* Reason);
	void SetMoveInputIgnored(bool bIgnore);

	TWeakObjectPtr<ACharacter> Character;

	UPROPERTY(Transient)
	TObjectPtr<UEnhancedInputComponent> JumpInputComponent;

	TWeakObjectPtr<APlayerController> InputController;
	TWeakObjectPtr<AController> IgnoredMoveController;

	EP9MantlePhase Phase = EP9MantlePhase::None;
	FVector PhaseStart = FVector::ZeroVector;
	FVector PhaseEnd = FVector::ZeroVector;
	FVector MantleEnd = FVector::ZeroVector;
	float PhaseDuration = 0.0f;
	float PhaseElapsed = 0.0f;
	float MantleElapsed = 0.0f;
	float Cooldown = 0.0f;
	FString LastRejectReason;
};
