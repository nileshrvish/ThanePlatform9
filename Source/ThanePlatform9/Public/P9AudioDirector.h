#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "P9AudioDirector.generated.h"

class UAudioComponent;
class USoundBase;
class AP9CampaignDirector;
class AP9SilhouetteStalker;
class USoundAttenuation;

/** Stage 9C: what the player is walking on (picked from the floor under the feet each step). */
UENUM()
enum class EP9StepSurface : uint8 { Platform, Footbridge, Concourse, Stairs, Metal, Train };

/** Stage 9C: one surface's footstep set (left / right foot, walk / run, scuffs for sudden stops and pivots). */
USTRUCT()
struct FP9StepSet
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category = "Steps") TArray<TSoftObjectPtr<USoundBase>> WalkL;
	UPROPERTY(EditAnywhere, Category = "Steps") TArray<TSoftObjectPtr<USoundBase>> WalkR;
	UPROPERTY(EditAnywhere, Category = "Steps") TArray<TSoftObjectPtr<USoundBase>> RunL;
	UPROPERTY(EditAnywhere, Category = "Steps") TArray<TSoftObjectPtr<USoundBase>> RunR;
	UPROPERTY(EditAnywhere, Category = "Steps") TArray<TSoftObjectPtr<USoundBase>> Scuff;
	/** Per-surface level trim (linear), so every surface sits at the same loudness in the mix. */
	UPROPERTY(EditAnywhere, Category = "Steps") float Gain = 1.0f;
};

/**
 * Stage 8A: the original audio pack's music and ambience layers, player footsteps and Kanchana's sounds. An isolated listener:
 * it only READS the campaign director (phase, epilogue progress, whether the player has met today's anomaly), the player's
 * movement and Kanchana's position - it changes nothing in the campaign, the player, the trains or Kanchana.
 *
 *  - Station: BGM_Main_Station_Ambience low under everything; the platform wind on the platforms, the footbridge bed on the
 *    footbridge; both lower inside a train (the trains keep their own sounds).
 *  - Towards Platform 9: BGM_Platform9_Horror rises with closeness (north footbridge, the stairs, the island).
 *  - An anomalous day: BGM_Anomaly_Ambience fades in ONLY after the player has seen / met the anomaly (user choice 2026-11-08,
 *    rule 8: never a cue before it is found; normal days never play it).
 *  - Epilogue: the station layers fade out and BGM_Final_Epilogue plays from the wake-up; on the last ride it fades to silence
 *    before the lights fail (the Greeting's scream comes out of quiet); the final screen plays its ending.
 *  - Stage 9C footsteps: six surfaces (platform stone, footbridge / concourse chequer tiles, stair treads, metal escalator
 *    steps, the train floor) picked from the floor the player stands on; played in 3D at the stepping foot (left / right of
 *    the body, at floor level) with a near-omni blend so they stay centred and natural in first person; a scuff on hard stops
 *    and pivots. Without the 9C sets the 9A / 8A sets are used as before.
 *  - Player footsteps (platform / stairs / footbridge by height and climb), jump and landing. Stage 9A: walk / run, left / right
 *    sets, cadence from the speed, a soft settling step when the player stops.
 *  - Stage 9A thunder: after each lightning flash (detected from the moon light the storm controller raises) thunder follows after
 *    the sound's travel time (distance / 343 m/s): a visible bolt gives a near strike; flashes without a bolt are far and only
 *    sometimes audible. The storm controller and its timing are not changed.
 *  - Kanchana, while she is out and near: breathing on her, her footsteps (close / distant), a low presence layer.
 * Console: p9.Audio.Status.
 */
UCLASS()
class THANEPLATFORM9_API AP9AudioDirector : public AActor
{
	GENERATED_BODY()

public:
	AP9AudioDirector();

	UPROPERTY(EditAnywhere, Category = "Audio|Music") TSoftObjectPtr<USoundBase> MainAmbience;
	UPROPERTY(EditAnywhere, Category = "Audio|Music") TSoftObjectPtr<USoundBase> Platform9Horror;
	UPROPERTY(EditAnywhere, Category = "Audio|Music") TSoftObjectPtr<USoundBase> AnomalyAmbience;
	UPROPERTY(EditAnywhere, Category = "Audio|Music") TSoftObjectPtr<USoundBase> EpilogueMusic;
	UPROPERTY(EditAnywhere, Category = "Audio|Ambience") TSoftObjectPtr<USoundBase> PlatformWind;
	UPROPERTY(EditAnywhere, Category = "Audio|Ambience") TSoftObjectPtr<USoundBase> FootbridgeBed;
	UPROPERTY(EditAnywhere, Category = "Audio|Player") TArray<TSoftObjectPtr<USoundBase>> StepsPlatform;
	UPROPERTY(EditAnywhere, Category = "Audio|Player") TArray<TSoftObjectPtr<USoundBase>> StepsStairs;
	UPROPERTY(EditAnywhere, Category = "Audio|Player") TArray<TSoftObjectPtr<USoundBase>> StepsFootbridge;
	/**
	 * Stage 9A footsteps: per surface, walk and run, left and right foot (stereo, a little off-centre). When a 9A set is
	 * empty the 8A sets above are used. Steps alternate left / right; the cadence follows the speed (about 2.2 steps/s at a
	 * walk, 4.3 at a full run), stairs are detected from the climb per stride, the footbridge from the deck height.
	 */
	UPROPERTY(EditAnywhere, Category = "Audio|Player9A") TArray<TSoftObjectPtr<USoundBase>> Steps9A_Platform_Walk_L;
	UPROPERTY(EditAnywhere, Category = "Audio|Player9A") TArray<TSoftObjectPtr<USoundBase>> Steps9A_Platform_Walk_R;
	UPROPERTY(EditAnywhere, Category = "Audio|Player9A") TArray<TSoftObjectPtr<USoundBase>> Steps9A_Platform_Run_L;
	UPROPERTY(EditAnywhere, Category = "Audio|Player9A") TArray<TSoftObjectPtr<USoundBase>> Steps9A_Platform_Run_R;
	UPROPERTY(EditAnywhere, Category = "Audio|Player9A") TArray<TSoftObjectPtr<USoundBase>> Steps9A_Footbridge_Walk_L;
	UPROPERTY(EditAnywhere, Category = "Audio|Player9A") TArray<TSoftObjectPtr<USoundBase>> Steps9A_Footbridge_Walk_R;
	UPROPERTY(EditAnywhere, Category = "Audio|Player9A") TArray<TSoftObjectPtr<USoundBase>> Steps9A_Footbridge_Run_L;
	UPROPERTY(EditAnywhere, Category = "Audio|Player9A") TArray<TSoftObjectPtr<USoundBase>> Steps9A_Footbridge_Run_R;
	UPROPERTY(EditAnywhere, Category = "Audio|Player9A") TArray<TSoftObjectPtr<USoundBase>> Steps9A_Stairs_Walk_L;
	UPROPERTY(EditAnywhere, Category = "Audio|Player9A") TArray<TSoftObjectPtr<USoundBase>> Steps9A_Stairs_Walk_R;
	UPROPERTY(EditAnywhere, Category = "Audio|Player9A") TArray<TSoftObjectPtr<USoundBase>> Steps9A_Stairs_Run_L;
	UPROPERTY(EditAnywhere, Category = "Audio|Player9A") TArray<TSoftObjectPtr<USoundBase>> Steps9A_Stairs_Run_R;
	/** Horizontal speed (cm/s) from which the run steps are used. */
	UPROPERTY(EditAnywhere, Category = "Audio|Player9A") float RunSpeed = 330.0f;
	/** Stage 9A weather: thunder after lightning (the storm controller is only read - its moon-light flash is the cue). */
	UPROPERTY(EditAnywhere, Category = "Audio|Weather") TArray<TSoftObjectPtr<USoundBase>> ThunderDistant;
	UPROPERTY(EditAnywhere, Category = "Audio|Weather") TArray<TSoftObjectPtr<USoundBase>> ThunderStrong;
	UPROPERTY(EditAnywhere, Category = "Audio|Weather") TArray<TSoftObjectPtr<USoundBase>> ThunderClose;
	/** Chance that a flash without a visible bolt is followed by audible thunder (far flashes are often silent). */
	UPROPERTY(EditAnywhere, Category = "Audio|Weather", meta = (ClampMin = "0.0", ClampMax = "1.0")) float ThunderChanceNoBolt = 0.55f;
	UPROPERTY(EditAnywhere, Category = "Audio|Mix") float ThunderVolume = 0.55f;
	UPROPERTY(EditAnywhere, Category = "Audio|Player") TSoftObjectPtr<USoundBase> JumpSound;
	UPROPERTY(EditAnywhere, Category = "Audio|Player") TSoftObjectPtr<USoundBase> LandSound;
	/** Stage 9C per-surface footstep sets (used when filled; else the 9A sets). */
	UPROPERTY(EditAnywhere, Category = "Audio|Player9C") FP9StepSet Steps9C_Platform;
	UPROPERTY(EditAnywhere, Category = "Audio|Player9C") FP9StepSet Steps9C_Footbridge;
	UPROPERTY(EditAnywhere, Category = "Audio|Player9C") FP9StepSet Steps9C_Concourse;
	UPROPERTY(EditAnywhere, Category = "Audio|Player9C") FP9StepSet Steps9C_Stairs;
	UPROPERTY(EditAnywhere, Category = "Audio|Player9C") FP9StepSet Steps9C_Metal;
	UPROPERTY(EditAnywhere, Category = "Audio|Player9C") FP9StepSet Steps9C_Train;
	/** Footsteps in 3D at the stepping foot (false: 2D as before). */
	UPROPERTY(EditAnywhere, Category = "Audio|Player9C") bool bSpatialFootsteps = true;
	/** Optional attenuation for the footsteps; empty = a built-in near-field one (full level within 3 m, mostly centred). */
	UPROPERTY(EditAnywhere, Category = "Audio|Player9C") TObjectPtr<USoundAttenuation> FootstepAttenuation;
	/** Sideways distance (cm) of each foot from the body centre. */
	UPROPERTY(EditAnywhere, Category = "Audio|Player9C") float FootSpacing = 11.0f;
	/** A point on the concourse floor: the floor found under it at play start is the concourse (the rest of the deck level is
	 *  the footbridge). */
	UPROPERTY(EditAnywhere, Category = "Audio|Player9C") FVector ConcourseProbe = FVector(0.0f, 3000.0f, 650.0f);
	UPROPERTY(EditAnywhere, Category = "Audio|Kanchana") TSoftObjectPtr<USoundBase> KanchanaBreathing;
	UPROPERTY(EditAnywhere, Category = "Audio|Kanchana") TSoftObjectPtr<USoundBase> KanchanaPresence;
	UPROPERTY(EditAnywhere, Category = "Audio|Kanchana") TArray<TSoftObjectPtr<USoundBase>> KanchanaStepsClose;
	UPROPERTY(EditAnywhere, Category = "Audio|Kanchana") TArray<TSoftObjectPtr<USoundBase>> KanchanaStepsDistant;

	/** Linear volumes of each layer at full strength. */
	UPROPERTY(EditAnywhere, Category = "Audio|Mix") float MainVolume = 0.30f;
	UPROPERTY(EditAnywhere, Category = "Audio|Mix") float Platform9Volume = 0.40f;
	UPROPERTY(EditAnywhere, Category = "Audio|Mix") float AnomalyVolume = 0.32f;
	UPROPERTY(EditAnywhere, Category = "Audio|Mix") float EpilogueVolume = 0.50f;
	UPROPERTY(EditAnywhere, Category = "Audio|Mix") float WindVolume = 0.20f;   // Stage 9A: 0.26 -> 0.20 with the softer 9A breeze
	UPROPERTY(EditAnywhere, Category = "Audio|Mix") float FootbridgeVolume = 0.30f;
	UPROPERTY(EditAnywhere, Category = "Audio|Mix") float FootstepVolume = 0.30f;
	UPROPERTY(EditAnywhere, Category = "Audio|Mix") float StrideLength = 72.0f;
	/** Station layers off entirely (testing / a player option later). */
	UPROPERTY(EditAnywhere, Category = "Audio|Mix") bool bEnabled = true;

	FString GetStatus() const;
	/** Stage 9A: queue a thunder (0 distant, 1 strong, 2 close) that arrives after Delay seconds. */
	void QueueThunder(int32 Kind, float Delay, float Distance = 0.0f);
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	struct FLayer
	{
		TObjectPtr<UAudioComponent> Comp = nullptr;
		float Volume = 0.0f;       // current (faded)
		float Target = 0.0f;
		float FadeRate = 0.25f;    // volume units per second
		bool bLoop = true;         // false: started explicitly once (the epilogue music), never restarted by the fader
	};
	UAudioComponent* MakeLoop(USoundBase* Sound, const TCHAR* Name);
	void DriveLayer(FLayer& L, float DeltaSeconds);
	void TickFootsteps(float DeltaSeconds);
	void TickKanchana(float DeltaSeconds);
	void TickThunder(float DeltaSeconds);
	USoundBase* PickStep(bool bStairs, bool bBridge, bool bRun, bool bLeft);
	// Stage 9C
	EP9StepSurface SurfaceUnder(const class ACharacter* C, const FVector& Feet, float StrideRiseNow) const;
	const FP9StepSet* StepSet(EP9StepSurface Surface) const;
	USoundBase* PickStep9C(EP9StepSurface Surface, bool bRun, bool bLeft, bool bScuff, float& OutGain);
	void PlayStep(USoundBase* S, const class ACharacter* C, const FVector& Feet, bool bLeft, float Volume, float Pitch);
	USoundBase* Pick(const TArray<TSoftObjectPtr<USoundBase>>& Set, int32& Last);

	UPROPERTY(Transient) TObjectPtr<AP9CampaignDirector> Director;
	UPROPERTY(Transient) TObjectPtr<AP9SilhouetteStalker> Stalker;
	UPROPERTY(Transient) TArray<TObjectPtr<UAudioComponent>> Owned;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> KanchanaBreath;
	FLayer Main, P9, Anomaly, Epilogue, Wind, Bridge, Presence;
	FRandomStream Rng;
	int32 LastPhase = -1;
	bool bEpilogueStarted = false;
	bool bEndingStarted = false;
	// footsteps
	FVector LastFeet = FVector::ZeroVector;
	bool bHaveLast = false;
	bool bWasFalling = false;
	float FallTime = 0.0f;
	float StrideLeft = 0.0f;
	float StrideRise = 0.0f;
	int32 LastStep = -1, LastKStep = -1;
	int32 Steps = 0;
	bool bLeftFoot = false;
	FString LastStepName;                                      // Stage 9B: test readout                                    // Stage 9A: the next step's foot
	float StepVolumeEase = 0.0f;
	// Stage 9C
	UPROPERTY(Transient) TObjectPtr<USoundAttenuation> StepAttenuation;
	UPROPERTY(Transient) TObjectPtr<AActor> ConcourseFloor;
	EP9StepSurface LastSurface = EP9StepSurface::Platform;
	float LastSpeed = 0.0f;
	float LastYaw = 0.0f;
	float ScuffCooldown = 0.0f;
	int32 LastScuff = -1;
	int32 Scuffs = 0;
	// Stage 9A thunder
	UPROPERTY(Transient) TObjectPtr<class ADirectionalLight> Moon;
	UPROPERTY(Transient) TObjectPtr<class AP9StormController> Storm;
	float MoonBase = -1.0f;
	float FlashCooldown = 0.0f;
	struct FThunder { float Delay; int32 Kind; float Volume; float Distance; };
	TArray<FThunder> PendingThunder;
	int32 LastThunder[3] = {-1, -1, -1};
	int32 Flashes = 0, Thunders = 0;
	// Kanchana
	FVector LastKanchana = FVector::ZeroVector;
	bool bHaveKanchana = false;
	float KStride = 0.0f;
};
