// Development-only console commands that drive the same Enhanced Input actions the touch widgets use,
// so movement, jump, look and ledge climbing can be exercised in PIE without a touch device.

#include "ThanePlatform9.h"

#if !UE_BUILD_SHIPPING

#include "Components/CapsuleComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/PanelSlot.h"
#include "Components/Widget.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "InputAction.h"
#include "P9LedgeClimbComponent.h"
#include "P9MobileControlsWidget.h"
#include "P9TouchJoystickWidget.h"
#include "TimerManager.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/SViewport.h"

namespace P9Test
{
	const TCHAR* MoveActionPath = TEXT("/Game/P9/Input/Actions/IA_Move.IA_Move");
	const TCHAR* LookActionPath = TEXT("/Game/P9/Input/Actions/IA_Look.IA_Look");
	const TCHAR* JumpActionPath = TEXT("/Game/P9/Input/Actions/IA_Jump.IA_Jump");

	UEnhancedInputLocalPlayerSubsystem* GetSubsystem(UWorld* World)
	{
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		ULocalPlayer* LocalPlayer = PC ? PC->GetLocalPlayer() : nullptr;
		return LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	}

	float ArgFloat(const TArray<FString>& Args, int32 Index, float Default)
	{
		return Args.IsValidIndex(Index) ? FCString::Atof(*Args[Index]) : Default;
	}

	/** Holds an action at a value for Seconds, then releases it. */
	void HoldAction(UWorld* World, const TCHAR* ActionPath, const FInputActionValue& Value, float Seconds)
	{
		UEnhancedInputLocalPlayerSubsystem* Subsystem = GetSubsystem(World);
		const UInputAction* Action = LoadObject<UInputAction>(nullptr, ActionPath);
		if (!Subsystem || !Action)
		{
			UE_LOG(LogP9, Warning, TEXT("p9.Test: no player input subsystem or action %s (is PIE running?)"), ActionPath);
			return;
		}

		Subsystem->StartContinuousInputInjectionForAction(Action, Value, {}, {});
		FTimerHandle Handle;
		TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> WeakSubsystem(Subsystem);
		World->GetTimerManager().SetTimer(Handle, [WeakSubsystem, Action]()
		{
			if (UEnhancedInputLocalPlayerSubsystem* Sub = WeakSubsystem.Get())
			{
				Sub->StopContinuousInputInjectionForAction(Action);
			}
		}, FMath::Max(Seconds, 0.02f), false);
		UE_LOG(LogP9, Log, TEXT("p9.Test: holding %s = %s for %.2fs"), ActionPath, *Value.ToString(), Seconds);
	}
}

static FAutoConsoleCommandWithWorldAndArgs GP9TestMove(
	TEXT("p9.Test.Move"),
	TEXT("p9.Test.Move <X right> <Y forward> <Seconds> - hold IA_Move"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		P9Test::HoldAction(World, P9Test::MoveActionPath,
			FInputActionValue(FVector2D(P9Test::ArgFloat(Args, 0, 0.0f), P9Test::ArgFloat(Args, 1, 1.0f))),
			P9Test::ArgFloat(Args, 2, 1.0f));
	}));

static FAutoConsoleCommandWithWorldAndArgs GP9TestJump(
	TEXT("p9.Test.Jump"),
	TEXT("p9.Test.Jump <HoldSeconds> - press and hold IA_Jump"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		P9Test::HoldAction(World, P9Test::JumpActionPath, FInputActionValue(true), P9Test::ArgFloat(Args, 0, 0.2f));
	}));

static FAutoConsoleCommandWithWorldAndArgs GP9TestLook(
	TEXT("p9.Test.Look"),
	TEXT("p9.Test.Look <X> <Y> <Seconds> - hold IA_Look at a value"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		P9Test::HoldAction(World, P9Test::LookActionPath,
			FInputActionValue(FVector2D(P9Test::ArgFloat(Args, 0, 0.0f), P9Test::ArgFloat(Args, 1, 0.0f))),
			P9Test::ArgFloat(Args, 2, 0.1f));
	}));

static FAutoConsoleCommandWithWorldAndArgs GP9TestTeleport(
	TEXT("p9.Test.Teleport"),
	TEXT("p9.Test.Teleport <X> <Y> <Z> <Yaw> - place the player pawn and face Yaw"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		if (!Pawn)
		{
			UE_LOG(LogP9, Warning, TEXT("p9.Test.Teleport: no player pawn (is PIE running?)"));
			return;
		}
		const FVector Location(P9Test::ArgFloat(Args, 0, 0.0f), P9Test::ArgFloat(Args, 1, 0.0f), P9Test::ArgFloat(Args, 2, 0.0f));
		const FRotator Rotation(0.0f, P9Test::ArgFloat(Args, 3, 0.0f), 0.0f);
		Pawn->TeleportTo(Location, Rotation);
		PC->SetControlRotation(Rotation);
		if (ACharacter* Character = Cast<ACharacter>(Pawn))
		{
			Character->GetCharacterMovement()->StopMovementImmediately();
		}
		UE_LOG(LogP9, Log, TEXT("p9.Test.Teleport: %s yaw %.0f"), *Location.ToCompactString(), Rotation.Yaw);
	}));

static FAutoConsoleCommandWithWorldAndArgs GP9TestState(
	TEXT("p9.Test.State"),
	TEXT("p9.Test.State - log the player's location, movement and ledge state"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		ACharacter* Character = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
		if (!Character)
		{
			UE_LOG(LogP9, Warning, TEXT("p9.Test.State: no player character (is PIE running?)"));
			return;
		}
		const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
		const UP9LedgeClimbComponent* Ledge = Character->FindComponentByClass<UP9LedgeClimbComponent>();
		UE_LOG(LogP9, Log, TEXT("p9.Test.State: loc=%s vel=%s mode=%s jumps=%d mantling=%s rot=%s ledgeComp=%s"),
			*Character->GetActorLocation().ToCompactString(),
			*Movement->Velocity.ToCompactString(),
			*Movement->GetMovementName(),
			Character->JumpCurrentCount,
			Ledge && Ledge->IsMantling() ? TEXT("yes") : TEXT("no"),
			*PC->GetControlRotation().ToCompactString(),
			Ledge ? TEXT("present") : TEXT("missing"));

		// Values the MCP cannot read from the Blueprint CDO (inherited native components).
		const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
		const float DeltaSeconds = World->GetDeltaSeconds();
		UE_LOG(LogP9, Log, TEXT("p9.Test.State: capsule r=%.1f hh=%.1f profile=%s | jumpZ=%.0f gravity=%.2f airControl=%.2f stepHeight=%.1f walkableAngle=%.1f maxWalk=%.0f | fps=%.1f"),
			Capsule->GetScaledCapsuleRadius(),
			Capsule->GetScaledCapsuleHalfHeight(),
			*Capsule->GetCollisionProfileName().ToString(),
			Movement->JumpZVelocity,
			Movement->GravityScale,
			Movement->AirControl,
			Movement->MaxStepHeight,
			Movement->GetWalkableFloorAngle(),
			Movement->MaxWalkSpeed,
			DeltaSeconds > 0.0f ? 1.0f / DeltaSeconds : 0.0f);
	}));

// ---------------------------------------------------------------------------------------------
// Touch routing tests: real Slate touch events into the PIE game viewport, so hit-testing,
// capture and bubbling between the joystick, Jump button and look area are exercised as on a device.
// Coordinates are normalized to the game viewport (0,0 = top-left, 1,1 = bottom-right).

namespace P9Test
{
	TMap<int32, FVector2D> LastTouchPositions;

	bool ViewportToScreen(UWorld* World, const FVector2D& Normalized, FVector2D& OutScreen)
	{
		UGameViewportClient* ViewportClient = World ? World->GetGameViewport() : nullptr;
		TSharedPtr<SViewport> ViewportWidget = ViewportClient ? ViewportClient->GetGameViewportWidget() : nullptr;
		if (!ViewportWidget.IsValid())
		{
			UE_LOG(LogP9, Warning, TEXT("p9.Test.Touch: no game viewport (is PIE running?)"));
			return false;
		}
		const FGeometry& Geometry = ViewportWidget->GetTickSpaceGeometry();
		OutScreen = FVector2D(Geometry.LocalToAbsolute(Normalized * FVector2D(Geometry.GetLocalSize())));
		return true;
	}

	void SendTouch(const FString& Phase, int32 Pointer, const FVector2D& Screen)
	{
		const FVector2D Last = LastTouchPositions.Contains(Pointer) ? LastTouchPositions[Pointer] : Screen;
		const FPointerEvent Event(0, static_cast<uint32>(Pointer), Screen, Last, 1.0f, true);
		FSlateApplication& Slate = FSlateApplication::Get();
		if (Phase.Equals(TEXT("Start"), ESearchCase::IgnoreCase))
		{
			Slate.ProcessTouchStartedEvent(nullptr, Event);
			LastTouchPositions.Add(Pointer, Screen);
		}
		else if (Phase.Equals(TEXT("Move"), ESearchCase::IgnoreCase))
		{
			Slate.ProcessTouchMovedEvent(Event);
			LastTouchPositions.Add(Pointer, Screen);
		}
		else
		{
			Slate.ProcessTouchEndedEvent(Event);
			LastTouchPositions.Remove(Pointer);
		}
	}
}

static FAutoConsoleCommandWithWorldAndArgs GP9TestTouch(
	TEXT("p9.Test.Touch"),
	TEXT("p9.Test.Touch <Start|Move|End> <Pointer> <X 0-1> <Y 0-1> - send one touch event to the PIE viewport"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 4)
		{
			UE_LOG(LogP9, Warning, TEXT("p9.Test.Touch: usage <Start|Move|End> <Pointer> <X> <Y>"));
			return;
		}
		FVector2D Screen;
		if (!P9Test::ViewportToScreen(World, FVector2D(FCString::Atof(*Args[2]), FCString::Atof(*Args[3])), Screen))
		{
			return;
		}
		const int32 Pointer = FCString::Atoi(*Args[1]);
		P9Test::SendTouch(Args[0], Pointer, Screen);
		UE_LOG(LogP9, Log, TEXT("p9.Test.Touch: %s pointer %d at %s"), *Args[0], Pointer, *Screen.ToString());
	}));

static FAutoConsoleCommandWithWorldAndArgs GP9TestTouchDrag(
	TEXT("p9.Test.TouchDrag"),
	TEXT("p9.Test.TouchDrag <Pointer> <ToX 0-1> <ToY 0-1> <Seconds> - move a touching pointer to a point over time"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		const int32 Pointer = Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) : 0;
		if (!P9Test::LastTouchPositions.Contains(Pointer) || !World)
		{
			UE_LOG(LogP9, Warning, TEXT("p9.Test.TouchDrag: pointer %d is not touching (use p9.Test.Touch Start first)"), Pointer);
			return;
		}
		FVector2D Target;
		if (!P9Test::ViewportToScreen(World, FVector2D(P9Test::ArgFloat(Args, 1, 0.5f), P9Test::ArgFloat(Args, 2, 0.5f)), Target))
		{
			return;
		}
		const FVector2D From = P9Test::LastTouchPositions[Pointer];
		const float Seconds = FMath::Max(P9Test::ArgFloat(Args, 3, 0.5f), 0.05f);
		const double StartTime = World->GetTimeSeconds();
		TWeakObjectPtr<UWorld> WeakWorld(World);
		TSharedRef<FTimerHandle> Handle = MakeShared<FTimerHandle>();
		World->GetTimerManager().SetTimer(*Handle, [WeakWorld, Handle, Pointer, From, Target, Seconds, StartTime]()
		{
			UWorld* TimerWorld = WeakWorld.Get();
			if (!TimerWorld || !P9Test::LastTouchPositions.Contains(Pointer))
			{
				if (TimerWorld)
				{
					TimerWorld->GetTimerManager().ClearTimer(*Handle);
				}
				return;
			}
			const float Alpha = FMath::Clamp(static_cast<float>((TimerWorld->GetTimeSeconds() - StartTime) / Seconds), 0.0f, 1.0f);
			P9Test::SendTouch(TEXT("Move"), Pointer, FMath::Lerp(From, Target, Alpha));
			if (Alpha >= 1.0f)
			{
				TimerWorld->GetTimerManager().ClearTimer(*Handle);
			}
		}, 1.0f / 60.0f, true);
		UE_LOG(LogP9, Log, TEXT("p9.Test.TouchDrag: pointer %d to %s over %.2fs"), Pointer, *Target.ToString(), Seconds);
	}));

static FAutoConsoleCommandWithWorldAndArgs GP9TestShowTouchControls(
	TEXT("p9.Test.ShowTouchControls"),
	TEXT("p9.Test.ShowTouchControls - make the mobile controls visible in desktop PIE"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		int32 Count = 0;
		for (TObjectIterator<UP9MobileControlsWidget> It; It; ++It)
		{
			if (It->GetWorld() == World && It->IsInViewport())
			{
				It->SetVisibility(ESlateVisibility::Visible);
				++Count;
			}
		}
		UE_LOG(LogP9, Log, TEXT("p9.Test.ShowTouchControls: %d widget(s) made visible"), Count);
	}));

static FAutoConsoleCommandWithWorldAndArgs GP9TestWidgetState(
	TEXT("p9.Test.WidgetState"),
	TEXT("p9.Test.WidgetState - log mobile controls and joystick state"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		for (TObjectIterator<UP9MobileControlsWidget> It; It; ++It)
		{
			if (It->GetWorld() == World && It->IsInViewport())
			{
				UE_LOG(LogP9, Log, TEXT("p9.Test.WidgetState: controls %s visibility=%s jumpButton=%s jumpHeld=%s looking=%s"),
					*It->GetName(),
					*UEnum::GetValueAsString(It->GetVisibility()),
					It->IsJumpButtonResolved() ? TEXT("resolved") : TEXT("MISSING"),
					It->IsJumpHeld() ? TEXT("yes") : TEXT("no"),
					It->IsLooking() ? TEXT("yes") : TEXT("no"));
			}
		}
		for (TObjectIterator<UP9TouchJoystickWidget> It; It; ++It)
		{
			if (It->GetWorld() == World && It->GetCachedWidget().IsValid())
			{
				const UWidget* Thumb = It->GetWidgetFromName(TEXT("Thumb"));
				UE_LOG(LogP9, Log, TEXT("p9.Test.WidgetState: joystick %s active=%s thumb=%s"),
					*It->GetName(),
					It->IsStickActive() ? TEXT("yes") : TEXT("no"),
					Thumb ? *Thumb->GetRenderTransform().Translation.ToString() : TEXT("none"));
			}
		}
	}));

// Read-only layout/appearance dump of every widget in the mobile controls and joystick trees:
// selected reflected properties (brushes, tints, opacity, size overrides, slot layout) plus the
// widget's real on-screen geometry, so UI issues can be diagnosed from facts rather than screenshots.
namespace P9Test
{
	FString ExportProperty(const UObject* Object, const TCHAR* Name)
	{
		const FProperty* Property = Object ? FindFProperty<FProperty>(Object->GetClass(), Name) : nullptr;
		if (!Property)
		{
			return FString();
		}
		FString Text;
		Property->ExportTextItem_Direct(Text, Property->ContainerPtrToValuePtr<void>(Object), nullptr, nullptr, PPF_None);
		return Text.Left(400);
	}

	void DumpWidgetTree(UUserWidget* Owner)
	{
		static const TCHAR* WidgetProps[] = { TEXT("Visibility"), TEXT("RenderOpacity"), TEXT("BrushColor"), TEXT("Background"),
			TEXT("ContentColorAndOpacity"), TEXT("ColorAndOpacity"), TEXT("Brush"), TEXT("WidgetStyle"), TEXT("BackgroundColor"),
			TEXT("WidthOverride"), TEXT("HeightOverride"), TEXT("bOverride_WidthOverride"), TEXT("bOverride_HeightOverride"), TEXT("Clipping") };
		static const TCHAR* SlotProps[] = { TEXT("LayoutData"), TEXT("bAutoSize"), TEXT("ZOrder"), TEXT("HorizontalAlignment"), TEXT("VerticalAlignment") };

		const FGeometry& OwnerGeometry = Owner->GetCachedGeometry();
		UE_LOG(LogP9, Log, TEXT("p9.Test.DumpControls: %s [%s] abs=%s absSize=%s localSize=%s"),
			*Owner->GetName(), *Owner->GetClass()->GetName(),
			*FVector2D(OwnerGeometry.GetAbsolutePosition()).ToString(), *FVector2D(OwnerGeometry.GetAbsoluteSize()).ToString(),
			*FVector2D(OwnerGeometry.GetLocalSize()).ToString());

		if (!Owner->WidgetTree)
		{
			return;
		}
		Owner->WidgetTree->ForEachWidget([](UWidget* Widget)
		{
			const FGeometry& Geometry = Widget->GetCachedGeometry();
			FString Line = FString::Printf(TEXT("  %s [%s] abs=%s absSize=%s localSize=%s"),
				*Widget->GetName(), *Widget->GetClass()->GetName(),
				*FVector2D(Geometry.GetAbsolutePosition()).ToString(), *FVector2D(Geometry.GetAbsoluteSize()).ToString(),
				*FVector2D(Geometry.GetLocalSize()).ToString());
			for (const TCHAR* Name : WidgetProps)
			{
				const FString Value = ExportProperty(Widget, Name);
				if (!Value.IsEmpty())
				{
					Line += FString::Printf(TEXT(" | %s=%s"), Name, *Value);
				}
			}
			if (Widget->Slot)
			{
				for (const TCHAR* Name : SlotProps)
				{
					const FString Value = ExportProperty(Widget->Slot, Name);
					if (!Value.IsEmpty())
					{
						Line += FString::Printf(TEXT(" | slot.%s=%s"), Name, *Value);
					}
				}
			}
			UE_LOG(LogP9, Log, TEXT("p9.Test.DumpControls:%s"), *Line);
		});
	}
}

static FAutoConsoleCommandWithWorldAndArgs GP9TestDumpControls(
	TEXT("p9.Test.DumpControls"),
	TEXT("p9.Test.DumpControls - log layout, brushes and geometry of the mobile controls and joystick widgets"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (UGameViewportClient* ViewportClient = World ? World->GetGameViewport() : nullptr)
		{
			FVector2D ViewportSize;
			ViewportClient->GetViewportSize(ViewportSize);
			UE_LOG(LogP9, Log, TEXT("p9.Test.DumpControls: viewport %s px, DPI scale %.3f"),
				*ViewportSize.ToString(), ViewportClient->GetDPIScale());
		}
		for (TObjectIterator<UP9MobileControlsWidget> It; It; ++It)
		{
			if (It->GetWorld() == World && It->IsInViewport())
			{
				P9Test::DumpWidgetTree(*It);
			}
		}
		for (TObjectIterator<UP9TouchJoystickWidget> It; It; ++It)
		{
			if (It->GetWorld() == World && It->GetCachedWidget().IsValid())
			{
				P9Test::DumpWidgetTree(*It);
			}
		}
	}));

// ---------------------------------------------------------------------------------------------
// Editor background throttling: PIE drops to a few fps when the editor isn't the foreground app,
// which breaks timing-sensitive tests. Toggles UEditorPerformanceSettings::bThrottleCPUWhenNotForeground
// on the class default object (in memory only; not saved to config) and reads the value back.

#if WITH_EDITOR
namespace P9Test
{
	TOptional<bool> OriginalEditorThrottle;

	bool AccessEditorThrottle(TOptional<bool> NewValue, bool& OutValue)
	{
		UClass* SettingsClass = FindObject<UClass>(nullptr, TEXT("/Script/UnrealEd.EditorPerformanceSettings"));
		const FBoolProperty* Property = SettingsClass ? FindFProperty<FBoolProperty>(SettingsClass, TEXT("bThrottleCPUWhenNotForeground")) : nullptr;
		UObject* Settings = SettingsClass ? SettingsClass->GetDefaultObject() : nullptr;
		if (!Property || !Settings)
		{
			return false;
		}
		if (NewValue.IsSet())
		{
			Property->SetPropertyValue_InContainer(Settings, NewValue.GetValue());
		}
		OutValue = Property->GetPropertyValue_InContainer(Settings);
		return true;
	}
}

static FAutoConsoleCommandWithWorldAndArgs GP9TestEditorThrottle(
	TEXT("p9.Test.EditorThrottle"),
	TEXT("p9.Test.EditorThrottle [0|1|restore] - read or set 'Use Less CPU when in Background' (in memory only)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		bool Current = false;
		if (!P9Test::AccessEditorThrottle({}, Current))
		{
			UE_LOG(LogP9, Warning, TEXT("p9.Test.EditorThrottle: EditorPerformanceSettings.bThrottleCPUWhenNotForeground not found"));
			return;
		}

		TOptional<bool> Requested;
		if (Args.IsValidIndex(0))
		{
			if (Args[0].Equals(TEXT("restore"), ESearchCase::IgnoreCase))
			{
				if (!P9Test::OriginalEditorThrottle.IsSet())
				{
					UE_LOG(LogP9, Log, TEXT("p9.Test.EditorThrottle: nothing to restore (value=%d)"), Current ? 1 : 0);
					return;
				}
				Requested = P9Test::OriginalEditorThrottle.GetValue();
			}
			else
			{
				Requested = FCString::Atoi(*Args[0]) != 0;
			}
		}

		if (Requested.IsSet() && !P9Test::OriginalEditorThrottle.IsSet())
		{
			P9Test::OriginalEditorThrottle = Current;
		}

		bool ReadBack = false;
		P9Test::AccessEditorThrottle(Requested, ReadBack);
		if (Requested.IsSet() && Args[0].Equals(TEXT("restore"), ESearchCase::IgnoreCase) && ReadBack == Requested.GetValue())
		{
			P9Test::OriginalEditorThrottle.Reset();
		}
		UE_LOG(LogP9, Log, TEXT("p9.Test.EditorThrottle: before=%d requested=%s now=%d original=%s"),
			Current ? 1 : 0,
			Requested.IsSet() ? (Requested.GetValue() ? TEXT("1") : TEXT("0")) : TEXT("-"),
			ReadBack ? 1 : 0,
			P9Test::OriginalEditorThrottle.IsSet() ? (P9Test::OriginalEditorThrottle.GetValue() ? TEXT("1") : TEXT("0")) : TEXT("-"));
	}));
#endif // WITH_EDITOR

#endif // !UE_BUILD_SHIPPING
