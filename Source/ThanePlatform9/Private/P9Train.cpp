#include "P9Train.h"
#include "ThanePlatform9.h"
#include "P9InteractableComponent.h"
#include "P9SeatComponent.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Canvas.h"
#include "Engine/CanvasRenderTarget2D.h"
#include "Engine/Engine.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"

// Car geometry (cm), mirrored from the Blender layout (train_layout2.py) that built the v003 meshes.
namespace P9TrainLayout
{
	constexpr float CarPitch = 2133.0f;            // 20.73 m body + 2 x 0.30 m couplers
	constexpr float HalfBody = 1036.5f;
	constexpr float Coupler = 30.0f;
	constexpr float HalfWidth = 183.0f;
	constexpr float Floor = 110.0f;
	constexpr float Lining = 10.0f;
	constexpr float Cantrail = 330.0f;
	constexpr float RoofTop = 400.0f;
	constexpr float CabPartitionX = 885.0f;
	constexpr float DoorX[3] = {-690.0f, 0.0f, 690.0f};
	constexpr float DoorHalfWidth = 65.0f;
	constexpr float LeafHalfWidth = 32.5f;
	constexpr float LeafY = 178.0f;
	constexpr float LeafTravel = 62.0f;
	constexpr float BogieX = 730.0f;
	constexpr float AxleX = 145.0f;
	constexpr float WheelRadius = 47.6f;
	constexpr float CapsuleHalfHeight = 88.0f;
	const TCHAR* DefaultDestination = TEXT("PALSHET  S");
	// seats (car-local): transverse bench backs + facing per bay (mirrored for x < 0), longitudinal end benches
	constexpr float BenchBacks[4][2] = {{110.0f, 1.0f}, {290.0f, -1.0f}, {300.0f, 1.0f}, {482.0f, -1.0f}};
	constexpr float SeatY[2] = {80.0f, 140.0f};
	constexpr float SeatSurface = 46.0f;
}
using namespace P9TrainLayout;

static const TCHAR* TrainStateName(EP9TrainState S)
{
	switch (S)
	{
	case EP9TrainState::StoppedAtPlatform: return TEXT("StoppedAtPlatform");
	case EP9TrainState::DoorsOpening: return TEXT("DoorsOpening");
	case EP9TrainState::DoorsOpen: return TEXT("DoorsOpen");
	case EP9TrainState::DoorsClosing: return TEXT("DoorsClosing");
	case EP9TrainState::ReadyToDepart: return TEXT("ReadyToDepart");
	case EP9TrainState::Accelerating: return TEXT("Accelerating");
	case EP9TrainState::Cruising: return TEXT("Cruising");
	case EP9TrainState::Braking: return TEXT("Braking");
	case EP9TrainState::Arriving: return TEXT("Arriving");
	}
	return TEXT("?");
}

static const TCHAR* DoorStateName(EP9DoorState S)
{
	switch (S)
	{
	case EP9DoorState::Closed: return TEXT("Closed");
	case EP9DoorState::Opening: return TEXT("Opening");
	case EP9DoorState::Open: return TEXT("Open");
	case EP9DoorState::Warning: return TEXT("Warning");
	case EP9DoorState::Closing: return TEXT("Closing");
	}
	return TEXT("?");
}

static void SetMaterialBySlotPrefix(UStaticMeshComponent* Comp, const TCHAR* Prefix, UMaterialInterface* Material)
{
	if (!Comp || !Material) return;
	const TArray<FName> Names = Comp->GetMaterialSlotNames();
	for (int32 i = 0; i < Names.Num(); ++i)
	{
		if (Names[i].ToString().StartsWith(Prefix))
		{
			Comp->SetMaterial(i, Material);
		}
	}
}

static UAudioComponent* MakeAudio(AActor* Owner, USceneComponent* Parent, const TCHAR* Name, float Falloff)
{
	UAudioComponent* Audio = Owner->CreateDefaultSubobject<UAudioComponent>(Name);
	Audio->SetupAttachment(Parent);
	Audio->bAutoActivate = false;
	Audio->bOverrideAttenuation = true;
	Audio->AttenuationOverrides.bAttenuate = true;
	Audio->AttenuationOverrides.bSpatialize = true;
	Audio->AttenuationOverrides.AttenuationShapeExtents = FVector(300.0f, 0.0f, 0.0f);
	Audio->AttenuationOverrides.FalloffDistance = Falloff;
	return Audio;
}

AP9Train::AP9Train()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;      // before CharacterMovement, which follows its moving base
	Tags.Add(TEXT("P9Train"));

	const TCHAR* T = TEXT("/Game/P9/Environment/Trains/");
	ShellMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(FString(T) + TEXT("SM_P9_LocalTrain_CabShell.SM_P9_LocalTrain_CabShell")));
	InteriorMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(FString(T) + TEXT("SM_P9_TrainInterior_Cab.SM_P9_TrainInterior_Cab")));
	BogieFrameMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(FString(T) + TEXT("SM_P9_TrainBogieFrame.SM_P9_TrainBogieFrame")));
	WheelsetMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(FString(T) + TEXT("SM_P9_TrainWheelset.SM_P9_TrainWheelset")));
	DoorLeafMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(FString(T) + TEXT("SM_P9_TrainDoorLeaf.SM_P9_TrainDoorLeaf")));
	LightsAtlas = TSoftObjectPtr<UTexture>(FSoftObjectPath(FString(T) + TEXT("Textures/T_P9_TrainLights.T_P9_TrainLights")));
	NumberMaterials = {
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(FString(T) + TEXT("Materials/MI_P9_Train_Number_9104C.MI_P9_Train_Number_9104C"))),
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(FString(T) + TEXT("Materials/MI_P9_Train_Number_9105B.MI_P9_Train_Number_9105B")))};
	auto Sound = [T](const TCHAR* Name) { return TSoftObjectPtr<USoundBase>(FSoftObjectPath(FString::Printf(TEXT("%sAudio/%s.%s"), T, Name, Name))); };
	DoorOpenSound = Sound(TEXT("SW_P9_TrainDoorOpen"));
	DoorCloseSound = Sound(TEXT("SW_P9_TrainDoorClose"));
	DoorWarningSound = Sound(TEXT("SW_P9_TrainDoorWarning"));
	DepartChimeSound = Sound(TEXT("SW_P9_TrainDepartChime"));
	RunLoopSound = Sound(TEXT("SW_P9_TrainRunLoop"));
	BrakeSound = Sound(TEXT("SW_P9_TrainBrake"));
	HornSound = Sound(TEXT("SW_P9_TrainHorn"));

	// default route: T2 through PF2/3 heading +X, a 140 m radius turn south, back west 280 m south of T2, and a turn back in
	auto Straight = [](float L) { FP9RouteSegment S; S.Length = L; return S; };
	auto Arc = [](float R, float A) { FP9RouteSegment S; S.Radius = R; S.AngleDegrees = A; return S; };
	RouteSegments = {Straight(12000.0f), Arc(14000.0f, -180.0f), Straight(12000.0f), Arc(14000.0f, -180.0f)};
	StopDistances = {6000.0f};                       // train centre at X 0 on T2, beside PF2/3

	TrainRoot = CreateDefaultSubobject<USceneComponent>(TEXT("TrainRoot"));
	TrainRoot->SetMobility(EComponentMobility::Movable);
	RootComponent = TrainRoot;

	for (int32 Car = 0; Car < NumCars; ++Car)
	{
		USceneComponent* CarRoot = CreateDefaultSubobject<USceneComponent>(*FString::Printf(TEXT("Car%d"), Car));
		CarRoot->SetupAttachment(TrainRoot);
		CarRoot->SetMobility(EComponentMobility::Movable);
		CarRoots.Add(CarRoot);

		auto MakeMesh = [this](USceneComponent* Parent, const FString& Name, bool bCollide, bool bShadow)
		{
			UStaticMeshComponent* Mesh = CreateDefaultSubobject<UStaticMeshComponent>(*Name);
			Mesh->SetupAttachment(Parent);
			Mesh->SetMobility(EComponentMobility::Movable);
			Mesh->SetCollisionProfileName(bCollide ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
			Mesh->SetCastShadow(bShadow);
			Mesh->SetCanEverAffectNavigation(false);
			return Mesh;
		};
		Shells.Add(MakeMesh(CarRoot, FString::Printf(TEXT("Car%d_Shell"), Car), true, true));
		Interiors.Add(MakeMesh(CarRoot, FString::Printf(TEXT("Car%d_Interior"), Car), true, false));
		for (int32 B = 0; B < 2; ++B)
		{
			UStaticMeshComponent* Frame = MakeMesh(CarRoot, FString::Printf(TEXT("Car%d_Bogie%d"), Car, B), false, false);
			BogieFrames.Add(Frame);
			for (int32 A = 0; A < 2; ++A)
			{
				Wheelsets.Add(MakeMesh(Frame, FString::Printf(TEXT("Car%d_Bogie%d_Wheelset%d"), Car, B, A), false, false));   // ride on their bogie
			}
		}
		for (int32 Side : {1, -1})
		{
			for (int32 D = 0; D < 3; ++D)
			{
				for (int32 Dir : {-1, 1})
				{
					FP9DoorLeaf Leaf;
					Leaf.Comp = MakeMesh(CarRoot, FString::Printf(TEXT("Car%d_Door%d_%s_%s"), Car, D, Side > 0 ? TEXT("PosY") : TEXT("NegY"), Dir < 0 ? TEXT("L") : TEXT("R")), true, true);
					Leaf.Car = Car; Leaf.Door = D; Leaf.Side = Side; Leaf.Dir = Dir;
					Leaves.Add(Leaf);
				}
			}
		}
		for (int32 L = 0; L < 2; ++L)
		{
			UPointLightComponent* Light = CreateDefaultSubobject<UPointLightComponent>(*FString::Printf(TEXT("Car%d_InteriorLight%d"), Car, L));
			Light->SetupAttachment(CarRoot);
			Light->SetMobility(EComponentMobility::Movable);
			Light->SetCastShadows(false);
			Light->SetIntensityUnits(ELightUnits::Candelas);
			Light->SetLightColor(FLinearColor(0.86f, 0.92f, 1.0f));
			InteriorLights.Add(Light);
		}
		for (int32 D = 0; D < 3; ++D)
		{
			UP9InteractableComponent* Interactable = CreateDefaultSubobject<UP9InteractableComponent>(*FString::Printf(TEXT("Car%d_DoorPrompt%d"), Car, D));
			Interactable->SetupAttachment(CarRoot);
			Interactable->InteractRadius = 230.0f;
			DoorInteractables.Add(Interactable);
		}
		DoorAudio.Add(MakeAudio(this, CarRoot, *FString::Printf(TEXT("Car%d_DoorAudio"), Car), 2500.0f));
	}
	ChimeAudio = MakeAudio(this, TrainRoot, TEXT("ChimeAudio"), 3500.0f);
	RunAudio = MakeAudio(this, TrainRoot, TEXT("RunAudio"), 4000.0f);
	BrakeAudio = MakeAudio(this, TrainRoot, TEXT("BrakeAudio"), 4000.0f);
	HornAudio = MakeAudio(this, TrainRoot, TEXT("HornAudio"), 9000.0f);
}

// ---------------- route ----------------
void AP9Train::BuildRoute()
{
	Segs.Reset();
	FVector P = RouteStart;
	float Yaw = RouteStartYaw;
	float Dist = 0.0f;
	for (const FP9RouteSegment& In : RouteSegments)
	{
		FRouteSeg S;
		S.Start = P; S.StartYaw = Yaw; S.StartDist = Dist; S.Radius = In.Radius; S.Sign = In.AngleDegrees < 0.0f ? -1.0f : 1.0f;
		const float Rad = FMath::DegreesToRadians(Yaw);
		if (In.Radius > 1.0f)
		{
			S.Length = In.Radius * FMath::DegreesToRadians(FMath::Abs(In.AngleDegrees));
			S.Center = P + S.Sign * In.Radius * FVector(-FMath::Sin(Rad), FMath::Cos(Rad), 0.0f);
			Yaw += In.AngleDegrees;
			const float EndRad = FMath::DegreesToRadians(Yaw);
			P = S.Center + S.Sign * In.Radius * FVector(FMath::Sin(EndRad), -FMath::Cos(EndRad), 0.0f);
		}
		else
		{
			S.Length = FMath::Max(0.0f, In.Length);
			P += FVector(FMath::Cos(Rad), FMath::Sin(Rad), 0.0f) * S.Length;
		}
		Dist += S.Length;
		Segs.Add(S);
	}
	RouteLength = Dist;
	ClosureGap = FVector::Dist(P, RouteStart);
	ClosureYawError = FMath::Abs(FRotator::NormalizeAxis(Yaw - RouteStartYaw));
	bRouteClosed = RouteLength > 100.0f && ClosureGap < 1.0f && ClosureYawError < 0.1f;
}

void AP9Train::EvaluateRoute(float InDistance, FVector& OutLocation, float& OutYaw) const
{
	if (Segs.Num() == 0 || RouteLength <= 0.0f)
	{
		OutLocation = RouteStart; OutYaw = RouteStartYaw;
		return;
	}
	float D = FMath::Fmod(InDistance, RouteLength);
	if (D < 0.0f) D += RouteLength;
	const FRouteSeg* S = &Segs.Last();
	for (const FRouteSeg& Seg : Segs)
	{
		if (D < Seg.StartDist + Seg.Length) { S = &Seg; break; }
	}
	const float Local = D - S->StartDist;
	if (S->Radius > 1.0f)
	{
		OutYaw = S->StartYaw + S->Sign * FMath::RadiansToDegrees(Local / S->Radius);
		const float Rad = FMath::DegreesToRadians(OutYaw);
		OutLocation = S->Center + S->Sign * S->Radius * FVector(FMath::Sin(Rad), -FMath::Cos(Rad), 0.0f);
	}
	else
	{
		OutYaw = S->StartYaw;
		const float Rad = FMath::DegreesToRadians(OutYaw);
		OutLocation = S->Start + FVector(FMath::Cos(Rad), FMath::Sin(Rad), 0.0f) * Local;
	}
}

void AP9Train::PlaceTrain(float TrainDistance)
{
	FVector P; float Yaw;
	EvaluateRoute(TrainDistance, P, Yaw);
	SetActorLocationAndRotation(P, FRotator(0.0f, Yaw, 0.0f), false, nullptr, ETeleportType::None);
	for (int32 Car = 0; Car < NumCars; ++Car)
	{
		const float Facing = Car == 0 ? 1.0f : -1.0f;                // car 1 runs turned round (cab towards the rear)
		const float Centre = TrainDistance + (Car == 0 ? CarPitch * 0.5f : -CarPitch * 0.5f);
		FVector Front, Rear; float YawF, YawR;
		EvaluateRoute(Centre + BogieX, Front, YawF);
		EvaluateRoute(Centre - BogieX, Rear, YawR);
		const float CarYaw = (Front - Rear).GetSafeNormal2D().Rotation().Yaw + (Car == 0 ? 0.0f : 180.0f);
		CarRoots[Car]->SetWorldLocationAndRotation((Front + Rear) * 0.5f, FRotator(0.0f, CarYaw, 0.0f), false, nullptr, ETeleportType::None);
		for (int32 B = 0; B < 2; ++B)
		{
			FVector BP; float BYaw;
			EvaluateRoute(Centre + (B == 0 ? BogieX : -BogieX) * Facing, BP, BYaw);
			BogieFrames[Car * 2 + B]->SetWorldLocationAndRotation(BP, FRotator(0.0f, BYaw + (Car == 0 ? 0.0f : 180.0f), 0.0f), false, nullptr, ETeleportType::None);
		}
	}
}

// ---------------- layout ----------------
int32 AP9Train::PlatformLocalSide(int32 Car) const
{
	const FVector LocalY = CarRoots[Car]->GetComponentTransform().TransformVectorNoScale(FVector::RightVector);
	return (LocalY.Y * PlatformSideWorldY) >= 0.0f ? 1 : -1;
}

void AP9Train::ApplyLayout()
{
	UStaticMesh* Shell = ShellMesh.LoadSynchronous();
	UStaticMesh* Interior = InteriorMesh.LoadSynchronous();
	UStaticMesh* Frame = BogieFrameMesh.LoadSynchronous();
	UStaticMesh* Wheelset = WheelsetMesh.LoadSynchronous();
	UStaticMesh* LeafMesh = DoorLeafMesh.LoadSynchronous();
	for (int32 Car = 0; Car < NumCars; ++Car)
	{
		CarRoots[Car]->SetRelativeLocationAndRotation(FVector(Car == 0 ? CarPitch * 0.5f : -CarPitch * 0.5f, 0.0f, 0.0f), FRotator(0.0f, Car == 0 ? 0.0f : 180.0f, 0.0f));
		Shells[Car]->SetStaticMesh(Shell);
		Interiors[Car]->SetStaticMesh(Interior);
		if (NumberMaterials.IsValidIndex(Car))
		{
			SetMaterialBySlotPrefix(Shells[Car], TEXT("M_P9_TrainNumber"), NumberMaterials[Car].LoadSynchronous());
		}
		for (int32 B = 0; B < 2; ++B)
		{
			UStaticMeshComponent* F = BogieFrames[Car * 2 + B];
			F->SetStaticMesh(Frame);
			F->SetRelativeLocationAndRotation(FVector(B == 0 ? BogieX : -BogieX, 0.0f, 0.0f), FRotator::ZeroRotator);
			for (int32 A = 0; A < 2; ++A)
			{
				UStaticMeshComponent* W = Wheelsets[(Car * 2 + B) * 2 + A];
				W->SetStaticMesh(Wheelset);
				W->SetRelativeLocation(FVector(A == 0 ? AxleX : -AxleX, 0.0f, WheelRadius));
			}
		}
		for (int32 L = 0; L < 2; ++L)
		{
			InteriorLights[Car * 2 + L]->SetRelativeLocation(FVector(L == 0 ? 450.0f : -450.0f, 0.0f, 300.0f));
			InteriorLights[Car * 2 + L]->SetIntensity(InteriorLightIntensity);
			InteriorLights[Car * 2 + L]->SetAttenuationRadius(InteriorLightRadius);
		}
		const int32 PSide = PlatformLocalSide(Car);
		for (int32 D = 0; D < 3; ++D)
		{
			DoorInteractables[Car * 3 + D]->SetRelativeLocation(FVector(DoorX[D], PSide * HalfWidth, Floor + 100.0f));
		}
		DoorAudio[Car]->SetRelativeLocation(FVector(0.0f, PSide * HalfWidth, Floor + 150.0f));
	}
	for (FP9DoorLeaf& Leaf : Leaves)
	{
		Leaf.ClosedLocation = FVector(DoorX[Leaf.Door] + Leaf.Dir * LeafHalfWidth, Leaf.Side * LeafY, Floor);
		Leaf.Comp->SetStaticMesh(LeafMesh);
		Leaf.Comp->SetRelativeLocationAndRotation(Leaf.ClosedLocation, FRotator(0.0f, Leaf.Side > 0 ? 0.0f : 180.0f, 0.0f));
	}
	ApplyDoorAlpha();
}

void AP9Train::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyLayout();
	BuildRoute();
}

void AP9Train::CreateSeats()
{
	if (!bCreateSeats) return;
	struct FSeatDef { FVector Loc; float Yaw; FVector Stand; };
	TArray<FSeatDef> Defs;
	for (float Mx : {1.0f, -1.0f})
	{
		for (const auto& Bench : BenchBacks)
		{
			const float XBack = Mx * Bench[0], F = Mx * Bench[1];
			const float HipX = XBack + F * 25.0f;
			for (float Sy : {1.0f, -1.0f})
			{
				for (float Y : SeatY)
				{
					Defs.Add({FVector(HipX, Sy * Y, Floor + SeatSurface), F > 0.0f ? 0.0f : 180.0f, FVector(HipX + F * 67.0f, 0.0f, Floor + CapsuleHalfHeight + 2.0f)});
				}
			}
		}
	}
	for (float Sy : {1.0f, -1.0f})
	{
		for (float X : {-985.0f, -905.0f, -825.0f, 800.0f, 855.0f})        // rear end benches, short benches by the cab partition
		{
			Defs.Add({FVector(X, Sy * 150.0f, Floor + SeatSurface), Sy > 0.0f ? -90.0f : 90.0f, FVector(X, Sy * 60.0f, Floor + CapsuleHalfHeight + 2.0f)});
		}
	}
	for (int32 Car = 0; Car < NumCars; ++Car)
	{
		for (int32 i = 0; i < Defs.Num(); ++i)
		{
			UP9SeatComponent* Seat = NewObject<UP9SeatComponent>(this, *FString::Printf(TEXT("Car%d_Seat%02d"), Car, i));
			Seat->SetupAttachment(CarRoots[Car]);
			Seat->SetRelativeLocationAndRotation(Defs[i].Loc, FRotator(0.0f, Defs[i].Yaw, 0.0f));
			Seat->StandOffset = FTransform(FRotator(0.0f, Defs[i].Yaw, 0.0f), Defs[i].Loc).InverseTransformPosition(Defs[i].Stand);
			Seat->InteractRadius = 120.0f;                                   // reachable from the aisle beside the bench
			Seat->RegisterComponent();
			Seats.Add(Seat);
		}
	}
}

void AP9Train::BeginPlay()
{
	Super::BeginPlay();
	ApplyLayout();
	BuildRoute();
	if (StopDistances.Num() == 0) StopDistances.Add(0.0f);
	CurrentStopIndex = TargetStopIndex = FMath::Clamp(StartStopIndex, 0, StopDistances.Num() - 1);
	Distance = TargetDistance = StopDistance(CurrentStopIndex);
	PlaceTrain(Distance);
	CreateSeats();
	FindRouteBlockers();
	for (UP9InteractableComponent* Interactable : DoorInteractables)
	{
		Interactable->OnInteract.AddDynamic(this, &AP9Train::HandleDoorInteract);
	}
	if (USoundBase* Loop = RunLoopSound.LoadSynchronous())
	{
		RunAudio->SetSound(Loop);
	}
	SetupDestinationBoard();
	UpdatePlayerAndPrompts();
	UE_LOG(LogP9, Log, TEXT("Train %s: route %.0f cm in %d segments, closed=%s (gap %.3f cm, heading %.4f deg), stop %d at %.0f, %d seats, %d route blockers"),
		*GetName(), RouteLength, Segs.Num(), bRouteClosed ? TEXT("yes") : TEXT("NO"), ClosureGap, ClosureYawError, CurrentStopIndex, Distance, Seats.Num(), RouteBlockers.Num());
}

void AP9Train::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetPlayerIgnoresBlockers(false);
	for (FKnockback& K : Knockbacks)
	{
		if (K.bInputBlocked && K.Character.IsValid() && K.Character->GetController())
		{
			K.Character->GetController()->SetIgnoreMoveInput(false);
		}
	}
	Knockbacks.Reset();
	Super::EndPlay(EndPlayReason);
}

void AP9Train::FindRouteBlockers()
{
	RouteBlockers.Reset();
	UWorld* World = GetWorld();
	if (!World || RouteLength <= 0.0f) return;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(P9TrainRouteBlockers), false, this);
	const FCollisionShape Box = FCollisionShape::MakeBox(FVector(160.0f, HalfWidth - Lining, 110.0f));
	for (float D = 0.0f; D < RouteLength; D += 300.0f)
	{
		FVector P; float Yaw;
		EvaluateRoute(D, P, Yaw);
		TArray<FOverlapResult> Overlaps;
		World->OverlapMultiByChannel(Overlaps, P + FVector(0.0f, 0.0f, Floor + 110.0f), FQuat(FRotator(0.0f, Yaw, 0.0f)), ECC_Pawn, Box, Params);
		for (const FOverlapResult& O : Overlaps)
		{
			const UPrimitiveComponent* Comp = O.GetComponent();
			AActor* Actor = O.GetActor();
			if (!Comp || !Actor || Actor == this || Actor->IsA<APawn>() || Comp->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Block) continue;
			if (!RouteBlockers.Contains(Actor))
			{
				RouteBlockers.Add(Actor);
				UE_LOG(LogP9, Log, TEXT("Train: route passes blocker %s (%s) at %.0f cm - ignored by the player only while aboard"), *Actor->GetActorNameOrLabel(), *Comp->GetName(), D);
			}
		}
	}
}

void AP9Train::SetPlayerIgnoresBlockers(bool bIgnore)
{
	if (bIgnore == bIgnoringBlockers) return;
	ACharacter* Character = Cast<ACharacter>(PlayerPawn());
	if (!Character) { bIgnoringBlockers = false; return; }
	for (const TWeakObjectPtr<AActor>& Blocker : RouteBlockers)
	{
		if (AActor* Actor = Blocker.Get())
		{
			Character->GetCapsuleComponent()->IgnoreActorWhenMoving(Actor, bIgnore);
		}
	}
	bIgnoringBlockers = bIgnore;
}

// ---------------- stops & state ----------------
float AP9Train::StopDistance(int32 Index) const
{
	if (!StopDistances.IsValidIndex(Index) || RouteLength <= 0.0f) return Distance;
	float D = FMath::Fmod(StopDistances[Index], RouteLength);
	return D < 0.0f ? D + RouteLength : D;
}

bool AP9Train::AtStop(int32 StopIndex) const
{
	return StopDistances.IsValidIndex(StopIndex) && FMath::Abs(Distance - StopDistance(StopIndex)) < 2.0f && Speed <= KINDA_SMALL_NUMBER;
}

int32 AP9Train::NextStopIndex() const
{
	return StopDistances.Num() > 0 ? (CurrentStopIndex + 1) % StopDistances.Num() : 0;
}

bool AP9Train::IsMoving() const
{
	return Speed > KINDA_SMALL_NUMBER || TrainState == EP9TrainState::Accelerating || TrainState == EP9TrainState::Cruising
		|| TrainState == EP9TrainState::Braking || TrainState == EP9TrainState::Arriving;
}

void AP9Train::SetState(EP9TrainState NewState)
{
	if (NewState == TrainState) return;
	const EP9TrainState Old = TrainState;
	TrainState = NewState;
	StateTime = 0.0f;
	UE_LOG(LogP9, Log, TEXT("Train: %s -> %s (distance %.0f, speed %.0f)"), TrainStateName(Old), TrainStateName(NewState), Distance, Speed);
	OnTrainStateChanged.Broadcast(Old, NewState);
}

void AP9Train::SetDoorState(EP9DoorState NewState)
{
	if (NewState == DoorState) return;
	DoorState = NewState;
	DoorPhaseTime = 0.0f;
	UE_LOG(LogP9, Log, TEXT("Train doors: %s (alpha %.2f)"), DoorStateName(NewState), DoorAlpha);
	OnDoorStateChanged.Broadcast(NewState);
}

void AP9Train::PlayOneShot(UAudioComponent* Audio, const TSoftObjectPtr<USoundBase>& Sound)
{
	if (USoundBase* S = Sound.LoadSynchronous())
	{
		Audio->SetSound(S);
		Audio->Play();
	}
}

bool AP9Train::RequestOpenDoors()
{
	const bool bStateOk = TrainState == EP9TrainState::StoppedAtPlatform || TrainState == EP9TrainState::ReadyToDepart || TrainState == EP9TrainState::DoorsClosing;
	if (!bStateOk || IsMoving() || !AtStop(CurrentStopIndex))
	{
		UE_LOG(LogP9, Log, TEXT("Train: open refused (%s, speed %.0f, at stop %s)"), TrainStateName(TrainState), Speed, AtStop(CurrentStopIndex) ? TEXT("yes") : TEXT("no"));
		return false;
	}
	if (DoorState == EP9DoorState::Open || DoorState == EP9DoorState::Opening) return true;
	bDeparturePending = false;
	SetDoorState(EP9DoorState::Opening);
	SetState(EP9TrainState::DoorsOpening);
	for (UAudioComponent* Audio : DoorAudio) PlayOneShot(Audio, DoorOpenSound);
	return true;
}

bool AP9Train::RequestCloseDoors()
{
	if (DoorState != EP9DoorState::Open && DoorState != EP9DoorState::Opening) return false;
	bDeparturePending = bDeparturePending || bAutoCycle;
	SetDoorState(EP9DoorState::Warning);
	SetState(EP9TrainState::DoorsClosing);
	PlayOneShot(ChimeAudio, DoorWarningSound);
	return true;
}

bool AP9Train::RequestDeparture()
{
	if (IsMoving()) return false;
	bDeparturePending = true;
	if (DoorState != EP9DoorState::Closed)
	{
		return RequestCloseDoors() || DoorState == EP9DoorState::Warning || DoorState == EP9DoorState::Closing;
	}
	SetState(EP9TrainState::ReadyToDepart);
	return true;
}

void AP9Train::SetDestinationText(const FString& NewText)
{
	DestinationText = NewText;
	SetupDestinationBoard();
}

void AP9Train::BeginDeparture()
{
	TargetStopIndex = NextStopIndex();
	float Target = StopDistance(TargetStopIndex);
	while (Target <= Distance + 1.0f) Target += RouteLength;      // always forward; a single stop means a full lap
	TargetDistance = Target;
	bDeparturePending = false;
	if (bHornOnDeparture) PlayOneShot(HornAudio, HornSound);
	UE_LOG(LogP9, Log, TEXT("Train: departing stop %d -> %d (%.0f -> %.0f cm, %.0f cm journey)"), CurrentStopIndex, TargetStopIndex, Distance, TargetDistance, TargetDistance - Distance);
	OnDeparted.Broadcast(CurrentStopIndex);
	SetState(EP9TrainState::Accelerating);
}

// ---------------- player ----------------
APawn* AP9Train::PlayerPawn() const
{
	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	return PC ? PC->GetPawn() : nullptr;
}

bool AP9Train::ComputePlayerAboard() const
{
	const APawn* Pawn = PlayerPawn();
	if (!Pawn) return false;
	if (const UP9SeatComponent* Seat = UP9SeatComponent::GetSeatOf(Pawn))
	{
		if (Seat->GetOwner() == this) return true;
	}
	for (int32 Car = 0; Car < NumCars; ++Car)
	{
		const FVector L = CarRoots[Car]->GetComponentTransform().InverseTransformPosition(Pawn->GetActorLocation());
		const float Feet = L.Z - CapsuleHalfHeight;
		if (L.X > -HalfBody + Lining && L.X < CabPartitionX && FMath::Abs(L.Y) < HalfWidth - Lining && Feet > Floor - 20.0f && Feet < Cantrail)
		{
			return true;
		}
	}
	return false;
}

bool AP9Train::IsPlayerInDoorway() const
{
	const APawn* Pawn = PlayerPawn();
	if (!Pawn) return false;
	for (int32 Car = 0; Car < NumCars; ++Car)
	{
		const FVector L = CarRoots[Car]->GetComponentTransform().InverseTransformPosition(Pawn->GetActorLocation());
		const float Out = L.Y * PlatformLocalSide(Car);
		const float Feet = L.Z - CapsuleHalfHeight;
		if (Out < HalfWidth - Lining - 45.0f || Out > HalfWidth + 40.0f || Feet < 40.0f || Feet > Floor + 60.0f) continue;
		for (float DX : DoorX)
		{
			if (FMath::Abs(L.X - DX) < DoorHalfWidth + 35.0f) return true;
		}
	}
	return false;
}

FString AP9Train::GetDepartureBlockReason() const
{
	if (DoorState != EP9DoorState::Closed || DoorAlpha > 0.0f) return TEXT("doors not closed and locked");
	if (IsPlayerInDoorway()) return TEXT("stand clear of the doors");
	if (!bRouteClosed) return TEXT("route is not a closed loop");
	return FString();
}

// ---------------- tick ----------------
void AP9Train::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	StateTime += DeltaSeconds;
	UpdateDoors(DeltaSeconds);
	UpdateMotion(DeltaSeconds);
	UpdateImpacts(DeltaSeconds);
	PromptTimer -= DeltaSeconds;
	if (PromptTimer <= 0.0f)
	{
		PromptTimer = 0.1f;
		UpdatePlayerAndPrompts();
	}
}

void AP9Train::ApplyDoorAlpha()
{
	const float Eased = FMath::SmoothStep(0.0f, 1.0f, DoorAlpha);
	for (const FP9DoorLeaf& Leaf : Leaves)
	{
		if (!Leaf.Comp) continue;
		const bool bPlatformSide = CarRoots.IsValidIndex(Leaf.Car) && Leaf.Side == PlatformLocalSide(Leaf.Car);
		const float Slide = bPlatformSide ? Leaf.Dir * LeafTravel * Eased : 0.0f;    // off-side doors stay locked shut
		Leaf.Comp->SetRelativeLocation(Leaf.ClosedLocation + FVector(Slide, 0.0f, 0.0f));
	}
}

void AP9Train::UpdateDoors(float DeltaSeconds)
{
	DoorPhaseTime += DeltaSeconds;
	switch (DoorState)
	{
	case EP9DoorState::Opening:
		DoorAlpha = FMath::Min(1.0f, DoorAlpha + DeltaSeconds / DoorOpenTime);
		ApplyDoorAlpha();
		if (DoorAlpha >= 1.0f)
		{
			SetDoorState(EP9DoorState::Open);
			SetState(EP9TrainState::DoorsOpen);
		}
		break;
	case EP9DoorState::Open:
		if (bAutoCycle && DoorPhaseTime >= DwellTime) RequestCloseDoors();
		break;
	case EP9DoorState::Warning:
		if (IsPlayerInDoorway())
		{
			DoorPhaseTime = 0.0f;                   // hold the warning while someone stands in a doorway
		}
		else if (DoorPhaseTime >= DoorWarningTime)
		{
			SetDoorState(EP9DoorState::Closing);
			for (UAudioComponent* Audio : DoorAudio) PlayOneShot(Audio, DoorCloseSound);
		}
		break;
	case EP9DoorState::Closing:
		if (IsPlayerInDoorway())
		{
			UE_LOG(LogP9, Log, TEXT("Train doors: obstruction - reopening"));
			const bool bPending = bDeparturePending;
			SetDoorState(EP9DoorState::Opening);
			SetState(EP9TrainState::DoorsOpening);
			bDeparturePending = bPending;
			break;
		}
		DoorAlpha = FMath::Max(0.0f, DoorAlpha - DeltaSeconds / DoorCloseTime);
		ApplyDoorAlpha();
		if (DoorAlpha <= 0.0f)
		{
			SetDoorState(EP9DoorState::Closed);
			SetState(bDeparturePending ? EP9TrainState::ReadyToDepart : EP9TrainState::StoppedAtPlatform);
		}
		break;
	default:
		break;
	}
	if (DoorState == EP9DoorState::Open && bDeparturePending && DoorPhaseTime >= ObstructionHoldTime)
	{
		RequestCloseDoors();                        // retry after an obstruction reopen
	}
}

void AP9Train::UpdateMotion(float DeltaSeconds)
{
	if (TrainState == EP9TrainState::StoppedAtPlatform && bArrivalOpenPending && StateTime >= AutoOpenDelay)
	{
		bArrivalOpenPending = false;
		RequestOpenDoors();
	}
	if (TrainState == EP9TrainState::ReadyToDepart)
	{
		const FString Reason = GetDepartureBlockReason();
		if (!Reason.IsEmpty())
		{
			if (Reason != LastBlockReason)
			{
				UE_LOG(LogP9, Log, TEXT("Train: departure held - %s"), *Reason);
				OnDepartureBlocked.Broadcast(Reason);
			}
			LastBlockReason = Reason;
			StateTime = 0.0f;
			bChimePlayed = false;
		}
		else
		{
			LastBlockReason.Reset();
			if (!bChimePlayed)
			{
				PlayOneShot(ChimeAudio, DepartChimeSound);
				bChimePlayed = true;
				StateTime = 0.0f;
			}
			else if (StateTime >= DepartChimeTime)
			{
				bChimePlayed = false;
				BeginDeparture();
			}
		}
	}

	const bool bMovingState = TrainState == EP9TrainState::Accelerating || TrainState == EP9TrainState::Cruising
		|| TrainState == EP9TrainState::Braking || TrainState == EP9TrainState::Arriving;
	if (bMovingState)
	{
		float Remaining = FMath::Max(0.0f, TargetDistance - Distance);
		switch (TrainState)
		{
		case EP9TrainState::Accelerating:
			Speed = FMath::Min(MaxSpeed, Speed + Acceleration * DeltaSeconds);
			if (Remaining <= Speed * Speed / (2.0f * ServiceDeceleration) + ArrivalDistance)
			{
				SetState(EP9TrainState::Braking);
				PlayOneShot(BrakeAudio, BrakeSound);
			}
			else if (Speed >= MaxSpeed)
			{
				SetState(EP9TrainState::Cruising);
			}
			break;
		case EP9TrainState::Cruising:
			if (Remaining <= Speed * Speed / (2.0f * ServiceDeceleration) + ArrivalDistance)
			{
				SetState(EP9TrainState::Braking);
				PlayOneShot(BrakeAudio, BrakeSound);
			}
			break;
		case EP9TrainState::Braking:
		{
			const float Needed = Speed * Speed / (2.0f * FMath::Max(Remaining - ArrivalDistance * 0.5f, 1.0f));
			Speed = FMath::Max(ArrivalSpeed, Speed - FMath::Clamp(Needed, 0.3f * ServiceDeceleration, 2.0f * ServiceDeceleration) * DeltaSeconds);
			if (Remaining <= ArrivalDistance) SetState(EP9TrainState::Arriving);
			break;
		}
		case EP9TrainState::Arriving:
			Speed = FMath::Clamp(FMath::Sqrt(2.0f * ServiceDeceleration * Remaining), 4.0f, FMath::Max(Speed, 4.0f));
			break;
		default:
			break;
		}

		const float Step = FMath::Min(Speed * DeltaSeconds, Remaining);
		Distance += Step;
		Remaining = FMath::Max(0.0f, TargetDistance - Distance);
		PlaceTrain(Distance);
		for (int32 Car = 0; Car < NumCars; ++Car)                // roll the wheelsets (relative to their bogie) with the distance travelled
		{
			WheelAngle[Car] = FMath::Fmod(WheelAngle[Car] + (Car == 0 ? 1.0f : -1.0f) * Step / WheelRadius, 2.0f * PI);
			for (int32 W = 0; W < 4; ++W)
			{
				Wheelsets[Car * 4 + W]->SetRelativeRotation(FRotator(-FMath::RadiansToDegrees(WheelAngle[Car]), 0.0f, 0.0f));
			}
		}
		if (Remaining <= 0.5f)
		{
			Distance = FMath::Fmod(TargetDistance, RouteLength);    // same point on the closed loop: no jump
			TargetDistance = Distance;
			Speed = 0.0f;
			PlaceTrain(Distance);
			CurrentStopIndex = TargetStopIndex;
			SetState(EP9TrainState::StoppedAtPlatform);
			UE_LOG(LogP9, Log, TEXT("Train: arrived at stop %d (distance %.1f, world %s)"), CurrentStopIndex, Distance, *GetActorLocation().ToCompactString());
			OnArrived.Broadcast(CurrentStopIndex);
			bArrivalOpenPending = bAutoCycle;
		}
	}

	if (RunAudio->Sound)
	{
		const float Ratio = FMath::Clamp(Speed / MaxSpeed, 0.0f, 1.0f);
		if (Speed > 1.0f)
		{
			if (!RunAudio->IsPlaying()) RunAudio->Play();
			RunAudio->SetVolumeMultiplier(0.25f + 0.75f * Ratio);
			RunAudio->SetPitchMultiplier(0.7f + 0.6f * Ratio);
		}
		else if (RunAudio->IsPlaying())
		{
			RunAudio->FadeOut(0.6f, 0.0f);
		}
	}
}

void AP9Train::UpdateImpacts(float DeltaSeconds)
{
	for (int32 i = Knockbacks.Num() - 1; i >= 0; --i)        // cooldowns and recovery
	{
		FKnockback& K = Knockbacks[i];
		K.Cooldown -= DeltaSeconds;
		K.Recovery -= DeltaSeconds;
		ACharacter* C = K.Character.Get();
		if (K.bInputBlocked && (!C || K.Recovery <= 0.0f || (K.Recovery < ImpactRecoveryTimeout - 0.25f && C->GetCharacterMovement()->IsMovingOnGround())))
		{
			if (C && C->GetController()) C->GetController()->SetIgnoreMoveInput(false);
			K.bInputBlocked = false;
			if (C) UE_LOG(LogP9, Log, TEXT("Train impact: %s recovered (%s) at %s"), *C->GetName(), *C->GetCharacterMovement()->GetMovementName(), *C->GetActorLocation().ToCompactString());
		}
		if (!K.bInputBlocked && K.Cooldown <= 0.0f) Knockbacks.RemoveAt(i);
	}
	if (!bEnableImpacts || Speed < 20.0f) return;

	const FVector Travel = GetActorForwardVector();
	for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
	{
		ACharacter* C = *It;
		const UPrimitiveComponent* Base = C->GetMovementBase();
		const UP9SeatComponent* Seat = UP9SeatComponent::GetSeatOf(C);
		if ((Base && Base->GetOwner() == this) || (Seat && Seat->GetOwner() == this)) continue;          // riding this train
		if (C == PlayerPawn() && bPlayerAboard) continue;
		if (Knockbacks.ContainsByPredicate([C](const FKnockback& K) { return K.Character.Get() == C; })) continue;
		const float Radius = C->GetCapsuleComponent()->GetScaledCapsuleRadius();
		const float Margin = 15.0f + Speed * DeltaSeconds;
		for (int32 Car = 0; Car < NumCars; ++Car)
		{
			const FVector L = CarRoots[Car]->GetComponentTransform().InverseTransformPosition(C->GetActorLocation());
			if (FMath::Abs(L.X) > HalfBody + Coupler + Radius + Margin || FMath::Abs(L.Y) > HalfWidth + Radius + Margin
				|| L.Z < -CapsuleHalfHeight || L.Z > RoofTop + CapsuleHalfHeight) continue;
			const float Lead = Car == 0 ? 1.0f : -1.0f;                 // car-local direction of travel
			const bool bFront = L.X * Lead > HalfBody - 60.0f;
			const FVector Lateral = CarRoots[Car]->GetRightVector() * (L.Y >= 0.0f ? 1.0f : -1.0f);
			FVector Horizontal = bFront
				? Travel * (Speed + ImpactBaseSpeed) + Lateral * (ImpactBaseSpeed * FrontHitSideBias + Speed * 0.3f)
				: Lateral * FMath::Min(ImpactBaseSpeed + Speed * ImpactSpeedScale, ImpactMaxSpeed) + Travel * (Speed * 0.5f);
			Horizontal = Horizontal.GetClampedToMaxSize2D(ImpactMaxSpeed + Speed);
			const FVector Launch(Horizontal.X, Horizontal.Y, ImpactVerticalSpeed);
			C->LaunchCharacter(Launch, true, true);
			FKnockback K; K.Character = C; K.Cooldown = ImpactCooldown; K.Recovery = ImpactRecoveryTimeout;
			if (bBlockMoveInputDuringKnockback && C->GetController())
			{
				C->GetController()->SetIgnoreMoveInput(true);
				K.bInputBlocked = true;
			}
			Knockbacks.Add(K);
			UE_LOG(LogP9, Log, TEXT("Train impact: %s hit by car %d (%s) at train speed %.0f - launch %s; train continues (%s)"),
				*C->GetName(), Car, bFront ? TEXT("front") : TEXT("side"), Speed, *Launch.ToCompactString(), TrainStateName(TrainState));
			OnCharacterHit.Broadcast(C, Launch);
			break;
		}
	}
}

void AP9Train::UpdatePlayerAndPrompts()
{
	const bool bAboard = ComputePlayerAboard();
	if (bAboard != bPlayerAboard)
	{
		bPlayerAboard = bAboard;
		UE_LOG(LogP9, Log, TEXT("Train: player %s"), bAboard ? TEXT("boarded") : TEXT("exited"));
		SetPlayerIgnoresBlockers(bAboard);
		bAboard ? OnPlayerBoarded.Broadcast() : OnPlayerExited.Broadcast();
	}

	FText Prompt; bool bAction = false;
	switch (TrainState)
	{
	case EP9TrainState::StoppedAtPlatform:
		bAction = DoorState == EP9DoorState::Closed && AtStop(CurrentStopIndex);
		Prompt = bAction ? NSLOCTEXT("P9", "TrainOpen", "Open doors") : NSLOCTEXT("P9", "TrainWait", "Doors closed");
		break;
	case EP9TrainState::DoorsOpening: Prompt = NSLOCTEXT("P9", "TrainOpening", "Doors opening..."); break;
	case EP9TrainState::DoorsOpen:
	{
		bAction = true;
		const FText Walk = bPlayerAboard ? NSLOCTEXT("P9", "TrainExitWalk", "Exit: walk out to the platform") : NSLOCTEXT("P9", "TrainBoardWalk", "Board: walk in");
		const FText Close = bAutoCycle
			? FText::Format(NSLOCTEXT("P9", "TrainCloseAuto", "INTERACT closes the doors (departs in {0} s)"), FText::AsNumber(FMath::Max(0, FMath::CeilToInt(DwellTime - DoorPhaseTime))))
			: NSLOCTEXT("P9", "TrainClose", "INTERACT closes the doors");
		Prompt = FText::Format(NSLOCTEXT("P9", "TrainOpenPrompt", "{0}  |  {1}"), Walk, Close);
		break;
	}
	case EP9TrainState::DoorsClosing: Prompt = NSLOCTEXT("P9", "TrainClosing", "Doors closing - stand clear"); break;
	case EP9TrainState::ReadyToDepart:
		Prompt = LastBlockReason.IsEmpty() ? NSLOCTEXT("P9", "TrainDeparting", "Train departing - stand clear")
			: FText::Format(NSLOCTEXT("P9", "TrainHeld", "Departure held: {0}"), FText::FromString(LastBlockReason));
		break;
	case EP9TrainState::Braking:
	case EP9TrainState::Arriving:
		Prompt = NSLOCTEXT("P9", "TrainArriving", "Train arriving - doors locked");
		break;
	default: Prompt = NSLOCTEXT("P9", "TrainMoving", "Doors locked - train moving"); break;
	}
	for (UP9InteractableComponent* Interactable : DoorInteractables)
	{
		Interactable->PromptText = Prompt;
		Interactable->bActionAvailable = bAction;
	}
}

void AP9Train::HandleDoorInteract(UP9InteractableComponent* Source, APawn* InteractingPawn)
{
	if (DoorState == EP9DoorState::Closed && TrainState == EP9TrainState::StoppedAtPlatform)
	{
		RequestOpenDoors();
	}
	else if (DoorState == EP9DoorState::Open)
	{
		RequestCloseDoors();
	}
}

FString AP9Train::GetStatusString() const
{
	return FString::Printf(TEXT("state=%s doors=%s alpha=%.2f distance=%.1f/%.0f target=%.1f speed=%.1f stop=%d aboard=%s pending=%s block='%s' world=%s yaw=%.1f"),
		TrainStateName(TrainState), DoorStateName(DoorState), DoorAlpha, Distance, RouteLength, TargetDistance, Speed, CurrentStopIndex,
		bPlayerAboard ? TEXT("yes") : TEXT("no"), bDeparturePending ? TEXT("yes") : TEXT("no"), *GetDepartureBlockReason(),
		*GetActorLocation().ToCompactString(), GetActorRotation().Yaw);
}

// ---------------- destination board ----------------
void AP9Train::SetupDestinationBoard()
{
	if (DestinationText.TrimStartAndEnd().Equals(FString(DefaultDestination).TrimStartAndEnd()) && !DestinationTarget)
	{
		return;                                              // the baked LED texture already shows the default
	}
	if (!DestinationTarget)
	{
		DestinationTarget = UCanvasRenderTarget2D::CreateCanvasRenderTarget2D(this, UCanvasRenderTarget2D::StaticClass(), 512, 512);
		if (!DestinationTarget) return;
		DestinationTarget->OnCanvasRenderTargetUpdate.AddDynamic(this, &AP9Train::DrawDestinationBoard);
		for (UStaticMeshComponent* Shell : Shells)
		{
			const TArray<FName> Names = Shell->GetMaterialSlotNames();
			for (int32 i = 0; i < Names.Num(); ++i)
			{
				if (Names[i].ToString().StartsWith(TEXT("M_P9_TrainLights")))
				{
					UMaterialInstanceDynamic* MID = Shell->CreateDynamicMaterialInstance(i);
					if (MID) MID->SetTextureParameterValue(TEXT("PanelTexture"), DestinationTarget);
				}
			}
		}
	}
	DestinationTarget->UpdateResource();
	UE_LOG(LogP9, Log, TEXT("Train: destination board set to '%s'"), *DestinationText);
}

void AP9Train::DrawDestinationBoard(UCanvas* Canvas, int32 Width, int32 Height)
{
	const FVector2D Size(Width, Height);
	if (UTexture* Atlas = LightsAtlas.LoadSynchronous())
	{
		Canvas->K2_DrawTexture(Atlas, FVector2D::ZeroVector, Size, FVector2D::ZeroVector, FVector2D::UnitVector, FLinearColor::White, BLEND_Opaque);
	}
	Canvas->K2_DrawTexture(nullptr, FVector2D(0.0f, Height * 0.5f), FVector2D(Width, Height * 0.25f), FVector2D::ZeroVector, FVector2D::UnitVector, FLinearColor(0.01f, 0.008f, 0.005f), BLEND_Opaque);
	Canvas->K2_DrawText(GEngine->GetLargeFont(), DestinationText.ToUpper(), FVector2D(Width * 0.5f, Height * 0.625f), FVector2D(2.6f, 3.2f),
		FLinearColor(1.0f, 0.55f, 0.08f), 0.0f, FLinearColor::Transparent, FVector2D::ZeroVector, true, true, false, FLinearColor::Black);
}
