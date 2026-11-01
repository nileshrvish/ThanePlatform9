#include "P9AnomalySpawnPoint.h"
#include "Components/ArrowComponent.h"

AP9AnomalySpawnPoint::AP9AnomalySpawnPoint()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Static);
	RootComponent = Root;
	SetHidden(true);
#if WITH_EDITORONLY_DATA
	Arrow = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("Arrow"));
	if (Arrow)
	{
		Arrow->SetupAttachment(Root);
		Arrow->ArrowColor = FColor(200, 60, 255);
		Arrow->ArrowSize = 0.6f;
		Arrow->bIsScreenSizeScaled = true;
	}
#endif
}

FString AP9AnomalySpawnPoint::GetTargetKey() const
{
	if (!TargetActor)
	{
		return FString::Printf(TEXT("point:%s"), *PointId.ToString());
	}
	return FString::Printf(TEXT("%s/%s"), *TargetActor->GetName(), *TargetComponent.ToString());
}
