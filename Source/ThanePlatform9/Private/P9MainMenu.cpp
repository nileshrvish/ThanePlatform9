#include "P9MainMenu.h"
#include "ThanePlatform9.h"
#include "P9AudioDirector.h"
#include "P9CampaignDirector.h"
#include "P9CampaignSave.h"
#include "P9Captions.h"
#include "P9GameSettings.h"
#include "P9MobileControlsWidget.h"
#include "Blueprint/UserWidget.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Engine/DataTable.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Sound/SoundBase.h"
#include "Styling/CoreStyle.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSafeZone.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "P9Menu"

namespace P9Menu
{
	constexpr int32 ZOrder = 100;
	enum EScreen : int32 { Main = 0, HowTo = 1, Settings = 2, Confirm = 3 };
	// colours: night-station blue-black, railway-sign red, signboard yellow, warm off-white text
	const FLinearColor Ink(0.012f, 0.016f, 0.03f, 1.0f);
	const FLinearColor Red(0.62f, 0.05f, 0.04f, 1.0f);
	const FLinearColor Yellow(0.98f, 0.76f, 0.05f, 1.0f);
	const FLinearColor Paper(0.95f, 0.93f, 0.88f, 1.0f);
	const FLinearColor Dim(0.72f, 0.70f, 0.66f, 1.0f);

	FSlateFontInfo Font(const char* Style, int32 Size, int32 Spacing = 0)
	{
		FSlateFontInfo F = FCoreStyle::GetDefaultFontStyle(Style, Size);
		F.LetterSpacing = Spacing;
		return F;
	}
	const FButtonStyle& MenuButton()
	{
		static FButtonStyle S = []()
		{
			FButtonStyle B = FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button");
			B.SetNormal(FSlateRoundedBoxBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.35f), 6.0f, FLinearColor(1.0f, 1.0f, 1.0f, 0.12f), 1.0f));
			B.SetHovered(FSlateRoundedBoxBrush(FLinearColor(Red.R, Red.G, Red.B, 0.55f), 6.0f, FLinearColor(1.0f, 0.85f, 0.8f, 0.5f), 1.0f));
			B.SetPressed(FSlateRoundedBoxBrush(FLinearColor(Red.R * 0.7f, Red.G, Red.B, 0.85f), 6.0f, FLinearColor(1.0f, 0.85f, 0.8f, 0.7f), 1.5f));
			B.SetDisabled(FSlateRoundedBoxBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.2f), 6.0f));
			B.SetNormalPadding(FMargin(0.0f)); B.SetPressedPadding(FMargin(0.0f));
			return B;
		}();
		return S;
	}
	const FSliderStyle& TouchSlider()
	{
		static FSliderStyle S = []()
		{
			FSliderStyle St = FCoreStyle::Get().GetWidgetStyle<FSliderStyle>("Slider");
			St.SetNormalBarImage(FSlateRoundedBoxBrush(FLinearColor(1.0f, 1.0f, 1.0f, 0.25f), 5.0f));
			St.SetHoveredBarImage(FSlateRoundedBoxBrush(FLinearColor(1.0f, 1.0f, 1.0f, 0.35f), 5.0f));
			St.SetNormalThumbImage(FSlateRoundedBoxBrush(Yellow, 20.0f, FVector2f(40.0f, 40.0f)));
			St.SetHoveredThumbImage(FSlateRoundedBoxBrush(FLinearColor(1.0f, 0.86f, 0.3f, 1.0f), 20.0f, FVector2f(40.0f, 40.0f)));
			St.SetBarThickness(10.0f);
			return St;
		}();
		return S;
	}
	static FSlateRoundedBoxBrush Rounded(const FLinearColor& C, float R = 8.0f) { return FSlateRoundedBoxBrush(C, R); }

	AP9MainMenu* Find(UWorld* World)
	{
		for (TActorIterator<AP9MainMenu> It(World); It; ++It) return *It;
		return nullptr;
	}
	static FAutoConsoleCommandWithWorld StatusCommand(TEXT("p9.Menu.Status"), TEXT("p9.Menu.Status - main menu state"),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World) { if (AP9MainMenu* M = Find(World)) UE_LOG(LogP9, Log, TEXT("p9.Menu.Status: %s"), *M->Describe()); }));
	static FAutoConsoleCommandWithWorld ShowCommand(TEXT("p9.Menu.Show"), TEXT("p9.Menu.Show - show the main menu (test; only before the campaign has started)"),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World) { if (AP9MainMenu* M = Find(World)) M->ShowMenu(); }));
	static FAutoConsoleCommandWithWorldAndArgs StartCommand(TEXT("p9.Menu.Start"), TEXT("p9.Menu.Start new|continue - press New Game / Continue (test)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (AP9MainMenu* M = Find(World)) M->StartGame(Args.Num() > 0 && Args[0].Equals(TEXT("new"), ESearchCase::IgnoreCase));
		}));
	static FAutoConsoleCommandWithWorldAndArgs ScreenCommand(TEXT("p9.Menu.Screen"), TEXT("p9.Menu.Screen main|howto|settings|confirm (test)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (AP9MainMenu* M = Find(World); M && Args.Num() > 0)
			{
				const FString S = Args[0].ToLower();
				M->ShowScreen(S == TEXT("howto") ? HowTo : S == TEXT("settings") ? Settings : S == TEXT("confirm") ? Confirm : Main);
			}
		}));
}

AP9MainMenu::AP9MainMenu()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	MenuCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("MenuCamera"));
	MenuCamera->SetupAttachment(Root);
	MenuCamera->SetFieldOfView(70.0f);
	MenuCamera->bConstrainAspectRatio = false;
	SetHidden(true);
}

bool AP9MainMenu::HoldsCampaign(const UWorld* World)
{
	for (TActorIterator<AP9MainMenu> It(const_cast<UWorld*>(World)); It; ++It)
	{
		if (It->bHolding) return true;
	}
	return false;
}

AP9CampaignDirector* AP9MainMenu::FindDirector() const
{
	for (TActorIterator<AP9CampaignDirector> It(GetWorld()); It; ++It) return *It;
	return nullptr;
}

bool AP9MainMenu::HasValidSave() const
{
	const AP9CampaignDirector* D = FindDirector();
	const FString Slot = D ? D->SaveSlotName : TEXT("P9Campaign");
	if (!UGameplayStatics::DoesSaveGameExist(Slot, 0)) return false;
	const UP9CampaignSave* S = Cast<UP9CampaignSave>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
	return S && S->Day >= 1 && S->Day <= 10 && S->RunSeed != 0;
}

void AP9MainMenu::BeginPlay()
{
	Super::BeginPlay();
	if (UP9SettingsSubsystem* S = UP9SettingsSubsystem::Get(this)) S->ApplyAudio(this);
	if (UP9CaptionSubsystem* C = UP9CaptionSubsystem::Get(this); C && CaptionTable) C->SetTable(CaptionTable);
	if (bShowOnStart) ShowMenu();
}

void AP9MainMenu::EndPlay(const EEndPlayReason::Type Reason)
{
	RemoveWidget();
	Super::EndPlay(Reason);
}

void AP9MainMenu::ShowMenu()
{
	const AP9CampaignDirector* D = FindDirector();
	if (D && D->GetPhase() != EP9DayPhase::Inactive)
	{
		UE_LOG(LogP9, Warning, TEXT("Menu: the campaign is already running - the menu is only shown at launch"));
		return;
	}
	bHolding = true;
	bBusy = false;
	bHaveSave = HasValidSave();
	MenuTime = 0.0f;
	Fade = 1.0f;                                             // the menu comes out of black
	FadeTarget = 0.0f;
	for (TActorIterator<AP9AudioDirector> It(GetWorld()); It; ++It) It->bEnabled = false;    // station layers wait for the game
	if (MenuMusic)
	{
		if (!MusicComp)
		{
			MusicComp = NewObject<UAudioComponent>(this, TEXT("MenuMusic"), RF_Transient);
			MusicComp->SetupAttachment(Root);
			MusicComp->bAllowSpatialization = false;
			MusicComp->bIsUISound = true;                       // keeps playing if the game is paused
			MusicComp->RegisterComponent();
		}
		MusicComp->SetSound(MenuMusic);
		MusicComp->FadeIn(2.5f, MusicVolume);
	}
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		PC->SetViewTargetWithBlend(this, 0.0f);
		PC->bShowMouseCursor = true;
	}
	if (!Widget.IsValid())
	{
		Widget = BuildWidget();
		if (UGameViewportClient* V = GetWorld()->GetGameViewport()) V->AddViewportWidgetContent(Widget.ToSharedRef(), P9Menu::ZOrder);
	}
	ShowScreen(P9Menu::Main);
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		PC->SetInputMode(FInputModeUIOnly());               // touch / mouse on the menu only (no widget focus needed)
	}
	SetTouchControlsHidden(true);
	UE_LOG(LogP9, Log, TEXT("Menu: shown (save %s) - the campaign waits"), bHaveSave ? TEXT("found") : TEXT("none"));
}

void AP9MainMenu::SetTouchControlsHidden(bool bHide)
{
	if (bHide)
	{
		for (TObjectIterator<UP9MobileControlsWidget> It; It; ++It)
		{
			if (It->GetWorld() != GetWorld() || HiddenTouch.Contains(*It)) continue;
			HiddenTouch.Add(*It, static_cast<uint8>(It->GetVisibility()));
			It->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}
	for (const TPair<TWeakObjectPtr<UUserWidget>, uint8>& P : HiddenTouch)
	{
		if (UUserWidget* W = P.Key.Get()) W->SetVisibility(static_cast<ESlateVisibility>(P.Value));
	}
	HiddenTouch.Reset();
}

void AP9MainMenu::Click(USoundBase* Sound)
{
	if (Sound) UGameplayStatics::PlaySound2D(this, Sound, 0.55f, 1.0f, 0.0f, nullptr, nullptr, false);
}

void AP9MainMenu::ShowScreen(int32 Index)
{
	if (Switcher.IsValid()) Switcher->SetActiveWidgetIndex(Index);
}

void AP9MainMenu::OnNewGamePressed()
{
	if (bBusy) return;
	Click(ConfirmSound);
	if (bHaveSave) { ShowScreen(P9Menu::Confirm); return; }       // never overwrite progress without asking
	const UP9SettingsSubsystem* S = UP9SettingsSubsystem::Get(this);
	if (S && !S->HasSeenHowToPlay()) { bHowToFromNewGame = true; ShowScreen(P9Menu::HowTo); return; }
	StartGame(true);
}

void AP9MainMenu::StartGame(bool bNewGame)
{
	if (bBusy || !bHolding) return;
	bBusy = true;                                            // no second press while the transition runs
	bPendingNew = bNewGame;
	FadeTarget = 1.0f;
	StartCountdown = 0.9f;
	if (MusicComp) MusicComp->FadeOut(1.8f, 0.0f);
	if (UP9SettingsSubsystem* S = UP9SettingsSubsystem::Get(this)) { if (bNewGame) S->MarkHowToPlaySeen(); S->Save(); }
	UE_LOG(LogP9, Log, TEXT("Menu: %s"), bNewGame ? TEXT("NEW GAME") : TEXT("CONTINUE"));
}

void AP9MainMenu::FinishStart()
{
	AP9CampaignDirector* D = FindDirector();
	if (bPendingNew && D && D->bUseSaveGame)
	{
		D->bUseSaveGame = false;                             // this first start ignores the old save -> fresh run with the opening
		bRestoreSaveSwitch = true;
	}
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (APawn* P = PC->GetPawn()) PC->SetViewTargetWithBlend(P, 0.0f);   // under black: no visible cut
		PC->SetInputMode(FInputModeGameOnly());
		PC->bShowMouseCursor = false;
	}
	SetTouchControlsHidden(false);
	for (TActorIterator<AP9AudioDirector> It(GetWorld()); It; ++It) It->bEnabled = true;     // station layers fade in from silence
	bHolding = false;
	FadeTarget = 0.0f;
	UE_LOG(LogP9, Log, TEXT("Menu: campaign released (%s)"), bPendingNew ? TEXT("new run") : TEXT("resume"));
}

void AP9MainMenu::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Dt = FMath::Min(DeltaSeconds, 0.1f);
	MenuTime += Dt;
	Fade = FMath::FInterpConstantTo(Fade, FadeTarget, Dt, 1.0f / 0.8f);
	if (bHolding)
	{
		// the background: a slow drift along Platform 9 and a gentle sway (no cuts)
		const float A = 0.5f - 0.5f * FMath::Cos(2.0f * PI * MenuTime / FMath::Max(10.0f, CameraPeriod));
		const FVector L = FMath::Lerp(CameraFrom, CameraTo, A) + FVector(0.0f, 0.0f, 4.0f * FMath::Sin(MenuTime * 0.21f));
		FRotator R = CameraRotation;
		R.Yaw += 2.5f * FMath::Sin(MenuTime * 0.09f);
		R.Pitch += 0.8f * FMath::Sin(MenuTime * 0.13f + 1.0f);
		MenuCamera->SetWorldLocationAndRotation(L, R);
		SetTouchControlsHidden(true);                          // touch widgets created after the menu are hidden too
	}
	if (StartCountdown > 0.0f && (StartCountdown -= Dt) <= 0.0f)
	{
		StartCountdown = -1.0f;
		FinishStart();
	}
	if (bRestoreSaveSwitch)
	{
		AP9CampaignDirector* D = FindDirector();
		if (!D || D->GetPhase() != EP9DayPhase::Inactive)
		{
			if (D) D->bUseSaveGame = true;                   // saving works as before for the new run
			bRestoreSaveSwitch = false;
			UE_LOG(LogP9, Log, TEXT("Menu: new run started (phase %d) - saving enabled"), D ? (int32)D->GetPhase() : -1);
		}
	}
	if (!bHolding && Fade <= 0.0f && Widget.IsValid()) RemoveWidget();
}

void AP9MainMenu::RemoveWidget()
{
	if (!Widget.IsValid()) return;
	if (UWorld* W = GetWorld())
	{
		if (UGameViewportClient* V = W->GetGameViewport()) V->RemoveViewportWidgetContent(Widget.ToSharedRef());
	}
	Widget.Reset();
	Switcher.Reset();
}

FString AP9MainMenu::Describe() const
{
	return FString::Printf(TEXT("holding %d busy %d save %d screen %d fade %.2f widget %d musicPlaying %d"), bHolding ? 1 : 0, bBusy ? 1 : 0, bHaveSave ? 1 : 0,
		Switcher.IsValid() ? Switcher->GetActiveWidgetIndex() : -1, Fade, Widget.IsValid() ? 1 : 0, MusicComp && MusicComp->IsPlaying() ? 1 : 0);
}

// =====================================================================================================================
// widgets (Slate, built in code; every screen sits in a scale box so it fits any landscape screen, inside the safe zone)
// =====================================================================================================================
TSharedRef<SWidget> AP9MainMenu::BuildWidget()
{
	TWeakObjectPtr<AP9MainMenu> Weak(this);
	static const FSlateBrush* White = FCoreStyle::Get().GetBrush("WhiteBrush");
	return SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SSafeZone)
			[
				SAssignNew(Switcher, SWidgetSwitcher)
				.Visibility_Lambda([Weak]() { return !Weak.IsValid() || !Weak->bHolding ? EVisibility::Collapsed : (Weak->bBusy ? EVisibility::HitTestInvisible : EVisibility::Visible); })
				+ SWidgetSwitcher::Slot()[BuildMain()]
				+ SWidgetSwitcher::Slot()[BuildHowTo()]
				+ SWidgetSwitcher::Slot()[BuildSettings()]
				+ SWidgetSwitcher::Slot()[BuildConfirm()]
			]
		]
		// black transition overlay
		+ SOverlay::Slot()
		[
			SNew(SBorder)
			.BorderImage(White)
			.Visibility(EVisibility::HitTestInvisible)
			.BorderBackgroundColor_Lambda([Weak]() { return FLinearColor(0.0f, 0.0f, 0.0f, Weak.IsValid() ? Weak->Fade : 0.0f); })
		];
}

TSharedRef<SWidget> AP9MainMenu::BuildMain()
{
	using namespace P9Menu;
	TWeakObjectPtr<AP9MainMenu> Weak(this);
	static const FSlateBrush* White = FCoreStyle::Get().GetBrush("WhiteBrush");
	auto Button = [Weak](const FText& Label, TFunction<void()> OnClick, TFunction<bool()> IsShown) -> TSharedRef<SWidget>
	{
		return SNew(SBox)
			.WidthOverride(430.0f).HeightOverride(70.0f)
			.Padding(FMargin(0.0f, 5.0f))
			.Visibility_Lambda([IsShown]() { return IsShown() ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				SNew(SButton)
				.ButtonStyle(&MenuButton())
				.HAlign(HAlign_Left).VAlign(VAlign_Center)
				.IsEnabled_Lambda([Weak]() { return Weak.IsValid() && !Weak->bBusy; })
				.OnClicked_Lambda([OnClick]() { OnClick(); return FReply::Handled(); })
				[
					SNew(STextBlock).Margin(FMargin(26.0f, 0.0f, 0.0f, 0.0f)).Font(Font("Bold", 26, 160)).ColorAndOpacity(Paper).Text(Label)
				]
			];
	};
	auto Always = []() { return true; };
	return SNew(SScaleBox)
		.Stretch(EStretch::ScaleToFit)
		[
			SNew(SBox).WidthOverride(1920.0f).HeightOverride(1080.0f)
			[
				SNew(SOverlay)
				// a dark wash on the left for legibility; the station stays visible on the right
				+ SOverlay::Slot().HAlign(HAlign_Left)
				[
					SNew(SBox).WidthOverride(900.0f)
					[
						SNew(SBorder).BorderImage(White).BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.02f, 0.62f))
					]
				]
				+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center).Padding(FMargin(120.0f, 0.0f, 0.0f, 0.0f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).Font(Font("Regular", 22, 600)).ColorAndOpacity(Yellow).Text(LOCTEXT("Kicker", "THANE  ·  23:47"))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 6.0f, 0.0f, 0.0f))
					[
						SNew(STextBlock).Font(Font("Bold", 84, 60)).ColorAndOpacity(Paper).Text(LOCTEXT("Title", "PLATFORM No. 9"))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f, 0.0f, 0.0f))
					[
						SNew(SBox).WidthOverride(520.0f).HeightOverride(6.0f)
						[
							SNew(SBorder).BorderImage(White).BorderBackgroundColor(Red)
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 14.0f, 0.0f, 46.0f))
					[
						SNew(STextBlock).Font(Font("Italic", 22)).ColorAndOpacity(Dim).Text(LOCTEXT("Tag", "The last local of the night. Something is not right."))
					]
					+ SVerticalBox::Slot().AutoHeight()[Button(LOCTEXT("Continue", "CONTINUE"), [Weak]() { if (Weak.IsValid()) { Weak->Click(Weak->ConfirmSound); Weak->StartGame(false); } },
						[Weak]() { return Weak.IsValid() && Weak->bHaveSave; })]
					+ SVerticalBox::Slot().AutoHeight()[Button(LOCTEXT("New", "NEW GAME"), [Weak]() { if (Weak.IsValid()) Weak->OnNewGamePressed(); }, Always)]
					+ SVerticalBox::Slot().AutoHeight()[Button(LOCTEXT("HowTo", "HOW TO PLAY"), [Weak]() { if (Weak.IsValid()) { Weak->Click(Weak->ClickSound); Weak->bHowToFromNewGame = false; Weak->ShowScreen(HowTo); } }, Always)]
					+ SVerticalBox::Slot().AutoHeight()[Button(LOCTEXT("Settings", "SETTINGS"), [Weak]() { if (Weak.IsValid()) { Weak->Click(Weak->ClickSound); Weak->ShowScreen(Settings); } }, Always)]
#if !PLATFORM_IOS
					+ SVerticalBox::Slot().AutoHeight()[Button(LOCTEXT("Quit", "QUIT GAME"), [Weak]()
						{
							if (!Weak.IsValid() || Weak->bBusy) return;
							Weak->Click(Weak->BackSound);
							UKismetSystemLibrary::QuitGame(Weak.Get(), Weak->GetWorld()->GetFirstPlayerController(), EQuitPreference::Quit, false);
						}, Always)]
#endif
				]
				+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(FMargin(0.0f, 0.0f, 70.0f, 50.0f))
				[
					SNew(STextBlock).Font(Font("Regular", 18, 200)).ColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.55f)).Text(LOCTEXT("Phones", "HEADPHONES RECOMMENDED"))
				]
			]
		];
}

TSharedRef<SWidget> AP9MainMenu::BuildHowTo()
{
	using namespace P9Menu;
	TWeakObjectPtr<AP9MainMenu> Weak(this);
	static const FSlateBrush* White = FCoreStyle::Get().GetBrush("WhiteBrush");
	static const FSlateRoundedBoxBrush Board(FLinearColor(0.015f, 0.03f, 0.075f, 0.96f), 12.0f, FLinearColor(0.85f, 0.85f, 0.85f, 0.35f), 3.0f);
	static const FSlateRoundedBoxBrush Circle(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f), 24.0f, FLinearColor(1.0f, 1.0f, 1.0f, 0.9f), 3.0f);
	static const FSlateRoundedBoxBrush Dot(FLinearColor(1.0f, 1.0f, 1.0f, 0.9f), 9.0f);
	static const FSlateRoundedBoxBrush Square(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f), 8.0f, FLinearColor(1.0f, 1.0f, 1.0f, 0.9f), 3.0f);
	auto Heading = [](const FText& T) -> TSharedRef<SWidget>
	{
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Font(Font("Bold", 26, 200)).ColorAndOpacity(Yellow).Text(T)]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f, 0.0f, 10.0f))
			[
				SNew(SBox).HeightOverride(3.0f).WidthOverride(90.0f)[SNew(SBorder).BorderImage(White).BorderBackgroundColor(Yellow)]
			];
	};
	auto Lines = [](std::initializer_list<FText> L) -> TSharedRef<SWidget>
	{
		TSharedRef<SVerticalBox> B = SNew(SVerticalBox);
		for (const FText& T : L)
		{
			B->AddSlot().AutoHeight().Padding(FMargin(0.0f, 3.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.0f, 0.0f, 12.0f, 0.0f))[SNew(STextBlock).Font(Font("Bold", 22)).ColorAndOpacity(Yellow).Text(FText::FromString(TEXT("•")))]
				+ SHorizontalBox::Slot().FillWidth(1.0f)[SNew(STextBlock).Font(Font("Regular", 22)).ColorAndOpacity(Paper).AutoWrapText(true).Text(T)]
			];
		}
		return B;
	};
	auto Icon = [](int32 Kind) -> TSharedRef<SWidget>
	{
		// 0 joystick (ring + knob), 1 look (drag arrows in a square), 2 jump (arrow in a ring)
		TSharedRef<SOverlay> O = SNew(SOverlay);
		O->AddSlot()[SNew(SBorder).BorderImage(Kind == 1 ? &Square : &Circle)];
		if (Kind == 0) O->AddSlot().HAlign(HAlign_Center).VAlign(VAlign_Center)[SNew(SBox).WidthOverride(18.0f).HeightOverride(18.0f)[SNew(SBorder).BorderImage(&Dot)]];
		else O->AddSlot().HAlign(HAlign_Center).VAlign(VAlign_Center)[SNew(STextBlock).Font(Font("Bold", 24)).ColorAndOpacity(FLinearColor::White)
			.Text(FText::FromString(Kind == 1 ? TEXT("↔") : TEXT("↑")))];
		return SNew(SBox).WidthOverride(48.0f).HeightOverride(48.0f)[O];
	};
	auto Control = [Icon](int32 Kind, const FText& T) -> TSharedRef<SWidget>
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.0f, 4.0f, 16.0f, 4.0f))[Icon(Kind)]
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)[SNew(STextBlock).Font(Font("Regular", 22)).ColorAndOpacity(Paper).AutoWrapText(true).Text(T)];
	};
	auto Btn = [Weak](const FText& Label, TFunction<void()> OnClick, TFunction<EVisibility()> Vis) -> TSharedRef<SWidget>
	{
		return SNew(SBox).WidthOverride(300.0f).HeightOverride(66.0f).Padding(FMargin(10.0f, 0.0f)).Visibility(TAttribute<EVisibility>::CreateLambda([Vis]() { return Vis(); }))
			[
				SNew(SButton).ButtonStyle(&MenuButton()).HAlign(HAlign_Center).VAlign(VAlign_Center)
				.IsEnabled_Lambda([Weak]() { return Weak.IsValid() && !Weak->bBusy; })
				.OnClicked_Lambda([OnClick]() { OnClick(); return FReply::Handled(); })
				[SNew(STextBlock).Font(Font("Bold", 24, 160)).ColorAndOpacity(Paper).Text(Label)]
			];
	};
	return SNew(SScaleBox)
		.Stretch(EStretch::ScaleToFit)
		[
			SNew(SBox).WidthOverride(1920.0f).HeightOverride(1080.0f).Padding(FMargin(110.0f, 60.0f))
			[
				SNew(SBorder).BorderImage(&Board).Padding(0.0f)
				[
					SNew(SVerticalBox)
					// signboard header: black on yellow, like a station information board
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SBorder).BorderImage(White).BorderBackgroundColor(Yellow).Padding(FMargin(40.0f, 14.0f))
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1.0f)[SNew(STextBlock).Font(Font("Bold", 40, 300)).ColorAndOpacity(FLinearColor(0.02f, 0.02f, 0.02f, 1.0f)).Text(LOCTEXT("HowTitle", "HOW TO PLAY"))]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(STextBlock).Font(Font("Bold", 22, 200)).ColorAndOpacity(FLinearColor(0.02f, 0.02f, 0.02f, 1.0f)).Text(LOCTEXT("HowPF", "PLATFORM No. 9"))]
						]
					]
					+ SVerticalBox::Slot().FillHeight(1.0f).Padding(FMargin(46.0f, 28.0f, 46.0f, 10.0f))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(FMargin(0.0f, 0.0f, 40.0f, 0.0f))
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()[Heading(LOCTEXT("H1", "YOUR JOURNEY"))]
							+ SVerticalBox::Slot().AutoHeight()[Lines({LOCTEXT("J1", "You have 10 days to complete your journey."),
								LOCTEXT("J2", "Your destination is Platform 9."), LOCTEXT("J3", "Pay attention to your surroundings.")})]
							+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 22.0f, 0.0f, 0.0f))[Heading(LOCTEXT("H2", "OBSERVE CAREFULLY"))]
							+ SVerticalBox::Slot().AutoHeight()[Lines({LOCTEXT("O1", "Look for anything unusual."), LOCTEXT("O2", "Watch the signs, clocks, lights and trains."),
								LOCTEXT("O3", "Listen carefully to the station."), LOCTEXT("O4", "Some things may not be what they seem.")})]
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()[Heading(LOCTEXT("H3", "MAKE YOUR DECISION"))]
							+ SVerticalBox::Slot().AutoHeight()[Lines({LOCTEXT("D1", "Everything looks normal? Continue to Platform 9 and board the train."),
								LOCTEXT("D2", "Noticed something unusual? Return to Platform 1.")})]
							+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 22.0f, 0.0f, 0.0f))[Heading(LOCTEXT("H4", "CONTROLS"))]
							+ SVerticalBox::Slot().AutoHeight()[Control(0, LOCTEXT("C1", "Use the left joystick to move."))]
							+ SVerticalBox::Slot().AutoHeight()[Control(1, LOCTEXT("C2", "Drag on the right side of the screen to look around."))]
							+ SVerticalBox::Slot().AutoHeight()[Control(2, LOCTEXT("C3", "Tap the Jump button when needed. Climbing ledges is automatic."))]
						]
					]
					// important: a red band
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(46.0f, 6.0f, 46.0f, 0.0f))
					[
						SNew(SBorder).BorderImage(White).BorderBackgroundColor(FLinearColor(Red.R, Red.G, Red.B, 0.9f)).Padding(FMargin(26.0f, 12.0f))
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Font(Font("Bold", 22, 200)).ColorAndOpacity(FLinearColor::White).Text(LOCTEXT("Imp", "IMPORTANT"))]
							+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Font(Font("Regular", 21)).ColorAndOpacity(FLinearColor::White).AutoWrapText(true)
								.Text(LOCTEXT("ImpText", "A wrong decision sends you back to Day 1.  Complete all 10 days to discover the ending.  Stay alert. Something may be following you."))]
						]
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.0f, 18.0f, 0.0f, 22.0f))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth()[Btn(LOCTEXT("Back", "BACK"), [Weak]() { if (Weak.IsValid()) { Weak->Click(Weak->BackSound); Weak->ShowScreen(Main); } },
							[]() { return EVisibility::Visible; })]
						+ SHorizontalBox::Slot().AutoWidth()[Btn(LOCTEXT("Start", "START"), [Weak]() { if (Weak.IsValid()) { Weak->Click(Weak->ConfirmSound); Weak->StartGame(true); } },
							[Weak]() { return Weak.IsValid() && (Weak->bHowToFromNewGame || !Weak->bHaveSave) ? EVisibility::Visible : EVisibility::Collapsed; })]
						+ SHorizontalBox::Slot().AutoWidth()[Btn(LOCTEXT("Cont2", "CONTINUE"), [Weak]() { if (Weak.IsValid()) { Weak->Click(Weak->ConfirmSound); Weak->StartGame(false); } },
							[Weak]() { return Weak.IsValid() && !Weak->bHowToFromNewGame && Weak->bHaveSave ? EVisibility::Visible : EVisibility::Collapsed; })]
					]
				]
			]
		];
}

TSharedRef<SWidget> AP9MainMenu::BuildSettings()
{
	using namespace P9Menu;
	TWeakObjectPtr<AP9MainMenu> Weak(this);
	static const FSlateRoundedBoxBrush Panel(FLinearColor(0.015f, 0.03f, 0.075f, 0.95f), 12.0f, FLinearColor(0.85f, 0.85f, 0.85f, 0.3f), 3.0f);
	static const FSlateBrush* White = FCoreStyle::Get().GetBrush("WhiteBrush");
	auto S = [Weak]() { return Weak.IsValid() ? UP9SettingsSubsystem::Get(Weak.Get()) : nullptr; };
	auto Row = [S](const FText& Label, TFunction<float(UP9SettingsSubsystem*)> Get, TFunction<void(UP9SettingsSubsystem*, float)> Set) -> TSharedRef<SWidget>
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).WidthOverride(330.0f)[SNew(STextBlock).Font(Font("Bold", 24, 120)).ColorAndOpacity(Paper).Text(Label)]]
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				SNew(SBox).HeightOverride(64.0f)
				[
					SNew(SSlider)
					.Style(&TouchSlider())
					.IndentHandle(true)
					.Value_Lambda([S, Get]() { UP9SettingsSubsystem* X = S(); return X ? Get(X) : 0.0f; })
					.OnValueChanged_Lambda([S, Set](float V) { if (UP9SettingsSubsystem* X = S()) Set(X, V); })
					.OnMouseCaptureEnd_Lambda([S]() { if (UP9SettingsSubsystem* X = S()) X->Save(); })
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(20.0f, 0.0f, 0.0f, 0.0f))
			[
				SNew(SBox).WidthOverride(90.0f)
				[
					SNew(STextBlock).Font(Font("Regular", 24)).ColorAndOpacity(Yellow).Justification(ETextJustify::Right)
					.Text_Lambda([S, Get]() { UP9SettingsSubsystem* X = S(); return FText::FromString(FString::Printf(TEXT("%d%%"), X ? FMath::RoundToInt(Get(X) * 100.0f) : 0)); })
				]
			];
	};
	auto Toggle = [Weak, S](const FText& Label, TFunction<bool(UP9SettingsSubsystem*)> Get, TFunction<void(UP9SettingsSubsystem*, bool)> Set) -> TSharedRef<SWidget>
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).WidthOverride(330.0f)[SNew(STextBlock).Font(Font("Bold", 24, 120)).ColorAndOpacity(Paper).Text(Label)]]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(180.0f).HeightOverride(62.0f)
				[
					SNew(SButton).ButtonStyle(&MenuButton()).HAlign(HAlign_Center).VAlign(VAlign_Center)
					.OnClicked_Lambda([Weak, S, Get, Set]()
					{
						if (UP9SettingsSubsystem* X = S()) { Set(X, !Get(X)); X->Save(); }
						if (Weak.IsValid()) Weak->Click(Weak->ClickSound);
						return FReply::Handled();
					})
					[
						SNew(STextBlock).Font(Font("Bold", 24, 160))
						.ColorAndOpacity_Lambda([S, Get]() { UP9SettingsSubsystem* X = S(); return FSlateColor(X && Get(X) ? Yellow : Dim); })
						.Text_Lambda([S, Get]() { UP9SettingsSubsystem* X = S(); return X && Get(X) ? LOCTEXT("On", "ON") : LOCTEXT("Off", "OFF"); })
					]
				]
			];
	};
	return SNew(SScaleBox)
		.Stretch(EStretch::ScaleToFit)
		[
			SNew(SBox).WidthOverride(1920.0f).HeightOverride(1080.0f).HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(1180.0f)
				[
					SNew(SBorder).BorderImage(&Panel).Padding(0.0f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(SBorder).BorderImage(White).BorderBackgroundColor(Yellow).Padding(FMargin(40.0f, 12.0f))
							[SNew(STextBlock).Font(Font("Bold", 36, 300)).ColorAndOpacity(FLinearColor(0.02f, 0.02f, 0.02f, 1.0f)).Text(LOCTEXT("SetTitle", "SETTINGS"))]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(50.0f, 30.0f, 50.0f, 6.0f))[Row(LOCTEXT("Master", "MASTER"), [](UP9SettingsSubsystem* X) { return X->GetMaster(); }, [](UP9SettingsSubsystem* X, float V) { X->SetMaster(V); })]
						+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(50.0f, 6.0f))[Row(LOCTEXT("Music", "MUSIC"), [](UP9SettingsSubsystem* X) { return X->GetMusic(); }, [](UP9SettingsSubsystem* X, float V) { X->SetMusic(V); })]
						+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(50.0f, 6.0f))[Row(LOCTEXT("Effects", "SOUND EFFECTS"), [](UP9SettingsSubsystem* X) { return X->GetEffects(); }, [](UP9SettingsSubsystem* X, float V) { X->SetEffects(V); })]
						+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(50.0f, 6.0f))[Row(LOCTEXT("Amb", "AMBIENCE"), [](UP9SettingsSubsystem* X) { return X->GetAmbience(); }, [](UP9SettingsSubsystem* X, float V) { X->SetAmbience(V); })]
						+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(50.0f, 18.0f, 50.0f, 6.0f))
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1.0f)[Toggle(LOCTEXT("Mute", "MUTE ALL"), [](UP9SettingsSubsystem* X) { return X->IsMuted(); }, [](UP9SettingsSubsystem* X, bool B) { X->SetMuted(B); })]
							+ SHorizontalBox::Slot().FillWidth(1.0f)[Toggle(LOCTEXT("Captions", "CAPTIONS"), [](UP9SettingsSubsystem* X) { return X->CaptionsEnabled(); }, [](UP9SettingsSubsystem* X, bool B) { X->SetCaptions(B); })]
						]
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.0f, 24.0f, 0.0f, 30.0f))
						[
							SNew(SBox).WidthOverride(300.0f).HeightOverride(66.0f)
							[
								SNew(SButton).ButtonStyle(&MenuButton()).HAlign(HAlign_Center).VAlign(VAlign_Center)
								.OnClicked_Lambda([Weak, S]() { if (UP9SettingsSubsystem* X = S()) X->Save(); if (Weak.IsValid()) { Weak->Click(Weak->BackSound); Weak->ShowScreen(Main); } return FReply::Handled(); })
								[SNew(STextBlock).Font(Font("Bold", 24, 160)).ColorAndOpacity(Paper).Text(LOCTEXT("Back2", "BACK"))]
							]
						]
					]
				]
			]
		];
}

TSharedRef<SWidget> AP9MainMenu::BuildConfirm()
{
	using namespace P9Menu;
	TWeakObjectPtr<AP9MainMenu> Weak(this);
	static const FSlateRoundedBoxBrush Panel(FLinearColor(0.015f, 0.03f, 0.075f, 0.96f), 12.0f, FLinearColor(0.85f, 0.85f, 0.85f, 0.3f), 3.0f);
	auto Btn = [Weak](const FText& Label, TFunction<void()> OnClick) -> TSharedRef<SWidget>
	{
		return SNew(SBox).WidthOverride(300.0f).HeightOverride(66.0f).Padding(FMargin(10.0f, 0.0f))
			[
				SNew(SButton).ButtonStyle(&MenuButton()).HAlign(HAlign_Center).VAlign(VAlign_Center)
				.IsEnabled_Lambda([Weak]() { return Weak.IsValid() && !Weak->bBusy; })
				.OnClicked_Lambda([OnClick]() { OnClick(); return FReply::Handled(); })
				[SNew(STextBlock).Font(Font("Bold", 24, 160)).ColorAndOpacity(Paper).Text(Label)]
			];
	};
	return SNew(SScaleBox)
		.Stretch(EStretch::ScaleToFit)
		[
			SNew(SBox).WidthOverride(1920.0f).HeightOverride(1080.0f).HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(900.0f)
				[
					SNew(SBorder).BorderImage(&Panel).Padding(FMargin(50.0f, 40.0f))
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Font(Font("Bold", 32, 200)).ColorAndOpacity(Yellow).Text(LOCTEXT("ConfTitle", "START A NEW GAME?"))]
						+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 16.0f, 0.0f, 30.0f))
						[SNew(STextBlock).Font(Font("Regular", 24)).ColorAndOpacity(Paper).AutoWrapText(true).Text(LOCTEXT("ConfText", "Your current journey will be lost. A new journey begins from Day 1."))]
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth()[Btn(LOCTEXT("No", "CANCEL"), [Weak]() { if (Weak.IsValid()) { Weak->Click(Weak->BackSound); Weak->ShowScreen(Main); } })]
							+ SHorizontalBox::Slot().AutoWidth()[Btn(LOCTEXT("Yes", "NEW GAME"), [Weak]()
							{
								if (!Weak.IsValid()) return;
								Weak->Click(Weak->ConfirmSound);
								const UP9SettingsSubsystem* St = UP9SettingsSubsystem::Get(Weak.Get());
								if (St && !St->HasSeenHowToPlay()) { Weak->bHowToFromNewGame = true; Weak->ShowScreen(HowTo); }
								else Weak->StartGame(true);
							})]
						]
					]
				]
			]
		];
}

#undef LOCTEXT_NAMESPACE
