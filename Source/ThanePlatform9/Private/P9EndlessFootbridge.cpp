#include "P9EndlessFootbridge.h"
#include "ThanePlatform9.h"
#include "P9AnomalySpawnPoint.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"

AP9EndlessFootbridge::AP9EndlessFootbridge()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	SetRootComponent(Root);
}

bool AP9EndlessFootbridge::Setup(AP9AnomalySpawnPoint* Point, UStaticMesh* South, UStaticMesh* End, UStaticMesh* Bay, UStaticMesh* LeadIn, UStaticMesh* TubeMesh)
{
	if (!Point || !South || !End || !Bay || !LeadIn || !Point->LoopTrigger.IsValid)
	{
		return false;
	}
	SouthMesh = South; EndMesh = End; BayMesh = Bay; LeadInMesh = LeadIn; Tube = TubeMesh;
	Trigger = Point->LoopTrigger;
	North = Point->LoopDirection.GetSafeNormal2D();
	if (North.IsNearlyZero()) North = FVector(0.0f, 1.0f, 0.0f);
	RealActors.Reset(); RealStates.Reset();
	for (AActor* A : Point->ExtraActors)
	{
		if (!A) continue;
		RealActors.Add(A);
		RealStates.Add({A->IsHidden(), A->GetActorEnableCollision()});
		if (const UPointLightComponent* L = A->FindComponentByClass<UPointLightComponent>())
		{
			LightIntensity = L->Intensity;
			LightRadius = L->AttenuationRadius;
			LightTemperature = L->Temperature;
			LightRel = L->GetComponentLocation() - FVector(0.0f, CutY, 0.0f);
		}
	}
	bSetup = RealActors.Num() >= 2;
	SetActorLocation(FVector::ZeroVector);
	UE_LOG(LogP9, Log, TEXT("Endless footbridge: armed - %d real end actors, trigger %s, bay %.0f cm"), RealActors.Num(), *Trigger.ToString(), BayLength);
	return bSetup;
}

UStaticMeshComponent* AP9EndlessFootbridge::AddMesh(USceneComponent* Parent, UStaticMesh* Mesh, const FTransform& Rel, bool bCollision, const TArray<UMaterialInterface*>& Mats)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
	C->SetMobility(EComponentMobility::Movable);
	C->SetStaticMesh(Mesh);
	for (int32 i = 0; i < Mats.Num(); ++i) if (Mats[i]) C->SetMaterial(i, Mats[i]);
	C->SetCollisionEnabled(bCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	if (bCollision) C->SetCollisionProfileName(TEXT("BlockAll"));
	C->SetCanEverAffectNavigation(false);
	C->bNeverDistanceCull = true;
	C->SetCastShadow(false);
	C->SetupAttachment(Parent);
	C->SetRelativeTransform(Rel);
	C->RegisterComponent();
	return C;
}

namespace P9Endless
{
	/** Collision of a bridge piece y 0..Length (piece-local cm): deck, both side walls to the roof, roof; optional end wall. */
	void AddPieceCollision(AActor* Owner, USceneComponent* Parent, const FVector& Origin, float Length, bool bEndWall)
	{
		auto Box = [&](const FVector& Centre, const FVector& Extent)
		{
			UBoxComponent* B = NewObject<UBoxComponent>(Owner, NAME_None, RF_Transient);
			B->SetMobility(EComponentMobility::Movable);
			B->SetupAttachment(Parent);
			B->SetRelativeLocation(Origin + Centre);
			B->SetBoxExtent(Extent);
			B->SetCollisionProfileName(TEXT("BlockAll"));
			B->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			B->SetGenerateOverlapEvents(false);
			B->SetCanEverAffectNavigation(false);
			B->SetHiddenInGame(true);
			B->RegisterComponent();
		};
		const float H = Length * 0.5f;
		Box(FVector(0.0f, H, 580.0f), FVector(205.0f, H, 20.0f));          // deck (top at 600, the real floor level)
		Box(FVector(-207.0f, H, 752.0f), FVector(10.0f, H, 152.0f));       // west wall, parapet + grille to the roof
		Box(FVector(207.0f, H, 752.0f), FVector(10.0f, H, 152.0f));        // east wall (closes the PF9 stair opening)
		Box(FVector(0.0f, H, 910.0f), FVector(225.0f, H, 10.0f));          // roof
		if (bEndWall) Box(FVector(0.0f, Length - 12.0f, 752.0f), FVector(205.0f, 12.0f, 152.0f));
	}
}

void AP9EndlessFootbridge::SetRealEndShown(bool bShown)
{
	for (int32 i = 0; i < RealActors.Num(); ++i)
	{
		AActor* A = RealActors[i];
		if (!IsValid(A)) continue;
		A->SetActorHiddenInGame(bShown ? RealStates[i].bHidden : true);
		// only the end wall (wholly past the cut + one bay) stops blocking; the deck B parapet blocks behind the bays keep their
		// collision and are only hidden (their grid faces would show through the slightly larger lead-in bay)
		const bool bEndWall = A->GetComponentsBoundingBox(true).Min.Y > CutY + 500.0f;
		A->SetActorEnableCollision(bShown ? RealStates[i].bCollision : (bEndWall ? false : RealStates[i].bCollision));
	}
}

void AP9EndlessFootbridge::Engage(const FVector& Feet)
{
	// the copy goes in exactly where the original is: nothing visibly changes this frame
	const TArray<UMaterialInterface*> NoMats;
	SouthComp = AddMesh(Root, SouthMesh, FTransform::Identity, false, NoMats);
	EndRoot = NewObject<USceneComponent>(this, TEXT("EndRoot"), RF_Transient);
	EndRoot->SetMobility(EComponentMobility::Movable);
	EndRoot->SetupAttachment(Root);
	EndRoot->SetRelativeLocation(FVector(0.0f, CutY, 0.0f));
	EndRoot->RegisterComponent();
	AddMesh(EndRoot, EndMesh, FTransform::Identity, false, NoMats);
	P9Endless::AddPieceCollision(this, EndRoot, FVector::ZeroVector, EndLength, true);
	AddMesh(EndRoot, LeadInMesh, FTransform(FVector(0.0f, -BayLength, 0.0f)), false, NoMats);
	// the end's furniture: copies of the real sign / tube light / CCTV meshes and of the end's light, same relative placement
	for (AActor* A : RealActors)
	{
		if (!IsValid(A)) continue;
		TArray<UStaticMeshComponent*> Meshes;
		A->GetComponents<UStaticMeshComponent>(Meshes);
		for (const UStaticMeshComponent* M : Meshes)
		{
			const FString Name = M->GetStaticMesh() ? M->GetStaticMesh()->GetName() : FString();
			if (Name.IsEmpty() || Name.Contains(TEXT("FOBShell")) || Name.Contains(TEXT("FOBFloor")) || Name == TEXT("Cube")) continue;
			const FTransform W = M->GetComponentTransform();
			if (W.GetLocation().Y < CutY) continue;
			TArray<UMaterialInterface*> Mats;
			for (int32 s = 0; s < M->GetNumMaterials(); ++s) Mats.Add(M->GetMaterial(s));
			AddMesh(EndRoot, M->GetStaticMesh(), W.GetRelativeTransform(FTransform(FVector(0.0f, CutY, 0.0f))), false, Mats);
		}
	}
	UPointLightComponent* EndLight = NewObject<UPointLightComponent>(this, TEXT("EndLight"), RF_Transient);
	EndLight->SetMobility(EComponentMobility::Movable);
	EndLight->SetupAttachment(EndRoot);
	EndLight->SetRelativeLocation(LightRel);
	EndLight->SetIntensityUnits(ELightUnits::Candelas);
	EndLight->SetIntensity(LightIntensity);
	EndLight->SetAttenuationRadius(LightRadius);
	EndLight->SetUseTemperature(true);
	EndLight->SetTemperature(LightTemperature);
	EndLight->SetCastShadows(false);
	EndLight->RegisterComponent();
	SetRealEndShown(false);
	bEngaged = true;
	EngageY = Feet.Y;
	MaxY = Feet.Y;
	Extension = 0.0f;
	AwayTime = 0.0f;
	++Engagements;
	UE_LOG(LogP9, Log, TEXT("Endless footbridge: ENGAGED at y %.0f (the end stays %.0f cm ahead), engagement %d"), Feet.Y, CutY - Feet.Y, Engagements);
}

void AP9EndlessFootbridge::AddBay()
{
	const TArray<UMaterialInterface*> NoMats;
	const int32 i = Bays.Num();
	Bays.Add(AddMesh(Root, BayMesh, FTransform(FVector(0.0f, CutY + i * BayLength, 0.0f)), false, NoMats));
	P9Endless::AddPieceCollision(this, Root, FVector(0.0f, CutY + i * BayLength, 0.0f), BayLength, false);
	// a tube light in the middle of every bay (the real deck has one about every 7-8 m); the lamp is lit only near the player
	if (Tube)
	{
		AddMesh(Root, Tube, FTransform(FRotator(0.0f, 90.0f, 0.0f), FVector(0.0f, CutY + (i + 0.5f) * BayLength, 900.0f)), false, NoMats);
	}
	UPointLightComponent* L = NewObject<UPointLightComponent>(this, NAME_None, RF_Transient);
	L->SetMobility(EComponentMobility::Movable);
	L->SetupAttachment(Root);
	L->SetRelativeLocation(FVector(0.0f, CutY + (i + 0.5f) * BayLength, 840.0f));
	L->SetIntensityUnits(ELightUnits::Candelas);
	L->SetIntensity(LightIntensity);
	L->SetAttenuationRadius(LightRadius);
	L->SetUseTemperature(true);
	L->SetTemperature(LightTemperature);
	L->SetCastShadows(false);
	L->RegisterComponent();
	BayLights.Add(L);
}

void AP9EndlessFootbridge::Collapse()
{
	// unseen: the player is well south on the bridge, looking away from the north end
	for (UActorComponent* C : GetComponents().Array())
	{
		if (C && C != Root) C->DestroyComponent();
	}
	Bays.Reset(); BayLights.Reset(); SouthComp = nullptr; EndRoot = nullptr;
	SetRealEndShown(true);
	bEngaged = false;
	UE_LOG(LogP9, Log, TEXT("Endless footbridge: collapsed back to the real end (unseen) after %.0f cm of extension"), Extension);
	Extension = 0.0f;
}

void AP9EndlessFootbridge::Restore()
{
	if (bEngaged) Collapse();
	SetRealEndShown(true);
	bSetup = false;
}

FVector AP9EndlessFootbridge::PlayerFeet(bool& bOk) const
{
	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	const ACharacter* C = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
	bOk = C != nullptr;
	return C ? C->GetActorLocation() - FVector(0.0f, 0.0f, C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()) : FVector::ZeroVector;
}

void AP9EndlessFootbridge::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bSetup) return;
	bool bOk = false;
	const FVector Feet = PlayerFeet(bOk);
	if (!bOk) return;
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const APawn* Pawn = PC->GetPawn();
	const float Along = FVector::DotProduct(Pawn->GetVelocity(), North);
	if (!bEngaged)
	{
		if (Trigger.IsInsideOrOn(Feet) && Along > 30.0f)
		{
			Engage(Feet);
		}
		return;
	}
	// the end recedes only while the player gains ground beyond their furthest point
	const bool bOnDeck = Feet.Z > 540.0f && Feet.Z < 760.0f && FMath::Abs(Feet.X) < 260.0f;
	if (bOnDeck && Feet.Y > MaxY && Bays.Num() < MaxBays)
	{
		MaxY = Feet.Y;
	}
	Extension = FMath::Max(0.0f, MaxY - EngageY);
	if (Bays.Num() >= MaxBays) Extension = FMath::Min(Extension, MaxBays * BayLength);
	if (EndRoot) EndRoot->SetRelativeLocation(FVector(0.0f, CutY + Extension, 0.0f));
	// a fixed bay goes in once the end has moved fully past it (never under the player: the end stays the engage distance ahead)
	while (Bays.Num() < MaxBays && (Bays.Num() + 1) * BayLength <= Extension) AddBay();
	if ((LightCheck -= DeltaSeconds) <= 0.0f)
	{
		LightCheck = 0.25f;
		for (UPointLightComponent* L : BayLights)
		{
			if (L) L->SetVisibility(FVector::Dist(L->GetComponentLocation(), Feet) < LitRange);
		}
	}
	// collapse back to the original end when the player is far back south and looking away from the north
	const FVector Look = PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraRotation().Vector() : FVector::ForwardVector;
	if (Feet.Y < CollapseBelowY && FVector::DotProduct(Look.GetSafeNormal2D(), North) < -0.2f)
	{
		if ((AwayTime += DeltaSeconds) > CollapseSeconds) Collapse();
	}
	else
	{
		AwayTime = 0.0f;
	}
}

FString AP9EndlessFootbridge::GetStatus() const
{
	return FString::Printf(TEXT("engaged=%d engagements=%d extension=%.0f bays=%d maxY=%.0f"), bEngaged ? 1 : 0, Engagements, Extension, Bays.Num(), MaxY);
}

void AP9EndlessFootbridge::EndPlay(const EEndPlayReason::Type Reason)
{
	SetRealEndShown(true);
	Super::EndPlay(Reason);
}
