#pragma once

#include "CoreMinimal.h"
#include "P9InteractableComponent.h"
#include "Engine/EngineTypes.h"
#include "P9SeatComponent.generated.h"

class ACharacter;

/**
 * A sitting position (component location = seat point on the cushion, X axis = facing direction). Uses the shared
 * interaction prompt: "Sit down" / "Stand up". Sitting saves the character's movement mode and capsule collision,
 * stops walking input (look stays free), attaches the character to the seat so it rides with a moving parent, and
 * places the eye SeatedEyeHeight above the seat. Standing detaches, restores everything and puts the character on
 * the floor at StandOffset (seat space), nudged to the nearest free spot. One occupant at a time.
 */
UCLASS(ClassGroup = (P9), meta = (BlueprintSpawnableComponent))
class THANEPLATFORM9_API UP9SeatComponent : public UP9InteractableComponent
{
	GENERATED_BODY()

public:
	UP9SeatComponent();

	/** Where the character stands up to, in seat space (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "P9|Seat") FVector StandOffset = FVector(60.0f, 0.0f, 45.0f);
	/** Eye height above the seat point while seated (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "P9|Seat", meta = (ClampMin = "20.0")) float SeatedEyeHeight = 78.0f;

	UFUNCTION(BlueprintCallable, Category = "P9|Seat") bool Sit(ACharacter* Character);
	UFUNCTION(BlueprintCallable, Category = "P9|Seat") bool Stand();
	UFUNCTION(BlueprintPure, Category = "P9|Seat") bool IsOccupied() const { return Occupant.IsValid(); }
	UFUNCTION(BlueprintPure, Category = "P9|Seat") ACharacter* GetOccupant() const { return Occupant.Get(); }

	/** The seat a character is currently sitting in, if any. */
	static UP9SeatComponent* GetSeatOf(const AActor* Actor);

	virtual bool IsInteractableBy(const APawn* Pawn) const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION() void HandleInteract(UP9InteractableComponent* Source, APawn* InteractingPawn);

	TWeakObjectPtr<ACharacter> Occupant;
	TEnumAsByte<EMovementMode> SavedMovementMode = MOVE_Walking;
	TEnumAsByte<ECollisionEnabled::Type> SavedCollision = ECollisionEnabled::QueryAndPhysics;
	bool bBlockedMoveInput = false;
};
