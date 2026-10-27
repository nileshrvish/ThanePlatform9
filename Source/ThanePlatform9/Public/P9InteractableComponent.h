#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "P9InteractableComponent.generated.h"

class UP9InteractableComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FP9InteractSignature, UP9InteractableComponent*, Source, APawn*, Instigator);

/**
 * A point the local player can interact with. Registered with UP9InteractionSubsystem, which picks the nearest
 * enabled interactable within InteractRadius, shows its prompt (touch button + E/gamepad) and fires OnInteract.
 * Owners update PromptText / bActionAvailable as their state changes; an unavailable action shows as information only.
 */
UCLASS(ClassGroup = (P9), meta = (BlueprintSpawnableComponent))
class THANEPLATFORM9_API UP9InteractableComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UP9InteractableComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "P9|Interaction")
	FText PromptText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "P9|Interaction", meta = (ClampMin = "10.0"))
	float InteractRadius = 220.0f;

	/** Not considered at all when false. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "P9|Interaction")
	bool bInteractionEnabled = true;

	/** Shown but cannot be triggered when false (e.g. "Doors locked - train moving"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "P9|Interaction")
	bool bActionAvailable = true;

	UPROPERTY(BlueprintAssignable, Category = "P9|Interaction")
	FP9InteractSignature OnInteract;

	/** Whether this interactable may be offered to Pawn (default: bInteractionEnabled). Seats narrow this to their occupant. */
	virtual bool IsInteractableBy(const APawn* Pawn) const { return bInteractionEnabled; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};
