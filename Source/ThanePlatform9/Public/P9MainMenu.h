#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "P9MainMenu.generated.h"

class SWidget;
class SWidgetSwitcher;
class UAudioComponent;
class UCameraComponent;
class UDataTable;
class USoundBase;
class AP9CampaignDirector;

/**
 * Stage 9B: the main menu, shown over the live station at launch (the startup map is unchanged).
 *
 * While it is up it HOLDS the campaign director at its first-tick start (AP9MainMenu::HoldsCampaign, read by the director), so
 * nothing of the campaign runs until the player chooses:
 *  - Continue (only with a valid campaign save): the director resumes the save exactly as before.
 *  - New Game: the director starts a fresh run with the Dadar -> Thane opening, ignoring the old save for this start (its
 *    bUseSaveGame switch is lowered for the first tick only and raised again at once; the new run's first save replaces the old one).
 *  - How to Play: the instruction board (offered automatically on the very first New Game; always reachable from the menu).
 *  - Settings: master / music / effects / ambience volume, mute, captions (UP9SettingsSubsystem, saved at once).
 *  - Quit (where the platform allows).
 * Background: a slow drifting camera over Platform 9 at night; the original menu loop plays (Music class) and fades out as
 * the game starts, while the station layers of the audio director fade in. The touch controls are hidden while the menu is up.
 * Console: p9.Menu.Status, p9.Menu.Show, p9.Menu.Start new|continue, p9.Menu.Screen main|howto|settings.
 */
UCLASS()
class THANEPLATFORM9_API AP9MainMenu : public AActor
{
	GENERATED_BODY()

public:
	AP9MainMenu();

	UPROPERTY(EditAnywhere, Category = "Menu") bool bShowOnStart = true;
	UPROPERTY(EditAnywhere, Category = "Menu|Audio") TObjectPtr<USoundBase> MenuMusic;
	UPROPERTY(EditAnywhere, Category = "Menu|Audio") TObjectPtr<USoundBase> ClickSound;
	UPROPERTY(EditAnywhere, Category = "Menu|Audio") TObjectPtr<USoundBase> ConfirmSound;
	UPROPERTY(EditAnywhere, Category = "Menu|Audio") TObjectPtr<USoundBase> BackSound;
	UPROPERTY(EditAnywhere, Category = "Menu|Audio") float MusicVolume = 0.8f;
	/** Captions (DT_P9_Captions); referenced here so it is cooked with the level. */
	UPROPERTY(EditAnywhere, Category = "Menu") TObjectPtr<UDataTable> CaptionTable;
	/** Background camera: drifts between these two world points (and gently sways) while the menu is up. */
	UPROPERTY(EditAnywhere, Category = "Menu|Camera") FVector CameraFrom = FVector(1650.0f, 7060.0f, 250.0f);
	UPROPERTY(EditAnywhere, Category = "Menu|Camera") FVector CameraTo = FVector(1150.0f, 7120.0f, 235.0f);
	UPROPERTY(EditAnywhere, Category = "Menu|Camera") FRotator CameraRotation = FRotator(-4.0f, 182.0f, 0.0f);
	UPROPERTY(EditAnywhere, Category = "Menu|Camera") float CameraPeriod = 70.0f;

	/** Read by the campaign director: true while a menu holds the campaign start. */
	static bool HoldsCampaign(const UWorld* World);

	void ShowMenu();
	void StartGame(bool bNewGame);
	void ShowScreen(int32 Index);
	bool HasValidSave() const;
	FString Describe() const;

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	TSharedRef<SWidget> BuildWidget();
	TSharedRef<SWidget> BuildMain();
	TSharedRef<SWidget> BuildHowTo();
	TSharedRef<SWidget> BuildSettings();
	TSharedRef<SWidget> BuildConfirm();
	void RemoveWidget();
	void Click(USoundBase* Sound);
	void SetTouchControlsHidden(bool bHide);
	void FinishStart();
	void OnNewGamePressed();
	AP9CampaignDirector* FindDirector() const;

	UPROPERTY(VisibleAnywhere, Category = "Menu") TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, Category = "Menu") TObjectPtr<UCameraComponent> MenuCamera;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> MusicComp;

	TSharedPtr<SWidget> Widget;
	TSharedPtr<SWidgetSwitcher> Switcher;
	bool bHolding = false;
	bool bBusy = false;
	bool bPendingNew = false;
	bool bRestoreSaveSwitch = false;
	bool bHowToFromNewGame = false;
	bool bHaveSave = false;
	float MenuTime = 0.0f;
	float Fade = 0.0f;              // black overlay (0..1)
	float FadeTarget = 0.0f;
	float StartCountdown = -1.0f;
	TMap<TWeakObjectPtr<class UUserWidget>, uint8> HiddenTouch;   // touch widgets hidden by the menu -> their visibility
};
