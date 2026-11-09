#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Subsystems/WorldSubsystem.h"
#include "P9Captions.generated.h"

class SWidget;
class USoundBase;

/**
 * Stage 9B: one caption line of a sound. Rows of the same SoundName are shown in Order, each from Start to End seconds after the
 * sound starts (times measured from the actual audio, see P9_Assets/Captions9B/build_captions_9b.py). Text is the original spoken
 * wording (romanised as in the scripts) with the script's English meaning in brackets, or a short [description] of a sound.
 */
USTRUCT(BlueprintType)
struct FP9CaptionRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Caption") FName SoundName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Caption") int32 Order = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Caption") float Start = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Caption") float End = 2.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Caption", meta = (MultiLine = "true")) FString Text;
};

/**
 * Stage 9B: captions at the bottom centre of the screen, synchronised to the sound that is playing, and the cinematic letterbox.
 * Sounds are reported by the places that play them (P9Captions::NotifySound) - the announcer, the campaign director's sound
 * helpers, Kanchana's lines, the Greeting, the Lantern Man's stinger, thunder. Only sounds with rows in DT_P9_Captions show text.
 * Respects the Captions setting. The letterbox eases in while the player is held in a story moment (seated in the opening /
 * epilogue train) and out when control returns. Console: p9.Captions.Test <SoundName>, p9.Captions.Status.
 */
UCLASS()
class THANEPLATFORM9_API UP9CaptionSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UP9CaptionSubsystem, STATGROUP_Tickables); }
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override { return WorldType == EWorldType::Game || WorldType == EWorldType::PIE; }

	static UP9CaptionSubsystem* Get(const UObject* WorldContext);
	/** A sound started now (Delay: it starts that many seconds later). */
	void OnSound(const USoundBase* Sound, float Delay = 0.0f);
	void OnSoundName(FName SoundName, float Delay = 0.0f);
	/** Stops the captions of a sound (it was cut off). */
	void StopSound(FName SoundName);
	void SetTable(UDataTable* InTable) { Table = InTable; }
	/** Cinematic letterbox request from outside (the main menu uses none; the story moments are detected here). */
	FString Describe() const;
	FText GetVisibleText(float& OutAlpha) const;
	float GetLetterbox() const { return Letterbox; }

private:
	struct FActive { FName Sound; TArray<FP9CaptionRow> Rows; double StartTime = 0.0; };
	void EnsureWidget();
	void RemoveWidget();
	bool IsStoryMoment() const;

	UPROPERTY(Transient) TObjectPtr<UDataTable> Table;
	TArray<FActive> Active;
	TSharedPtr<SWidget> Widget;
	FText Shown;
	float Alpha = 0.0f;
	float Letterbox = 0.0f;
	int32 Started = 0;
};

namespace P9Captions
{
	/** Report a sound that is starting (safe with null sounds / no world). */
	THANEPLATFORM9_API void NotifySound(const UObject* WorldContext, const USoundBase* Sound, float Delay = 0.0f);
	THANEPLATFORM9_API void StopSound(const UObject* WorldContext, const USoundBase* Sound);
}
