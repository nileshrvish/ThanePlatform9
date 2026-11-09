#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "P9GreetingScare.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class USoundBase;
class APlayerController;

/**
 * Stage 8A: the Greeting - the epilogue's final jumpscare, replacing the black figure that used to stand in the aisle when the
 * carriage lights snap back on after the flicker (only that appearance: the passenger seen at the far end during the flicker
 * is unchanged). Spawned by the director at the same moment and place (in the aisle 1.4 m from the player, facing them,
 * riding with the train).
 *
 *   0.00 s  she stands there, screaming face lit, in a dead-silent carriage (the music has already faded out)
 *   PauseSeconds  the lunge: reaching pose, an aggressive scream, she flies at the camera (accelerating, LungeSeconds) and
 *           stops with her face filling the view - never through the camera (the stop distance keeps her reaching hands in
 *           front of the near plane; the view narrows a little so the face fills it)
 *   + LungeSeconds  impact: low hit + metallic layer, a short camera shake; held until the director's blackout
 * The director hides her at the blackout; Finish() puts the view back.
 */
UCLASS(NotPlaceable)
class THANEPLATFORM9_API AP9GreetingScare : public AActor
{
	GENERATED_BODY()

public:
	AP9GreetingScare();

	void Begin(UStaticMesh* Stand, UStaticMesh* Lunge, APlayerController* PC);
	/** Restores the camera FOV (called at the blackout / when destroyed). */
	void Finish();
	bool HasImpacted() const { return bImpact; }

	/** The FBX faces +Y: yaw so she faces the actor's +X. */
	UPROPERTY(EditAnywhere, Category = "Greeting") float MeshYaw = -90.0f;
	UPROPERTY(EditAnywhere, Category = "Greeting") float PauseSeconds = 0.5f;
	UPROPERTY(EditAnywhere, Category = "Greeting") float LungeSeconds = 0.22f;
	/** Her face (lunge pose) in her own frame: x forward, z up from the feet (cm) - measured: nose 34.9 cm forward at 169 cm. */
	UPROPERTY(EditAnywhere, Category = "Greeting") FVector FaceLocal = FVector(35.0f, 0.0f, 167.0f);
	/**
	 * How far the face stops in front of the camera. Her reaching hands are 46 cm ahead of the face and 29-36 cm to the sides
	 * (measured), so at 64 cm they stop 18 cm in front of the camera, about 60 degrees off-axis: outside the narrowed view, never
	 * through the near plane.
	 */
	UPROPERTY(EditAnywhere, Category = "Greeting") float FaceStopDistance = 64.0f;
	UPROPERTY(EditAnywhere, Category = "Greeting") float LungeFOV = 62.0f;
	UPROPERTY(EditAnywhere, Category = "Greeting|Audio") TSoftObjectPtr<USoundBase> Scream;
	UPROPERTY(EditAnywhere, Category = "Greeting|Audio") TSoftObjectPtr<USoundBase> DistortedLayer;
	UPROPERTY(EditAnywhere, Category = "Greeting|Audio") TSoftObjectPtr<USoundBase> Impact;

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Greeting") TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, Category = "Greeting") TObjectPtr<UStaticMeshComponent> StandMesh;
	UPROPERTY(VisibleAnywhere, Category = "Greeting") TObjectPtr<UStaticMeshComponent> LungeMesh;
	TWeakObjectPtr<APlayerController> Player;
	FVector StartWorld = FVector::ZeroVector;
	float Time = 0.0f;
	float StartFOV = 90.0f;
	bool bLunging = false;
	bool bImpact = false;
	bool bFovLocked = false;
	FRotator ShakeOffset = FRotator::ZeroRotator;
	float ShakeLeft = 0.0f;
};
