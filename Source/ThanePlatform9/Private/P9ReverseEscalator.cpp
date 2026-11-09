#include "P9ReverseEscalator.h"
#include "ThanePlatform9.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"

namespace P9Escalator
{
	constexpr int32 FirstStep = -1, LastStep = 17;   // 19 moving steps: one rising out of the floor ... one sliding under the landing
	constexpr float StepSink = 0.3f;                 // the moving treads sit this far under the static landing / floor (no z-fighting)
	constexpr float RampLift = 1.0f;                 // the ramp lies this far above the real step cubes' nosings
}

AP9ReverseEscalator::AP9ReverseEscalator()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;     // after the character has moved this frame
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	SetRootComponent(Root);
}

float AP9ReverseEscalator::RampZLocal(float X) const
{
	// the real collision steps: cube k (1..17) spans x [1400-30k, 1430-30k], top 16.667 k; its nosing line z = (1400 - x) * rise/tread
	const float CubeFoot = BottomNoseX - 0.5f;
	return FMath::Clamp((CubeFoot - X) * Rise / Tread, 0.0f, LandingTop - 0.5f) + P9Escalator::RampLift;
}

bool AP9ReverseEscalator::Setup(AActor* Stair, UStaticMesh* Upper, UStaticMesh* Step)
{
	if (!Stair || !Upper || !Step)
	{
		return false;
	}
	RealStair = Stair;
	SetActorTransform(Stair->GetActorTransform());
	UStaticMeshComponent* Template = Stair->FindComponentByClass<UStaticMeshComponent>();
	auto Make = [this](UStaticMesh* Mesh, const FName Name)
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this, Name, RF_Transient);
		C->SetMobility(EComponentMobility::Movable);
		C->SetStaticMesh(Mesh);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCanEverAffectNavigation(false);
		C->SetCastShadow(false);
		C->bNeverDistanceCull = true;
		C->SetupAttachment(Root);
		C->RegisterComponent();
		return C;
	};
	UpperComp = Make(Upper, TEXT("UpperFlight"));
	if (Template)
	{
		// same materials as the real finish (tread + plaster); the pieces were cut with the same slot order
		for (int32 i = 0; i < Template->GetNumMaterials() && i < UpperComp->GetNumMaterials(); ++i) UpperComp->SetMaterial(i, Template->GetMaterial(i));
	}
	for (int32 j = P9Escalator::FirstStep; j <= P9Escalator::LastStep; ++j)
	{
		UStaticMeshComponent* S = Make(Step, *FString::Printf(TEXT("Step%02d"), j + 1));
		if (Template) for (int32 i = 0; i < Template->GetNumMaterials() && i < S->GetNumMaterials(); ++i) S->SetMaterial(i, Template->GetMaterial(i));
		Steps.Add(S);
	}
	// the comfortable ramp: Pawn-only blocking box whose top runs 1 cm above the real steps' nosings, foot to landing
	const float CubeFoot = BottomNoseX - 0.5f;
	const float Run = CubeFoot - LandingX, Height = LandingTop - 0.5f;
	const float Theta = FMath::Atan2(Height, Run);
	const float HalfLen = FMath::Sqrt(Run * Run + Height * Height) * 0.5f + 2.0f;
	const FVector Normal(FMath::Sin(Theta), 0.0f, FMath::Cos(Theta));
	const FVector Mid((CubeFoot + LandingX) * 0.5f, 0.0f, Height * 0.5f + P9Escalator::RampLift);
	Ramp = NewObject<UBoxComponent>(this, TEXT("Ramp"), RF_Transient);
	Ramp->SetupAttachment(Root);
	Ramp->SetBoxExtent(FVector(HalfLen, FlightHalfWidth, 10.0f));
	Ramp->SetRelativeLocationAndRotation(Mid - Normal * 10.0f, FRotator(FMath::RadiansToDegrees(Theta), 180.0f, 0.0f));
	Ramp->SetCollisionObjectType(ECC_WorldStatic);
	Ramp->SetCollisionResponseToAllChannels(ECR_Ignore);
	Ramp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	Ramp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Ramp->SetGenerateOverlapEvents(false);
	Ramp->SetCanEverAffectNavigation(false);
	Ramp->SetHiddenInGame(true);
	Ramp->RegisterComponent();
	Stair->SetActorHiddenInGame(true);
	bRealHidden = true;
	Phase = 0.0f;
	Speed = IdleSpeed;
	PlaceSteps();
	bSetup = true;
	UE_LOG(LogP9, Log, TEXT("Reverse escalator: armed on %s - %d moving steps, ramp %.0f cm at %.1f deg"), *Stair->GetName(), Steps.Num(), HalfLen * 2.0f, FMath::RadiansToDegrees(Theta));
	return true;
}

void AP9ReverseEscalator::PlaceSteps()
{
	for (int32 i = 0; i < Steps.Num(); ++i)
	{
		const float J = static_cast<float>(i + P9Escalator::FirstStep) + Phase;
		const float X = BottomNoseX - Tread * J;
		const float Z = FMath::Min(Rise * (J + 1.0f) + 0.5f, LandingTop) - P9Escalator::StepSink;
		Steps[i]->SetRelativeLocation(FVector(X, 0.0f, Z));
	}
}

void AP9ReverseEscalator::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bSetup) return;
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	ACharacter* C = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
	float Target = bStoppedByClimb ? 0.0f : IdleSpeed;
	bool bOnFlight = false, bHold = false, bCarry = false;
	FVector Feet = FVector::ZeroVector;
	float HalfHeight = 90.0f;
	if (C)
	{
		HalfHeight = C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		const FTransform Xf = GetActorTransform();
		Feet = Xf.InverseTransformPosition(C->GetActorLocation() - FVector(0.0f, 0.0f, HalfHeight));
		const FVector V = Xf.InverseTransformVector(C->GetVelocity());
		const float Down = V.X;                                                       // the flight descends towards local +X
		const bool bFalling = C->GetCharacterMovement() && C->GetCharacterMovement()->IsFalling();
		bOnFlight = !bFalling && Feet.X > LandingX - 5.0f && Feet.X < BottomNoseX + 10.0f && FMath::Abs(Feet.Y) < FlightHalfWidth
			&& Feet.Z > RampZLocal(Feet.X) - 40.0f && Feet.Z < RampZLocal(Feet.X) + 60.0f;
		const bool bNear = Feet.X > 150.0f && Feet.X < BottomNoseX + 300.0f && FMath::Abs(Feet.Y) < FlightHalfWidth + 200.0f && Feet.Z < 700.0f;
		if (Down > 40.0f) { DownTime += DeltaSeconds; UpTime = 0.0f; }
		else if (Down < -40.0f) { UpTime += DeltaSeconds; DownTime = 0.0f; }
		else { UpTime = 0.0f; DownTime = 0.0f; }
		// walking up the flight stops the escalator; heading down again (on the stair or its landing) starts it
		if (bOnFlight && UpTime > 0.3f && !bStoppedByClimb) { bStoppedByClimb = true; UE_LOG(LogP9, Log, TEXT("Reverse escalator: player climbing - stopping")); }
		if (bStoppedByClimb && bNear && DownTime > 0.25f) { bStoppedByClimb = false; UE_LOG(LogP9, Log, TEXT("Reverse escalator: player heading down again - running")); }
		if (!bStoppedByClimb)
		{
			if (bOnFlight && Down > 40.0f) { bHold = true; Target = FMath::Max(IdleSpeed, Down * HoldFactor); }
			else if (bOnFlight && FMath::Abs(Down) <= 40.0f) { bCarry = true; }
		}
		// the player has met it: on the flight, or close and looking at the moving steps
		if (!bEngaged && PC->PlayerCameraManager)
		{
			const FVector Mid = Xf.TransformPosition(FVector((LandingX + BottomNoseX) * 0.5f, 0.0f, 150.0f));
			const FVector Cam = PC->PlayerCameraManager->GetCameraLocation();
			const bool bSees = FVector::Dist(Cam, Mid) < 1100.0f && FVector::DotProduct((Mid - Cam).GetSafeNormal(), PC->PlayerCameraManager->GetCameraRotation().Vector()) > 0.6f;
			if (bOnFlight || bSees) { bEngaged = true; UE_LOG(LogP9, Log, TEXT("Reverse escalator: the player has met the moving stair")); }
		}
	}
	// the steps' speed eases (fast enough to match a walker, gently when stopping or starting)
	const float A = (bHold && Target > Speed) ? Accel * 6.0f : Accel;
	Speed = FMath::FInterpConstantTo(Speed, Target, DeltaSeconds, A);
	Phase = FMath::Fmod(Phase + Speed * DeltaSeconds / Tread, 1.0f);
	PlaceSteps();
	// the player: progress down the flight is taken back; a player standing on it rides up with the steps
	if (C && bOnFlight && !bStoppedByClimb)
	{
		const FTransform Xf = GetActorTransform();
		float NewX = Feet.X;
		if (bHold && bHadLast)
		{
			const float Dx = FMath::Max(0.0f, Feet.X - LastFeetLocal.X);
			NewX = Feet.X - Dx * HoldFactor;
			HeldCm += Dx * HoldFactor;
		}
		else if (bCarry)
		{
			NewX = FMath::Max(LandingX - 20.0f, Feet.X - Speed * DeltaSeconds);
		}
		if (!FMath::IsNearlyEqual(NewX, Feet.X, 0.01f))
		{
			// up the ramp by the same slope, so the capsule keeps exactly the contact the movement gave it
			const FVector NewFeet(NewX, Feet.Y, Feet.Z + (Feet.X - NewX) * Rise / Tread);
			C->SetActorLocation(Xf.TransformPosition(NewFeet) + FVector(0.0f, 0.0f, HalfHeight), false, nullptr, ETeleportType::None);
			Feet = NewFeet;
		}
		LastFeetLocal = Feet;
		bHadLast = true;
	}
	else
	{
		bHadLast = false;
	}
}

void AP9ReverseEscalator::Restore()
{
	if (bRealHidden && IsValid(RealStair))
	{
		RealStair->SetActorHiddenInGame(false);
	}
	bRealHidden = false;
	for (UActorComponent* Comp : GetComponents().Array())
	{
		if (Comp && Comp != Root) Comp->DestroyComponent();
	}
	Steps.Reset();
	UpperComp = nullptr;
	Ramp = nullptr;
	bSetup = false;
}

FString AP9ReverseEscalator::GetStatus() const
{
	return FString::Printf(TEXT("engaged=%d stoppedByClimb=%d speed=%.1f phase=%.2f heldCm=%.0f steps=%d"), bEngaged ? 1 : 0, bStoppedByClimb ? 1 : 0, Speed, Phase, HeldCm, Steps.Num());
}

void AP9ReverseEscalator::EndPlay(const EEndPlayReason::Type Reason)
{
	Restore();
	Super::EndPlay(Reason);
}
