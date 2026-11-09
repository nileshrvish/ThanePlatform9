#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "P9ReverseEscalator.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UBoxComponent;

/**
 * Stage 8A: the endless stair (anomaly "EndlessStair") as a reverse escalator, replacing the Stage 6A loop teleport.
 *
 * The Platform 9/10 stair finish (the visible treads) is swapped for an identical copy of its upper flight + landing and 19
 * moving steps on the lower flight (the real step's tread + riser, cut from the station's own mesh). The steps glide up the
 * flight, rising out of the platform floor at the foot and sliding in under the landing edge at the top; step j at the end of
 * its cycle is exactly step j+1 at the start, so the motion has no seam. A smooth ramp (Pawn-only collision through the
 * nosings of the real stepped collision) replaces the riser-by-riser step-ups while the anomaly is active, so being carried
 * is comfortable and the camera never jolts.
 *
 *  - Walking down the lower flight: the steps run up as fast as the player walks and the player's progress down the flight is
 *    taken back each frame - they walk but stay in the same place (speed and facing untouched, no teleport).
 *  - Standing on the flight: carried up at a slow escalator pace towards the landing.
 *  - Walking up: the escalator slows to a stop (0.6 s) and the stair is an ordinary stair until the player heads down again.
 * Ended by the director at day end; the real finish comes back exactly.
 */
UCLASS(NotPlaceable)
class THANEPLATFORM9_API AP9ReverseEscalator : public AActor
{
	GENERATED_BODY()

public:
	AP9ReverseEscalator();

	/** Stair: the real stair finish actor (hidden while active). Upper / Step: the cut pieces (stair-actor local). */
	bool Setup(AActor* Stair, UStaticMesh* Upper, UStaticMesh* Step);
	void Restore();
	bool IsEngaged() const { return bEngaged; }
	FString GetStatus() const;

	/** Lower flight (stair-actor local cm): nosing of the bottom visual riser, tread, rise, landing top and the ramp line. */
	UPROPERTY(EditAnywhere, Category = "Escalator") float BottomNoseX = 1400.5f;
	UPROPERTY(EditAnywhere, Category = "Escalator") float Tread = 30.0f;
	UPROPERTY(EditAnywhere, Category = "Escalator") float Rise = 16.6667f;
	UPROPERTY(EditAnywhere, Category = "Escalator") float LandingTop = 300.5f;
	UPROPERTY(EditAnywhere, Category = "Escalator") float LandingX = 860.0f;
	UPROPERTY(EditAnywhere, Category = "Escalator") float FlightHalfWidth = 150.0f;
	/** Escalator pace (cm/s along the flight, horizontal) while nobody walks against it. */
	UPROPERTY(EditAnywhere, Category = "Escalator") float IdleSpeed = 55.0f;
	/** Share of the player's downhill progress that is taken back (1 = stays put, >1 = drifts up while walking down). */
	UPROPERTY(EditAnywhere, Category = "Escalator") float HoldFactor = 1.0f;
	UPROPERTY(EditAnywhere, Category = "Escalator") float Accel = 140.0f;

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	float RampZLocal(float X) const;                    // collision ramp (the real step cubes' nosing line), stair-local
	void PlaceSteps();

	UPROPERTY(VisibleAnywhere, Category = "Escalator") TObjectPtr<USceneComponent> Root;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> UpperComp;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Steps;
	UPROPERTY(Transient) TObjectPtr<UBoxComponent> Ramp;
	UPROPERTY(Transient) TObjectPtr<AActor> RealStair;
	bool bRealHidden = false;
	bool bSetup = false;
	bool bEngaged = false;        // the player has met the moving stair (for the "after it's seen" ambience)
	bool bStoppedByClimb = false;
	float Phase = 0.0f;           // 0..1 cycle of one tread
	float Speed = 0.0f;           // current step speed (cm/s horizontal, up the flight)
	float UpTime = 0.0f, DownTime = 0.0f;
	FVector LastFeetLocal = FVector::ZeroVector;
	bool bHadLast = false;
	float HeldCm = 0.0f;
};
