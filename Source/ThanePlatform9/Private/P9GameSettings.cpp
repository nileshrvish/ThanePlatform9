#include "P9GameSettings.h"
#include "ThanePlatform9.h"
#include "AudioDevice.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/AudioSettings.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"

namespace P9Settings
{
	const TCHAR* Slot = TEXT("P9Settings");
	USoundClass* Class(const TCHAR* Name)
	{
		return LoadObject<USoundClass>(nullptr, *FString::Printf(TEXT("/Game/P9/Audio/OriginalAudioPack/Classes/%s.%s"), Name, Name));
	}
	static FAutoConsoleCommandWithWorld StatusCommand(TEXT("p9.Settings.Status"), TEXT("p9.Settings.Status - the player's volume / caption settings"),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (UP9SettingsSubsystem* S = UP9SettingsSubsystem::Get(World)) UE_LOG(LogP9, Log, TEXT("p9.Settings.Status: %s"), *S->Describe());
		}));
	static FAutoConsoleCommandWithWorldAndArgs SetCommand(TEXT("p9.Settings.Set"), TEXT("p9.Settings.Set <master|music|effects|ambience|mute|captions> <value> (test)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UP9SettingsSubsystem* S = UP9SettingsSubsystem::Get(World);
			if (!S || Args.Num() < 2) return;
			const float V = FCString::Atof(*Args[1]);
			const FString K = Args[0].ToLower();
			if (K == TEXT("master")) S->SetMaster(V); else if (K == TEXT("music")) S->SetMusic(V); else if (K == TEXT("effects")) S->SetEffects(V);
			else if (K == TEXT("ambience")) S->SetAmbience(V); else if (K == TEXT("mute")) S->SetMuted(V > 0.5f); else if (K == TEXT("captions")) S->SetCaptions(V > 0.5f);
			S->Save();
			UE_LOG(LogP9, Log, TEXT("p9.Settings.Set: %s"), *S->Describe());
		}));
}

UP9SettingsSubsystem* UP9SettingsSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* W = WorldContext ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UGameInstance* GI = W ? W->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UP9SettingsSubsystem>() : nullptr;
}

void UP9SettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Load();
}

void UP9SettingsSubsystem::Load()
{
	if (UGameplayStatics::DoesSaveGameExist(P9Settings::Slot, 0))
	{
		if (const UP9SettingsSave* S = Cast<UP9SettingsSave>(UGameplayStatics::LoadGameFromSlot(P9Settings::Slot, 0)))
		{
			Data = S->Settings;
		}
	}
	UE_LOG(LogP9, Log, TEXT("Settings: %s"), *Describe());
}

void UP9SettingsSubsystem::Save()
{
	UP9SettingsSave* S = Cast<UP9SettingsSave>(UGameplayStatics::CreateSaveGameObject(UP9SettingsSave::StaticClass()));
	if (!S) return;
	S->Settings = Data;
	const bool bOk = UGameplayStatics::SaveGameToSlot(S, P9Settings::Slot, 0);
	bDirty = false;
	UE_LOG(LogP9, Log, TEXT("Settings: saved (%s) %s"), bOk ? TEXT("ok") : TEXT("FAILED"), *Describe());
}

void UP9SettingsSubsystem::Changed()
{
	bDirty = true;
	if (UGameInstance* GI = GetGameInstance()) ApplyAudio(GI->GetWorld());
}

void UP9SettingsSubsystem::ApplyAudio(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World) return;
	if (!Mix)
	{
		Mix = NewObject<USoundMix>(this, TEXT("P9SettingsMix"), RF_Transient);
		Mix->FadeInTime = 0.0f;
		Mix->FadeOutTime = 0.0f;
		MusicClasses = {P9Settings::Class(TEXT("SC_P9_Music"))};
		AmbienceClasses = {P9Settings::Class(TEXT("SC_P9_StationAmbience"))};
		EffectClasses = {P9Settings::Class(TEXT("SC_P9_Player")), P9Settings::Class(TEXT("SC_P9_Railway")), P9Settings::Class(TEXT("SC_P9_NPC")),
			P9Settings::Class(TEXT("SC_P9_Creature")), P9Settings::Class(TEXT("SC_P9_HorrorFX"))};
		// sounds without a class of their own (announcements, voices, the older effects) play in the default class
		if (USoundClass* Default = LoadObject<USoundClass>(nullptr, *GetDefault<UAudioSettings>()->DefaultSoundClassName.ToString())) EffectClasses.Add(Default);
	}
	auto Set = [&](const TArray<TObjectPtr<USoundClass>>& Classes, float V)
	{
		for (USoundClass* C : Classes)
		{
			if (C) UGameplayStatics::SetSoundMixClassOverride(World, Mix, C, FMath::Max(V, 0.0001f), 1.0f, 0.15f, false);
		}
	};
	Set(MusicClasses, Data.Music);
	Set(AmbienceClasses, Data.Ambience);
	Set(EffectClasses, Data.Effects);
	if (MixWorld.Get() != World)
	{
		UGameplayStatics::PushSoundMixModifier(World, Mix);
		MixWorld = World;
	}
	if (FAudioDeviceHandle Dev = World->GetAudioDevice())
	{
		Dev->SetTransientPrimaryVolume(Data.bMuted ? 0.0f : Data.Master);
	}
}

FString UP9SettingsSubsystem::Describe() const
{
	return FString::Printf(TEXT("master %.2f music %.2f effects %.2f ambience %.2f muted %d captions %d seenHowTo %d%s"), Data.Master, Data.Music, Data.Effects,
		Data.Ambience, Data.bMuted ? 1 : 0, Data.bCaptions ? 1 : 0, Data.bSeenHowToPlay ? 1 : 0, bDirty ? TEXT(" (unsaved)") : TEXT(""));
}
