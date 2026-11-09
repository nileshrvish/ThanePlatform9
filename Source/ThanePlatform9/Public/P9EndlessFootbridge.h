#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "P9EndlessFootbridge.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UPointLightComponent;
class AP9AnomalySpawnPoint;

/**
 * Stage 8A: the endless footbridge (anomaly "EndlessFootbridge"), replacing the Stage 6A loop teleport.
 *
 * Nothing moves the player. When the player walks north into the point's LoopTrigger (deck B, about 6 m before the Platform 9
 * stair opening) the real north end of deck B - shell + floor beyond y 7200 with the PF9 opening, the 9/10 sign, its tube
 * light, the CCTV and the end wall - is swapped, in the same frame and at the same place, for a movable copy (the same
 * meshes, cut in Blender from the station's own source files). From then on the copy stays exactly as far ahead of the
 * player as it was when the swap happened: the end recedes at the player's own pace and is never reached. Behind it,
 * 600 cm bays (a whole number of texture repeats of every material, so they tile without seams) are laid down as the
 * player walks; a slightly larger "lead-in" bay fixed to the end hides the moving joint. Bays carry the floor, walls and
 * roof collision; the PF9 opening is closed by the bays' east wall.
 *
 * Walking back is ordinary walking: the extension stops growing as soon as the player stops gaining ground (the end only
 * recedes while the player passes their furthest point). Back on the south part of the bridge, looking away from the
 * north end, the original end is restored unseen; walking north again re-engages it. Ended (and everything restored) by
 * the director at day end.
 */
UCLASS(NotPlaceable)
class THANEPLATFORM9_API AP9EndlessFootbridge : public AActor
{
	GENERATED_BODY()

public:
	AP9EndlessFootbridge();

	/** Point: LoopTrigger = engage box, ExtraActors = the real actors of the north end (shell, floor, sign, tube, its light, CCTV, end wall). */
	bool Setup(AP9AnomalySpawnPoint* Point, UStaticMesh* South, UStaticMesh* End, UStaticMesh* Bay, UStaticMesh* LeadIn, UStaticMesh* TubeMesh);
	/** Puts every real actor back and removes every copy. Safe to call twice. */
	void Restore();
	bool IsEngaged() const { return bEngaged; }
	int32 GetBayCount() const { return Bays.Num(); }
	float GetExtension() const { return Extension; }
	FString GetStatus() const;

	/** Bay length (cm) - endless_8a.json bay_length_cm. */
	UPROPERTY(EditAnywhere, Category = "Endless") float BayLength = 600.0f;
	/** Where the real end piece starts (world y, cm). */
	UPROPERTY(EditAnywhere, Category = "Endless") float CutY = 7200.0f;
	/** Length of the end piece (cut y to the end wall, cm). */
	UPROPERTY(EditAnywhere, Category = "Endless") float EndLength = 730.0f;
	/** Bays further than this from the player have their light off (the far bridge is dark; the end itself stays lit). */
	UPROPERTY(EditAnywhere, Category = "Endless") float LitRange = 2600.0f;
	/** Collapse back to the real end when the player is south of this y and has looked away from the north for CollapseSeconds. */
	UPROPERTY(EditAnywhere, Category = "Endless") float CollapseBelowY = 5900.0f;
	UPROPERTY(EditAnywhere, Category = "Endless") float CollapseSeconds = 1.5f;
	/** Never more than this many bays (about 1.2 km); the end stops receding there. */
	UPROPERTY(EditAnywhere, Category = "Endless") int32 MaxBays = 200;

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	void Engage(const FVector& PlayerFeet);
	void Collapse();
	void SetRealEndShown(bool bShown);
	UStaticMeshComponent* AddMesh(USceneComponent* Parent, UStaticMesh* Mesh, const FTransform& Rel, bool bCollision, const TArray<UMaterialInterface*>& Mats);
	void AddBay();
	FVector PlayerFeet(bool& bOk) const;

	UPROPERTY(VisibleAnywhere, Category = "Endless") TObjectPtr<USceneComponent> Root;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> EndRoot;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> SouthComp;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Bays;
	UPROPERTY(Transient) TArray<TObjectPtr<UPointLightComponent>> BayLights;
	UPROPERTY(Transient) TArray<TObjectPtr<AActor>> RealActors;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> SouthMesh;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> EndMesh;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> BayMesh;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> LeadInMesh;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> Tube;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInterface>> ShellMaterials;   // per FBX slot, taken from the real shell + floor
	struct FRealState { bool bHidden = false; bool bCollision = true; };
	TArray<FRealState> RealStates;
	FBox Trigger = FBox(ForceInit);
	FVector North = FVector(0.0f, 1.0f, 0.0f);
	bool bSetup = false;
	bool bEngaged = false;
	float EngageY = 0.0f;
	float MaxY = 0.0f;
	float Extension = 0.0f;
	float AwayTime = 0.0f;
	float LightCheck = 0.0f;
	int32 Engagements = 0;
	// the real end light's settings (copied to the moving end and to the bay lights)
	float LightIntensity = 123.75f;
	float LightRadius = 1400.0f;
	float LightTemperature = 4200.0f;
	FVector LightRel = FVector(0.0f, 310.0f, 840.0f);
};
