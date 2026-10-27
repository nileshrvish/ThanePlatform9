#include "P9SeatComponent.h"
#include "ThanePlatform9.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"

UP9SeatComponent::UP9SeatComponent()
{
	InteractRadius = 95.0f;
	PromptText = NSLOCTEXT("P9", "SeatSit", "Sit down");
}

void UP9SeatComponent::BeginPlay()
{
	Super::BeginPlay();
	OnInteract.AddDynamic(this, &UP9SeatComponent::HandleInteract);
}

void UP9SeatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Occupant.IsValid())
	{
		Stand();
	}
	Super::EndPlay(EndPlayReason);
}

UP9SeatComponent* UP9SeatComponent::GetSeatOf(const AActor* Actor)
{
	const USceneComponent* Root = Actor ? Actor->GetRootComponent() : nullptr;
	return Root ? Cast<UP9SeatComponent>(Root->GetAttachParent()) : nullptr;
}

bool UP9SeatComponent::IsInteractableBy(const APawn* Pawn) const
{
	if (!bInteractionEnabled) return false;
	if (Occupant.IsValid()) return Occupant.Get() == Pawn;          // only the occupant sees "Stand up"
	return GetSeatOf(Pawn) == nullptr;                              // a seated pawn is not offered other seats
}

void UP9SeatComponent::HandleInteract(UP9InteractableComponent* Source, APawn* InteractingPawn)
{
	ACharacter* Character = Cast<ACharacter>(InteractingPawn);
	if (Character && Occupant.Get() == Character)
	{
		Stand();
	}
	else
	{
		Sit(Character);
	}
}

bool UP9SeatComponent::Sit(ACharacter* Character)
{
	if (!Character || Occupant.IsValid() || GetSeatOf(Character))
	{
		return false;
	}
	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	SavedMovementMode = Movement->MovementMode;
	SavedCollision = Capsule->GetCollisionEnabled();
	Movement->StopMovementImmediately();
	Movement->DisableMovement();
	Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	bBlockedMoveInput = false;
	if (AController* Controller = Character->GetController())
	{
		Controller->SetIgnoreMoveInput(true);                       // look input stays live
		bBlockedMoveInput = true;
	}
	const FRotator Facing(0.0f, GetComponentRotation().Yaw, 0.0f);
	const FVector Centre = GetComponentLocation() + FVector(0.0f, 0.0f, SeatedEyeHeight - Character->BaseEyeHeight);
	Character->SetActorLocationAndRotation(Centre, Facing, false, nullptr, ETeleportType::TeleportPhysics);
	Character->AttachToComponent(this, FAttachmentTransformRules::KeepWorldTransform);
	if (AController* Controller = Character->GetController())
	{
		Controller->SetControlRotation(Facing);
	}
	Occupant = Character;
	PromptText = NSLOCTEXT("P9", "SeatStand", "Stand up");
	UE_LOG(LogP9, Log, TEXT("Seat %s: %s sat down at %s"), *GetName(), *Character->GetName(), *Centre.ToCompactString());
	return true;
}

bool UP9SeatComponent::Stand()
{
	ACharacter* Character = Occupant.Get();
	Occupant.Reset();
	PromptText = NSLOCTEXT("P9", "SeatSit", "Sit down");
	if (!Character)
	{
		return false;
	}
	Character->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	FVector Spot = GetComponentTransform().TransformPosition(StandOffset);
	const FRotator Facing(0.0f, Character->GetActorRotation().Yaw, 0.0f);
	Character->GetCapsuleComponent()->SetCollisionEnabled(SavedCollision);
	if (UWorld* World = GetWorld())
	{
		World->FindTeleportSpot(Character, Spot, Facing);
	}
	Character->SetActorLocationAndRotation(Spot, Facing, false, nullptr, ETeleportType::TeleportPhysics);
	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	Movement->SetMovementMode(SavedMovementMode == MOVE_None || SavedMovementMode == MOVE_Falling ? MOVE_Walking : SavedMovementMode.GetValue());
	if (bBlockedMoveInput)
	{
		if (AController* Controller = Character->GetController())
		{
			Controller->SetIgnoreMoveInput(false);
		}
		bBlockedMoveInput = false;
	}
	UE_LOG(LogP9, Log, TEXT("Seat %s: %s stood up at %s"), *GetName(), *Character->GetName(), *Spot.ToCompactString());
	return true;
}
