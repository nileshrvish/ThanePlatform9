#include "P9Captions.h"
#include "ThanePlatform9.h"
#include "P9CampaignDirector.h"
#include "P9GameSettings.h"
#include "P9SeatComponent.h"
#include "P9Train.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Sound/SoundBase.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSafeZone.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace P9Captions
{
	constexpr int32 ZOrder = 60;
	constexpr float MinRead = 1.2f;          // a line stays at least this long (never past the next line)
	constexpr float FadeRate = 6.0f;         // alpha per second
	const TCHAR* TablePath = TEXT("/Game/P9/UI/DT_P9_Captions.DT_P9_Captions");

	void NotifySound(const UObject* WorldContext, const USoundBase* Sound, float Delay)
	{
		if (!Sound) return;
		if (UP9CaptionSubsystem* C = UP9CaptionSubsystem::Get(WorldContext)) C->OnSound(Sound, Delay);
	}
	void StopSound(const UObject* WorldContext, const USoundBase* Sound)
	{
		if (!Sound) return;
		if (UP9CaptionSubsystem* C = UP9CaptionSubsystem::Get(WorldContext)) C->StopSound(Sound->GetFName());
	}
	static FAutoConsoleCommandWithWorldAndArgs TestCommand(TEXT("p9.Captions.Test"), TEXT("p9.Captions.Test <SoundName> - show that sound's captions now (test)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UP9CaptionSubsystem* C = UP9CaptionSubsystem::Get(World); C && Args.Num() > 0) C->OnSoundName(FName(*Args[0]));
		}));
	static FAutoConsoleCommandWithWorld StatusCommand(TEXT("p9.Captions.Status"), TEXT("p9.Captions.Status - active captions and the letterbox"),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (UP9CaptionSubsystem* C = UP9CaptionSubsystem::Get(World)) UE_LOG(LogP9, Log, TEXT("p9.Captions.Status: %s"), *C->Describe());
		}));
}

UP9CaptionSubsystem* UP9CaptionSubsystem::Get(const UObject* WorldContext)
{
	UWorld* W = WorldContext ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return W ? W->GetSubsystem<UP9CaptionSubsystem>() : nullptr;
}

void UP9CaptionSubsystem::Deinitialize()
{
	RemoveWidget();
	Super::Deinitialize();
}

void UP9CaptionSubsystem::OnSound(const USoundBase* Sound, float Delay)
{
	if (Sound) OnSoundName(Sound->GetFName(), Delay);
}

void UP9CaptionSubsystem::OnSoundName(FName SoundName, float Delay)
{
	if (!Table) Table = LoadObject<UDataTable>(nullptr, P9Captions::TablePath);
	if (!Table) return;
	FActive A;
	A.Sound = SoundName;
	A.StartTime = FPlatformTime::Seconds() + Delay;
	Table->ForeachRow<FP9CaptionRow>(TEXT("P9Captions"), [&](const FName&, const FP9CaptionRow& R) { if (R.SoundName == SoundName) A.Rows.Add(R); });
	if (A.Rows.IsEmpty()) return;
	A.Rows.Sort([](const FP9CaptionRow& X, const FP9CaptionRow& Y) { return X.Order < Y.Order; });
	Active.RemoveAll([&](const FActive& E) { return E.Sound == SoundName; });          // a replayed sound restarts its captions
	Active.Add(MoveTemp(A));
	++Started;
	EnsureWidget();
}

void UP9CaptionSubsystem::StopSound(FName SoundName)
{
	Active.RemoveAll([&](const FActive& E) { return E.Sound == SoundName; });
}

bool UP9CaptionSubsystem::IsStoryMoment() const
{
	// the player is held in a story moment: seated in the opening / epilogue train while the scene plays (no control yet)
	const UWorld* W = GetWorld();
	const APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
	const APawn* P = PC ? PC->GetPawn() : nullptr;
	if (!P || !UP9SeatComponent::GetSeatOf(P)) return false;
	for (TActorIterator<AP9CampaignDirector> It(W); It; ++It)
	{
		// ... until the train doors open: from then on the player has control again (the bars ease out)
		const EP9DayPhase Ph = It->GetPhase();
		const bool bDoorsOpen = It->OpeningTrain && It->OpeningTrain->GetDoorState() == EP9DoorState::Open;
		return !bDoorsOpen && (Ph == EP9DayPhase::Opening || (Ph == EP9DayPhase::Epilogue && It->GetEpilogueStep() == 0));
	}
	return false;
}

FText UP9CaptionSubsystem::GetVisibleText(float& OutAlpha) const
{
	OutAlpha = Alpha;
	return Shown;
}

void UP9CaptionSubsystem::Tick(float DeltaTime)
{
	const double Now = FPlatformTime::Seconds();
	const float Dt = FMath::Min(DeltaTime, 0.1f);
	// the line to show: the most recently started sound's current row
	FText Want;
	for (int32 i = Active.Num() - 1; i >= 0; --i)
	{
		const FActive& A = Active[i];
		const float T = static_cast<float>(Now - A.StartTime);
		const float Last = A.Rows.Last().End;
		if (T > FMath::Max(Last, A.Rows.Last().Start + P9Captions::MinRead) + 0.3f) { Active.RemoveAt(i); continue; }
		if (!Want.IsEmpty()) continue;
		for (int32 r = 0; r < A.Rows.Num(); ++r)
		{
			const FP9CaptionRow& R = A.Rows[r];
			float End = FMath::Max(R.End, R.Start + P9Captions::MinRead);
			if (A.Rows.IsValidIndex(r + 1)) End = FMath::Min(End, A.Rows[r + 1].Start);
			if (T >= R.Start && T < End) { Want = FText::FromString(R.Text); break; }
		}
	}
	const UP9SettingsSubsystem* S = UP9SettingsSubsystem::Get(GetWorld());
	if (S && !S->CaptionsEnabled()) Want = FText::GetEmpty();
	if (!Want.IsEmpty())
	{
		if (!Want.EqualTo(Shown) && Alpha > 0.05f && !Shown.IsEmpty())
		{
			Shown = Want;                                                  // the next sentence replaces the last one directly
		}
		Shown = Want;
		Alpha = FMath::Min(1.0f, Alpha + P9Captions::FadeRate * Dt);
	}
	else
	{
		Alpha = FMath::Max(0.0f, Alpha - P9Captions::FadeRate * Dt);
		if (Alpha <= 0.0f) Shown = FText::GetEmpty();
	}
	const float LTarget = IsStoryMoment() ? 1.0f : 0.0f;
	Letterbox = FMath::FInterpConstantTo(Letterbox, LTarget, Dt, 0.6f);         // ~1.7 s ease in / out
	if (Alpha > 0.0f || Letterbox > 0.0f || !Active.IsEmpty()) EnsureWidget();
}

void UP9CaptionSubsystem::EnsureWidget()
{
	if (Widget.IsValid()) return;
	UWorld* W = GetWorld();
	UGameViewportClient* Viewport = W ? W->GetGameViewport() : nullptr;
	if (!Viewport) return;
	TWeakObjectPtr<UP9CaptionSubsystem> Weak(this);
	static const FSlateRoundedBoxBrush Back(FLinearColor(0.0f, 0.0f, 0.0f, 1.0f), 10.0f);
	static const FSlateBrush* White = FCoreStyle::Get().GetBrush("WhiteBrush");
	const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", 24);
	auto A = [Weak]() { return Weak.IsValid() ? Weak->Alpha : 0.0f; };
	auto L = [Weak]() { return Weak.IsValid() ? Weak->Letterbox : 0.0f; };
	Widget = SNew(SOverlay)
		.Visibility(EVisibility::HitTestInvisible)
		// letterbox (cinematic bars), eased
		+ SOverlay::Slot()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().FillHeight(TAttribute<float>::CreateLambda([L]() { return 0.0001f + 0.085f * FMath::SmoothStep(0.0f, 1.0f, L()); }))
			[
				SNew(SBorder).BorderImage(White).BorderBackgroundColor(FLinearColor::Black)
			]
			+ SVerticalBox::Slot().FillHeight(1.0f) [ SNullWidget::NullWidget ]
			+ SVerticalBox::Slot().FillHeight(TAttribute<float>::CreateLambda([L]() { return 0.0001f + 0.085f * FMath::SmoothStep(0.0f, 1.0f, L()); }))
			[
				SNew(SBorder).BorderImage(White).BorderBackgroundColor(FLinearColor::Black)
			]
		]
		// the caption, bottom centre, inside the safe zone and above the touch controls' bottom edge
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom)
		[
			SNew(SSafeZone)
			.Padding(FMargin(24.0f, 0.0f, 24.0f, 0.0f))
			[
				SNew(SBox)
				.MaxDesiredWidth(1200.0f)
				.Padding(TAttribute<FMargin>::CreateLambda([L]() { return FMargin(0.0f, 0.0f, 0.0f, 265.0f + 20.0f * FMath::SmoothStep(0.0f, 1.0f, L())); }))   // above the interaction prompt and the touch controls
				[
					SNew(SBorder)
					.BorderImage(&Back)
					.Padding(FMargin(26.0f, 12.0f))
					.BorderBackgroundColor_Lambda([A]() { return FLinearColor(0.0f, 0.0f, 0.0f, 0.58f * A()); })
					.Visibility_Lambda([A]() { return A() > 0.001f ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
					[
						SNew(STextBlock)
						.Font(Font)
						.Justification(ETextJustify::Center)
						.WrapTextAt(1080.0f)                                   // fixed wrap width (auto-wrap collapses in a centred box)
						.LineHeightPercentage(1.08f)
						.ShadowOffset(FVector2D(1.5f, 1.5f))
						.ShadowColorAndOpacity_Lambda([A]() { return FLinearColor(0.0f, 0.0f, 0.0f, 0.9f * A()); })
						.ColorAndOpacity_Lambda([A]() { return FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f, A())); })
						.Text_Lambda([Weak]() { return Weak.IsValid() ? Weak->Shown : FText::GetEmpty(); })
					]
				]
			]
		];
	Viewport->AddViewportWidgetContent(Widget.ToSharedRef(), P9Captions::ZOrder);
}

void UP9CaptionSubsystem::RemoveWidget()
{
	if (!Widget.IsValid()) return;
	if (UWorld* W = GetWorld())
	{
		if (UGameViewportClient* Viewport = W->GetGameViewport()) Viewport->RemoveViewportWidgetContent(Widget.ToSharedRef());
	}
	Widget.Reset();
}

FString UP9CaptionSubsystem::Describe() const
{
	FString S = FString::Printf(TEXT("table %s, started %d, active %d, alpha %.2f, letterbox %.2f, shown '%s'"), Table ? *Table->GetName() : TEXT("none"), Started, Active.Num(),
		Alpha, Letterbox, *Shown.ToString().Replace(TEXT("\n"), TEXT(" / ")));
	for (const FActive& A : Active) S += FString::Printf(TEXT(" [%s +%.1fs]"), *A.Sound.ToString(), static_cast<float>(FPlatformTime::Seconds() - A.StartTime));
	return S;
}
