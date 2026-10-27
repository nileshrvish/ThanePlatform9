#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "P9MobileControlsWidget.generated.h"

class UButton;
class UInputAction;
class UEnhancedInputLocalPlayerSubsystem;

/**
 * Native base for WBP_MobileControls.
 *
 * - Touch-drag look: any finger that lands on empty screen (not on the joystick or the Jump
 *   button) drives IA_Look. Each finger is tracked by pointer index and captured, so a look drag
 *   that crosses the joystick or the button keeps working, and the joystick finger never rotates
 *   the camera. Deltas are in Slate units, so sensitivity is the same on any screen density.
 * - Jump button: while the JumpButton widget is pressed, IA_Jump is held through Enhanced Input
 *   injection, the same pattern the joystick uses for IA_Move.
 */
UCLASS(Abstract)
class THANEPLATFORM9_API UP9MobileControlsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Action fed by touch-drag look (X = yaw, Y = pitch). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "P9|Look")
	TObjectPtr<UInputAction> LookAction;

	/**
	 * Look input per Slate unit dragged. With the project's legacy input scale (2.5) the default
	 * 0.06 is roughly 0.15 degrees per unit, about a 160 degree turn across a phone in landscape.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "P9|Look", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "0.3"))
	float LookSensitivity = 0.06f;

	/** Vertical sensitivity relative to horizontal. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "P9|Look", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "2.0"))
	float LookVerticalScale = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "P9|Look")
	bool bInvertLookY = false;

	/** How quickly smoothed look catches up with the finger (1/s). 0 disables smoothing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "P9|Look", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "60.0"))
	float LookSmoothingSpeed = 30.0f;

	/** Action held while the Jump button is pressed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "P9|Jump")
	TObjectPtr<UInputAction> JumpAction;

	/** Name of the Jump button widget in this Widget Blueprint. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "P9|Jump")
	FName JumpButtonName = TEXT("JumpButton");

	/** Name of the legacy look border; it is made non-hit-testable so its old bindings don't double the look. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "P9|Look")
	FName LegacyLookAreaName = TEXT("LookArea");

	/** Collapse the controls when there is no touch input (desktop without "Use Mouse for Touch"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "P9|Platform")
	bool bHideWithoutTouchInput = true;

	UFUNCTION(BlueprintPure, Category = "P9|Look")
	bool IsLooking() const { return LookPointerIndex != INDEX_NONE; }

	/** True when the JumpButton widget was found at construct time. */
	UFUNCTION(BlueprintPure, Category = "P9|Jump")
	bool IsJumpButtonResolved() const { return JumpButtonWidget != nullptr; }

	/** True while IA_Jump is being held by the Jump button. */
	UFUNCTION(BlueprintPure, Category = "P9|Jump")
	bool IsJumpHeld() const { return bJumpHeld; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	virtual FReply NativeOnTouchMoved(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	virtual FReply NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;

private:
	UEnhancedInputLocalPlayerSubsystem* GetInputSubsystem() const;
	static bool HasTouchInput();
	void ResetLook();
	void SetJumpHeld(bool bHeld);

	UPROPERTY(Transient)
	TObjectPtr<UButton> JumpButtonWidget;

	int32 LookPointerIndex = INDEX_NONE;
	FVector2D LastLookPosition = FVector2D::ZeroVector;
	FVector2D PendingLookDelta = FVector2D::ZeroVector;
	FVector2D SmoothedLookDelta = FVector2D::ZeroVector;
	bool bJumpHeld = false;
};
