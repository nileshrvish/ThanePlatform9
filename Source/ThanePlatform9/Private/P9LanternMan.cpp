#include "P9LanternMan.h"
#include "P9Captions.h"
#include "ThanePlatform9.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/LightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"

namespace P9Lantern
{
	const TCHAR* AudioDir = TEXT("/Game/P9/Audio/OriginalAudioPack/Creatures/");
	TSoftObjectPtr<USoundBase> Snd(const TCHAR* Name)
	{
		return TSoftObjectPtr<USoundBase>(FSoftObjectPath(FString::Printf(TEXT("%s%s.%s"), AudioDir, Name, Name)));
	}
	void Attenuate(UAudioComponent* A, float Inner, float Falloff)
	{
		A->bOverrideAttenuation = true;
		A->AttenuationOverrides.bAttenuate = true;
		A->AttenuationOverrides.bSpatialize = true;
		A->AttenuationOverrides.AttenuationShape = EAttenuationShape::Sphere;
		A->AttenuationOverrides.AttenuationShapeExtents = FVector(Inner, 0.0f, 0.0f);
		A->AttenuationOverrides.FalloffDistance = Falloff;
	}
}

AP9LanternMan::AP9LanternMan()
{
	PrimaryActorTick.bCanEverTick = true;
	Body = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Body"));
	Body->InitCapsuleSize(30.0f, 95.0f);
	Body->SetCollisionObjectType(ECC_WorldDynamic);
	Body->SetCollisionResponseToAllChannels(ECR_Ignore);
	Body->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);              // the player cannot walk through him; traces pass
	Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Body->SetGenerateOverlapEvents(false);
	Body->SetCanEverAffectNavigation(false);
	Body->CanCharacterStepUpOn = ECB_No;
	SetRootComponent(Body);

	Figure = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Figure"));
	Figure->SetupAttachment(Body);
	Figure->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, -95.0f), FRotator(0.0f, MeshYaw, 0.0f));
	Figure->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Figure->SetCanEverAffectNavigation(false);
	Figure->bNeverDistanceCull = true;
	Figure->SetCastShadow(false);

	Lantern = CreateDefaultSubobject<UPointLightComponent>(TEXT("Lantern"));
	Lantern->SetupAttachment(Body);
	Lantern->SetMobility(EComponentMobility::Movable);
	Lantern->SetCastShadows(false);
	Lantern->SetIntensityUnits(ELightUnits::Candelas);

	FlameAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("FlameAudio"));
	FlameAudio->SetupAttachment(Body);
	FlameAudio->bAutoActivate = false;
	P9Lantern::Attenuate(FlameAudio, 120.0f, 700.0f);
	RumbleAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("RumbleAudio"));
	RumbleAudio->SetupAttachment(Body);
	RumbleAudio->bAutoActivate = false;
	P9Lantern::Attenuate(RumbleAudio, 300.0f, 1500.0f);
	LampAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("LampAudio"));
	LampAudio->bAutoActivate = false;
	P9Lantern::Attenuate(LampAudio, 150.0f, 1600.0f);

	FlameLoop = P9Lantern::Snd(TEXT("SFX_LanternMan_Flame_Loop"));
	RumbleLoop = P9Lantern::Snd(TEXT("SFX_LanternMan_LowRumble_Loop"));
	FlickerSound = P9Lantern::Snd(TEXT("SFX_LanternMan_ElectricalFlicker"));
	Stinger = P9Lantern::Snd(TEXT("SFX_LanternMan_Stinger"));
}

void AP9LanternMan::Setup(UStaticMesh* Mesh, const TArray<ULightComponent*>& LampLights, UStaticMeshComponent* LampTube, int32 Seed)
{
	Rng.Initialize(Seed);
	Figure->SetStaticMesh(Mesh);
	Figure->SetRelativeRotation(FRotator(0.0f, MeshYaw, 0.0f));
	Lantern->SetRelativeLocation(LanternOffset - FVector(0.0f, 0.0f, 95.0f));
	Lantern->SetLightColor(LanternColor);
	Lantern->SetIntensity(LanternIntensity);
	Lantern->SetAttenuationRadius(LanternRadius);
	if (Figure->GetNumMaterials() > 0 && Figure->GetMaterial(0))
	{
		GlassMID = Figure->CreateDynamicMaterialInstance(0);
		if (GlassMID) GlassMID->SetScalarParameterValue(TEXT("EmissiveStrength"), LanternGlow);
	}
	Lamp.Reset(); LampBase.Reset();
	for (ULightComponent* L : LampLights)
	{
		if (!L) continue;
		Lamp.Add(L);
		LampBase.Add(L->Intensity);
	}
	if (LampTube)
	{
		for (int32 i = 0; i < LampTube->GetNumMaterials(); ++i)
		{
			const UMaterialInterface* M = LampTube->GetMaterial(i);
			if (M && M->GetName().Contains(TEXT("LampTube")))
			{
				TubeMID = LampTube->CreateDynamicMaterialInstance(i);
				if (TubeMID) TubeMID->GetScalarParameterValue(TEXT("Intensity"), TubeBase);
				break;
			}
		}
	}
	if (!Lamp.IsEmpty())
	{
		LampAudio->AttachToComponent(Lamp[0], FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	}
	if (USoundBase* S = FlameLoop.LoadSynchronous()) { FlameAudio->SetSound(S); FlameAudio->SetVolumeMultiplier(0.45f); FlameAudio->Play(Rng.FRandRange(0.0f, 6.0f)); }
	if (USoundBase* S = RumbleLoop.LoadSynchronous()) { RumbleAudio->SetSound(S); RumbleAudio->SetVolumeMultiplier(0.5f); RumbleAudio->Play(Rng.FRandRange(0.0f, 8.0f)); }
	bFlicker = true;
	NextBurst = Rng.FRandRange(0.8f, 2.0f);                     // the lamp starts misbehaving soon after he appears
	SetLamp(LampSteady);
	UE_LOG(LogP9, Log, TEXT("Lantern Man: standing at %s facing %.0f, lamp lights %d, tube glow %s"), *GetActorLocation().ToCompactString(), GetActorRotation().Yaw,
		Lamp.Num(), TubeMID ? TEXT("driven") : TEXT("none"));
}

void AP9LanternMan::SetLamp(float Scale)
{
	for (int32 i = 0; i < Lamp.Num(); ++i)
	{
		if (Lamp[i]) Lamp[i]->SetIntensity(LampBase[i] * Scale);
	}
	if (TubeMID) TubeMID->SetScalarParameterValue(TEXT("Intensity"), TubeBase * FMath::Max(0.02f, Scale));
}

void AP9LanternMan::StopFlicker()
{
	if (!bFlicker) return;
	bFlicker = false;
	SetLamp(1.0f);
	if (LampAudio) LampAudio->Stop();
}

void AP9LanternMan::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Time += DeltaSeconds;
	// the lantern's own flame: a slow, small waver of the light (the figure itself never moves)
	const float Waver = 1.0f + 0.07f * FMath::Sin(Time * 7.3f) + 0.05f * FMath::PerlinNoise1D(Time * 3.1f);
	Lantern->SetIntensity(LanternIntensity * Waver);
	if (GlassMID) GlassMID->SetScalarParameterValue(TEXT("EmissiveStrength"), LanternGlow * Waver);
	if (bFlicker)
	{
		if (BurstLeft > 0.0f)
		{
			BurstLeft -= DeltaSeconds;
			if ((BurstSwitch -= DeltaSeconds) <= 0.0f)
			{
				// electrical: hard on/off steps and brown-outs at irregular 20-140 ms intervals
				BurstSwitch = Rng.FRandRange(0.02f, 0.14f);
				const float R = Rng.FRand();
				BurstLevel = R < 0.35f ? 0.0f : (R < 0.6f ? Rng.FRandRange(0.08f, 0.3f) : (R < 0.85f ? 1.05f : LampSteady));
				SetLamp(BurstLevel);
			}
			if (BurstLeft <= 0.0f)
			{
				SetLamp(LampSteady);
				NextBurst = Rng.FRandRange(MinGap, MaxGap);
				if (LampAudio) LampAudio->FadeOut(0.15f, 0.0f);
			}
		}
		else if ((NextBurst -= DeltaSeconds) <= 0.0f)
		{
			BurstLeft = Rng.FRandRange(0.5f, 1.8f);
			BurstSwitch = 0.0f;
			++Bursts;
			if (USoundBase* S = FlickerSound.LoadSynchronous())
			{
				LampAudio->SetSound(S);
				LampAudio->SetVolumeMultiplier(0.55f);
				LampAudio->Play(Rng.FRandRange(0.0f, 1.0f));
			}
		}
	}
	// the first time the player sees him: one deep stinger (from him)
	if (!bSeen && (SeenCheck -= DeltaSeconds) <= 0.0f)
	{
		SeenCheck = 0.2f;
		const APlayerController* PC = GetWorld()->GetFirstPlayerController();
		if (PC && PC->PlayerCameraManager)
		{
			const FVector Cam = PC->PlayerCameraManager->GetCameraLocation();
			const FVector Head = GetActorLocation() + FVector(0.0f, 0.0f, 70.0f);
			const FVector To = Head - Cam;
			const float Dist = To.Size();
			if (Dist < 3000.0f && FVector::DotProduct(To / FMath::Max(Dist, 1.0f), PC->PlayerCameraManager->GetCameraRotation().Vector()) > 0.88f)
			{
				FHitResult Hit;
				FCollisionQueryParams Params(SCENE_QUERY_STAT(P9LanternSeen), false, this);
				Params.AddIgnoredActor(PC->GetPawn());
				if (!GetWorld()->LineTraceSingleByChannel(Hit, Cam, Head, ECC_Visibility, Params))
				{
					bSeen = true;
					if (USoundBase* S = Stinger.LoadSynchronous())
					{
						UGameplayStatics::SpawnSoundAtLocation(this, S, Head, FRotator::ZeroRotator, 0.8f, 1.0f, 0.0f, nullptr, nullptr, true);
						P9Captions::NotifySound(this, S);                // Stage 9B
					}
					UE_LOG(LogP9, Log, TEXT("Lantern Man: seen by the player from %.0f cm"), Dist);
				}
			}
		}
	}
}

void AP9LanternMan::EndPlay(const EEndPlayReason::Type Reason)
{
	StopFlicker();
	Super::EndPlay(Reason);
}
