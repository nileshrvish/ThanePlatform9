#include "P9MobileControlsWidget.h"

#include "ThanePlatform9.h"
#include "Components/Button.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/InputSettings.h"
#include "InputAction.h"

void UP9MobileControlsWidget::NativeConstruct()
{
	Super::NativeConstruct();

	JumpButtonWidget = Cast<UButton>(GetWidgetFromName(JumpButtonName));
	if (!JumpButtonWidget)
	{
		UE_LOG(LogP9, Warning, TEXT("MobileControls: no Button named '%s'; Jump button disabled."), *JumpButtonName.ToString());
	}

	// The legacy look border still has its mouse bindings; keep it in the tree but take it out of hit-testing.
	if (UWidget* LegacyLookArea = GetWidgetFromName(LegacyLookAreaName))
	{
		LegacyLookArea->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	if (bHideWithoutTouchInput && !HasTouchInput())
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	// The widget itself catches touches that land on empty screen; children (joystick, button) still win where they are.
	SetVisibility(ESlateVisibility::Visible);
}

void UP9MobileControlsWidget::NativeDestruct()
{
	SetJumpHeld(false);
	ResetLook();
	Super::NativeDestruct();
}

void UP9MobileControlsWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Jump: mirror the button's pressed state into IA_Jump.
	SetJumpHeld(JumpButtonWidget && JumpButtonWidget->IsPressed() && GetVisibility() != ESlateVisibility::Collapsed);

	// Look: gather this frame's drag and optionally smooth it.
	const FVector2D FrameDelta = PendingLookDelta;
	PendingLookDelta = FVector2D::ZeroVector;

	if (LookSmoothingSpeed > 0.0f)
	{
		const float Alpha = 1.0f - FMath::Exp(-LookSmoothingSpeed * InDeltaTime);
		SmoothedLookDelta = FMath::Lerp(SmoothedLookDelta, FrameDelta, Alpha);
	}
	else
	{
		SmoothedLookDelta = FrameDelta;
	}

	if (SmoothedLookDelta.IsNearlyZero(0.001))
	{
		SmoothedLookDelta = FVector2D::ZeroVector;
		return;
	}

	if (!LookAction)
	{
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = GetInputSubsystem())
	{
		// Screen Y grows downwards; with the legacy (negative) pitch scale a positive value looks down,
		// so dragging up (negative Y) looks up without flipping the sign here.
		const float YSign = bInvertLookY ? -1.0f : 1.0f;
		const FVector2D LookValue(
			SmoothedLookDelta.X * LookSensitivity,
			SmoothedLookDelta.Y * LookSensitivity * LookVerticalScale * YSign);
		Subsystem->InjectInputForAction(LookAction, FInputActionValue(LookValue), {}, {});
	}
}

FReply UP9MobileControlsWidget::NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	FReply Reply = Super::NativeOnTouchStarted(InGeometry, InGestureEvent);
	if (Reply.IsEventHandled() || LookPointerIndex != INDEX_NONE)
	{
		return Reply;
	}

	TSharedPtr<SWidget> CachedWidget = GetCachedWidget();
	if (!CachedWidget.IsValid())
	{
		return Reply;
	}

	LookPointerIndex = static_cast<int32>(InGestureEvent.GetPointerIndex());
	LastLookPosition = InGeometry.AbsoluteToLocal(InGestureEvent.GetScreenSpacePosition());
	PendingLookDelta = FVector2D::ZeroVector;
	return FReply::Handled().CaptureMouse(CachedWidget.ToSharedRef());
}

FReply UP9MobileControlsWidget::NativeOnTouchMoved(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	if (static_cast<int32>(InGestureEvent.GetPointerIndex()) != LookPointerIndex)
	{
		return Super::NativeOnTouchMoved(InGeometry, InGestureEvent);
	}

	const FVector2D LocalPosition = InGeometry.AbsoluteToLocal(InGestureEvent.GetScreenSpacePosition());
	PendingLookDelta += LocalPosition - LastLookPosition;
	LastLookPosition = LocalPosition;
	return FReply::Handled();
}

FReply UP9MobileControlsWidget::NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	if (static_cast<int32>(InGestureEvent.GetPointerIndex()) != LookPointerIndex)
	{
		return Super::NativeOnTouchEnded(InGeometry, InGestureEvent);
	}

	ResetLook();
	return FReply::Handled().ReleaseMouseCapture();
}

void UP9MobileControlsWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	Super::NativeOnMouseCaptureLost(CaptureLostEvent);

	if (static_cast<int32>(CaptureLostEvent.PointerIndex) == LookPointerIndex)
	{
		ResetLook();
	}
}

void UP9MobileControlsWidget::ResetLook()
{
	LookPointerIndex = INDEX_NONE;
	PendingLookDelta = FVector2D::ZeroVector;
}

void UP9MobileControlsWidget::SetJumpHeld(bool bHeld)
{
	if (bHeld == bJumpHeld || !JumpAction)
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* Subsystem = GetInputSubsystem();
	if (!Subsystem)
	{
		return;
	}

	bJumpHeld = bHeld;
	if (bHeld)
	{
		Subsystem->StartContinuousInputInjectionForAction(JumpAction, FInputActionValue(true), {}, {});
	}
	else
	{
		Subsystem->StopContinuousInputInjectionForAction(JumpAction);
	}
}

UEnhancedInputLocalPlayerSubsystem* UP9MobileControlsWidget::GetInputSubsystem() const
{
	const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	return LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
}

bool UP9MobileControlsWidget::HasTouchInput()
{
#if PLATFORM_ANDROID || PLATFORM_IOS
	return true;
#else
	const UInputSettings* InputSettings = GetDefault<UInputSettings>();
	const bool bFakingTouch = FSlateApplication::IsInitialized() && FSlateApplication::Get().IsFakingTouchEvents();
	return bFakingTouch || InputSettings->bUseMouseForTouch || InputSettings->bAlwaysShowTouchInterface;
#endif
}
