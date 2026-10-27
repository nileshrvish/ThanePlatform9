#include "P9InstanceSet.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"

AP9InstanceSet::AP9InstanceSet()
{
	PrimaryActorTick.bCanEverTick = false;
	Instances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Instances"));
	Instances->SetMobility(EComponentMobility::Static);
	Instances->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Instances->SetCanEverAffectNavigation(false);
	RootComponent = Instances;
}
