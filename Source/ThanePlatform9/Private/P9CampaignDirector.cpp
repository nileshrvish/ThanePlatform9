#include "P9CampaignDirector.h"
#include "ThanePlatform9.h"
#include "P9AnomalySpawnPoint.h"
#include "P9CampaignSave.h"
#include "P9SeatComponent.h"
#include "P9Train.h"
#include "P9StationAnnouncer.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/LightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Canvas.h"
#include "Engine/CanvasRenderTarget2D.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace P9Campaign
{
	constexpr int32 HudZOrder = 30;                  // above the interaction prompt (20) and the mobile controls
	constexpr float ResolveFadeTime = 1.0f;
	constexpr float ResolveHoldTime = 2.6f;          // feedback on screen before the next day is set up (screen black)
	constexpr float IntroFadeTime = 1.4f;
	constexpr float DecisionGraceTime = 1.0f;        // events in the first second of a day belong to the previous one
	constexpr float ZonePollInterval = 0.1f;
	constexpr float FigureVanishDistance = 1100.0f;
	const FName SpawnedTag(TEXT("P9Anomaly"));
	TAutoConsoleVariable<float> CVarScareFigureLife(TEXT("p9.Campaign.ScareHoldSeconds"), 0.65f,
		TEXT("How long the close-range apparition stays in front of the player (seconds; raise only to inspect it)."));

	FSoftObjectPath AssetPath(const TCHAR* Folder, const TCHAR* Name)
	{
		return FSoftObjectPath(FString::Printf(TEXT("%s/%s.%s"), Folder, Name, Name));
	}
	const TCHAR* MaterialFolder = TEXT("/Game/P9/Anomalies/Materials");
	const TCHAR* AudioFolder = TEXT("/Game/P9/Anomalies/Audio");

	FP9AnomalyVariant Variant(const TCHAR* Id, int32 Difficulty, float Weight = 1.0f)
	{
		FP9AnomalyVariant V;
		V.Id = FName(Id);
		V.Difficulty = Difficulty;
		V.Weight = Weight;
		return V;
	}
	FP9AnomalyVariant MaterialVariant(const TCHAR* Id, int32 Difficulty, const TCHAR* Material)
	{
		FP9AnomalyVariant V = Variant(Id, Difficulty);
		V.Material = TSoftObjectPtr<UMaterialInterface>(AssetPath(MaterialFolder, Material));
		return V;
	}
	/** Stage 4D: a variant that only changes once the player has seen the target and looked away. */
	FP9AnomalyVariant LaterVariant(const TCHAR* Id, int32 Difficulty, const TCHAR* Material)
	{
		FP9AnomalyVariant V = MaterialVariant(Id, Difficulty, Material);
		V.bChangeAfterLookAway = true;
		return V;
	}
	FP9AnomalyVariant MeshVariant(const TCHAR* Id, int32 Difficulty, const TCHAR* MeshPath, float Param = 0.0f)
	{
		FP9AnomalyVariant V = Variant(Id, Difficulty);
		V.Mesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(MeshPath));
		V.Param = Param;
		return V;
	}
	const TCHAR* HeroPath = TEXT("/Game/P9/Entities/SM_P9_Entity_Hero.SM_P9_Entity_Hero");
	const TCHAR* TallPath = TEXT("/Game/P9/Entities/SM_P9_Entity_Tall.SM_P9_Entity_Tall");
	const TCHAR* PassengerPath = TEXT("/Game/P9/Entities/SM_P9_Entity_Passenger.SM_P9_Entity_Passenger");

	template <typename T>
	T* FindNamedComponent(const AActor* Actor, const FString& Name)
	{
		if (!Actor)
		{
			return nullptr;
		}
		TArray<T*> Components;
		Actor->GetComponents<T>(Components);
		for (T* C : Components)
		{
			if (C->GetName() == Name)
			{
				return C;
			}
		}
		return nullptr;
	}

	int32 PickWeighted(FRandomStream& Rng, const TArray<float>& Weights)
	{
		float Total = 0.0f;
		for (float W : Weights)
		{
			Total += FMath::Max(W, 0.0f);
		}
		if (Total <= 0.0f)
		{
			return INDEX_NONE;
		}
		float R = Rng.FRand() * Total;
		for (int32 i = 0; i < Weights.Num(); ++i)
		{
			R -= FMath::Max(Weights[i], 0.0f);
			if (R < 0.0f)
			{
				return i;
			}
		}
		for (int32 i = Weights.Num() - 1; i >= 0; --i)
		{
			if (Weights[i] > 0.0f)
			{
				return i;
			}
		}
		return INDEX_NONE;
	}

	bool SharesTag(const TArray<FName>& A, const TArray<FName>& B)
	{
		for (const FName& Tag : A)
		{
			if (B.Contains(Tag))
			{
				return true;
			}
		}
		return false;
	}
}

AP9CampaignDirector::AP9CampaignDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetHidden(true);
	FigureMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/P9/Anomalies/SM_P9_Apparition.SM_P9_Apparition")));
	auto Scare = [](const TCHAR* N) { return TSoftObjectPtr<USoundBase>(P9Campaign::AssetPath(TEXT("/Game/P9/Anomalies/Audio4D"), N)); };
	StingClose = Scare(TEXT("SW_P9_Scare_StingClose"));
	StingCarriage = Scare(TEXT("SW_P9_Scare_StingCarriage"));
	StingFootbridge = Scare(TEXT("SW_P9_Scare_StingFootbridge"));
	ScareDrone = Scare(TEXT("SW_P9_Scare_Drone"));
	ScareRush = Scare(TEXT("SW_P9_Scare_ClothRush"));
	HeroMesh = TSoftObjectPtr<UStaticMesh>(P9Campaign::AssetPath(TEXT("/Game/P9/Entities"), TEXT("SM_P9_Entity_Hero")));
	TallMesh = TSoftObjectPtr<UStaticMesh>(P9Campaign::AssetPath(TEXT("/Game/P9/Entities"), TEXT("SM_P9_Entity_Tall")));
	PassengerMesh = TSoftObjectPtr<UStaticMesh>(P9Campaign::AssetPath(TEXT("/Game/P9/Entities"), TEXT("SM_P9_Entity_Passenger")));
	ChaiwalaMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/P9/Characters/SM_P9_Chaiwala.SM_P9_Chaiwala")));
	ChaiwalaWakeLine = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/P9/Announcements/SW_P9_VO_Chaiwala_Wake.SW_P9_VO_Chaiwala_Wake")));
	ChaiwalaCallLine = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/P9/Announcements/SW_P9_VO_Chaiwala_Call.SW_P9_VO_Chaiwala_Call")));
	TrainRunAmbience = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/P9/Environment/Trains/Audio/SW_P9_TrainRunLoop.SW_P9_TrainRunLoop")));
	// Stage 4B zones (cm, world): the footbridge route and Platform 9 allow randomised anomalies; Platforms 1-8 never do
	AnomalyZones = {
		FBox(FVector(-750.0f, -1760.0f, 540.0f), FVector(750.0f, 7960.0f, 1300.0f)),       // footbridge decks + concourse
		FBox(FVector(-3200.0f, 6560.0f, -160.0f), FVector(3200.0f, 7960.0f, 720.0f))};     // Platform 9/10 island, its stairs and track 9
	NormalZones = {
		FBox(FVector(-3200.0f, -1760.0f, -160.0f), FVector(3200.0f, -860.0f, 540.0f)),     // Platform 1
		FBox(FVector(-3200.0f, -120.0f, -160.0f), FVector(3200.0f, 980.0f, 540.0f)),       // Platforms 2/3
		FBox(FVector(-3200.0f, 1700.0f, -160.0f), FVector(3200.0f, 2800.0f, 540.0f)),      // Platforms 4/5
		FBox(FVector(-3200.0f, 3520.0f, -160.0f), FVector(3200.0f, 4620.0f, 540.0f)),      // Platforms 6/7
		FBox(FVector(-3200.0f, 5340.0f, -160.0f), FVector(3200.0f, 6130.0f, 540.0f))};     // Platform 8
	Story.IndicatorTextures = {
		{TEXT("/Game/P9/Environment/StationSignage/MI_P9_TrainIndicator_PF1"), TEXT("T_P9_TrainIndicator_PF1_4B")},
		{TEXT("/Game/P9/Environment/StationSignage/MI_P9_TrainIndicator_PF9_10"), TEXT("T_P9_TrainIndicator_PF9_10_4B")},
		{TEXT("/Game/P9/Environment/StationSignage/MI_P9_TrainIndicator_Concourse"), TEXT("T_P9_TrainIndicator_Concourse_4B")}};
	ReflectionSting = TSoftObjectPtr<USoundBase>(P9Campaign::AssetPath(P9Campaign::AudioFolder, TEXT("SW_P9_Anom_StingReflection")));
	ApparitionSting = TSoftObjectPtr<USoundBase>(P9Campaign::AssetPath(P9Campaign::AudioFolder, TEXT("SW_P9_Anom_StingApparition")));
	BuildDefaultCatalogue();
}

void AP9CampaignDirector::BuildDefaultCatalogue()
{
	using namespace P9Campaign;
	auto Def = [this](const TCHAR* Id, const TCHAR* Name, EP9AnomalyCategory Category, EP9AnomalyHandler Handler, float Weight,
		TArray<EP9AnomalyLocation> Locations, TArray<FName> InTags, TArray<FName> Exclude) -> FP9AnomalyDef&
	{
		FP9AnomalyDef& D = Anomalies.AddDefaulted_GetRef();
		D.Id = FName(Id);
		D.DisplayName = Name;
		D.Category = Category;
		D.Handler = Handler;
		D.Weight = Weight;
		D.LocationTypes = MoveTemp(Locations);
		D.Tags = MoveTemp(InTags);
		D.ExcludeTags = MoveTemp(Exclude);
		return D;
	};
	const FName Sign(TEXT("Sign")), Light(TEXT("Light")), Bench(TEXT("Bench")), Figure(TEXT("Figure")), TrainTag(TEXT("Train")), PA(TEXT("PA"));

	// ---- signage / environment ----
	{
		FP9AnomalyDef& D = Def(TEXT("PlatformSignWrong"), TEXT("Platform sign shows the wrong number"), EP9AnomalyCategory::Signage, EP9AnomalyHandler::MaterialSwap, 1.0f,
			{EP9AnomalyLocation::PlatformSign}, {Sign}, {});
		D.MaterialSlotMatch = TEXT("Sign_PF9_10");                    // Stage 4B: the Platform 9/10 signs
		D.Variants = {MaterialVariant(TEXT("Number13"), 2, TEXT("MI_P9_Anom_SignPF9_10_A")), LaterVariant(TEXT("ChangesBehindYou"), 3, TEXT("MI_P9_Anom_SignPF9_10_A")),
			MaterialVariant(TEXT("Misspelt"), 1, TEXT("MI_P9_Anom_SignPF9_10_C"))};
	}
	{
		FP9AnomalyDef& D = Def(TEXT("FOBSignWrong"), TEXT("Footbridge sign arrows / number wrong"), EP9AnomalyCategory::Signage, EP9AnomalyHandler::MaterialSwap, 0.9f,
			{EP9AnomalyLocation::FOBSign}, {Sign}, {});
		D.MaterialSlotMatch = TEXT("FOBSign_PF2_3");
		D.Variants = {MaterialVariant(TEXT("ArrowsReversed"), 2, TEXT("MI_P9_Anom_FOBSignPF2_3_A")), MaterialVariant(TEXT("Platform9"), 2, TEXT("MI_P9_Anom_FOBSignPF2_3_B"))};
	}
	{
		FP9AnomalyDef& D = Def(TEXT("FOBSignPF9Wrong"), TEXT("Footbridge sign for platforms 9/10 wrong"), EP9AnomalyCategory::Signage, EP9AnomalyHandler::MaterialSwap, 0.9f,
			{EP9AnomalyLocation::FOBSignPF9}, {Sign}, {});
		D.MaterialSlotMatch = TEXT("FOBSign_PF9_10");
		D.Variants = {MaterialVariant(TEXT("ArrowsReversed"), 2, TEXT("MI_P9_Anom_FOBSignPF9_10_A")), MaterialVariant(TEXT("Platform13"), 2, TEXT("MI_P9_Anom_FOBSignPF9_10_B"))};
	}
	{
		FP9AnomalyDef& D = Def(TEXT("IndicatorWrong"), TEXT("Train indicator shows something impossible"), EP9AnomalyCategory::Signage, EP9AnomalyHandler::MaterialSwap, 1.0f,
			{EP9AnomalyLocation::Indicator}, {Sign}, {});
		D.MaterialSlotMatch = TEXT("TrainIndicator_PF9_10");          // Stage 4B: the Platform 9/10 indicator (Vashi)
		D.Variants = {MaterialVariant(TEXT("ImpossibleTime"), 1, TEXT("MI_P9_Anom_IndicatorPF9_10_A")), MaterialVariant(TEXT("DoNotBoard"), 3, TEXT("MI_P9_Anom_IndicatorPF9_10_B")),
			MaterialVariant(TEXT("NoReturn"), 2, TEXT("MI_P9_Anom_IndicatorPF9_10_C")), LaterVariant(TEXT("NoReturnLater"), 3, TEXT("MI_P9_Anom_IndicatorPF9_10_C"))};
	}
	{
		FP9AnomalyDef& D = Def(TEXT("ClockWrong"), TEXT("Station clock shows the wrong time"), EP9AnomalyCategory::Environment, EP9AnomalyHandler::MaterialSwap, 1.0f,
			{EP9AnomalyLocation::Clock}, {}, {});
		D.MaterialSlotMatch = TEXT("StationClock");
		D.Variants = {MaterialVariant(TEXT("HalfHourBack"), 1, TEXT("MI_P9_Anom_Clock_A")), MaterialVariant(TEXT("Impossible"), 2, TEXT("MI_P9_Anom_Clock_B")),
			MaterialVariant(TEXT("Midnight"), 2, TEXT("MI_P9_Anom_Clock_C")), LaterVariant(TEXT("StopsAtMidnight"), 3, TEXT("MI_P9_Anom_Clock_C"))};
	}
	{
		FP9AnomalyDef& D = Def(TEXT("LampFault"), TEXT("Platform lamp off / red / flickering"), EP9AnomalyCategory::Environment, EP9AnomalyHandler::LampFault, 1.0f,
			{EP9AnomalyLocation::Lamp}, {Light}, {});
		FP9AnomalyVariant Off = Variant(TEXT("Off"), 2); Off.Param = 0.0f;
		FP9AnomalyVariant Flicker = Variant(TEXT("Flicker"), 1); Flicker.Param = 1.0f;
		FP9AnomalyVariant Red = Variant(TEXT("Red"), 3); Red.Param = 2.0f; Red.Color = FLinearColor(1.0f, 0.08f, 0.05f);
		D.Variants = {Off, Flicker, Red};
	}
	{
		FP9AnomalyDef& D = Def(TEXT("BenchMissing"), TEXT("A platform bench is gone"), EP9AnomalyCategory::Environment, EP9AnomalyHandler::HideActor, 0.9f,
			{EP9AnomalyLocation::Bench}, {Bench}, {Bench});
		D.Variants = {Variant(TEXT("Gone"), 2)};
	}
	{
		FP9AnomalyDef& D = Def(TEXT("ExtraBench"), TEXT("An extra bench on the platform"), EP9AnomalyCategory::Other, EP9AnomalyHandler::ExtraProp, 0.8f,
			{EP9AnomalyLocation::BenchSlot}, {Bench}, {Bench});
		FP9AnomalyVariant Aligned = Variant(TEXT("Aligned"), 1); Aligned.Param = 0.0f;
		FP9AnomalyVariant Turned = Variant(TEXT("Turned"), 2); Turned.Param = 90.0f;
		D.Variants = {Aligned, Turned};
	}
	// ---- train ----
	{
		FP9AnomalyDef& D = Def(TEXT("TrainDestination"), TEXT("Train destination board is wrong"), EP9AnomalyCategory::Train, EP9AnomalyHandler::TrainDestination, 0.9f,
			{EP9AnomalyLocation::TrainDestination}, {TrainTag}, {});
		FP9AnomalyVariant A = Variant(TEXT("Platform9"), 2); A.Text = TEXT("PLATFORM 9");
		FP9AnomalyVariant B = Variant(TEXT("NoReturn"), 3); B.Text = TEXT("NO RETURN");
		FP9AnomalyVariant C = Variant(TEXT("Misspelt"), 1); C.Text = TEXT("VAHSI  S");
		D.Variants = {A, B, C};
	}
	{
		FP9AnomalyDef& D = Def(TEXT("TrainSeats"), TEXT("Train seats are the wrong colour"), EP9AnomalyCategory::Train, EP9AnomalyHandler::TrainSeatTint, 0.8f,
			{EP9AnomalyLocation::TrainSeats}, {TrainTag}, {});
		FP9AnomalyVariant Red = MaterialVariant(TEXT("Red"), 2, TEXT("MI_P9_Anom_TrainSeat_Red"));
		FP9AnomalyVariant Pale = Variant(TEXT("Bloodless"), 1); Pale.Color = FLinearColor(1.6f, 1.6f, 1.6f);
		D.Variants = {Red, Pale};
	}
	{
		FP9AnomalyDef& D = Def(TEXT("TrainLights"), TEXT("Train interior lights abnormal"), EP9AnomalyCategory::Train, EP9AnomalyHandler::TrainLights, 0.8f,
			{EP9AnomalyLocation::TrainLights}, {TrainTag, Light}, {});
		FP9AnomalyVariant Off = Variant(TEXT("Dark"), 2); Off.Param = 0.0f;
		FP9AnomalyVariant Flicker = Variant(TEXT("Flicker"), 1); Flicker.Param = 1.0f;
		FP9AnomalyVariant Red = Variant(TEXT("Red"), 3); Red.Param = 2.0f; Red.Color = FLinearColor(1.0f, 0.05f, 0.03f);
		D.Variants = {Off, Flicker, Red};
	}
	// ---- audio ----
	{
		FP9AnomalyDef& D = Def(TEXT("AnnouncementWrong"), TEXT("Station announcement is wrong"), EP9AnomalyCategory::Audio, EP9AnomalyHandler::PAAnnouncement, 1.0f,
			{EP9AnomalyLocation::PASystem}, {PA}, {PA});
		// Stage 4B: the Marathi Platform 9 arrival announcement for the Vashi local, altered (played when it is called in)
		const TCHAR* PAFolder = TEXT("/Game/P9/Announcements");
		FP9AnomalyVariant P13 = Variant(TEXT("Platform13"), 2); P13.Sound = TSoftObjectPtr<USoundBase>(AssetPath(PAFolder, TEXT("SW_P9_PA_PF9_Anom_Platform13"))); P13.Param = 1.0f;
		FP9AnomalyVariant Turn = Variant(TEXT("DoNotTurn"), 3); Turn.Sound = TSoftObjectPtr<USoundBase>(AssetPath(PAFolder, TEXT("SW_P9_PA_PF9_Anom_DoNotTurn"))); Turn.Param = 1.0f;
		FP9AnomalyVariant Slow = Variant(TEXT("Slowed"), 1); Slow.Sound = TSoftObjectPtr<USoundBase>(AssetPath(PAFolder, TEXT("SW_P9_PA_PF9_VashiArrival"))); Slow.Param = 0.78f;
		D.Variants = {P13, Turn, Slow};
	}
	{
		FP9AnomalyDef& D = Def(TEXT("Footsteps"), TEXT("Footsteps following the player"), EP9AnomalyCategory::Audio, EP9AnomalyHandler::FollowFootsteps, 0.8f,
			{EP9AnomalyLocation::PlayerFollow}, {}, {});
		FP9AnomalyVariant V = Variant(TEXT("Behind"), 2); V.Sound = TSoftObjectPtr<USoundBase>(AssetPath(AudioFolder, TEXT("SW_P9_Anom_Footsteps")));
		D.Variants = {V};
	}
	{
		FP9AnomalyDef& D = Def(TEXT("PhantomHorn"), TEXT("Train horn from where no track runs"), EP9AnomalyCategory::Audio, EP9AnomalyHandler::PhantomHorn, 0.7f,
			{EP9AnomalyLocation::HornSource}, {}, {});
		FP9AnomalyVariant V = Variant(TEXT("Distant"), 2); V.Sound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/P9/Environment/Trains/Audio/SW_P9_TrainHorn.SW_P9_TrainHorn")));
		D.Variants = {V};
		D.bEnabled = false;                                          // Stage 4D: replaced by GhostTrain (sound with a visible source)
	}
	{
		FP9AnomalyDef& D = Def(TEXT("GhostTrain"), TEXT("A dark train on track 10 that should not exist"), EP9AnomalyCategory::Train, EP9AnomalyHandler::StaticProp, 0.7f,
			{EP9AnomalyLocation::DistantView}, {TEXT("Distant")}, {});
		// the local's own livery and lit headlights: a near-black shell vanished into the night ground in PIE
		FP9AnomalyVariant V = MeshVariant(TEXT("Waiting"), 2, TEXT("/Game/P9/Environment/Trains/SM_P9_LocalTrain_CabShell.SM_P9_LocalTrain_CabShell"));
		V.Sound = TSoftObjectPtr<USoundBase>(AssetPath(TEXT("/Game/P9/Anomalies/Audio4D"), TEXT("SW_P9_Scare_GhostTrain")));
		D.Variants = {V};
	}
	{
		FP9AnomalyDef& D = Def(TEXT("PosterWrong"), TEXT("The tea advertisement is watching / someone is behind him"), EP9AnomalyCategory::Signage, EP9AnomalyHandler::MaterialSwap, 1.0f,
			{EP9AnomalyLocation::Poster}, {TEXT("Poster")}, {});
		D.MaterialSlotMatch = TEXT("AdPoster");
		D.Variants = {MaterialVariant(TEXT("Watching"), 1, TEXT("MI_P9_Anom_AdPoster_Watching")), MaterialVariant(TEXT("SomeoneBehind"), 2, TEXT("MI_P9_Anom_AdPoster_Behind")),
			LaterVariant(TEXT("WatchingLater"), 3, TEXT("MI_P9_Anom_AdPoster_Watching"))};
	}
	{
		FP9AnomalyDef& D = Def(TEXT("WallSilhouette"), TEXT("A human shape pressed into the end wall"), EP9AnomalyCategory::Supernatural, EP9AnomalyHandler::StaticProp, 0.8f,
			{EP9AnomalyLocation::WallRelief}, {Figure}, {Figure});
		FP9AnomalyVariant Pressed = MeshVariant(TEXT("Pressed"), 3, HeroPath); Pressed.Scale = FVector(0.16f, 1.0f, 1.0f);
		FP9AnomalyVariant Emerging = MeshVariant(TEXT("Emerging"), 2, HeroPath); Emerging.Scale = FVector(0.45f, 1.0f, 1.0f);
		for (FP9AnomalyVariant* P : {&Pressed, &Emerging}) P->Material = TSoftObjectPtr<UMaterialInterface>(AssetPath(MaterialFolder, TEXT("MI_P9_Anom_WallStain")));
		D.Variants = {Pressed, Emerging};
	}
	{
		FP9AnomalyDef& D = Def(TEXT("ImpossibleShadow"), TEXT("A person's shadow with nobody there"), EP9AnomalyCategory::Supernatural, EP9AnomalyHandler::StaticProp, 0.8f,
			{EP9AnomalyLocation::FloorShadow}, {TEXT("Shadow")}, {});
		FP9AnomalyVariant Stand = MeshVariant(TEXT("Standing"), 2, TEXT("/Game/P9/Anomalies/SM_P9_FloorShadow.SM_P9_FloorShadow"));
		FP9AnomalyVariant Long = MeshVariant(TEXT("Long"), 1, TEXT("/Game/P9/Anomalies/SM_P9_FloorShadow.SM_P9_FloorShadow")); Long.Scale = FVector(1.6f, 1.1f, 1.0f);
		for (FP9AnomalyVariant* P : {&Stand, &Long}) P->Material = TSoftObjectPtr<UMaterialInterface>(AssetPath(MaterialFolder, TEXT("MI_P9_Anom_FloorShadow")));
		D.Variants = {Stand, Long};
	}
	// ---- supernatural ----
	{
		FP9AnomalyDef& D = Def(TEXT("DistantFigure"), TEXT("A figure at the end of the platform"), EP9AnomalyCategory::Supernatural, EP9AnomalyHandler::DistantFigure, 0.9f,
			{EP9AnomalyLocation::PlatformEnd}, {Figure}, {Figure});
		FP9AnomalyVariant Facing = MeshVariant(TEXT("Facing"), 2, HeroPath, 0.0f);
		FP9AnomalyVariant Away = MeshVariant(TEXT("BackTurned"), 2, HeroPath, 180.0f);
		D.Variants = {Facing, Away};
		D.Jumpscare = EP9JumpscareType::Apparition;
		D.JumpscareChance = 0.45f;
	}
	{
		FP9AnomalyDef& D = Def(TEXT("WatchingFigure"), TEXT("A figure on the footbridge, gone when you look away"), EP9AnomalyCategory::Supernatural, EP9AnomalyHandler::WatchingFigure, 0.8f,
			{EP9AnomalyLocation::Footbridge}, {Figure}, {Figure});
		FP9AnomalyVariant V = MeshVariant(TEXT("Watching"), 2, TallPath, 0.0f);
		FP9AnomalyVariant Near = MeshVariant(TEXT("Approaching"), 3, TallPath, 1.0f);   // closer every time you look back
		D.Variants = {V, Near};
		D.Jumpscare = EP9JumpscareType::Footbridge;                 // only the Approaching variant can lunge
		D.JumpscareChance = 0.5f;
	}
	{
		FP9AnomalyDef& D = Def(TEXT("WindowFigure"), TEXT("Someone standing inside the train"), EP9AnomalyCategory::Supernatural, EP9AnomalyHandler::WindowFigure, 0.7f,
			{EP9AnomalyLocation::TrainWindow}, {Figure, TrainTag}, {Figure});
		FP9AnomalyVariant FarEnd = MeshVariant(TEXT("FarEnd"), 2, HeroPath, 0.0f);       // can become jumpscare B
		FP9AnomalyVariant Door = MeshVariant(TEXT("AtDoor"), 2, HeroPath, 1.0f);
		FP9AnomalyVariant Window = MeshVariant(TEXT("FacingWindow"), 3, HeroPath, 2.0f);  // standing, face to the glass
		D.Variants = {FarEnd, Door, Window};
		D.Jumpscare = EP9JumpscareType::Carriage;
		D.JumpscareChance = 0.5f;
	}
}

// =====================================================================================================================
// generation
// =====================================================================================================================
FP9DayPlan AP9CampaignDirector::GenerateDayPlan(int32 InRunSeed, int32 InDay, int32 Attempt, const TArray<FString>& InHistory) const
{
	using namespace P9Campaign;
	FP9DayPlan Plan;
	Plan.Day = InDay;
	Plan.Attempt = Attempt;
	// the day's type and difficulty depend on (seed, day) only, so retries re-roll the anomalies, never the answer
	const uint32 DaySeed = HashCombine(GetTypeHash(InRunSeed), GetTypeHash(InDay * 7919 + 13));
	FRandomStream DayRng(static_cast<int32>(DaySeed));
	Plan.bAnomalous = DayRng.FRand() < AnomalyDayChance;
	Plan.Difficulty = FMath::Max(1, PickWeighted(DayRng, DifficultyWeights) + 1);
	Plan.Seed = static_cast<int32>(HashCombine(DaySeed, GetTypeHash(Attempt * 104729 + 7)));
	if (!Plan.bAnomalous)
	{
		return Plan;
	}
	FRandomStream Rng(Plan.Seed);
	// extreme days always stack at least two anomalies
	const int32 Want = FMath::Max(Plan.Difficulty >= 4 ? 2 : 1, PickWeighted(Rng, AnomalyCountWeights) + 1);

	TSet<int32> UsedPoints;
	TSet<FString> UsedTargets;
	TArray<FName> DayTags;
	TArray<int32> UsedDefs;
	auto FreePoints = [&](const FP9AnomalyDef& D)
	{
		TArray<int32> Out;
		for (int32 p = 0; p < SpawnPoints.Num(); ++p)
		{
			const AP9AnomalySpawnPoint* Point = SpawnPoints[p];
			if (Point && D.LocationTypes.Contains(Point->LocationType) && !UsedPoints.Contains(p) && !UsedTargets.Contains(Point->GetTargetKey()) && IsPointAllowed(Point))
			{
				Out.Add(p);
			}
		}
		return Out;
	};
	for (int32 n = 0; n < Want; ++n)
	{
		TArray<int32> Candidates;
		TArray<float> Weights;
		for (int32 d = 0; d < Anomalies.Num(); ++d)
		{
			const FP9AnomalyDef& D = Anomalies[d];
			if (!D.bEnabled || D.Variants.IsEmpty() || UsedDefs.Contains(d) || SharesTag(D.ExcludeTags, DayTags) || FreePoints(D).IsEmpty())
			{
				continue;
			}
			bool bExcludedByPicked = false;
			for (int32 u : UsedDefs)
			{
				bExcludedByPicked |= SharesTag(Anomalies[u].ExcludeTags, D.Tags);
			}
			if (bExcludedByPicked)
			{
				continue;
			}
			float W = D.Weight;
			for (int32 k = 1; k <= RepetitionPenalty.Num() && k <= InHistory.Num(); ++k)
			{
				TArray<FString> Ids;
				InHistory[InHistory.Num() - k].ParseIntoArray(Ids, TEXT("|"));
				if (Ids.Contains(D.Id.ToString()))
				{
					W *= RepetitionPenalty[k - 1];
				}
			}
			Candidates.Add(d);
			Weights.Add(W);
		}
		const int32 Pick = PickWeighted(Rng, Weights);
		if (Pick == INDEX_NONE)
		{
			break;
		}
		const int32 DefIndex = Candidates[Pick];
		const FP9AnomalyDef& D = Anomalies[DefIndex];
		const TArray<int32> Points = FreePoints(D);
		const int32 PointIndex = Points[Rng.RandRange(0, Points.Num() - 1)];
		TArray<float> VariantWeights;
		for (const FP9AnomalyVariant& V : D.Variants)
		{
			VariantWeights.Add(V.Weight * (V.Difficulty == FMath::Min(Plan.Difficulty, 3) ? 2.0f : 1.0f));
		}
		const int32 VariantIndex = FMath::Max(0, PickWeighted(Rng, VariantWeights));
		// subtle days never scare; extreme days scare more often (still a roll, so never guaranteed)
		const float ScareChance = Plan.Difficulty <= 1 ? 0.0f : D.JumpscareChance * (Plan.Difficulty >= 4 ? 1.5f : 1.0f);
		const bool bScare = D.Jumpscare != EP9JumpscareType::None && Rng.FRand() < ScareChance;

		FP9PlannedAnomaly& A = Plan.Anomalies.AddDefaulted_GetRef();
		A.DefIndex = DefIndex;
		A.VariantIndex = VariantIndex;
		A.PointIndex = PointIndex;
		A.bJumpscare = bScare;
		A.DefId = D.Id;
		A.VariantId = D.Variants[VariantIndex].Id;
		A.PointId = SpawnPoints[PointIndex]->PointId;
		UsedDefs.Add(DefIndex);
		UsedPoints.Add(PointIndex);
		UsedTargets.Add(SpawnPoints[PointIndex]->GetTargetKey());
		DayTags.Append(D.Tags);
	}
	return Plan;
}

int32 AP9CampaignDirector::FindAnomalyDef(FName Id) const
{
	return Anomalies.IndexOfByPredicate([Id](const FP9AnomalyDef& D) { return D.Id == Id; });
}

int32 AP9CampaignDirector::FindSpawnPoint(FName Id) const
{
	return SpawnPoints.IndexOfByPredicate([Id](const AP9AnomalySpawnPoint* P) { return P && P->PointId == Id; });
}

// =====================================================================================================================
// lifecycle
// =====================================================================================================================
void AP9CampaignDirector::BeginPlay()
{
	Super::BeginPlay();
	SpatialAttenuation = NewObject<USoundAttenuation>(this, TEXT("P9CampaignAttenuation"));
	SpatialAttenuation->Attenuation.bAttenuate = true;
	SpatialAttenuation->Attenuation.bSpatialize = true;
	SpatialAttenuation->Attenuation.AttenuationShapeExtents = FVector(150.0f);
	SpatialAttenuation->Attenuation.FalloffDistance = 2500.0f;
	if (Train)
	{
		Train->bAutoCycle = false;                                  // the director calls the Vashi local in and sends it off
		Train->OnDeparted.AddDynamic(this, &AP9CampaignDirector::HandleTrainDeparted);
		Train->OnArrived.AddDynamic(this, &AP9CampaignDirector::HandleTrainArrived);
	}
	if (OpeningTrain)
	{
		OpeningTrain->bAutoCycle = false;
		OpeningTrain->OnArrived.AddDynamic(this, &AP9CampaignDirector::HandleOpeningArrived);
	}
	int32 Invalid = 0, Rejected = 0;
	for (const AP9AnomalySpawnPoint* P : SpawnPoints)
	{
		Invalid += P ? 0 : 1;
		FString Why;
		if (P && !IsPointAllowed(P, &Why))
		{
			++Rejected;
			UE_LOG(LogP9, Warning, TEXT("Campaign: spawn point %s rejected for randomised anomalies - %s"), *P->PointId.ToString(), *Why);
		}
	}
	UE_LOG(LogP9, Log, TEXT("Campaign: director ready - %d anomaly definitions, %d spawn points (%d invalid, %d outside the anomaly zone), Vashi local %s, Dadar local %s, announcer %s"),
		Anomalies.Num(), SpawnPoints.Num(), Invalid, Rejected, Train ? *Train->GetName() : TEXT("missing"), OpeningTrain ? *OpeningTrain->GetName() : TEXT("missing"),
		Announcer ? *Announcer->GetName() : TEXT("missing"));
	// the first day starts on the first tick, when every actor (train seats, player pawn) has begun play
}

void AP9CampaignDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CleanupDay();
	SetEndingInput(false);
	RemoveHud();
	if (Train)
	{
		Train->OnDeparted.RemoveDynamic(this, &AP9CampaignDirector::HandleTrainDeparted);
		Train->OnArrived.RemoveDynamic(this, &AP9CampaignDirector::HandleTrainArrived);
	}
	if (OpeningTrain)
	{
		OpeningTrain->OnArrived.RemoveDynamic(this, &AP9CampaignDirector::HandleOpeningArrived);
	}
	Super::EndPlay(EndPlayReason);
}

void AP9CampaignDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	WorldTimeNow = GetWorld()->GetTimeSeconds();
	if (Phase == EP9DayPhase::Inactive)
	{
		if (!PlayerCharacter())
		{
			return;
		}
		TakeInitialSnapshot();
		EnsureHud();
		if (DebugSeed == 0 && bUseSaveGame && LoadProgress())
		{
			UE_LOG(LogP9, Log, TEXT("Campaign: resumed save - run %d seed %d day %d status %s"), RunId, RunSeed, Day,
				Status == EP9CampaignStatus::Completed ? TEXT("completed") : TEXT("in progress"));
			HideConnectingAtHold();
			SetOpeningTrainShown(false);                             // resumed: the opening is over; StartEpilogue shows it again
			if (Status == EP9CampaignStatus::Completed)
			{
				// Day 10 was completed: the epilogue replays from the wake-up unless it was finished
				EpilogueStage >= 2 ? EnterEnding() : StartEpilogue();
				return;
			}
			StartDay();
		}
		else
		{
			RunId = 1;
			RunSeed = DebugSeed != 0 ? DebugSeed : NewRandomSeed();
			Day = 1;
			History.Reset();
			Status = EP9CampaignStatus::InProgress;
			EpilogueStage = 0;
			HideConnectingAtHold();
			bPlayOpening && OpeningTrain ? StartOpening() : StartDay();
		}
		return;
	}
	DayTime += DeltaSeconds;
	if (Phase == EP9DayPhase::Exploring || Phase == EP9DayPhase::Resolving)
	{
		for (FP9ActiveAnomaly& A : ActiveAnomalies)
		{
			TickAnomaly(A, DeltaSeconds);
		}
		TickDecisionZone(DeltaSeconds);
	}
	if (Phase == EP9DayPhase::Opening) TickOpening(DeltaSeconds);
	if (Phase == EP9DayPhase::Epilogue) TickEpilogue(DeltaSeconds);
	TickConnectingTrain(DeltaSeconds);
	TickPhase(DeltaSeconds);
	TickPresentation(DeltaSeconds);
}

int32 AP9CampaignDirector::NewRandomSeed() const
{
	if (DebugSeed != 0)
	{
		return static_cast<int32>(HashCombine(GetTypeHash(DebugSeed), GetTypeHash(RunId)) & 0x7fffffff);
	}
	const int64 Ticks = FDateTime::UtcNow().GetTicks();
	return static_cast<int32>(HashCombine(GetTypeHash(Ticks), GetTypeHash(FPlatformTime::Cycles())) & 0x7fffffff);
}

void AP9CampaignDirector::StartNewRun(int32 Seed)
{
	CleanupDay();
	SetEndingInput(false);
	++RunId;
	RunSeed = Seed != 0 ? Seed : NewRandomSeed();
	Day = 1;
	History.Reset();
	Status = EP9CampaignStatus::InProgress;
	EpilogueStage = 0;
	bFinalEnding = false;
	DayLabelOverride = FText::GetEmpty();
	UE_LOG(LogP9, Log, TEXT("Campaign: new run %d seed %d"), RunId, RunSeed);
	EnsureHud();
	StartDay();
}

void AP9CampaignDirector::RestartDay()
{
	Status = EP9CampaignStatus::InProgress;
	SetEndingInput(false);
	StartDay();
}

void AP9CampaignDirector::SetDay(int32 NewDay)
{
	Day = FMath::Clamp(NewDay, 1, NumDays);
	Status = EP9CampaignStatus::InProgress;
	SetEndingInput(false);
	StartDay();
}

void AP9CampaignDirector::StartDay(bool bTeleport)
{
	++DayToken;
	CleanupDay();
	if (Announcer)
	{
		Announcer->StopAll(1.0f);                                    // scene change: let no announcement run into the new day
	}
	Phase = EP9DayPhase::Exploring;
	bDecisionLocked = false;
	bZoneArmed = false;
	bOpeningDone = true;
	DayTime = 0.0f;
	ZonePollTime = 0.0f;
	if (bTeleport)
	{
		PlacePlayerAtStart();
	}
	// Stage 5B: the screen is black here; the Vashi local must not carry yesterday's position or call-in state into the day
	ResetConnectingTrainForNewDay(bTeleport);

	FP9DayPlan Plan;
	if (bHasForcedPlan)
	{
		Plan = ForcedPlan;
		Plan.Day = Day;
		bHasForcedPlan = false;
		for (const FP9PlannedAnomaly& P : Plan.Anomalies)
		{
			ActivateAnomaly(P);
		}
		UE_LOG(LogP9, Log, TEXT("Campaign: forced plan used (debug)"));
	}
	else
	{
		for (int32 Attempt = 0; ; ++Attempt)
		{
			Plan = GenerateDayPlan(RunSeed, Day, Attempt, History);
			for (const FP9PlannedAnomaly& P : Plan.Anomalies)
			{
				ActivateAnomaly(P);
			}
			if (!Plan.bAnomalous || ActiveAnomalies.Num() > 0)
			{
				break;
			}
			CleanupDay();
			if (Attempt >= MaxGenerationRetries)
			{
				Plan.bFallbackNormal = true;
				Plan.Anomalies.Reset();
				UE_LOG(LogP9, Warning, TEXT("Campaign: no anomaly could be activated after %d attempts - day %d falls back to a normal day"), Attempt + 1, Day);
				break;
			}
			UE_LOG(LogP9, Log, TEXT("Campaign: attempt %d activated nothing - retrying selection"), Attempt);
		}
	}
	CurrentPlan = Plan;
	PresentationStream.Initialize(Plan.Seed ^ 0x2F6E2B1);
	for (FP9ActiveAnomaly& A : ActiveAnomalies)
	{
		A.NextEvent = PresentationStream.FRandRange(6.0f, 14.0f);
	}
	SaveProgress();

	FString Active;
	for (const FP9ActiveAnomaly& A : ActiveAnomalies)
	{
		Active += FString::Printf(TEXT(" %s:%s@%s%s"), *A.Plan.DefId.ToString(), *A.Plan.VariantId.ToString(), *A.Plan.PointId.ToString(), A.Plan.bJumpscare ? TEXT("(scare)") : TEXT(""));
	}
	UE_LOG(LogP9, Log, TEXT("Campaign: DAY %d/%d started (run %d seed %d token %d) plan: %s | active %d:%s"),
		Day, NumDays, RunId, RunSeed, DayToken, *Plan.ToString(), ActiveAnomalies.Num(), *Active);

	FadeAlpha = 1.0f;
	FadeTarget = 0.0f;
	FadeSpeed = 1.0f / P9Campaign::IntroFadeTime;
	ShowTitle(FText::Format(NSLOCTEXT("P9", "DayTitle", "DAY {0} / {1}"), FText::AsNumber(Day), FText::AsNumber(NumDays)),
		NSLOCTEXT("P9", "Objective", "Look carefully on your way to Platform 9.\nIf anything is wrong, go back to Platform 1.\nIf everything is normal, take the Vashi local from Platform 9."), 6.0f);
	OnDayStarted.Broadcast(Day);
}

// =====================================================================================================================
// decisions
// =====================================================================================================================
void AP9CampaignDirector::HandleTrainDeparted(int32 StopIndex)
{
	if (!Train || StopIndex != ConnectingPlatformStop)
	{
		return;                                                       // leaving the hold stop: not a departure from Platform 9
	}
	bConnectingDepartRequested = false;
	bConnectingCalled = false;
	ConnectingOpenAt = -1.0f;
	ConnectingDwell = 0.0f;
	ConnectingAboardTime = 0.0f;
	if (Announcer)
	{
		Announcer->Play(TEXT("PA_PF9_VashiDeparture"), FString::Printf(TEXT("PF9-departure-%d"), ConnectingCalls));
	}
	const bool bAboard = Train->IsPlayerAboard();
	UE_LOG(LogP9, Log, TEXT("Campaign: Vashi local departed Platform 9 (%s)"), bAboard ? TEXT("player aboard") : TEXT("without the player"));
	if (!bAboard)
	{
		return;
	}
	if (Phase == EP9DayPhase::Epilogue)
	{
		if (EpilogueStep == 1)
		{
			EpilogueStep = 2;
			RideTime = 0.0f;
			StoryCue = 0;
			UE_LOG(LogP9, Log, TEXT("Story: epilogue - riding the Vashi local"));
		}
		return;
	}
	ResolveDecision(EP9Decision::BoardTrain, DayToken);
}

void AP9CampaignDirector::TickDecisionZone(float DeltaSeconds)
{
	ZonePollTime += DeltaSeconds;
	if (ZonePollTime < P9Campaign::ZonePollInterval || Phase != EP9DayPhase::Exploring || bDecisionLocked)
	{
		return;
	}
	ZonePollTime = 0.0f;
	if (!PlayerCharacter())
	{
		return;
	}
	const bool bInside = Platform1Zone.IsInsideOrOn(PlayerFeet());
	if (!bInside)
	{
		if (!bZoneArmed && DayTime > 0.5f)
		{
			bZoneArmed = true;
		}
	}
	else if (bZoneArmed)
	{
		UE_LOG(LogP9, Log, TEXT("Campaign: player reached Platform 1 at %s"), *PlayerFeet().ToCompactString());
		ResolveDecision(EP9Decision::ReturnToPlatform1, DayToken);
	}
}

void AP9CampaignDirector::ResolveDecision(EP9Decision Decision, int32 Token)
{
	const TCHAR* Name = Decision == EP9Decision::ReturnToPlatform1 ? TEXT("return-to-PF1") : TEXT("board-train");
	if (Token != DayToken || Phase != EP9DayPhase::Exploring || bDecisionLocked)
	{
		UE_LOG(LogP9, Log, TEXT("Campaign: decision %s ignored (token %d/%d phase %d locked %d)"), Name, Token, DayToken, static_cast<int32>(Phase), bDecisionLocked ? 1 : 0);
		return;
	}
	if (DayTime < P9Campaign::DecisionGraceTime)
	{
		UE_LOG(LogP9, Log, TEXT("Campaign: decision %s ignored (%.2fs into the day - stale event)"), Name, DayTime);
		return;
	}
	bDecisionLocked = true;
	Phase = EP9DayPhase::Resolving;
	PhaseTime = 0.0f;
	LastDecision = Decision;
	const int32 Active = ActiveAnomalies.Num();
	bLastCorrect = Decision == EP9Decision::ReturnToPlatform1 ? Active > 0 : Active == 0;
	UE_LOG(LogP9, Log, TEXT("Campaign: DECISION day %d %s with %d active anomalies -> %s"), Day, Name, Active, bLastCorrect ? TEXT("CORRECT") : TEXT("WRONG"));
	OnDecision.Broadcast(Day, Decision, bLastCorrect);
	if (bLastCorrect)
	{
		ShowTitle(NSLOCTEXT("P9", "Correct", "CORRECT"), FText::Format(NSLOCTEXT("P9", "DayCleared", "Day {0} cleared"), FText::AsNumber(Day)), P9Campaign::ResolveHoldTime + 0.3f);
	}
	else
	{
		ShowTitle(NSLOCTEXT("P9", "Wrong", "WRONG"), NSLOCTEXT("P9", "BackToDay1", "Back to Day 1"), P9Campaign::ResolveHoldTime + 0.3f);
	}
	FadeTarget = 1.0f;
	FadeSpeed = 1.0f / P9Campaign::ResolveFadeTime;
}

void AP9CampaignDirector::TickPhase(float DeltaSeconds)
{
	if (Phase != EP9DayPhase::Resolving)
	{
		return;
	}
	PhaseTime += DeltaSeconds;
	if (PhaseTime >= P9Campaign::ResolveHoldTime)
	{
		AfterResolution();
	}
}

void AP9CampaignDirector::AfterResolution()
{
	if (!bLastCorrect)
	{
		UE_LOG(LogP9, Log, TEXT("Campaign: wrong decision - run %d ends on day %d"), RunId, Day);
		StartNewRun(0);
		return;
	}
	if (Day >= NumDays)
	{
		StartEpilogue();                                             // Stage 4B: the chaiwala wakes the player; no reset
		return;
	}
	History.Add(TodayHistoryEntry());
	++Day;
	StartDay();
}

FString AP9CampaignDirector::TodayHistoryEntry() const
{
	TArray<FString> Ids;
	for (const FP9ActiveAnomaly& A : ActiveAnomalies)
	{
		Ids.Add(A.Plan.DefId.ToString());
	}
	return FString::Join(Ids, TEXT("|"));
}

void AP9CampaignDirector::EnterEnding()
{
	++DayToken;
	CleanupDay();
	Phase = EP9DayPhase::Ending;
	Status = EP9CampaignStatus::Completed;
	bDecisionLocked = true;
	bFinalEnding = true;
	EpilogueStage = 2;
	SaveProgress();
	EnsureHud();
	FadeTarget = 0.85f;
	FadeSpeed = 1.0f;
	ShowTitle(NSLOCTEXT("P9", "EndTitle", "PLATFORM NO. 9"),
		NSLOCTEXT("P9", "EndBody4B", "The Vashi local did not stop at Airoli.\n\nThe nightmare is not over.\n\nTHE END"), -1.0f);
	SetEndingInput(true);
	UE_LOG(LogP9, Log, TEXT("Campaign: COMPLETED run %d (seed %d) - ending screen shown"), RunId, RunSeed);
	OnCampaignCompleted.Broadcast(Day);
}

// =====================================================================================================================
// activation
// =====================================================================================================================
bool AP9CampaignDirector::ActivateAnomaly(const FP9PlannedAnomaly& Plan)
{
	if (!Anomalies.IsValidIndex(Plan.DefIndex) || !SpawnPoints.IsValidIndex(Plan.PointIndex) || !SpawnPoints[Plan.PointIndex]
		|| !Anomalies[Plan.DefIndex].Variants.IsValidIndex(Plan.VariantIndex))
	{
		UE_LOG(LogP9, Warning, TEXT("Campaign: anomaly %s has an invalid plan entry - not activated"), *Plan.DefId.ToString());
		return false;
	}
	const FP9AnomalyDef& Def = Anomalies[Plan.DefIndex];
	const FP9AnomalyVariant& V = Def.Variants[Plan.VariantIndex];
	AP9AnomalySpawnPoint* Point = SpawnPoints[Plan.PointIndex];
	FString ZoneWhy;
	if (!IsPointAllowed(Point, &ZoneWhy))                         // also guards forced (debug) plans: never on Platforms 1-8
	{
		UE_LOG(LogP9, Warning, TEXT("Campaign: anomaly %s@%s refused - %s"), *Plan.DefId.ToString(), *Plan.PointId.ToString(), *ZoneWhy);
		return false;
	}
	const int32 BaselineMark = Baseline.Num();
	FP9ActiveAnomaly A;
	A.Plan = Plan;
	bool bOk = false;
	switch (Def.Handler)
	{
	case EP9AnomalyHandler::MaterialSwap: bOk = ActivateMaterialSwap(A, Def, V, Point); break;
	case EP9AnomalyHandler::LampFault: bOk = ActivateLampFault(A, V, Point); break;
	case EP9AnomalyHandler::HideActor: bOk = ActivateHideActor(Point); break;
	case EP9AnomalyHandler::ExtraProp: bOk = ActivateExtraProp(V, Point); break;
	case EP9AnomalyHandler::TrainDestination: bOk = ActivateTrainDestination(A, V); break;
	case EP9AnomalyHandler::TrainSeatTint: bOk = ActivateTrainSeatTint(A, V); break;
	case EP9AnomalyHandler::TrainLights: bOk = ActivateTrainLights(A, V, Point); break;
	case EP9AnomalyHandler::PAAnnouncement:
	case EP9AnomalyHandler::FollowFootsteps:
	case EP9AnomalyHandler::PhantomHorn:
		bOk = V.Sound.LoadSynchronous() != nullptr;
		break;
	case EP9AnomalyHandler::DistantFigure:
	case EP9AnomalyHandler::WatchingFigure:
	case EP9AnomalyHandler::WindowFigure:
		bOk = ActivateFigure(A, Def, V, Point);
		break;
	case EP9AnomalyHandler::StaticProp:
		bOk = ActivateStaticProp(A, V, Point);
		break;
	}
	if (!bOk)
	{
		// undo whatever the handler changed before it failed: a failed anomaly leaves no trace and is not counted
		while (Baseline.Num() > BaselineMark)
		{
			FP9BaselineRecord R = Baseline.Pop();
			if (R.Spawned) R.Spawned->Destroy();
			if (R.Mesh) for (int32 i = 0; i < R.Materials.Num(); ++i) R.Mesh->SetMaterial(i, R.Materials[i]);
			if (R.Light) { R.Light->SetIntensity(R.Intensity); R.Light->SetLightFColor(R.LightColor); R.Light->SetVisibility(R.bLightVisible); }
			if (R.Actor) { R.Actor->SetActorHiddenInGame(R.bHidden); R.Actor->SetActorEnableCollision(R.bCollision); }
		}
		UE_LOG(LogP9, Warning, TEXT("Campaign: anomaly %s:%s@%s FAILED to activate - not counted"), *Plan.DefId.ToString(), *Plan.VariantId.ToString(), *Plan.PointId.ToString());
		return false;
	}
	ActiveAnomalies.Add(A);
	return true;
}

bool AP9CampaignDirector::ActivateMaterialSwap(FP9ActiveAnomaly& A, const FP9AnomalyDef& Def, const FP9AnomalyVariant& V, AP9AnomalySpawnPoint* Point)
{
	UStaticMeshComponent* Mesh = Point->TargetActor ? Point->TargetActor->FindComponentByClass<UStaticMeshComponent>() : nullptr;
	UMaterialInterface* Material = V.Material.LoadSynchronous();
	if (!Mesh || !Material)
	{
		return false;
	}
	for (int32 i = 0; i < Mesh->GetNumMaterials(); ++i)
	{
		const UMaterialInterface* Current = Mesh->GetMaterial(i);
		if (Current && Current->GetName().Contains(Def.MaterialSlotMatch))
		{
			RecordMesh(Mesh);
			if (V.bChangeAfterLookAway)
			{
				A.PendingMesh = Mesh;                                 // looks normal until seen, then changes behind the player's back
				A.PendingMaterial = Material;
				A.PendingSlot = i;
			}
			else
			{
				Mesh->SetMaterial(i, Material);
			}
			return true;
		}
	}
	return false;
}

bool AP9CampaignDirector::ActivateLampFault(FP9ActiveAnomaly& A, const FP9AnomalyVariant& V, AP9AnomalySpawnPoint* Point)
{
	// the lamp's light plus any further lights listed on the point (its fill light): all change together
	TArray<ULightComponent*> Lights;
	ULightComponent* Main = Point->TargetActor ? Point->TargetActor->FindComponentByClass<ULightComponent>() : nullptr;
	if (!Main)
	{
		return false;
	}
	Lights.Add(Main);
	for (const AActor* Extra : Point->ExtraActors)
	{
		if (ULightComponent* L = Extra ? Extra->FindComponentByClass<ULightComponent>() : nullptr)
		{
			Lights.Add(L);
		}
	}
	for (const ULightComponent* L : Lights)
	{
		if (L->Mobility == EComponentMobility::Static)
		{
			return false;
		}
	}
	for (ULightComponent* L : Lights)
	{
		RecordLight(L);
		A.Lights.Add(L);
		A.BaseIntensity.Add(L->Intensity);
	}
	UStaticMeshComponent* Tube = Point->SecondaryActor ? Point->SecondaryActor->FindComponentByClass<UStaticMeshComponent>() : nullptr;
	if (Tube)
	{
		RecordMesh(Tube);
		for (int32 i = 0; i < Tube->GetNumMaterials(); ++i)
		{
			const UMaterialInterface* Current = Tube->GetMaterial(i);
			if (!Current || !Current->GetName().Contains(TEXT("LampTube")))
			{
				continue;
			}
			if (UMaterialInstanceDynamic* MID = Tube->CreateDynamicMaterialInstance(i))
			{
				A.MIDs.Add(MID);
				A.BaseIntensity.Add(MID->K2_GetScalarParameterValue(TEXT("Intensity")));
			}
		}
	}
	if (V.Param == 0.0f)
	{
		for (ULightComponent* L : A.Lights) L->SetIntensity(0.0f);
		for (UMaterialInstanceDynamic* MID : A.MIDs) MID->SetScalarParameterValue(TEXT("Intensity"), 0.0f);
	}
	else if (V.Param >= 2.0f)
	{
		for (ULightComponent* L : A.Lights) L->SetLightColor(V.Color);
		for (UMaterialInstanceDynamic* MID : A.MIDs) MID->SetVectorParameterValue(TEXT("TubeColor"), V.Color);
	}
	return true;
}

bool AP9CampaignDirector::ActivateHideActor(AP9AnomalySpawnPoint* Point)
{
	AActor* Target = Point->TargetActor;
	if (!Target || Target->IsHidden())
	{
		return false;
	}
	RecordActor(Target);
	Target->SetActorHiddenInGame(true);
	Target->SetActorEnableCollision(false);
	return true;
}

bool AP9CampaignDirector::ActivateExtraProp(const FP9AnomalyVariant& V, AP9AnomalySpawnPoint* Point)
{
	const UStaticMeshComponent* Template = Point->TargetActor ? Point->TargetActor->FindComponentByClass<UStaticMeshComponent>() : nullptr;
	UStaticMesh* Mesh = Template ? Template->GetStaticMesh() : nullptr;
	if (!Mesh)
	{
		return false;
	}
	const FTransform Xf(FRotator(0.0f, Point->GetActorRotation().Yaw + V.Param, 0.0f), Point->GetActorLocation());
	const FBoxSphereBounds Bounds = Mesh->GetBounds();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(P9ExtraProp), false);
	const FVector Center = Xf.TransformPosition(Bounds.Origin) + FVector(0.0f, 0.0f, 4.0f);
	if (GetWorld()->OverlapBlockingTestByChannel(Center, Xf.GetRotation(), ECC_Pawn, FCollisionShape::MakeBox(Bounds.BoxExtent * 0.92f), Params))
	{
		UE_LOG(LogP9, Log, TEXT("Campaign: extra prop at %s overlaps something - spawn rejected"), *Point->PointId.ToString());
		return false;
	}
	AStaticMeshActor* Prop = GetWorld()->SpawnActorDeferred<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Xf, this);
	if (!Prop)
	{
		return false;
	}
	UStaticMeshComponent* C = Prop->GetStaticMeshComponent();
	C->SetMobility(EComponentMobility::Movable);
	C->SetStaticMesh(Mesh);
	for (int32 i = 0; i < Template->GetNumMaterials(); ++i)
	{
		C->SetMaterial(i, Template->GetMaterial(i));
	}
	C->SetCollisionProfileName(TEXT("BlockAll"));
	Prop->Tags.Add(P9Campaign::SpawnedTag);
	Prop->FinishSpawning(Xf);
	RecordSpawned(Prop);
	return true;
}

bool AP9CampaignDirector::ActivateTrainDestination(FP9ActiveAnomaly& A, const FP9AnomalyVariant& V)
{
	if (!Train || V.Text.IsEmpty())
	{
		return false;
	}
	UCanvasRenderTarget2D* Target = UCanvasRenderTarget2D::CreateCanvasRenderTarget2D(this, UCanvasRenderTarget2D::StaticClass(), 512, 512);
	if (!Target)
	{
		return false;
	}
	int32 Slots = 0;
	for (int32 Car = 0; Car < 2; ++Car)
	{
		UStaticMeshComponent* Shell = P9Campaign::FindNamedComponent<UStaticMeshComponent>(Train, FString::Printf(TEXT("Car%d_Shell"), Car));
		if (!Shell)
		{
			continue;
		}
		const TArray<FName> Names = Shell->GetMaterialSlotNames();
		bool bRecorded = false;
		for (int32 i = 0; i < Names.Num(); ++i)
		{
			if (Names[i].ToString().StartsWith(TEXT("M_P9_TrainLights")))
			{
				if (!bRecorded)
				{
					RecordMesh(Shell);
					bRecorded = true;
				}
				if (UMaterialInstanceDynamic* MID = Shell->CreateDynamicMaterialInstance(i))
				{
					MID->SetTextureParameterValue(TEXT("PanelTexture"), Target);
					A.MIDs.Add(MID);
					++Slots;
				}
			}
		}
	}
	if (Slots == 0)
	{
		return false;
	}
	DestinationTextNow = V.Text;
	A.Target = Target;
	Target->OnCanvasRenderTargetUpdate.AddDynamic(this, &AP9CampaignDirector::DrawDestination);
	Target->UpdateResource();
	return true;
}

void AP9CampaignDirector::DrawDestination(UCanvas* Canvas, int32 Width, int32 Height)
{
	// same layout as AP9Train's own board: the baked lights atlas with the destination band redrawn
	const FVector2D Size(Width, Height);
	if (UTexture* Atlas = Train ? Train->LightsAtlas.LoadSynchronous() : nullptr)
	{
		Canvas->K2_DrawTexture(Atlas, FVector2D::ZeroVector, Size, FVector2D::ZeroVector, FVector2D::UnitVector, FLinearColor::White, BLEND_Opaque);
	}
	Canvas->K2_DrawTexture(nullptr, FVector2D(0.0f, Height * 0.5f), FVector2D(Width, Height * 0.25f), FVector2D::ZeroVector, FVector2D::UnitVector, FLinearColor(0.01f, 0.008f, 0.005f), BLEND_Opaque);
	Canvas->K2_DrawText(GEngine->GetLargeFont(), DestinationTextNow.ToUpper(), FVector2D(Width * 0.5f, Height * 0.625f), FVector2D(2.6f, 3.2f),
		FLinearColor(1.0f, 0.55f, 0.08f), 0.0f, FLinearColor::Transparent, FVector2D::ZeroVector, true, true, false, FLinearColor::Black);
}

bool AP9CampaignDirector::ActivateTrainSeatTint(FP9ActiveAnomaly& A, const FP9AnomalyVariant& V)
{
	int32 Slots = 0;
	for (int32 Car = 0; Car < 2; ++Car)
	{
		UStaticMeshComponent* Interior = P9Campaign::FindNamedComponent<UStaticMeshComponent>(Train, FString::Printf(TEXT("Car%d_Interior"), Car));
		if (!Interior)
		{
			continue;
		}
		for (int32 i = 0; i < Interior->GetNumMaterials(); ++i)
		{
			const UMaterialInterface* Current = Interior->GetMaterial(i);
			if (Current && Current->GetName().Contains(TEXT("Train_Seat")))
			{
				RecordMesh(Interior);
				// a variant material replaces the upholstery outright (a multiply tint cannot turn the blue fabric red)
				if (UMaterialInterface* Replacement = V.Material.LoadSynchronous())
				{
					Interior->SetMaterial(i, Replacement);
					++Slots;
				}
				else if (UMaterialInstanceDynamic* MID = Interior->CreateDynamicMaterialInstance(i))
				{
					MID->SetVectorParameterValue(TEXT("Tint"), V.Color);
					A.MIDs.Add(MID);
					++Slots;
				}
			}
		}
	}
	return Slots > 0;
}

bool AP9CampaignDirector::ActivateTrainLights(FP9ActiveAnomaly& A, const FP9AnomalyVariant& V, AP9AnomalySpawnPoint* Point)
{
	const FString Car = Point->TargetComponent.IsNone() ? FString(TEXT("Car1")) : Point->TargetComponent.ToString();
	for (int32 L = 0; L < 2; ++L)
	{
		if (ULightComponent* Light = P9Campaign::FindNamedComponent<ULightComponent>(Train, FString::Printf(TEXT("%s_InteriorLight%d"), *Car, L)))
		{
			RecordLight(Light);
			A.Lights.Add(Light);
			A.BaseIntensity.Add(Light->Intensity);
			if (V.Param == 0.0f) Light->SetIntensity(0.0f);
			else if (V.Param >= 2.0f) Light->SetLightColor(V.Color);
		}
	}
	return A.Lights.Num() > 0;
}

bool AP9CampaignDirector::ActivateFigure(FP9ActiveAnomaly& A, const FP9AnomalyDef& Def, const FP9AnomalyVariant& V, AP9AnomalySpawnPoint* Point)
{
	if (Def.Handler == EP9AnomalyHandler::WindowFigure)
	{
		const FString CarName = Point->TargetComponent.IsNone() ? FString(TEXT("Car1")) : Point->TargetComponent.ToString();
		USceneComponent* Car = Train ? P9Campaign::FindNamedComponent<USceneComponent>(Train, CarName) : nullptr;
		if (!Car)
		{
			return false;
		}
		// car-local placement; a car turned round mirrors the side
		const bool bReversed = FMath::Abs(FRotator::NormalizeAxis(Car->GetRelativeRotation().Yaw)) > 90.0f;
		const int32 Side = (Train->PlatformSideWorldY >= 0 ? 1 : -1) * (bReversed ? -1 : 1);
		A.CarIndex = CarName.EndsWith(TEXT("0")) ? 0 : 1;
		// Param 0: far end of the carriage in the aisle, facing down it; 1: just inside the door nearest the cab; 2: in a seat bay,
		// face to the platform-side window (seen from the platform as a face at the glass)
		// (x 348 is centred on a window, 391 stood behind the pier; 0.88 height so the face sits in the glass, not behind the shutter)
		FVector Local(690.0f, Side * 140.0f, 110.0f);
		float RelYaw = Side * 90.0f;
		if (V.Param == 0.0f) { Local = FVector(-760.0f, 0.0f, 110.0f); RelYaw = 0.0f; }
		else if (V.Param >= 2.0f) { Local = FVector(348.0f, Side * 150.0f, 110.0f); RelYaw = Side * 90.0f; }
		AActor* Figure = SpawnFigure(Car->GetComponentTransform().TransformPosition(Local), Car->GetComponentRotation().Yaw + RelYaw, V.Mesh.LoadSynchronous(),
			V.Param >= 2.0f ? FVector(1.0f, 1.0f, 0.88f) : FVector::OneVector);
		if (!Figure)
		{
			return false;
		}
		Figure->AttachToComponent(Car, FAttachmentTransformRules::KeepWorldTransform);
		A.Figure = Figure;
		return true;
	}
	AActor* Figure = SpawnFigure(Point->GetActorLocation(), Point->GetActorRotation().Yaw, V.Mesh.LoadSynchronous());
	if (!Figure)
	{
		return false;
	}
	A.Figure = Figure;
	A.ScareFrom = Point->GetActorLocation();
	return true;
}

bool AP9CampaignDirector::ActivateStaticProp(FP9ActiveAnomaly& A, const FP9AnomalyVariant& V, AP9AnomalySpawnPoint* Point)
{
	UStaticMesh* Mesh = V.Mesh.LoadSynchronous();
	UMaterialInterface* Override = V.Material.IsNull() ? nullptr : V.Material.LoadSynchronous();
	if (!Mesh || (!V.Material.IsNull() && !Override) || (!V.Sound.IsNull() && !V.Sound.LoadSynchronous()))
	{
		return false;
	}
	A.Figure = SpawnFigure(Point->GetActorLocation(), Point->GetActorRotation().Yaw, Mesh, V.Scale, Override);
	return A.Figure != nullptr;
}

AActor* AP9CampaignDirector::SpawnFigure(const FVector& Location, float Yaw, UStaticMesh* InMesh, const FVector& Scale, UMaterialInterface* OverrideAll)
{
	UStaticMesh* Mesh = InMesh ? InMesh : FigureMesh.LoadSynchronous();
	if (!Mesh)
	{
		return nullptr;
	}
	const FTransform Xf(FRotator(0.0f, Yaw, 0.0f), Location);
	AStaticMeshActor* Figure = GetWorld()->SpawnActorDeferred<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Xf, this);
	if (!Figure)
	{
		return nullptr;
	}
	UStaticMeshComponent* C = Figure->GetStaticMeshComponent();
	C->SetMobility(EComponentMobility::Movable);
	C->SetStaticMesh(Mesh);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetCollisionProfileName(TEXT("NoCollision"));
	C->SetCastShadow(true);
	C->SetWorldScale3D(Scale);
	C->bNeverDistanceCull = true;                          // the level's cull distances would hide distant anomalies (e.g. the train on track 10)
	if (OverrideAll)
	{
		for (int32 i = 0; i < Mesh->GetStaticMaterials().Num(); ++i) C->SetMaterial(i, OverrideAll);
	}
	Figure->Tags.Add(P9Campaign::SpawnedTag);
	Figure->FinishSpawning(Xf);
	RecordSpawned(Figure);
	return Figure;
}

// =====================================================================================================================
// per-tick anomaly behaviour
// =====================================================================================================================
void AP9CampaignDirector::TickAnomaly(FP9ActiveAnomaly& A, float DeltaSeconds)
{
	A.Time += DeltaSeconds;
	const FP9AnomalyDef& Def = Anomalies[A.Plan.DefIndex];
	const FP9AnomalyVariant& V = Def.Variants[A.Plan.VariantIndex];
	ACharacter* Player = PlayerCharacter();
	APlayerController* PC = PlayerController();
	if (A.PendingMesh && A.PendingMaterial && SpawnPoints.IsValidIndex(A.Plan.PointIndex))
	{
		// "it changed after you passed it": the player must have looked at it (0.5 s), then away (0.4 s)
		// (bounds include non-colliding components: the signs have no collision)
		const AActor* Target = SpawnPoints[A.Plan.PointIndex]->TargetActor;
		const FVector Where = Target ? Target->GetComponentsBoundingBox(true).GetCenter() : SpawnPoints[A.Plan.PointIndex]->GetActorLocation();
		if (IsInView(Where, 0.9f, 3500.0f, Target, false))
		{
			A.SeenTime += DeltaSeconds; A.AwayTime = 0.0f; A.bSeen |= A.SeenTime > 0.5f;
		}
		else if (A.bSeen && !IsInView(Where, 0.45f, 100000.0f, Target, false) && (A.AwayTime += DeltaSeconds) > 0.4f)
		{
			A.PendingMesh->SetMaterial(A.PendingSlot, A.PendingMaterial);
			A.PendingMesh = nullptr;
			UE_LOG(LogP9, Log, TEXT("Campaign: %s changed while the player looked away"), *A.Plan.DefId.ToString());
		}
	}
	switch (Def.Handler)
	{
	case EP9AnomalyHandler::StaticProp:
		if (A.Figure && !V.Sound.IsNull() && DayTime >= A.NextEvent && Player && IsInAnomalyZone(PlayerFeet()))
		{
			PlaySoundAt(V.Sound.LoadSynchronous(), A.Figure->GetActorLocation() + FVector(0.0f, 0.0f, 300.0f), ScareVolume, 1.0f, 16000.0f);
			A.NextEvent = DayTime + PresentationStream.FRandRange(30.0f, 50.0f);
		}
		break;
	case EP9AnomalyHandler::LampFault:
	case EP9AnomalyHandler::TrainLights:
		if (V.Param == 1.0f)
		{
			// irregular flicker: mostly on, with dropouts
			const float N = FMath::PerlinNoise1D(A.Time * 6.3f + A.Plan.PointIndex * 11.1f);
			const float Scale = N > -0.12f ? 1.0f : 0.04f;
			for (int32 i = 0; i < A.Lights.Num(); ++i) A.Lights[i]->SetIntensity(A.BaseIntensity[i] * Scale);
			for (int32 i = 0; i < A.MIDs.Num(); ++i)
			{
				const int32 Base = A.Lights.Num() + i;
				if (A.BaseIntensity.IsValidIndex(Base)) A.MIDs[i]->SetScalarParameterValue(TEXT("Intensity"), A.BaseIntensity[Base] * Scale);
			}
		}
		break;
	case EP9AnomalyHandler::FollowFootsteps:
		if (Player && DayTime >= A.NextEvent)
		{
			const UCharacterMovementComponent* Move = Player->GetCharacterMovement();
			const bool bWalking = Move && Move->IsMovingOnGround() && Move->Velocity.Size2D() > 80.0f && !(Train && Train->IsPlayerAboard()) && IsInAnomalyZone(PlayerFeet());
			if (bWalking)
			{
				const FVector Behind = Player->GetActorLocation() - Player->GetActorForwardVector().GetSafeNormal2D() * 380.0f;
				PlaySoundAt(V.Sound.LoadSynchronous(), FVector(Behind.X, Behind.Y, PlayerFeet().Z + 10.0f), 1.0f, 1.0f, 2200.0f);
				A.NextEvent = DayTime + PresentationStream.FRandRange(22.0f, 38.0f);
				UE_LOG(LogP9, Log, TEXT("Campaign: footsteps played behind the player"));
			}
		}
		break;
	case EP9AnomalyHandler::PhantomHorn:
		if (DayTime >= A.NextEvent && SpawnPoints.IsValidIndex(A.Plan.PointIndex) && Player && IsInAnomalyZone(PlayerFeet()))
		{
			PlaySoundAt(V.Sound.LoadSynchronous(), SpawnPoints[A.Plan.PointIndex]->GetActorLocation(), 1.0f, 0.92f, 14000.0f);
			A.NextEvent = DayTime + PresentationStream.FRandRange(28.0f, 45.0f);
			UE_LOG(LogP9, Log, TEXT("Campaign: phantom horn from %s"), *A.Plan.PointId.ToString());
		}
		break;
	case EP9AnomalyHandler::DistantFigure:
		if (A.Figure && !A.bGone && Player)
		{
			const FVector ToPlayer = Player->GetActorLocation() - A.Figure->GetActorLocation();
			A.Figure->SetActorRotation(FRotator(0.0f, ToPlayer.Rotation().Yaw + V.Param, 0.0f));
			if (ToPlayer.Size2D() < P9Campaign::FigureVanishDistance)
			{
				A.bGone = true;
				A.Figure->SetActorHiddenInGame(true);
				UE_LOG(LogP9, Log, TEXT("Campaign: distant figure vanished as the player approached"));
				if (A.Plan.bJumpscare && !A.bScareDone && ScareAvailable() && Phase == EP9DayPhase::Exploring)
				{
					// A - anticipation: a low drone where it stood, then nothing (false safety)
					A.ScareStage = 1; A.ScareTime = 0.0f;
					PlaySoundAt(ScareDrone.LoadSynchronous(), A.Figure->GetActorLocation() + FVector(0.0f, 0.0f, 150.0f), 0.8f * ScareVolume, 1.0f, 2500.0f);
				}
			}
		}
		if (A.ScareStage > 0 && Player)
		{
			A.ScareTime += DeltaSeconds;
			if (A.ScareStage == 1 && A.ScareTime >= 1.8f)
			{
				// reveal: in front of the player at a validated distance, face and silhouette lit by a short close light
				float Yaw = 0.0f;
				const FVector Spot = ScareSpotInFront(165.0f, Yaw);
				A.Figure->SetActorLocationAndRotation(Spot, FRotator(0.0f, Yaw, 0.0f));
				A.Figure->SetActorHiddenInGame(false);
				ScareLightAt(Spot + (PC && PC->PlayerCameraManager ? (PC->PlayerCameraManager->GetCameraLocation() - Spot).GetSafeNormal2D() * 60.0f : FVector::ZeroVector) + FVector(0.0f, 0.0f, 200.0f), 900.0f, 1.0f);
				PlayScare2D(StingClose, 1.0f);
				ShakeTime = 0.25f; FlashAlpha = 0.12f;
				LastJumpscareTime = WorldTimeNow; A.bScareDone = true; A.ScareStage = 2; A.ScareTime = 0.0f;
				UE_LOG(LogP9, Log, TEXT("Campaign: JUMPSCARE A (close apparition) at %s"), *Spot.ToCompactString());
			}
			else if (A.ScareStage == 2 && A.ScareTime >= 0.95f)
			{
				A.Figure->SetActorHiddenInGame(true);              // aftermath: gone, a rush of cloth past the player
				PlaySoundAt(ScareRush.LoadSynchronous(), Player->GetActorLocation(), 0.7f * ScareVolume, 1.0f, 1500.0f);
				A.ScareStage = 3;
			}
		}
		break;
	case EP9AnomalyHandler::WatchingFigure:
		if (V.Param == 1.0f && A.Figure && !A.bGone && Player)
		{
			const FVector Head = A.Figure->GetActorLocation() + FVector(0.0f, 0.0f, 210.0f);
			const FVector ToPlayer = Player->GetActorLocation() - A.Figure->GetActorLocation();
			if (A.ScareStage == 0)
			{
				A.Figure->SetActorRotation(FRotator(0.0f, ToPlayer.Rotation().Yaw, 0.0f));
				const bool bView = IsInView(Head, 0.85f, 5000.0f, A.Figure, true);
				if (bView) { A.SeenTime += DeltaSeconds; A.AwayTime = 0.0f; A.bSeen |= A.SeenTime > 0.4f; }
				else if (A.bSeen && !IsInView(Head, 0.5f, 100000.0f, A.Figure, false) && (A.AwayTime += DeltaSeconds) > 0.5f && ToPlayer.Size2D() > 800.0f)
				{
					// closer every time you look back: 40% of the way, staying on the footbridge deck and at least 7 m off
					FVector Next = A.Figure->GetActorLocation() + ToPlayer.GetSafeNormal2D() * FMath::Min(ToPlayer.Size2D() * 0.4f, ToPlayer.Size2D() - 700.0f);
					Next.X = FMath::Clamp(Next.X, -150.0f, 150.0f); Next.Z = A.ScareFrom.Z;
					A.Figure->SetActorLocation(Next);
					A.bSeen = false; A.SeenTime = 0.0f; A.AwayTime = 0.0f; ++A.Steps;
					PlaySoundAt(ScareRush.LoadSynchronous(), Next + FVector(0.0f, 0.0f, 120.0f), 0.35f * ScareVolume, 0.8f, 2500.0f);
					UE_LOG(LogP9, Log, TEXT("Campaign: approaching figure stepped closer (%d, %.0f m away)"), A.Steps, ToPlayer.Size2D() / 100.0f);
				}
				// C - after it has come closer twice, the next clear look within 14 m makes it lunge once
				if (A.Plan.bJumpscare && !A.bScareDone && A.Steps >= 2 && bView && A.SeenTime > 0.3f && ToPlayer.Size2D() < 1400.0f && ScareAvailable() && Phase == EP9DayPhase::Exploring)
				{
					float Yaw = 0.0f;
					A.ScareTo = ScareSpotInFront(230.0f, Yaw); A.ScareTo.Z = A.Figure->GetActorLocation().Z;
					A.ScareFrom = A.Figure->GetActorLocation(); A.ScareStage = 1; A.ScareTime = 0.0f;
					PlayScare2D(StingFootbridge, 1.0f);
					LastJumpscareTime = WorldTimeNow; A.bScareDone = true;
					UE_LOG(LogP9, Log, TEXT("Campaign: JUMPSCARE C (footbridge lunge)"));
				}
			}
			else
			{
				A.ScareTime += DeltaSeconds;
				const float L = FMath::Clamp(A.ScareTime / 0.28f, 0.0f, 1.0f);
				if (A.ScareStage == 1)
				{
					A.Figure->SetActorLocation(FMath::Lerp(A.ScareFrom, A.ScareTo, L * L * L));   // one sudden, accelerating movement
					A.Figure->SetActorRotation(FRotator(0.0f, ToPlayer.Rotation().Yaw, 0.0f));
					if (L >= 1.0f) { A.ScareStage = 2; ScareLightAt(A.ScareTo + FVector(0.0f, 0.0f, 260.0f) + ToPlayer.GetSafeNormal2D() * 80.0f, 700.0f, 0.9f); ShakeTime = 0.22f; }
				}
				else if (A.ScareStage == 2 && A.ScareTime >= 1.25f)
				{
					A.bGone = true; A.Figure->SetActorHiddenInGame(true); A.ScareStage = 3;
				}
			}
		}
		else if (A.Figure && !A.bGone && Player)
		{
			const FVector ToPlayer = Player->GetActorLocation() - A.Figure->GetActorLocation();
			A.Figure->SetActorRotation(FRotator(0.0f, ToPlayer.Rotation().Yaw + V.Param, 0.0f));
			const FVector Head = A.Figure->GetActorLocation() + FVector(0.0f, 0.0f, 160.0f);
			if (IsInView(Head, 0.85f, 4500.0f, A.Figure, true))
			{
				A.SeenTime += DeltaSeconds;
				A.AwayTime = 0.0f;
				A.bSeen |= A.SeenTime > 0.4f;
			}
			else if (A.bSeen && !IsInView(Head, 0.5f, 100000.0f, A.Figure, false))
			{
				A.AwayTime += DeltaSeconds;
				if (A.AwayTime > 0.5f)
				{
					A.bGone = true;
					A.Figure->SetActorHiddenInGame(true);
					UE_LOG(LogP9, Log, TEXT("Campaign: watching figure gone after the player looked away"));
				}
			}
		}
		break;
	case EP9AnomalyHandler::WindowFigure:
		if (A.Figure && !A.bGone && A.Plan.bJumpscare && !A.bScareDone && V.Param == 0.0f && Train && Player)
		{
			USceneComponent* Car = P9Campaign::FindNamedComponent<USceneComponent>(Train, FString::Printf(TEXT("Car%d"), A.CarIndex));
			const FVector Head = A.Figure->GetActorLocation() + FVector(0.0f, 0.0f, 160.0f);
			const bool bInCar = Car && Train->IsPlayerAboard() && FVector::Dist2D(Car->GetComponentLocation(), Player->GetActorLocation()) < 1000.0f;
			if (A.ScareStage == 0)
			{
				// B - the passenger at the far end is seen from inside the carriage; then the lights fail
				A.SeenTime = bInCar && IsInView(Head, 0.9f, 2200.0f, A.Figure, false) ? A.SeenTime + DeltaSeconds : 0.0f;
				if (A.SeenTime > 0.6f && ScareAvailable() && Phase == EP9DayPhase::Exploring)
				{
					for (int32 L = 0; L < 2; ++L)
					{
						if (ULightComponent* Light = P9Campaign::FindNamedComponent<ULightComponent>(Train, FString::Printf(TEXT("Car%d_InteriorLight%d"), A.CarIndex, L)))
						{
							RecordLight(Light); A.Lights.Add(Light); A.BaseIntensity.Add(Light->Intensity); Light->SetIntensity(0.0f);
						}
					}
					PlayScare2D(ScareDrone, 0.5f);
					A.ScareStage = 1; A.ScareTime = 0.0f; LastJumpscareTime = WorldTimeNow; A.bScareDone = true;
					UE_LOG(LogP9, Log, TEXT("Campaign: JUMPSCARE B (carriage) - lights out"));
				}
			}
		}
		if (A.ScareStage > 0 && A.Figure && Train && Player)
		{
			A.ScareTime += DeltaSeconds;
			USceneComponent* Car = P9Campaign::FindNamedComponent<USceneComponent>(Train, FString::Printf(TEXT("Car%d"), A.CarIndex));
			if (A.ScareStage == 1 && A.ScareTime >= 0.8f && Car)
			{
				// lights back: it stands in the aisle 2.3 m away, between the player and where it was
				const FTransform CarXf = Car->GetComponentTransform();
				const FVector P = CarXf.InverseTransformPosition(Player->GetActorLocation());
				const FVector F = CarXf.InverseTransformPosition(A.Figure->GetActorLocation());
				const float Dir = F.X >= P.X ? 1.0f : -1.0f;
				const FVector Spot = CarXf.TransformPosition(FVector(FMath::Clamp(P.X + Dir * 230.0f, -850.0f, 850.0f), 0.0f, 110.0f));
				A.Figure->SetActorLocationAndRotation(Spot, FRotator(0.0f, (Player->GetActorLocation() - Spot).GetSafeNormal2D().Rotation().Yaw, 0.0f));
				for (int32 i = 0; i < A.Lights.Num(); ++i) if (A.Lights[i]) A.Lights[i]->SetIntensity(A.BaseIntensity[i] * 0.45f);   // a brown-out, not full light
				PlayScare2D(StingCarriage, 1.0f);
				ShakeTime = 0.22f; FlashAlpha = 0.08f; A.ScareStage = 2;
				UE_LOG(LogP9, Log, TEXT("Campaign: JUMPSCARE B - lights back, passenger in the aisle"));
			}
			else if (A.ScareStage == 2 && A.ScareTime >= 2.0f)
			{
				for (ULightComponent* L : A.Lights) if (L) L->SetIntensity(0.0f);
				A.Figure->SetActorHiddenInGame(true); A.bGone = true; A.ScareStage = 3;
			}
			else if (A.ScareStage == 3 && A.ScareTime >= 2.3f)
			{
				for (int32 i = 0; i < A.Lights.Num(); ++i) if (A.Lights[i]) A.Lights[i]->SetIntensity(A.BaseIntensity[i]);   // recovery
				A.ScareStage = 4;
			}
		}
		break;
	default:
		break;
	}
}

// =====================================================================================================================
// jumpscares and presentation
// =====================================================================================================================
void AP9CampaignDirector::TriggerJumpscare(EP9JumpscareType Type, FP9ActiveAnomaly* Source)
{
	if (Source && WorldTimeNow - LastJumpscareTime < JumpscareCooldown)
	{
		UE_LOG(LogP9, Log, TEXT("Campaign: jumpscare skipped (cooldown %.0fs left)"), JumpscareCooldown - (WorldTimeNow - LastJumpscareTime));
		return;
	}
	LastJumpscareTime = WorldTimeNow;
	APlayerController* PC = PlayerController();
	ACharacter* Player = PlayerCharacter();
	if (Type == EP9JumpscareType::Apparition && PC && Player && PC->PlayerCameraManager)
	{
		const FVector Forward = PC->PlayerCameraManager->GetCameraRotation().Vector().GetSafeNormal2D();
		const FVector Feet = PlayerFeet();
		const FVector Location = FVector(Player->GetActorLocation().X, Player->GetActorLocation().Y, Feet.Z) + Forward * 125.0f;
		if (ScareFigure)
		{
			ScareFigure->Destroy();
		}
		ScareFigure = SpawnFigure(Location, (-Forward).Rotation().Yaw);
		ScareFigureTime = FMath::Max(0.1f, P9Campaign::CVarScareFigureLife.GetValueOnGameThread());
		PlaySound2D(ApparitionSting.LoadSynchronous(), 1.0f, 1.0f);
		FlashAlpha = 0.55f;
		ShakeTime = 0.5f;
	}
	else
	{
		PlaySound2D(ReflectionSting.LoadSynchronous(), 1.0f, 1.0f);
		FlashAlpha = 0.85f;
		ShakeTime = 0.35f;
	}
	UE_LOG(LogP9, Log, TEXT("Campaign: JUMPSCARE %s"), Type == EP9JumpscareType::Apparition ? TEXT("apparition") : TEXT("reflection"));
}

FVector AP9CampaignDirector::ScareSpotInFront(float Distance, float& OutYaw) const
{
	// straight ahead of the camera on the player's floor, pulled closer if anything solid is in the way (never inside geometry)
	const APlayerController* PC = PlayerController();
	const ACharacter* C = PlayerCharacter();
	if (!PC || !PC->PlayerCameraManager || !C) { OutYaw = 0.0f; return FVector::ZeroVector; }
	const FVector Eye = PC->PlayerCameraManager->GetCameraLocation();
	const FVector Fwd = PC->PlayerCameraManager->GetCameraRotation().Vector().GetSafeNormal2D();
	float D = Distance;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(P9ScareSpot), false, C);
	const FVector Waist(Eye.X, Eye.Y, PlayerFeet().Z + 90.0f);  // bins, benches and seat backs sit below eye height
	for (const FVector& From : {Eye, Waist})
	{
		if (GetWorld()->SweepSingleByChannel(Hit, From, From + Fwd * (Distance + 30.0f), FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(25.0f), Params))
		{
			D = FMath::Min(D, FMath::Max(95.0f, Hit.Distance - 40.0f));
		}
	}
	OutYaw = (-Fwd).Rotation().Yaw;
	return FVector(C->GetActorLocation().X, C->GetActorLocation().Y, PlayerFeet().Z) + Fwd * D;
}

void AP9CampaignDirector::ScareLightAt(const FVector& Where, float Intensity, float Duration)
{
	if (!ScareLight)
	{
		ScareLight = NewObject<UPointLightComponent>(this, TEXT("P9ScareLight"));
		ScareLight->SetMobility(EComponentMobility::Movable);
		ScareLight->SetupAttachment(GetRootComponent());
		ScareLight->RegisterComponent();
		ScareLight->SetIntensityUnits(ELightUnits::Candelas);
		ScareLight->SetCastShadows(false);
		ScareLight->SetAttenuationRadius(450.0f);
		ScareLight->SetLightColor(FLinearColor(0.85f, 0.9f, 1.0f));
	}
	ScareLight->SetWorldLocation(Where);
	ScareLight->SetIntensity(Intensity);
	ScareLight->SetVisibility(true);
	ScareLightTime = Duration;
}

void AP9CampaignDirector::PlayScare2D(const TSoftObjectPtr<USoundBase>& Sound, float Volume)
{
	if (ScareVolume > 0.0f) PlaySound2D(Sound.LoadSynchronous(), Volume * ScareVolume, 1.0f);
}

void AP9CampaignDirector::DebugJumpscare(EP9JumpscareType Type)
{
	TriggerJumpscare(Type, nullptr);
}

void AP9CampaignDirector::TickPresentation(float DeltaSeconds)
{
	FadeAlpha = FMath::FInterpConstantTo(FadeAlpha, FadeTarget, DeltaSeconds, FadeSpeed);
	FlashAlpha = FMath::Max(0.0f, FlashAlpha - DeltaSeconds * 2.2f);
	TitleTime += DeltaSeconds;
	HintTime += DeltaSeconds;
	if (ScareLight && ScareLightTime > 0.0f && (ScareLightTime -= DeltaSeconds) <= 0.0f)
	{
		ScareLight->SetVisibility(false);                         // the scare light never outlives its beat
	}
	if (ScareFigure)
	{
		ScareFigureTime -= DeltaSeconds;
		if (ScareFigureTime <= 0.0f)
		{
			ScareFigure->SetActorHiddenInGame(true);   // destroyed with the day's baseline
			ScareFigure = nullptr;
		}
	}
	APlayerController* PC = PlayerController();
	if (PC && (ShakeTime > 0.0f || !ShakeOffset.IsNearlyZero()))
	{
		const FRotator Base = PC->GetControlRotation() - ShakeOffset;
		ShakeTime -= DeltaSeconds;
		const float S = FMath::Clamp(ShakeTime / 0.5f, 0.0f, 1.0f);
		ShakeOffset = ShakeTime > 0.0f ? FRotator(FMath::FRandRange(-2.0f, 2.0f) * S, FMath::FRandRange(-2.5f, 2.5f) * S, 0.0f) : FRotator::ZeroRotator;
		PC->SetControlRotation(Base + ShakeOffset);
	}
}

bool AP9CampaignDirector::IsInView(const FVector& Location, float MinDot, float MaxDistance, const AActor* IgnoreActor, bool bTrace) const
{
	const APlayerController* PC = PlayerController();
	if (!PC || !PC->PlayerCameraManager)
	{
		return false;
	}
	const FVector Eye = PC->PlayerCameraManager->GetCameraLocation();
	const FVector To = Location - Eye;
	const float Dist = To.Size();
	if (Dist > MaxDistance || Dist < 1.0f || FVector::DotProduct(To / Dist, PC->PlayerCameraManager->GetCameraRotation().Vector()) < MinDot)
	{
		return false;
	}
	if (!bTrace)
	{
		return true;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(P9CampaignView), false);
	Params.AddIgnoredActor(PC->GetPawn());
	Params.AddIgnoredActor(IgnoreActor);
	FHitResult Hit;
	return !GetWorld()->LineTraceSingleByChannel(Hit, Eye, Location, ECC_Visibility, Params);
}

void AP9CampaignDirector::PlaySoundAt(USoundBase* Sound, const FVector& Location, float Volume, float Pitch, float Falloff)
{
	if (!Sound)
	{
		return;
	}
	USoundAttenuation* Attenuation = SpatialAttenuation;
	if (Attenuation && !FMath::IsNearlyEqual(Attenuation->Attenuation.FalloffDistance, Falloff))
	{
		Attenuation->Attenuation.FalloffDistance = Falloff;
	}
	if (UAudioComponent* Audio = UGameplayStatics::SpawnSoundAtLocation(this, Sound, Location, FRotator::ZeroRotator, Volume, Pitch, 0.0f, Attenuation))
	{
		CampaignSounds.Add(Audio);
	}
}

void AP9CampaignDirector::PlaySound2D(USoundBase* Sound, float Volume, float Pitch)
{
	if (!Sound)
	{
		return;
	}
	if (UAudioComponent* Audio = UGameplayStatics::SpawnSound2D(this, Sound, Volume, Pitch))
	{
		CampaignSounds.Add(Audio);
	}
}

void AP9CampaignDirector::StopCampaignSounds()
{
	for (UAudioComponent* Audio : CampaignSounds)
	{
		if (IsValid(Audio))
		{
			Audio->Stop();
		}
	}
	CampaignSounds.Reset();
}

// =====================================================================================================================
// baseline capture / restore
// =====================================================================================================================
void AP9CampaignDirector::RecordMesh(UMeshComponent* Mesh)
{
	FP9BaselineRecord& R = Baseline.AddDefaulted_GetRef();
	R.Mesh = Mesh;
	for (int32 i = 0; i < Mesh->GetNumMaterials(); ++i)
	{
		R.Materials.Add(Mesh->GetMaterial(i));
	}
}

void AP9CampaignDirector::RecordLight(ULightComponent* Light)
{
	FP9BaselineRecord& R = Baseline.AddDefaulted_GetRef();
	R.Light = Light;
	R.Intensity = Light->Intensity;
	R.LightColor = Light->LightColor;
	R.bLightVisible = Light->IsVisible();
}

void AP9CampaignDirector::RecordActor(AActor* Actor)
{
	FP9BaselineRecord& R = Baseline.AddDefaulted_GetRef();
	R.Actor = Actor;
	R.bHidden = Actor->IsHidden();
	R.bCollision = Actor->GetActorEnableCollision();
}

void AP9CampaignDirector::RecordSpawned(AActor* Actor)
{
	FP9BaselineRecord& R = Baseline.AddDefaulted_GetRef();
	R.Spawned = Actor;
}

void AP9CampaignDirector::RestoreBaseline()
{
	for (int32 r = Baseline.Num() - 1; r >= 0; --r)
	{
		FP9BaselineRecord& R = Baseline[r];
		if (IsValid(R.Spawned))
		{
			R.Spawned->Destroy();
		}
		if (IsValid(R.Mesh))
		{
			for (int32 i = 0; i < R.Materials.Num(); ++i)
			{
				R.Mesh->SetMaterial(i, R.Materials[i]);
			}
		}
		if (IsValid(R.Light))
		{
			R.Light->SetIntensity(R.Intensity);
			R.Light->SetLightFColor(R.LightColor);
			R.Light->SetVisibility(R.bLightVisible);
		}
		if (IsValid(R.Actor))
		{
			R.Actor->SetActorHiddenInGame(R.bHidden);
			R.Actor->SetActorEnableCollision(R.bCollision);
		}
	}
	Baseline.Reset();
}

void AP9CampaignDirector::CleanupDay()
{
	const int32 Count = ActiveAnomalies.Num();
	for (FP9ActiveAnomaly& A : ActiveAnomalies)
	{
		if (A.Target)
		{
			A.Target->OnCanvasRenderTargetUpdate.RemoveAll(this);
		}
	}
	RestoreBaseline();
	ActiveAnomalies.Reset();
	StopCampaignSounds();
	if (IsValid(ScareFigure))
	{
		ScareFigure->Destroy();
	}
	ScareFigure = nullptr;
	if (ScareLight) { ScareLight->SetVisibility(false); ScareLightTime = 0.0f; }
	if (APlayerController* PC = PlayerController())
	{
		if (!ShakeOffset.IsNearlyZero())
		{
			PC->SetControlRotation(PC->GetControlRotation() - ShakeOffset);
		}
	}
	ShakeOffset = FRotator::ZeroRotator;
	ShakeTime = 0.0f;
	FlashAlpha = 0.0f;
	if (Count > 0)
	{
		UE_LOG(LogP9, Log, TEXT("Campaign: cleaned up %d anomalies"), Count);
	}
}

TMap<FString, FString> AP9CampaignDirector::CaptureSnapshot() const
{
	TMap<FString, FString> Snap;
	TSet<const AActor*> Actors;
	for (const AP9AnomalySpawnPoint* P : SpawnPoints)
	{
		if (P && P->TargetActor) Actors.Add(P->TargetActor);
		if (P && P->SecondaryActor) Actors.Add(P->SecondaryActor);
		if (P) for (const AActor* Extra : P->ExtraActors) if (Extra) Actors.Add(Extra);
	}
	for (const AActor* Actor : Actors)
	{
		// trains: visibility/collision belong to the story (hold stop, opening), not to anomalies
		Snap.Add(Actor->GetName(), Actor->IsA<AP9Train>() ? FString(TEXT("train")) : FString::Printf(TEXT("hidden=%d collision=%d"), Actor->IsHidden() ? 1 : 0, Actor->GetActorEnableCollision() ? 1 : 0));
		TArray<UMeshComponent*> Meshes;
		Actor->GetComponents<UMeshComponent>(Meshes);
		for (const UMeshComponent* M : Meshes)
		{
			FString Mats;
			for (int32 i = 0; i < M->GetNumMaterials(); ++i)
			{
				Mats += GetNameSafe(M->GetMaterial(i)) + TEXT(",");
			}
			Snap.Add(Actor->GetName() + TEXT("/") + M->GetName(), Mats);
		}
		TArray<ULightComponent*> Lights;
		Actor->GetComponents<ULightComponent>(Lights);
		for (const ULightComponent* L : Lights)
		{
			Snap.Add(Actor->GetName() + TEXT("/") + L->GetName(), FString::Printf(TEXT("i=%.2f c=%s v=%d"), L->Intensity, *L->LightColor.ToString(), L->IsVisible() ? 1 : 0));
		}
	}
	int32 Spawned = 0;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		Spawned += It->Tags.Contains(P9Campaign::SpawnedTag) && !It->IsActorBeingDestroyed() ? 1 : 0;
	}
	Snap.Add(TEXT("#spawned"), FString::FromInt(Spawned));
	return Snap;
}

void AP9CampaignDirector::TakeInitialSnapshot()
{
	InitialSnapshot = CaptureSnapshot();
	UE_LOG(LogP9, Log, TEXT("Campaign: baseline snapshot of %d entries"), InitialSnapshot.Num());
}

int32 AP9CampaignDirector::VerifyBaseline(bool bLog) const
{
	const TMap<FString, FString> Now = CaptureSnapshot();
	int32 Diffs = 0;
	for (const TPair<FString, FString>& P : InitialSnapshot)
	{
		const FString* Current = Now.Find(P.Key);
		if (!Current || *Current != P.Value)
		{
			++Diffs;
			if (bLog)
			{
				UE_LOG(LogP9, Log, TEXT("Campaign: baseline DIFF %s: was '%s' now '%s'"), *P.Key, *P.Value, Current ? **Current : TEXT("<missing>"));
			}
		}
	}
	if (bLog)
	{
		UE_LOG(LogP9, Log, TEXT("Campaign: baseline verify - %d entries, %d differences, %d active anomalies"), InitialSnapshot.Num(), Diffs, ActiveAnomalies.Num());
	}
	return Diffs;
}

// =====================================================================================================================
// player helpers
// =====================================================================================================================
APlayerController* AP9CampaignDirector::PlayerController() const
{
	return GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
}

ACharacter* AP9CampaignDirector::PlayerCharacter() const
{
	const APlayerController* PC = PlayerController();
	return PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
}

FVector AP9CampaignDirector::PlayerFeet() const
{
	const ACharacter* C = PlayerCharacter();
	if (!C)
	{
		return FVector::ZeroVector;
	}
	return C->GetActorLocation() - FVector(0.0f, 0.0f, C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
}

void AP9CampaignDirector::PlacePlayerAtStart()
{
	ACharacter* C = PlayerCharacter();
	APlayerController* PC = PlayerController();
	if (!C)
	{
		return;
	}
	if (UP9SeatComponent* Seat = UP9SeatComponent::GetSeatOf(C))
	{
		Seat->Stand();
	}
	const FRotator Facing(0.0f, StartYaw, 0.0f);
	C->TeleportTo(StartLocation, Facing, false, true);
	C->GetCharacterMovement()->StopMovementImmediately();
	if (PC)
	{
		PC->SetControlRotation(Facing);
	}
}

// =====================================================================================================================
// save / load
// =====================================================================================================================
bool AP9CampaignDirector::SaveProgress() const
{
	if (!bUseSaveGame)
	{
		return false;
	}
	UP9CampaignSave* Save = Cast<UP9CampaignSave>(UGameplayStatics::CreateSaveGameObject(UP9CampaignSave::StaticClass()));
	Save->Version = UP9CampaignSave::CurrentVersion;
	Save->Day = Day;
	Save->Status = Status;
	Save->RunSeed = RunSeed;
	Save->RunId = RunId;
	Save->History = History;
	Save->SavedAt = FDateTime::Now().ToString();
	Save->bOpeningDone = bOpeningDone;
	Save->EpilogueStage = EpilogueStage;
	const bool bOk = UGameplayStatics::SaveGameToSlot(Save, SaveSlotName, 0);
	UE_LOG(LogP9, Log, TEXT("Campaign: saved day %d run %d seed %d status %d history %d -> %s"), Day, RunId, RunSeed, static_cast<int32>(Status), History.Num(), bOk ? TEXT("ok") : TEXT("FAILED"));
	return bOk;
}

bool AP9CampaignDirector::LoadProgress()
{
	if (!UGameplayStatics::DoesSaveGameExist(SaveSlotName, 0))
	{
		return false;
	}
	const UP9CampaignSave* Save = Cast<UP9CampaignSave>(UGameplayStatics::LoadGameFromSlot(SaveSlotName, 0));
	if (!Save || Save->Version < 0 || Save->Version > UP9CampaignSave::CurrentVersion || Save->Day < 1 || Save->Day > NumDays || Save->RunSeed == 0)
	{
		UE_LOG(LogP9, Warning, TEXT("Campaign: save '%s' unreadable or incompatible - ignored"), *SaveSlotName);
		return false;
	}
	Day = Save->Day;
	Status = Save->Status;
	RunSeed = Save->RunSeed;
	RunId = Save->RunId;
	History = Save->History;
	// version 1 (Stage 4A) saves have no story fields: the opening counts as done, the epilogue as not reached
	bOpeningDone = Save->Version >= 2 ? Save->bOpeningDone : true;
	EpilogueStage = Save->Version >= 2 ? Save->EpilogueStage : 0;
	if (Save->Version < UP9CampaignSave::CurrentVersion)
	{
		UE_LOG(LogP9, Log, TEXT("Campaign: migrated save version %d -> %d"), Save->Version, UP9CampaignSave::CurrentVersion);
	}
	return true;
}

bool AP9CampaignDirector::DeleteSave() const
{
	return UGameplayStatics::DoesSaveGameExist(SaveSlotName, 0) && UGameplayStatics::DeleteGameInSlot(SaveSlotName, 0);
}

FString AP9CampaignDirector::GetStatusString() const
{
	FString Active;
	for (const FP9ActiveAnomaly& A : ActiveAnomalies)
	{
		Active += FString::Printf(TEXT(" %s:%s@%s%s%s"), *A.Plan.DefId.ToString(), *A.Plan.VariantId.ToString(), *A.Plan.PointId.ToString(),
			A.Plan.bJumpscare ? TEXT("(scare") : TEXT(""), A.Plan.bJumpscare ? (A.bScareDone ? TEXT(" done)") : TEXT(")")) : TEXT(""));
	}
	return FString::Printf(TEXT("day=%d/%d run=%d seed=%d phase=%d status=%d token=%d dayTime=%.1f locked=%d zoneArmed=%d history=[%s] baselineRecords=%d active=%d:%s"),
		Day, NumDays, RunId, RunSeed, static_cast<int32>(Phase), static_cast<int32>(Status), DayToken, DayTime, bDecisionLocked ? 1 : 0, bZoneArmed ? 1 : 0,
		*FString::Join(History, TEXT(",")), Baseline.Num(), ActiveAnomalies.Num(), *Active);
}

// =====================================================================================================================
// HUD
// =====================================================================================================================
void AP9CampaignDirector::ShowTitle(const FText& Title, const FText& Subtitle, float Duration)
{
	TitleText = Title;
	SubtitleText = Subtitle;
	TitleTime = 0.0f;
	TitleDuration = Duration;
}

void AP9CampaignDirector::SetEndingInput(bool bEnable)
{
	if (bEndingInput == bEnable)
	{
		return;
	}
	bEndingInput = bEnable;
	if (APlayerController* PC = PlayerController())
	{
		PC->SetShowMouseCursor(bEnable);
	}
}

void AP9CampaignDirector::EnsureHud()
{
	if (HudWidget.IsValid())
	{
		return;
	}
	UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	if (!Viewport)
	{
		return;
	}
	TWeakObjectPtr<AP9CampaignDirector> WeakThis(this);
	auto TitleAlpha = [WeakThis]() -> float
	{
		if (!WeakThis.IsValid()) return 0.0f;
		const AP9CampaignDirector* D = WeakThis.Get();
		if (D->TitleDuration < 0.0f) return FMath::Clamp(D->TitleTime / 1.5f, 0.0f, 1.0f);
		const float In = FMath::Clamp(D->TitleTime / 0.4f, 0.0f, 1.0f);
		const float Out = FMath::Clamp((D->TitleDuration - D->TitleTime) / 0.8f, 0.0f, 1.0f);
		return FMath::Min(In, Out);
	};
	const FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 52);
	const FSlateFontInfo SubFont = FCoreStyle::GetDefaultFontStyle("Regular", 21);
	const FSlateFontInfo DayFont = FCoreStyle::GetDefaultFontStyle("Bold", 17);
	const FSlateBrush* White = FCoreStyle::Get().GetBrush("WhiteBrush");

	HudWidget = SNew(SOverlay)
		.Visibility(EVisibility::SelfHitTestInvisible)
		+ SOverlay::Slot()
		[
			SNew(SBorder)
			.Visibility(EVisibility::HitTestInvisible)
			.BorderImage(White)
			.BorderBackgroundColor_Lambda([WeakThis]() { return FSlateColor(FLinearColor(0.0f, 0.0f, 0.0f, WeakThis.IsValid() ? WeakThis->FadeAlpha : 0.0f)); })
		]
		+ SOverlay::Slot()
		[
			SNew(SBorder)
			.Visibility(EVisibility::HitTestInvisible)
			.BorderImage(White)
			.BorderBackgroundColor_Lambda([WeakThis]() { return FSlateColor(FLinearColor(0.06f, 0.0f, 0.0f, WeakThis.IsValid() ? WeakThis->FlashAlpha : 0.0f)); })
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Top)
		.Padding(FMargin(28.0f, 22.0f))
		[
			SNew(STextBlock)
			.Visibility(EVisibility::HitTestInvisible)
			.Font(DayFont)
			.ShadowOffset(FVector2D(1.0f, 1.0f))
			.ColorAndOpacity_Lambda([WeakThis]()
			{
				if (WeakThis.IsValid() && !WeakThis->DayLabelOverride.IsEmpty())
				{
					const float Flicker = FMath::PerlinNoise1D(WeakThis->TitleTime * 9.0f) > -0.2f ? 0.9f : 0.15f;
					return FSlateColor(FLinearColor(0.85f, 0.12f, 0.1f, Flicker));
				}
				const bool bShow = WeakThis.IsValid() && (WeakThis->Phase == EP9DayPhase::Exploring || WeakThis->Phase == EP9DayPhase::Resolving);
				return FSlateColor(FLinearColor(1.0f, 0.96f, 0.88f, bShow ? 0.7f : 0.0f));
			})
			.ShadowColorAndOpacity_Lambda([WeakThis]()
			{
				const bool bShow = WeakThis.IsValid() && (!WeakThis->DayLabelOverride.IsEmpty() || WeakThis->Phase == EP9DayPhase::Exploring || WeakThis->Phase == EP9DayPhase::Resolving);
				return FLinearColor(0.0f, 0.0f, 0.0f, bShow ? 0.7f : 0.0f);
			})
			.Text_Lambda([WeakThis]()
			{
				if (WeakThis.IsValid() && !WeakThis->DayLabelOverride.IsEmpty()) return WeakThis->DayLabelOverride;
				return WeakThis.IsValid() ? FText::Format(NSLOCTEXT("P9", "DayLabel", "DAY {0} / {1}"), FText::AsNumber(WeakThis->Day), FText::AsNumber(WeakThis->NumDays)) : FText::GetEmpty();
			})
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Top)
		.Padding(FMargin(0.0f, 64.0f, 0.0f, 0.0f))
		[
			SNew(STextBlock)
			.Visibility(EVisibility::HitTestInvisible)
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 19))
			.Justification(ETextJustify::Center)
			.ShadowOffset(FVector2D(1.0f, 1.0f))
			.ColorAndOpacity_Lambda([WeakThis]()
			{
				if (!WeakThis.IsValid() || WeakThis->HintDuration <= 0.0f) return FSlateColor(FLinearColor::Transparent);
				const AP9CampaignDirector* D = WeakThis.Get();
				const float A = FMath::Min(FMath::Clamp(D->HintTime / 0.5f, 0.0f, 1.0f), FMath::Clamp((D->HintDuration - D->HintTime) / 0.8f, 0.0f, 1.0f));
				return FSlateColor(FLinearColor(1.0f, 0.95f, 0.82f, 0.9f * A));
			})
			.ShadowColorAndOpacity_Lambda([WeakThis]()
			{
				if (!WeakThis.IsValid() || WeakThis->HintDuration <= 0.0f) return FLinearColor::Transparent;
				const AP9CampaignDirector* D = WeakThis.Get();
				const float A = FMath::Min(FMath::Clamp(D->HintTime / 0.5f, 0.0f, 1.0f), FMath::Clamp((D->HintDuration - D->HintTime) / 0.8f, 0.0f, 1.0f));
				return FLinearColor(0.0f, 0.0f, 0.0f, 0.8f * A);
			})
			.Text_Lambda([WeakThis]() { return WeakThis.IsValid() ? WeakThis->HintText : FText::GetEmpty(); })
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			.Visibility(EVisibility::SelfHitTestInvisible)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Visibility(EVisibility::HitTestInvisible)
				.Font(TitleFont)
				.Justification(ETextJustify::Center)
				.ShadowOffset(FVector2D(2.0f, 2.0f))
				.ColorAndOpacity_Lambda([WeakThis, TitleAlpha]() { return FSlateColor(FLinearColor(1.0f, 0.95f, 0.85f, TitleAlpha())); })
				.ShadowColorAndOpacity_Lambda([TitleAlpha]() { return FLinearColor(0.0f, 0.0f, 0.0f, 0.8f * TitleAlpha()); })
				.Text_Lambda([WeakThis]() { return WeakThis.IsValid() ? WeakThis->TitleText : FText::GetEmpty(); })
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(0.0f, 14.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Visibility(EVisibility::HitTestInvisible)
				.Font(SubFont)
				.Justification(ETextJustify::Center)
				.ShadowOffset(FVector2D(1.0f, 1.0f))
				.ColorAndOpacity_Lambda([WeakThis, TitleAlpha]() { return FSlateColor(FLinearColor(0.92f, 0.9f, 0.86f, TitleAlpha())); })
				.ShadowColorAndOpacity_Lambda([TitleAlpha]() { return FLinearColor(0.0f, 0.0f, 0.0f, 0.8f * TitleAlpha()); })
				.Text_Lambda([WeakThis]() { return WeakThis.IsValid() ? WeakThis->SubtitleText : FText::GetEmpty(); })
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(0.0f, 36.0f, 0.0f, 0.0f)
			[
				SNew(SBox)
				.WidthOverride(320.0f)
				.HeightOverride(78.0f)
				.Visibility_Lambda([WeakThis]() { return WeakThis.IsValid() && WeakThis->Phase == EP9DayPhase::Ending && WeakThis->TitleTime > 2.0f ? EVisibility::Visible : EVisibility::Collapsed; })
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.ButtonColorAndOpacity(FLinearColor(0.85f, 0.55f, 0.05f, 0.9f))
					.OnClicked_Lambda([WeakThis]()
					{
						if (WeakThis.IsValid())
						{
							WeakThis->StartNewRun(0);
						}
						return FReply::Handled();
					})
					[
						SNew(STextBlock)
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 20))
						.ColorAndOpacity(FLinearColor::White)
						.Text(NSLOCTEXT("P9", "NewRun", "START A NEW RUN"))
					]
				]
			]
		];
	Viewport->AddViewportWidgetContent(HudWidget.ToSharedRef(), P9Campaign::HudZOrder);
}

void AP9CampaignDirector::RemoveHud()
{
	if (HudWidget.IsValid())
	{
		if (UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
		{
			Viewport->RemoveViewportWidgetContent(HudWidget.ToSharedRef());
		}
		HudWidget.Reset();
	}
}
