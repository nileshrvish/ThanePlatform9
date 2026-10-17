#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "P9TouchJoystickWidget.generated.h"

/**
 * Native base for WBP_TouchJoystick.
 *
 * The stick logic itself (OnTouchStarted / OnTouchMoved / OnTouchEnded, UpdateStick and the
 * IA_Move continuous injection) stays in the Blueprint. This class only adds what Blueprint
 * cannot express without extra nodes:
 *  - captures the finger the Blueprint accepted, so the stick keeps tracking it outside its bounds
 *    and always receives the touch-end;
 *  - if that capture is lost (or the widget is destroyed) it runs the Blueprint OnTouchEnded,
 *    which stops the movement injection, so the character can never keep walking on its own.
 */
UCLASS(Abstract)
class THANEPLATFORM9_API UP9TouchJoystickWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** True while a finger is captured by the stick. */
	UFUNCTION(BlueprintPure, Category = "P9|Touch")
	bool IsStickActive() const { return CapturedPointerIndex != INDEX_NONE; }

	/** Releases the captured finger and runs the Blueprint touch-end logic. */
	UFUNCTION(BlueprintCallable, Category = "P9|Touch")
	void ForceRelease();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	virtual FReply NativeOnTouchMoved(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	virtual FReply NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;

private:
	/** Reads the Blueprint's ActivePointer variable; INDEX_NONE if the variable is missing. */
	int32 GetBlueprintActivePointer() const;

	int32 CapturedPointerIndex = INDEX_NONE;
	uint32 CapturedUserIndex = 0;
	FGeometry LastGeometry;
	FVector2D LastScreenPosition = FVector2D::ZeroVector;
};
