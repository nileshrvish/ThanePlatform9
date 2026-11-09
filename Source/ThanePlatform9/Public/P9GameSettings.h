#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "P9GameSettings.generated.h"

class USoundClass;
class USoundMix;

/** Stage 9B: the player's settings values. */
USTRUCT()
struct FP9SettingsData
{
	GENERATED_BODY()

	UPROPERTY() float Master = 1.0f;
	UPROPERTY() float Music = 0.8f;
	UPROPERTY() float Effects = 1.0f;
	UPROPERTY() float Ambience = 0.8f;
	UPROPERTY() bool bMuted = false;
	UPROPERTY() bool bCaptions = true;
	/** The How to Play board was shown once (offered on the first New Game; it can always be opened again from the menu). */
	UPROPERTY() bool bSeenHowToPlay = false;
};

/** Stage 9B: the settings save slot ("P9Settings"; the campaign save is separate and untouched). */
UCLASS()
class THANEPLATFORM9_API UP9SettingsSave : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY() int32 Version = 1;
	UPROPERTY() FP9SettingsData Settings;
};

/**
 * Stage 9B: settings for the whole game session - audio volumes (applied through one engine sound mix on the project's existing
 * sound classes; nothing in the sound assets changes) and captions. Persisted in the "P9Settings" slot, applied immediately.
 *  - Master: the audio device's primary volume (everything, menu included); Mute sets it to 0.
 *  - Music: SC_P9_Music.  Ambience: SC_P9_StationAmbience.
 *  - Effects: SC_P9_Player, SC_P9_Railway, SC_P9_NPC, SC_P9_Creature, SC_P9_HorrorFX and the default sound class (announcements,
 *    voices and older effects that have no class).
 * Console: p9.Settings.Status.
 */
UCLASS()
class THANEPLATFORM9_API UP9SettingsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	static UP9SettingsSubsystem* Get(const UObject* WorldContext);

	float GetMaster() const { return Data.Master; }
	float GetMusic() const { return Data.Music; }
	float GetEffects() const { return Data.Effects; }
	float GetAmbience() const { return Data.Ambience; }
	bool IsMuted() const { return Data.bMuted; }
	bool CaptionsEnabled() const { return Data.bCaptions; }
	bool HasSeenHowToPlay() const { return Data.bSeenHowToPlay; }

	void SetMaster(float V) { Data.Master = FMath::Clamp(V, 0.0f, 1.0f); Changed(); }
	void SetMusic(float V) { Data.Music = FMath::Clamp(V, 0.0f, 1.0f); Changed(); }
	void SetEffects(float V) { Data.Effects = FMath::Clamp(V, 0.0f, 1.0f); Changed(); }
	void SetAmbience(float V) { Data.Ambience = FMath::Clamp(V, 0.0f, 1.0f); Changed(); }
	void SetMuted(bool b) { Data.bMuted = b; Changed(); }
	void SetCaptions(bool b) { Data.bCaptions = b; Changed(); }
	void MarkHowToPlaySeen() { if (!Data.bSeenHowToPlay) { Data.bSeenHowToPlay = true; Save(); } }

	/** Applies the volumes to the current world's audio device (call once a world is playing; cheap). */
	void ApplyAudio(const UObject* WorldContext);
	/** Writes the slot now (sliders save when released / when the settings screen closes). */
	void Save();
	FString Describe() const;

private:
	void Changed();
	void Load();

	FP9SettingsData Data;
	UPROPERTY(Transient) TObjectPtr<USoundMix> Mix;
	UPROPERTY(Transient) TArray<TObjectPtr<USoundClass>> MusicClasses;
	UPROPERTY(Transient) TArray<TObjectPtr<USoundClass>> EffectClasses;
	UPROPERTY(Transient) TArray<TObjectPtr<USoundClass>> AmbienceClasses;
	TWeakObjectPtr<UWorld> MixWorld;
	bool bDirty = false;
};
