#include "P9TouchJoystickWidget.h"

#include "ThanePlatform9.h"
#include "Components/Widget.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "UObject/UnrealType.h"

void UP9TouchJoystickWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Make the whole stick rectangle hit-testable, not only its visible images,
	// so a thumb landing near the edge of the base still grabs the stick.
	SetVisibility(ESlateVisibility::Visible);
}

void UP9TouchJoystickWidget::NativeDestruct()
{
	ForceRelease();
	Super::NativeDestruct();
}

FReply UP9TouchJoystickWidget::NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	// Runs the Blueprint OnTouchStarted (sets ActivePointer, starts IA_Move injection).
	FReply Reply = Super::NativeOnTouchStarted(InGeometry, InGestureEvent);

	const int32 PointerIndex = static_cast<int32>(InGestureEvent.GetPointerIndex());
	if (CapturedPointerIndex == INDEX_NONE && GetBlueprintActivePointer() == PointerIndex)
	{
		CapturedPointerIndex = PointerIndex;
		CapturedUserIndex = InGestureEvent.GetUserIndex();
		LastGeometry = InGeometry;
		LastScreenPosition = InGestureEvent.GetScreenSpacePosition();

		TSharedPtr<SWidget> CachedWidget = GetCachedWidget();
		if (CachedWidget.IsValid())
		{
			Reply = FReply::Handled().CaptureMouse(CachedWidget.ToSharedRef());
		}
	}
	return Reply;
}

FReply UP9TouchJoystickWidget::NativeOnTouchMoved(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	if (static_cast<int32>(InGestureEvent.GetPointerIndex()) == CapturedPointerIndex)
	{
		LastGeometry = InGeometry;
		LastScreenPosition = InGestureEvent.GetScreenSpacePosition();
	}
	return Super::NativeOnTouchMoved(InGeometry, InGestureEvent);
}

FReply UP9TouchJoystickWidget::NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	// Runs the Blueprint OnTouchEnded (stops IA_Move injection, recentres the thumb).
	FReply Reply = Super::NativeOnTouchEnded(InGeometry, InGestureEvent);

	if (static_cast<int32>(InGestureEvent.GetPointerIndex()) == CapturedPointerIndex)
	{
		CapturedPointerIndex = INDEX_NONE;
		Reply = FReply::Handled().ReleaseMouseCapture();
	}
	return Reply;
}

void UP9TouchJoystickWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	Super::NativeOnMouseCaptureLost(CaptureLostEvent);

	if (CapturedPointerIndex != INDEX_NONE && static_cast<int32>(CaptureLostEvent.PointerIndex) == CapturedPointerIndex)
	{
		ForceRelease();
	}
}

void UP9TouchJoystickWidget::ForceRelease()
{
	if (CapturedPointerIndex == INDEX_NONE)
	{
		return;
	}

	const int32 PointerIndex = CapturedPointerIndex;
	CapturedPointerIndex = INDEX_NONE;

	UE_LOG(LogP9, Verbose, TEXT("Joystick: releasing pointer %d"), PointerIndex);

	// The Blueprint touch-end stops IA_Move injection through the owning player's Enhanced Input subsystem.
	// During teardown (e.g. PIE stopping) that player or subsystem can already be gone; the injection dies
	// with the subsystem, so skip the Blueprint call rather than let it read a null subsystem.
	const APlayerController* OwningPlayer = GetOwningPlayer();
	const ULocalPlayer* LocalPlayer = OwningPlayer ? OwningPlayer->GetLocalPlayer() : nullptr;
	const UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (!InputSubsystem)
	{
		UE_LOG(LogP9, Log, TEXT("Joystick: pointer %d released without Blueprint touch-end; owning player %s, local player %s, input subsystem %s"),
			PointerIndex,
			OwningPlayer ? TEXT("valid") : TEXT("null"),
			LocalPlayer ? TEXT("valid") : TEXT("null"),
			InputSubsystem ? TEXT("valid") : TEXT("null"));
		return;
	}

	// Synthesize the touch-end the Blueprint never got, so its own cleanup runs.
	const FPointerEvent TouchEnd(CapturedUserIndex, static_cast<uint32>(PointerIndex), LastScreenPosition, LastScreenPosition, 0.0f, false);
	OnTouchEnded(LastGeometry, TouchEnd);
}

int32 UP9TouchJoystickWidget::GetBlueprintActivePointer() const
{
	static const FName ActivePointerName(TEXT("ActivePointer"));
	if (const FIntProperty* Property = FindFProperty<FIntProperty>(GetClass(), ActivePointerName))
	{
		return Property->GetPropertyValue_InContainer(this);
	}
	return INDEX_NONE;
}
