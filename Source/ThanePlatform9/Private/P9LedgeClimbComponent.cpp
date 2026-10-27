#include "P9LedgeClimbComponent.h"

#include "ThanePlatform9.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"

namespace P9Ledge
{
	/** Shrink applied to query capsules so grazing a wall the character already touches isn't a hit. */
	constexpr float QuerySkin = 1.5f;
	/** Gap left between the capsule bottom and the ledge top. */
	constexpr float FloorGap = 2.0f;
	constexpr float WallProbeRadius = 8.0f;
	constexpr float TopProbeRadius = 4.0f;
	/** Walls must be close to vertical: |normal.Z| below this. */
	constexpr float MaxWallNormalZ = 0.35f;
}

UP9LedgeClimbComponent::UP9LedgeClimbComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UP9LedgeClimbComponent::BeginPlay()
{
	Super::BeginPlay();

	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (!OwnerCharacter)
	{
		UE_LOG(LogP9, Warning, TEXT("LedgeClimb: owner %s is not a Character; component disabled."), *GetNameSafe(GetOwner()));
		SetComponentTickEnabled(false);
		return;
	}

	Character = OwnerCharacter;
	OwnerCharacter->ReceiveControllerChangedDelegate.AddDynamic(this, &UP9LedgeClimbComponent::HandleControllerChanged);
	HandleControllerChanged(OwnerCharacter, nullptr, OwnerCharacter->GetController());
}

void UP9LedgeClimbComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsMantling())
	{
		FinishMantle(false, TEXT("end play"));
	}
	if (ACharacter* OwnerCharacter = Character.Get())
	{
		OwnerCharacter->ReceiveControllerChangedDelegate.RemoveDynamic(this, &UP9LedgeClimbComponent::HandleControllerChanged);
	}
	if (APlayerController* PC = InputController.Get())
	{
		PC->PopInputComponent(JumpInputComponent);
	}
	InputController.Reset();
	Super::EndPlay(EndPlayReason);
}

void UP9LedgeClimbComponent::HandleControllerChanged(APawn* Pawn, AController* OldController, AController* NewController)
{
	if (IsMantling())
	{
		FinishMantle(false, TEXT("controller changed"));
	}

	if (APlayerController* OldPC = InputController.Get())
	{
		OldPC->PopInputComponent(JumpInputComponent);
		InputController.Reset();
	}

	APlayerController* NewPC = Cast<APlayerController>(NewController);
	if (!NewPC || !NewPC->IsLocalController())
	{
		return;
	}

	if (!JumpAction)
	{
		UE_LOG(LogP9, Warning, TEXT("LedgeClimb: JumpAction is not set; jump input is not bound."));
		return;
	}

	if (!JumpInputComponent)
	{
		JumpInputComponent = NewObject<UEnhancedInputComponent>(this, TEXT("P9LedgeClimbInput"));
		JumpInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &UP9LedgeClimbComponent::OnJumpStarted);
		JumpInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &UP9LedgeClimbComponent::OnJumpCompleted);
		JumpInputComponent->BindAction(JumpAction, ETriggerEvent::Canceled, this, &UP9LedgeClimbComponent::OnJumpCompleted);
	}

	NewPC->PushInputComponent(JumpInputComponent);
	InputController = NewPC;
}

void UP9LedgeClimbComponent::OnJumpStarted(const FInputActionValue& Value)
{
	ACharacter* OwnerCharacter = Character.Get();
	if (OwnerCharacter && !IsMantling())
	{
		OwnerCharacter->Jump();
	}
}

void UP9LedgeClimbComponent::OnJumpCompleted(const FInputActionValue& Value)
{
	if (ACharacter* OwnerCharacter = Character.Get())
	{
		OwnerCharacter->StopJumping();
	}
}

void UP9LedgeClimbComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	ACharacter* OwnerCharacter = Character.Get();
	if (!OwnerCharacter)
	{
		return;
	}

	if (IsMantling())
	{
		UpdateMantle(DeltaTime);
		return;
	}

	Cooldown = FMath::Max(0.0f, Cooldown - DeltaTime);
	const UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement();
	if (!bEnableLedgeClimb || Cooldown > 0.0f || !Movement || !Movement->IsFalling())
	{
		return;
	}
	if (bOnlyAfterJump && OwnerCharacter->JumpCurrentCount == 0)
	{
		return;
	}
	if (Movement->Velocity.Z < -MaxGrabFallSpeed)
	{
		return;
	}

	const FVector Direction = GetApproachDirection();
	if (Direction.IsZero())
	{
		return;
	}

	FLedgeCandidate Candidate;
	FString RejectReason;
	if (FindLedge(Direction, Candidate, RejectReason))
	{
		StartMantle(Candidate);
	}
	else if (!RejectReason.IsEmpty() && RejectReason != LastRejectReason)
	{
		// Logged once per distinct reason, so tuning is possible without log spam.
		UE_LOG(LogP9, Log, TEXT("LedgeClimb: no grab - %s"), *RejectReason);
		LastRejectReason = RejectReason;
	}
}

FVector UP9LedgeClimbComponent::GetApproachDirection() const
{
	const ACharacter* OwnerCharacter = Character.Get();
	FVector Direction = OwnerCharacter->GetLastMovementInputVector().GetSafeNormal2D();
	if (Direction.IsZero())
	{
		const FVector Velocity2D(OwnerCharacter->GetVelocity().X, OwnerCharacter->GetVelocity().Y, 0.0f);
		if (Velocity2D.SizeSquared() > FMath::Square(50.0f))
		{
			Direction = Velocity2D.GetSafeNormal();
		}
	}
	return Direction;
}

bool UP9LedgeClimbComponent::FindLedge(const FVector& Direction, FLedgeCandidate& OutCandidate, FString& OutRejectReason) const
{
	const ACharacter* OwnerCharacter = Character.Get();
	const UCapsuleComponent* Capsule = OwnerCharacter->GetCapsuleComponent();
	const UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement();
	UWorld* World = GetWorld();

	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const FVector Location = OwnerCharacter->GetActorLocation();
	const float FeetZ = Location.Z - HalfHeight;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(P9LedgeClimb), false, OwnerCharacter);
	FCollisionResponseParams ResponseParams;
	Movement->InitCollisionParams(QueryParams, ResponseParams);
	const ECollisionChannel Channel = Capsule->GetCollisionObjectType();

	// 1) Wall: sweep forward at head, chest and waist height; the highest hit is closest to the ledge.
	const float ProbeHeights[] = { HalfHeight * 0.9f, HalfHeight * 0.3f, -HalfHeight * 0.3f };
	FHitResult WallHit;
	bool bFoundWall = false;
	for (const float HeightOffset : ProbeHeights)
	{
		const FVector Start = Location + FVector(0.0f, 0.0f, HeightOffset);
		const FVector End = Start + Direction * (Radius + WallReach);
		FHitResult Hit;
		const bool bHit = World->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, Channel,
			FCollisionShape::MakeSphere(P9Ledge::WallProbeRadius), QueryParams, ResponseParams);
		if (bDrawDebug)
		{
			DrawDebugLine(World, Start, bHit ? Hit.Location : End, bHit ? FColor::Orange : FColor::Silver, false, 1.0f);
		}
		if (bHit && !Hit.bStartPenetrating)
		{
			WallHit = Hit;
			bFoundWall = true;
			break;
		}
	}
	if (!bFoundWall)
	{
		OutRejectReason = TEXT("no wall in reach");
		return false;
	}

	const FVector WallNormal2D = FVector(WallHit.ImpactNormal.X, WallHit.ImpactNormal.Y, 0.0f).GetSafeNormal();
	if (FMath::Abs(WallHit.ImpactNormal.Z) > P9Ledge::MaxWallNormalZ || WallNormal2D.IsZero())
	{
		OutRejectReason = TEXT("surface ahead is not a wall");
		return false;
	}
	const FVector WallDirection = -WallNormal2D;
	if (FVector::DotProduct(WallDirection, Direction) < MinFacingDot)
	{
		OutRejectReason = TEXT("not moving toward the wall");
		return false;
	}
	if (Cast<APawn>(WallHit.GetActor()) || !WallHit.GetComponent() || !WallHit.GetComponent()->CanCharacterStepUp(const_cast<ACharacter*>(OwnerCharacter)))
	{
		OutRejectReason = TEXT("wall is not climbable (pawn or CanCharacterStepUpOn=No)");
		return false;
	}

	// 2) Ledge top: sweep down just past the wall face, from above reach to the minimum ledge height.
	const FVector TopXY = FVector(WallHit.ImpactPoint.X, WallHit.ImpactPoint.Y, 0.0f) + WallDirection * (Radius + LedgeInset);
	const float MaxLedgeZ = FeetZ + 2.0f * HalfHeight + MaxReachAboveHead;
	const FVector TopStart(TopXY.X, TopXY.Y, MaxLedgeZ + P9Ledge::TopProbeRadius + 1.0f);
	const FVector TopEnd(TopXY.X, TopXY.Y, FeetZ + MinLedgeHeight);
	FHitResult TopHit;
	const bool bTopHit = World->SweepSingleByChannel(TopHit, TopStart, TopEnd, FQuat::Identity, Channel,
		FCollisionShape::MakeSphere(P9Ledge::TopProbeRadius), QueryParams, ResponseParams);
	if (bDrawDebug)
	{
		DrawDebugLine(World, TopStart, bTopHit ? TopHit.Location : TopEnd, bTopHit ? FColor::Yellow : FColor::Silver, false, 1.0f);
	}
	if (!bTopHit)
	{
		OutRejectReason = TEXT("no ledge top within reach");
		return false;
	}
	if (TopHit.bStartPenetrating)
	{
		OutRejectReason = TEXT("ledge top is above reach (solid geometry at max reach height)");
		return false;
	}
	if (!Movement->IsWalkable(TopHit))
	{
		OutRejectReason = TEXT("ledge top is not walkable");
		return false;
	}
	const float LedgeZ = TopHit.ImpactPoint.Z;
	if (WallHit.ImpactPoint.Z > LedgeZ + 5.0f)
	{
		OutRejectReason = TEXT("wall continues above the found top");
		return false;
	}

	// 3) The capsule must fit on top, and both legs of the move must be clear (no ceiling, no clipping).
	const FVector EndLocation(TopXY.X, TopXY.Y, LedgeZ + HalfHeight + P9Ledge::FloorGap);
	const FVector RiseLocation(Location.X, Location.Y, FMath::Max(Location.Z, EndLocation.Z));
	if (!IsCapsuleSpotFree(EndLocation))
	{
		OutRejectReason = TEXT("no room to stand on the ledge");
		return false;
	}
	if (!IsCapsulePathClear(Location, RiseLocation))
	{
		OutRejectReason = TEXT("ceiling or overhang blocks the climb");
		return false;
	}
	if (!IsCapsulePathClear(RiseLocation, EndLocation))
	{
		OutRejectReason = TEXT("path onto the ledge is blocked");
		return false;
	}

	if (bDrawDebug)
	{
		DrawDebugCapsule(World, EndLocation, HalfHeight, Radius, FQuat::Identity, FColor::Green, false, 2.0f);
	}

	OutCandidate.WallPoint = WallHit.ImpactPoint;
	OutCandidate.LedgePoint = TopHit.ImpactPoint;
	OutCandidate.RiseLocation = RiseLocation;
	OutCandidate.EndLocation = EndLocation;
	return true;
}

bool UP9LedgeClimbComponent::IsCapsulePathClear(const FVector& From, const FVector& To) const
{
	if (From.Equals(To, 0.1f))
	{
		return true;
	}

	const ACharacter* OwnerCharacter = Character.Get();
	const UCapsuleComponent* Capsule = OwnerCharacter->GetCapsuleComponent();
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(P9LedgeClimbPath), false, OwnerCharacter);
	FCollisionResponseParams ResponseParams;
	OwnerCharacter->GetCharacterMovement()->InitCollisionParams(QueryParams, ResponseParams);

	const FCollisionShape Shape = FCollisionShape::MakeCapsule(
		Capsule->GetScaledCapsuleRadius() - P9Ledge::QuerySkin,
		Capsule->GetScaledCapsuleHalfHeight() - P9Ledge::QuerySkin);
	FHitResult Hit;
	return !GetWorld()->SweepSingleByChannel(Hit, From, To, FQuat::Identity, Capsule->GetCollisionObjectType(), Shape, QueryParams, ResponseParams);
}

bool UP9LedgeClimbComponent::IsCapsuleSpotFree(const FVector& Location) const
{
	const ACharacter* OwnerCharacter = Character.Get();
	const UCapsuleComponent* Capsule = OwnerCharacter->GetCapsuleComponent();
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(P9LedgeClimbFit), false, OwnerCharacter);
	FCollisionResponseParams ResponseParams;
	OwnerCharacter->GetCharacterMovement()->InitCollisionParams(QueryParams, ResponseParams);

	const FCollisionShape Shape = FCollisionShape::MakeCapsule(
		Capsule->GetScaledCapsuleRadius() - P9Ledge::QuerySkin,
		Capsule->GetScaledCapsuleHalfHeight() - P9Ledge::QuerySkin);
	return !GetWorld()->OverlapBlockingTestByChannel(Location, FQuat::Identity, Capsule->GetCollisionObjectType(), Shape, QueryParams, ResponseParams);
}

void UP9LedgeClimbComponent::StartMantle(const FLedgeCandidate& Candidate)
{
	ACharacter* OwnerCharacter = Character.Get();
	UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement();

	OwnerCharacter->StopJumping();
	Movement->StopMovementImmediately();
	Movement->SetMovementMode(MOVE_Flying);
	SetMoveInputIgnored(true);

	MantleEnd = Candidate.EndLocation;
	MantleElapsed = 0.0f;
	PhaseElapsed = 0.0f;
	PhaseStart = OwnerCharacter->GetActorLocation();
	PhaseEnd = Candidate.RiseLocation;
	PhaseDuration = FMath::Max(0.08f, FVector::Dist(PhaseStart, PhaseEnd) / RiseSpeed);
	Phase = EP9MantlePhase::Rising;
	LastRejectReason.Reset();

	UE_LOG(LogP9, Log, TEXT("LedgeClimb: grab - ledge %.0f cm above feet, mantle to %s"),
		Candidate.LedgePoint.Z - (PhaseStart.Z - OwnerCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()),
		*MantleEnd.ToCompactString());
	OnMantleStarted.Broadcast();
}

void UP9LedgeClimbComponent::UpdateMantle(float DeltaTime)
{
	ACharacter* OwnerCharacter = Character.Get();
	UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement();

	MantleElapsed += DeltaTime;
	if (MantleElapsed > MaxMantleDuration)
	{
		FinishMantle(false, TEXT("timed out"));
		return;
	}
	if (Movement->MovementMode != MOVE_Flying)
	{
		FinishMantle(false, TEXT("movement mode changed externally"));
		return;
	}

	PhaseElapsed += DeltaTime;
	const float Alpha = FMath::Clamp(PhaseElapsed / PhaseDuration, 0.0f, 1.0f);
	const FVector Target = FMath::Lerp(PhaseStart, PhaseEnd, FMath::SmoothStep(0.0f, 1.0f, Alpha));

	FHitResult Hit;
	OwnerCharacter->SetActorLocation(Target, true, &Hit);
	if (Hit.bBlockingHit)
	{
		// A graze against the wall the capsule is touching is fine if the target spot itself is free;
		// anything actually occupying the path (a moving train, a door) aborts.
		if (!IsCapsuleSpotFree(Target))
		{
			FinishMantle(false, TEXT("path became blocked"));
			return;
		}
		OwnerCharacter->SetActorLocation(Target, false, nullptr, ETeleportType::None);
	}
	Movement->Velocity = FVector::ZeroVector;

	if (Alpha < 1.0f)
	{
		return;
	}

	if (Phase == EP9MantlePhase::Rising)
	{
		Phase = EP9MantlePhase::Forward;
		PhaseStart = OwnerCharacter->GetActorLocation();
		PhaseEnd = MantleEnd;
		PhaseElapsed = 0.0f;
		PhaseDuration = FMath::Max(0.08f, FVector::Dist(PhaseStart, PhaseEnd) / ForwardSpeed);
	}
	else
	{
		FinishMantle(true, TEXT("completed"));
	}
}

void UP9LedgeClimbComponent::FinishMantle(bool bCompleted, const TCHAR* Reason)
{
	Phase = EP9MantlePhase::None;
	SetMoveInputIgnored(false);

	if (ACharacter* OwnerCharacter = Character.Get())
	{
		UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement();
		Movement->Velocity = FVector::ZeroVector;
		// Walking resets the jump state; the movement component re-finds the floor next tick and falls if there is none.
		Movement->SetMovementMode(bCompleted ? MOVE_Walking : MOVE_Falling);
	}

	Cooldown = bCompleted ? 0.15f : RetryCooldown;
	UE_LOG(LogP9, Log, TEXT("LedgeClimb: mantle %s (%s, %.2fs)"), bCompleted ? TEXT("finished") : TEXT("aborted"), Reason, MantleElapsed);
	OnMantleFinished.Broadcast(bCompleted);
}

void UP9LedgeClimbComponent::CancelMantle()
{
	if (IsMantling())
	{
		FinishMantle(false, TEXT("cancelled"));
	}
}

void UP9LedgeClimbComponent::SetMoveInputIgnored(bool bIgnore)
{
	if (bIgnore)
	{
		AController* Controller = Character.IsValid() ? Character->GetController() : nullptr;
		if (Controller && !IgnoredMoveController.IsValid())
		{
			Controller->SetIgnoreMoveInput(true);
			IgnoredMoveController = Controller;
		}
	}
	else if (AController* Controller = IgnoredMoveController.Get())
	{
		Controller->SetIgnoreMoveInput(false);
		IgnoredMoveController.Reset();
	}
}
