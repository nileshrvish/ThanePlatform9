#include "P9InteractableComponent.h"
#include "P9InteractionSubsystem.h"
#include "Engine/World.h"

UP9InteractableComponent::UP9InteractableComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UP9InteractableComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UP9InteractionSubsystem* Subsystem = GetWorld() ? GetWorld()->GetSubsystem<UP9InteractionSubsystem>() : nullptr)
	{
		Subsystem->RegisterInteractable(this);
	}
}

void UP9InteractableComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UP9InteractionSubsystem* Subsystem = GetWorld() ? GetWorld()->GetSubsystem<UP9InteractionSubsystem>() : nullptr)
	{
		Subsystem->UnregisterInteractable(this);
	}
	Super::EndPlay(EndPlayReason);
}
