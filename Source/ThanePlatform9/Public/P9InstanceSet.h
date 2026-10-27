#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "P9InstanceSet.generated.h"

class UHierarchicalInstancedStaticMeshComponent;

/** One mesh drawn many times (trackside trees, buildings, poles...) through a single HISM: few draw calls, culled per cluster. */
UCLASS()
class THANEPLATFORM9_API AP9InstanceSet : public AActor
{
	GENERATED_BODY()

public:
	AP9InstanceSet();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "P9")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Instances;
};
