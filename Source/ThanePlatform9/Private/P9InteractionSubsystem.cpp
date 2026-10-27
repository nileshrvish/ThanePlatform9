#include "P9InteractionSubsystem.h"
#include "ThanePlatform9.h"
#include "P9InteractableComponent.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"

namespace P9Interaction
{
	const TCHAR* InteractActionPath = TEXT("/Game/P9/Input/Actions/IA_Interact.IA_Interact");
	constexpr int32 PromptZOrder = 20;   // above WBP_MobileControls (added with the default order)
}

bool UP9InteractionSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UP9InteractionSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UP9InteractionSubsystem, STATGROUP_Tickables);
}

void UP9InteractionSubsystem::RegisterInteractable(UP9InteractableComponent* Interactable)
{
	Interactables.AddUnique(Interactable);
}

void UP9InteractionSubsystem::UnregisterInteractable(UP9InteractableComponent* Interactable)
{
	Interactables.Remove(Interactable);
	if (Current.Get() == Interactable)
	{
		Current.Reset();
	}
}

void UP9InteractionSubsystem::Deinitialize()
{
	RemovePromptWidget();
	if (APlayerController* PC = BoundController.Get())
	{
		PC->PopInputComponent(InteractInput);
	}
	Super::Deinitialize();
}

void UP9InteractionSubsystem::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!PC || !PC->IsLocalController())
	{
		return;
	}
	EnsureInputBound(PC);

	UP9InteractableComponent* Best = nullptr;
	if (Pawn)
	{
		const FVector Location = Pawn->GetActorLocation();
		float BestDistSq = TNumericLimits<float>::Max();
		Interactables.RemoveAll([](const TWeakObjectPtr<UP9InteractableComponent>& Weak) { return !Weak.IsValid(); });
		for (const TWeakObjectPtr<UP9InteractableComponent>& Weak : Interactables)
		{
			UP9InteractableComponent* Candidate = Weak.Get();
			if (!Candidate->IsInteractableBy(Pawn))
			{
				continue;
			}
			const float DistSq = FVector::DistSquared(Candidate->GetComponentLocation(), Location);
			if (DistSq <= FMath::Square(Candidate->InteractRadius) && DistSq < BestDistSq)
			{
				Best = Candidate;
				BestDistSq = DistSq;
			}
		}
	}
	Current = Best;

	if (Best)
	{
		EnsurePromptWidget();
	}
	if (PromptWidget.IsValid())
	{
		PromptWidget->SetVisibility(Best ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed);
	}
}

FText UP9InteractionSubsystem::GetPromptText() const
{
	const UP9InteractableComponent* Interactable = Current.Get();
	return Interactable ? Interactable->PromptText : FText::GetEmpty();
}

bool UP9InteractionSubsystem::IsActionAvailable() const
{
	const UP9InteractableComponent* Interactable = Current.Get();
	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	return Interactable && Interactable->IsInteractableBy(PC ? PC->GetPawn() : nullptr) && Interactable->bActionAvailable;
}

bool UP9InteractionSubsystem::TriggerInteract()
{
	UP9InteractableComponent* Interactable = Current.Get();
	if (!Interactable || !IsActionAvailable())
	{
		UE_LOG(LogP9, Log, TEXT("Interaction: nothing to trigger (%s)"), Interactable ? *Interactable->PromptText.ToString() : TEXT("no interactable in range"));
		return false;
	}
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	UE_LOG(LogP9, Log, TEXT("Interaction: %s -> \"%s\""), *GetNameSafe(Interactable->GetOwner()), *Interactable->PromptText.ToString());
	Interactable->OnInteract.Broadcast(Interactable, PC ? PC->GetPawn() : nullptr);
	return true;
}

void UP9InteractionSubsystem::EnsureInputBound(APlayerController* PC)
{
	if (BoundController.Get() == PC && InteractInput)
	{
		return;
	}
	if (APlayerController* OldPC = BoundController.Get())
	{
		OldPC->PopInputComponent(InteractInput);
	}
	const UInputAction* Action = LoadObject<UInputAction>(nullptr, P9Interaction::InteractActionPath);
	if (!Action)
	{
		UE_LOG(LogP9, Warning, TEXT("Interaction: %s not found; keyboard/gamepad interact is unavailable (touch prompt still works)."), P9Interaction::InteractActionPath);
		BoundController = PC;
		return;
	}
	InteractInput = NewObject<UEnhancedInputComponent>(PC, TEXT("P9InteractionInput"));
	InteractInput->BindAction(Action, ETriggerEvent::Started, this, &UP9InteractionSubsystem::HandleInteractAction);
	PC->PushInputComponent(InteractInput);
	BoundController = PC;
}

void UP9InteractionSubsystem::HandleInteractAction(const FInputActionValue& Value)
{
	TriggerInteract();
}

void UP9InteractionSubsystem::EnsurePromptWidget()
{
	if (PromptWidget.IsValid())
	{
		return;
	}
	UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	if (!Viewport)
	{
		return;
	}
	TWeakObjectPtr<UP9InteractionSubsystem> WeakThis(this);
	FSlateFontInfo PromptFont = FCoreStyle::GetDefaultFontStyle("Bold", 20);
	FSlateFontInfo ButtonFont = FCoreStyle::GetDefaultFontStyle("Bold", 22);

	PromptWidget = SNew(SOverlay)
		.Visibility(EVisibility::SelfHitTestInvisible)
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(0.0f, 0.0f, 0.0f, 150.0f))
		[
			SNew(SVerticalBox)
			.Visibility(EVisibility::SelfHitTestInvisible)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SNew(SBorder)
				.Visibility(EVisibility::HitTestInvisible)
				.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
				.BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f))
				.Padding(FMargin(18.0f, 8.0f))
				[
					SNew(STextBlock)
					.Font(PromptFont)
					.ColorAndOpacity(FLinearColor(1.0f, 0.95f, 0.85f))
					.Text_Lambda([WeakThis]() { return WeakThis.IsValid() ? WeakThis->GetPromptText() : FText::GetEmpty(); })
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(260.0f)
				.HeightOverride(84.0f)
				.Visibility_Lambda([WeakThis]() { return WeakThis.IsValid() && WeakThis->IsActionAvailable() ? EVisibility::Visible : EVisibility::Collapsed; })
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.ButtonColorAndOpacity(FLinearColor(0.85f, 0.55f, 0.05f, 0.9f))
					.OnClicked_Lambda([WeakThis]()
					{
						if (WeakThis.IsValid())
						{
							WeakThis->TriggerInteract();
						}
						return FReply::Handled();
					})
					[
						SNew(STextBlock)
						.Font(ButtonFont)
						.ColorAndOpacity(FLinearColor::White)
						.Text(NSLOCTEXT("P9", "InteractButton", "INTERACT  [E]"))
					]
				]
			]
		];
	Viewport->AddViewportWidgetContent(PromptWidget.ToSharedRef(), P9Interaction::PromptZOrder);
}

void UP9InteractionSubsystem::RemovePromptWidget()
{
	if (PromptWidget.IsValid())
	{
		if (UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
		{
			Viewport->RemoveViewportWidgetContent(PromptWidget.ToSharedRef());
		}
		PromptWidget.Reset();
	}
}
