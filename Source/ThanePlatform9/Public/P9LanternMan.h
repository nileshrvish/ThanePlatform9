#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "P9LanternMan.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UPointLightComponent;
class UCapsuleComponent;
class UAudioComponent;
class ULightComponent;
class UMaterialInstanceDynamic;
class USoundBase;
class USoundAttenuation;

/**
 * Stage 8A: the Lantern Man (anomaly "LanternMan"). A completely still figure: a static mesh (no skeleton, no animation, no
 * breathing, no turning) standing in the darkest part of Platform 9, holding a lantern whose glass glows (emissive) and lights
 * the ground round him a little (one small warm point light, no shadows - a lantern, not a torch).
 *
 * The station lamp beside him (its spot and fill lights and its tube's glow, given by the director) flickers in short electrical
 * bursts while he is there; between bursts it burns a little dimmer than normal. Only that lamp: every other light is left
 * alone. Positional sounds belong to him: the lantern's low flame, a deep rumble close to him, the lamp's buzz during a burst
 * and a stinger the first time the player sees him. The director restores the lamp exactly when the day ends.
 */
UCLASS(NotPlaceable)
class THANEPLATFORM9_API AP9LanternMan : public AActor
{
	GENERATED_BODY()

public:
	AP9LanternMan();

	/** Lamp lights to flicker (their own intensities are the baseline) and the lamp's tube mesh (its glow slot is driven too). */
	void Setup(UStaticMesh* Mesh, const TArray<ULightComponent*>& LampLights, UStaticMeshComponent* LampTube, int32 Seed);
	/** Puts the lamp back to its own intensities (the director's baseline restores it too). */
	void StopFlicker();
	bool HasBeenSeen() const { return bSeen; }

	/** Mesh yaw so the figure faces the actor's +X (the FBX faces +Y). */
	UPROPERTY(EditAnywhere, Category = "Lantern") float MeshYaw = -90.0f;
	/** Lantern glass centre in the actor's frame (cm; x forward, y right, z up above the feet) - from the emissive bake (right hand). */
	UPROPERTY(EditAnywhere, Category = "Lantern") FVector LanternOffset = FVector(8.3f, 29.2f, 64.9f);
	UPROPERTY(EditAnywhere, Category = "Lantern") FLinearColor LanternColor = FLinearColor(1.0f, 0.55f, 0.22f);
	UPROPERTY(EditAnywhere, Category = "Lantern") float LanternIntensity = 14.0f;     // candelas
	UPROPERTY(EditAnywhere, Category = "Lantern") float LanternRadius = 380.0f;
	UPROPERTY(EditAnywhere, Category = "Lantern") float LanternGlow = 6.0f;          // emissive multiplier of the glass
	/** Lamp between bursts (x its own intensity), bursts every MinGap..MaxGap s lasting 0.5..1.8 s. */
	UPROPERTY(EditAnywhere, Category = "Lantern") float LampSteady = 0.8f;
	UPROPERTY(EditAnywhere, Category = "Lantern") float MinGap = 2.5f;
	UPROPERTY(EditAnywhere, Category = "Lantern") float MaxGap = 7.0f;
	UPROPERTY(EditAnywhere, Category = "Lantern|Audio") TSoftObjectPtr<USoundBase> FlameLoop;
	UPROPERTY(EditAnywhere, Category = "Lantern|Audio") TSoftObjectPtr<USoundBase> RumbleLoop;
	UPROPERTY(EditAnywhere, Category = "Lantern|Audio") TSoftObjectPtr<USoundBase> FlickerSound;
	UPROPERTY(EditAnywhere, Category = "Lantern|Audio") TSoftObjectPtr<USoundBase> Stinger;

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	void SetLamp(float Scale);

	UPROPERTY(VisibleAnywhere, Category = "Lantern") TObjectPtr<UCapsuleComponent> Body;
	UPROPERTY(VisibleAnywhere, Category = "Lantern") TObjectPtr<UStaticMeshComponent> Figure;
	UPROPERTY(VisibleAnywhere, Category = "Lantern") TObjectPtr<UPointLightComponent> Lantern;
	UPROPERTY(VisibleAnywhere, Category = "Lantern") TObjectPtr<UAudioComponent> FlameAudio;
	UPROPERTY(VisibleAnywhere, Category = "Lantern") TObjectPtr<UAudioComponent> RumbleAudio;
	UPROPERTY(VisibleAnywhere, Category = "Lantern") TObjectPtr<UAudioComponent> LampAudio;
	UPROPERTY(Transient) TArray<TObjectPtr<ULightComponent>> Lamp;
	TArray<float> LampBase;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> TubeMID;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> GlassMID;
	float TubeBase = 1.0f;
	FRandomStream Rng;
	float NextBurst = 3.0f;
	float BurstLeft = 0.0f;
	float BurstLevel = 1.0f;
	float BurstSwitch = 0.0f;
	float Time = 0.0f;
	float SeenCheck = 0.0f;
	bool bSeen = false;
	bool bFlicker = false;
	int32 Bursts = 0;
};
