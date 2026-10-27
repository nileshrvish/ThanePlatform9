#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "P9InteractionSubsystem.generated.h"

class UP9InteractableComponent;
class UEnhancedInputComponent;
class APlayerController;
class SWidget;
struct FInputActionValue;

/**
 * Game-world interaction for the local player without touching the player Blueprint or the mobile controls:
 * - binds IA_Interact (E / gamepad face-bottom, already in IMC_Default) on its own pushed input component;
 * - each frame picks the nearest enabled UP9InteractableComponent within its radius;
 * - shows a small Slate prompt with a touch button above the existing joystick/jump controls.
 */
UCLASS()
class THANEPLATFORM9_API UP9InteractionSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	void RegisterInteractable(UP9InteractableComponent* Interactable);
	void UnregisterInteractable(UP9InteractableComponent* Interactable);

	/** Fires the current candidate if its action is available. Returns true when something was triggered. */
	UFUNCTION(BlueprintCallable, Category = "P9|Interaction")
	bool TriggerInteract();

	UFUNCTION(BlueprintPure, Category = "P9|Interaction")
	UP9InteractableComponent* GetCurrentInteractable() const { return Current.Get(); }

	FText GetPromptText() const;
	bool IsActionAvailable() const;

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual void Deinitialize() override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void EnsureInputBound(APlayerController* PC);
	void HandleInteractAction(const FInputActionValue& Value);
	void EnsurePromptWidget();
	void RemovePromptWidget();

	TArray<TWeakObjectPtr<UP9InteractableComponent>> Interactables;
	TWeakObjectPtr<UP9InteractableComponent> Current;

	UPROPERTY(Transient)
	TObjectPtr<UEnhancedInputComponent> InteractInput;

	TWeakObjectPtr<APlayerController> BoundController;
	TSharedPtr<SWidget> PromptWidget;
};
