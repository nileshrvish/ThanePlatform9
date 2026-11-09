#include "P9GreetingScare.h"
#include "P9Captions.h"
#include "ThanePlatform9.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

namespace P9Greeting
{
	TSoftObjectPtr<USoundBase> Snd(const TCHAR* Name)
	{
		return TSoftObjectPtr<USoundBase>(FSoftObjectPath(FString::Printf(TEXT("/Game/P9/Audio/OriginalAudioPack/Creatures/%s.%s"), Name, Name)));
	}
}

AP9GreetingScare::AP9GreetingScare()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	SetRootComponent(Root);
	auto Make = [this](const TCHAR* Name)
	{
		UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetupAttachment(Root);
		C->SetMobility(EComponentMobility::Movable);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCanEverAffectNavigation(false);
		C->SetCastShadow(false);
		C->bNeverDistanceCull = true;
		return C;
	};
	StandMesh = Make(TEXT("Stand"));
	LungeMesh = Make(TEXT("Lunge"));
	LungeMesh->SetHiddenInGame(true);
	Scream = P9Greeting::Snd(TEXT("SFX_Greeting_Scream"));
	DistortedLayer = P9Greeting::Snd(TEXT("SFX_Greeting_DistortedLayer"));
	Impact = P9Greeting::Snd(TEXT("SFX_Greeting_JumpscareImpact"));
}

void AP9GreetingScare::Begin(UStaticMesh* Stand, UStaticMesh* Lunge, APlayerController* PC)
{
	StandMesh->SetStaticMesh(Stand);
	LungeMesh->SetStaticMesh(Lunge ? Lunge : Stand);
	StandMesh->SetRelativeRotation(FRotator(0.0f, MeshYaw, 0.0f));
	LungeMesh->SetRelativeRotation(FRotator(0.0f, MeshYaw, 0.0f));
	Player = PC;
	Time = 0.0f;
	// sounds are loaded now so the lunge frame never waits on disk
	Scream.LoadSynchronous(); DistortedLayer.LoadSynchronous(); Impact.LoadSynchronous();
	UE_LOG(LogP9, Log, TEXT("Greeting: standing in the aisle at %s (stand %s, lunge %s)"), *GetActorLocation().ToCompactString(), Stand ? *Stand->GetName() : TEXT("none"),
		Lunge ? *Lunge->GetName() : TEXT("none"));
}

void AP9GreetingScare::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	APlayerController* PC = Player.Get();
	if (!PC || !PC->PlayerCameraManager || IsHidden()) return;
	Time += DeltaSeconds;
	APlayerCameraManager* Cam = PC->PlayerCameraManager;
	if (!bLunging && Time >= PauseSeconds)
	{
		bLunging = true;
		StandMesh->SetHiddenInGame(true);
		LungeMesh->SetHiddenInGame(false);
		// kept in the carriage's frame: the train moves at up to 11 m/s during the lunge
		const USceneComponent* Parent = GetRootComponent()->GetAttachParent();
		StartWorld = Parent ? Parent->GetComponentTransform().InverseTransformPosition(GetActorLocation()) : GetActorLocation();
		StartFOV = Cam->GetFOVAngle();
		if (USoundBase* S = Scream.Get()) { UGameplayStatics::PlaySound2D(this, S, 1.0f, 1.0f); P9Captions::NotifySound(this, S); }   // Stage 9B
		if (USoundBase* S = DistortedLayer.Get()) UGameplayStatics::PlaySound2D(this, S, 0.45f, 1.0f);
		UE_LOG(LogP9, Log, TEXT("Greeting: LUNGE (scream) at t=%.2f"), Time);
	}
	if (bLunging)
	{
		// the actor slides so her face ends FaceStopDistance in front of the camera, along the line to the camera (accelerating)
		const float A = FMath::Clamp((Time - PauseSeconds) / FMath::Max(0.05f, LungeSeconds), 0.0f, 1.0f);
		const float E = A * A * A;                                                   // ease-in: sudden, faster and faster
		const USceneComponent* Parent = GetRootComponent()->GetAttachParent();
		const FVector Start = Parent ? Parent->GetComponentTransform().TransformPosition(StartWorld) : StartWorld;
		const FVector CamLoc = Cam->GetCameraLocation();
		const FVector Flat = (CamLoc - Start).GetSafeNormal2D();
		const FRotator Face = Flat.Rotation();
		const FVector FaceOffset = FRotationMatrix(Face).TransformVector(FaceLocal);   // face point relative to the actor origin
		const FVector Goal = CamLoc - (CamLoc - (Start + FaceOffset)).GetSafeNormal() * FaceStopDistance - FaceOffset;
		SetActorLocationAndRotation(FMath::Lerp(Start, Goal, E), FRotator(0.0f, Face.Yaw, 0.0f));
		if (!bFovLocked) { bFovLocked = true; }
		Cam->SetFOV(FMath::Lerp(StartFOV, LungeFOV, E));
		if (!bImpact && A >= 1.0f)
		{
			bImpact = true;
			ShakeLeft = 0.45f;
			if (USoundBase* S = Impact.Get()) UGameplayStatics::PlaySound2D(this, S, 1.0f, 1.0f);
			UE_LOG(LogP9, Log, TEXT("Greeting: IMPACT at t=%.2f, face %.0f cm from the camera"), Time,
				FVector::Dist(CamLoc, GetActorLocation() + FaceOffset));
		}
	}
	// short, decaying camera shake on impact (restored exactly afterwards)
	if (bImpact && (ShakeLeft > 0.0f || !ShakeOffset.IsZero()))
	{
		const FRotator Base = PC->GetControlRotation() - ShakeOffset;
		ShakeLeft -= DeltaSeconds;
		const float S = FMath::Clamp(ShakeLeft / 0.45f, 0.0f, 1.0f);
		ShakeOffset = ShakeLeft > 0.0f ? FRotator(FMath::FRandRange(-2.5f, 2.5f) * S, FMath::FRandRange(-3.0f, 3.0f) * S, FMath::FRandRange(-2.0f, 2.0f) * S) : FRotator::ZeroRotator;
		PC->SetControlRotation(Base + ShakeOffset);
	}
}

void AP9GreetingScare::Finish()
{
	APlayerController* PC = Player.Get();
	if (PC && !ShakeOffset.IsZero()) PC->SetControlRotation(PC->GetControlRotation() - ShakeOffset);
	ShakeOffset = FRotator::ZeroRotator;
	ShakeLeft = 0.0f;
	if (PC && PC->PlayerCameraManager && bFovLocked) PC->PlayerCameraManager->UnlockFOV();
	bFovLocked = false;
}

void AP9GreetingScare::EndPlay(const EEndPlayReason::Type Reason)
{
	Finish();
	Super::EndPlay(Reason);
}
