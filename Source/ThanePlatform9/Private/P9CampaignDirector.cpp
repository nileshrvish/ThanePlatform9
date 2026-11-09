#include "P9CampaignDirector.h"
#include "P9Captions.h"
#include "P9MainMenu.h"
#include "ThanePlatform9.h"
#include "P9AnomalySpawnPoint.h"
#include "P9CampaignSave.h"
#include "P9SeatComponent.h"
#include "P9SilhouetteStalker.h"
#include "P9AnalogClock.h"
#include "P9EndlessFootbridge.h"
#include "P9LanternMan.h"
#include "P9ReverseEscalator.h"
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
#include "Engine/Texture2D.h"
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
	const FName SpawnedTag(TEXT("P9Anomaly"));

	// ---- Stage 6A constants ----
	const TCHAR* Folder6A = TEXT("/Game/P9/Anomalies6A");
	const TCHAR* Materials6A = TEXT("/Game/P9/Anomalies6A/Materials");
	const TCHAR* PAFolder = TEXT("/Game/P9/Announcements");
	/** Dynamic clock: atlas cells (px) and where each of the five characters "HH:MM" is drawn on the 512x192 face. */
	constexpr int32 ClockFaceW = 512, ClockFaceH = 192;
	constexpr float ClockAtlasW = 2048.0f, ClockAtlasH = 256.0f, ClockCellW = 128.0f, ClockCellH = 192.0f;
	constexpr float ClockSlotX[5] = {25.0f, 125.0f, 190.0f, 260.0f, 360.0f};   // left edge of each 128 px cell (gen_6a_textures.py fit, 5 px dot pitch)
	/** Poster eyes (AdStand local cm, poster plane faces +X at x 2.1): eye centres, pupil lift off the paper, travel. */
	const FVector EyeCentres[2] = {FVector(2.1f, 9.14f, 168.6f), FVector(2.1f, -9.14f, 168.6f)};
	constexpr float PupilLift = 0.45f;
	constexpr float PupilTravelY = 2.6f, PupilTravelZ = 1.0f;          // eye whites: 40 x 28 px = 4.7 x 3.0 cm radii
	const FVector PupilScale(0.006f, 0.034f, 0.034f);                  // the 100 cm engine sphere -> a 3.4 cm disc
	/** Loop anomalies: the screen dips to this darkness when the player is sent back. */
	constexpr float LoopBlinkAlpha = 0.92f, LoopBlinkSeconds = 0.45f;
	// ---- Stage 8A constants ----
	const TCHAR* Endless8A = TEXT("/Game/P9/Anomalies8A/Endless");
	const TCHAR* Materials8A = TEXT("/Game/P9/Anomalies8A/Materials");
	/** Seconds the player must have an anomaly in view before it counts as noticed (anomaly ambience only). */
	constexpr float NoticeSeconds = 0.5f;

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
	/** Stage 6A: a material variant from the 6A folder, usable only at points whose id contains PointMatch. */
	FP9AnomalyVariant Material6A(const TCHAR* Id, int32 Difficulty, const TCHAR* Material, const TCHAR* PointMatch = TEXT(""), bool bLater = false)
	{
		FP9AnomalyVariant V = Variant(Id, Difficulty);
		V.Material = TSoftObjectPtr<UMaterialInterface>(AssetPath(Materials6A, Material));
		V.PointMatch = PointMatch;
		V.bChangeAfterLookAway = bLater;
		return V;
	}
	/** Stage 4D material (still in use for the 6A signboard anomaly), restricted to matching points. */
	FP9AnomalyVariant Material4D(const TCHAR* Id, int32 Difficulty, const TCHAR* Material, const TCHAR* PointMatch)
	{
		FP9AnomalyVariant V = MaterialVariant(Id, Difficulty, Material);
		V.PointMatch = PointMatch;
		return V;
	}
	/** "HH:MM" -> minutes since midnight (wraps); anything unparsable -> 23:47, the station clock's own time. */
	int32 ParseClock(const FString& Text)
	{
		FString H, M;
		if (!Text.Split(TEXT(":"), &H, &M) || !H.IsNumeric() || !M.IsNumeric())
		{
			return 23 * 60 + 47;
		}
		return ((FCString::Atoi(*H) * 60 + FCString::Atoi(*M)) % 1440 + 1440) % 1440;
	}
	FString FormatClock(int32 Minutes)
	{
		Minutes = (Minutes % 1440 + 1440) % 1440;
		return FString::Printf(TEXT("%02d:%02d"), Minutes / 60, Minutes % 60);
	}

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
	ClockDigits = TSoftObjectPtr<UTexture2D>(P9Campaign::AssetPath(TEXT("/Game/P9/Anomalies6A/Textures"), TEXT("T_P9_Anom6_ClockDigits")));
	PupilMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Sphere.Sphere")));
	PupilMaterial = TSoftObjectPtr<UMaterialInterface>(P9Campaign::AssetPath(P9Campaign::Materials6A, TEXT("MI_P9_Anom6_PosterPupil")));
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
	ApparitionSting = TSoftObjectPtr<USoundBase>(P9Campaign::AssetPath(P9Campaign::AudioFolder, TEXT("SW_P9_Anom_StingApparition")));
	// Stage 8A assets
	{
		using namespace P9Campaign;
		auto SM = [](const TCHAR* Folder, const TCHAR* Name) { return TSoftObjectPtr<UStaticMesh>(AssetPath(Folder, Name)); };
		LanternManMesh = SM(TEXT("/Game/P9/Creatures8A/LanternMan"), TEXT("SM_P9_LanternMan"));
		GreetingStandMesh = SM(TEXT("/Game/P9/Creatures8A/Greeting"), TEXT("SM_P9_Greeting_rest"));
		GreetingLungeMesh = SM(TEXT("/Game/P9/Creatures8A/Greeting"), TEXT("SM_P9_Greeting_lunge"));
		BridgeSouthMesh = SM(Endless8A, TEXT("SM_P9_FOBB_South8A"));
		BridgeEndMesh = SM(Endless8A, TEXT("SM_P9_FOBB_End8A"));
		BridgeBayMesh = SM(Endless8A, TEXT("SM_P9_FOBB_Bay8A"));
		BridgeLeadInMesh = SM(Endless8A, TEXT("SM_P9_FOBB_BayLeadIn8A"));
		BridgeTubeMesh = SM(TEXT("/Game/P9/Environment/StationLighting"), TEXT("SM_P9_TubeLight"));
		EscalatorUpperMesh = SM(Endless8A, TEXT("SM_P9_StairPF910_Upper8A"));
		EscalatorStepMesh = SM(Endless8A, TEXT("SM_P9_EscalatorStep8A"));
	}
	BuildDefaultCatalogue();
}

void AP9CampaignDirector::BuildDefaultCatalogue()
{
	// Stage 6A: the catalogue was replaced by the user's eleven anomalies (one per anomalous day; Kanchana scheduled on days 3-5)
	using namespace P9Campaign;
	auto Def = [this](const TCHAR* Id, const TCHAR* Name, EP9AnomalyCategory Category, EP9AnomalyHandler Handler, float Weight,
		TArray<EP9AnomalyLocation> Locations) -> FP9AnomalyDef&
	{
		FP9AnomalyDef& D = Anomalies.AddDefaulted_GetRef();
		D.Id = FName(Id);
		D.DisplayName = Name;
		D.Category = Category;
		D.Handler = Handler;
		D.Weight = Weight;
		D.LocationTypes = MoveTemp(Locations);
		return D;
	};

	// 1. the Platform 9/10 clock: its time changes (redrawn at runtime in the board's own dot-matrix glyphs)
	{
		FP9AnomalyDef& D = Def(TEXT("ClockWrong"), TEXT("Station clock time changes"), EP9AnomalyCategory::Environment, EP9AnomalyHandler::DynamicClock, 1.0f,
			{EP9AnomalyLocation::Clock});
		D.MaterialSlotMatch = TEXT("StationClock");
		FP9AnomalyVariant Back = Variant(TEXT("RunsBackwards"), 2); Back.Text = TEXT("23:47"); Back.Param = 1.0f; Back.Scale.X = 2.5f;   // one minute back every 2.5 s
		FP9AnomalyVariant Race = Variant(TEXT("Racing"), 1); Race.Text = TEXT("23:47"); Race.Param = 2.0f; Race.Scale.X = 0.35f;       // minutes fly by
		FP9AnomalyVariant Hour = Variant(TEXT("WrongHour"), 3); Hour.Text = TEXT("03:47");                                              // fixed, four hours off
		FP9AnomalyVariant Bad = Variant(TEXT("Impossible"), 1); Bad.Text = TEXT("27:63");                                               // a time that cannot exist
		D.Variants = {Back, Race, Hour, Bad};
	}
	// 2. the Platform 9/10 train indicator: another destination instead of Vashi
	{
		FP9AnomalyDef& D = Def(TEXT("IndicatorWrong"), TEXT("Train indicator shows another destination"), EP9AnomalyCategory::Signage, EP9AnomalyHandler::MaterialSwap, 1.0f,
			{EP9AnomalyLocation::Indicator});
		D.MaterialSlotMatch = TEXT("TrainIndicator_PF9_10");
		D.Variants = {Material6A(TEXT("Panvel"), 3, TEXT("MI_P9_Anom6_Indicator_Panvel")), Material6A(TEXT("Dadar"), 2, TEXT("MI_P9_Anom6_Indicator_Dadar")),
			Material6A(TEXT("Thane"), 1, TEXT("MI_P9_Anom6_Indicator_Thane")), Material6A(TEXT("PanvelLater"), 3, TEXT("MI_P9_Anom6_Indicator_Panvel"), TEXT(""), true)};
	}
	// 3. endless footbridge. Stage 8A: no teleport - walking north on deck B the bridge's end recedes as the player walks
	//    (AP9EndlessFootbridge: the real north end is swapped for a moving copy, bays are laid behind it; walking back is free)
	{
		FP9AnomalyDef& D = Def(TEXT("EndlessFootbridge"), TEXT("The footbridge never reaches Platform 9"), EP9AnomalyCategory::Environment, EP9AnomalyHandler::EndlessBridge, 1.0f,
			{EP9AnomalyLocation::FOBLoop});
		D.Variants = {Variant(TEXT("Recedes"), 2)};
	}
	// 4. Kanchana: one seeded day of days 3-5; once seen she follows and speaks. Stage 8A: she starts on Platform 9 (the two
	//    KANCHANA_PF9* points, checked clear of benches, columns, walls and the train)
	{
		FP9AnomalyDef& D = Def(TEXT("Kanchana"), TEXT("Kanchana waits on Platform 9 and follows once seen"), EP9AnomalyCategory::Supernatural, EP9AnomalyHandler::Stalker, 1.0f,
			{EP9AnomalyLocation::StalkerSpot});
		D.bScheduled = true;
		D.ScheduleFirstDay = 3;
		D.ScheduleLastDay = 5;
		FP9AnomalyVariant Follows = Variant(TEXT("Follows"), 3);
		Follows.PointMatch = TEXT("KANCHANA_PF9");
		D.Variants = {Follows};
	}
	// 5. the Platform 9 arrival announcement is wrong (played when the Vashi local is called in)
	{
		FP9AnomalyDef& D = Def(TEXT("AnnouncementWrong"), TEXT("Station announcement is wrong"), EP9AnomalyCategory::Audio, EP9AnomalyHandler::PAAnnouncement, 1.0f,
			{EP9AnomalyLocation::PASystem});
		auto PA = [](const TCHAR* Id, int32 Difficulty, const TCHAR* Sound, float Pitch)
		{
			FP9AnomalyVariant V = Variant(Id, Difficulty);
			V.Sound = TSoftObjectPtr<USoundBase>(AssetPath(PAFolder, Sound));
			V.Param = Pitch;
			return V;
		};
		D.Variants = {PA(TEXT("Platform13"), 2, TEXT("SW_P9_PA_PF9_Anom_Platform13"), 1.0f), PA(TEXT("Platform10"), 3, TEXT("SW_P9_PA_PF9_Anom6_Platform10"), 1.0f),
			PA(TEXT("Panvel"), 3, TEXT("SW_P9_PA_PF9_Anom6_Panvel"), 1.0f), PA(TEXT("DoNotTurn"), 1, TEXT("SW_P9_PA_PF9_Anom_DoNotTurn"), 1.0f),
			PA(TEXT("Slowed"), 2, TEXT("SW_P9_PA_PF9_VashiArrival"), 0.78f)};
	}
	// 6. a signboard's number or arrows are wrong (footbridge signs on the route, the Platform 9/10 island signs)
	{
		FP9AnomalyDef& D = Def(TEXT("SignboardWrong"), TEXT("Signboard number or arrows wrong"), EP9AnomalyCategory::Signage, EP9AnomalyHandler::MaterialSwap, 1.0f,
			{EP9AnomalyLocation::FOBSign, EP9AnomalyLocation::FOBSignPF9, EP9AnomalyLocation::PlatformSign});
		D.MaterialSlotMatch = TEXT("Sign_");            // MI_P9_FOBSign_<n> / MI_P9_Sign_PF9_10_Red, never the frame
		D.Variants = {
			Material4D(TEXT("PF23Arrows"), 2, TEXT("MI_P9_Anom_FOBSignPF2_3_A"), TEXT("SignPF23")), Material4D(TEXT("PF23Number"), 2, TEXT("MI_P9_Anom_FOBSignPF2_3_B"), TEXT("SignPF23")),
			Material6A(TEXT("PF45Arrows"), 2, TEXT("MI_P9_Anom6_FOBSignPF4_5_Arrows"), TEXT("SignPF45")), Material6A(TEXT("PF45Number"), 2, TEXT("MI_P9_Anom6_FOBSignPF4_5_Number"), TEXT("SignPF45")),
			Material6A(TEXT("PF67Arrows"), 2, TEXT("MI_P9_Anom6_FOBSignPF6_7_Arrows"), TEXT("SignPF67")), Material6A(TEXT("PF67Number"), 2, TEXT("MI_P9_Anom6_FOBSignPF6_7_Number"), TEXT("SignPF67")),
			Material6A(TEXT("PF8Arrows"), 2, TEXT("MI_P9_Anom6_FOBSignPF8_Arrows"), TEXT("SignPF8")), Material6A(TEXT("PF8Number"), 2, TEXT("MI_P9_Anom6_FOBSignPF8_Number"), TEXT("SignPF8")),
			Material4D(TEXT("PF910Arrows"), 2, TEXT("MI_P9_Anom_FOBSignPF9_10_A"), TEXT("FOB_SignPF910")), Material4D(TEXT("PF910Number"), 2, TEXT("MI_P9_Anom_FOBSignPF9_10_B"), TEXT("FOB_SignPF910")),
			Material4D(TEXT("Island13"), 2, TEXT("MI_P9_Anom_SignPF9_10_A"), TEXT("PF910_Sign")), Material4D(TEXT("IslandSwapped"), 3, TEXT("MI_P9_Anom_SignPF9_10_B"), TEXT("PF910_Sign"))};
	}
	// 7. a closed platform entry on the footbridge stands open
	{
		FP9AnomalyDef& D = Def(TEXT("ClosedEntryOpen"), TEXT("A closed platform entry stands open"), EP9AnomalyCategory::Environment, EP9AnomalyHandler::HideActor, 1.0f,
			{EP9AnomalyLocation::ClosedEntry});
		// the CLOSED - NO ENTRY plate hangs on the panel, so it goes with it (left alone it would float in the doorway)
		FP9AnomalyVariant Open = Variant(TEXT("Open"), 2); Open.Param = 1.0f;
		D.Variants = {Open};
	}
	// 8. the advertisement becomes a boy whose eyes follow the player
	{
		FP9AnomalyDef& D = Def(TEXT("PosterBoy"), TEXT("The poster shows a boy whose eyes follow you"), EP9AnomalyCategory::Signage, EP9AnomalyHandler::PosterEyes, 1.0f,
			{EP9AnomalyLocation::Poster});
		D.MaterialSlotMatch = TEXT("AdPoster");
		D.Variants = {Material6A(TEXT("BoyWatching"), 2, TEXT("MI_P9_Anom6_AdPoster_Boy")), Material6A(TEXT("BoyWatchingLater"), 3, TEXT("MI_P9_Anom6_AdPoster_Boy"), TEXT(""), true)};
	}
	// 9. endless stair. Stage 8A: the lower flight to Platform 9 runs upwards like an escalator against the player (no teleport);
	//    climbing back up stops it (AP9ReverseEscalator)
	{
		FP9AnomalyDef& D = Def(TEXT("EndlessStair"), TEXT("The Platform 9 stair carries you back up"), EP9AnomalyCategory::Environment, EP9AnomalyHandler::ReverseEscalator, 1.0f,
			{EP9AnomalyLocation::StairLoop});
		D.Variants = {Variant(TEXT("Escalator"), 2)};
	}
	// 10. a section of track 9 or 10 is missing
	{
		FP9AnomalyDef& D = Def(TEXT("TrackMissing"), TEXT("A section of track is missing"), EP9AnomalyCategory::Environment, EP9AnomalyHandler::MeshSwap, 1.0f,
			{EP9AnomalyLocation::Track});
		auto Gap = [](const TCHAR* Id, int32 Difficulty, const TCHAR* Mesh)
		{
			FP9AnomalyVariant V = Variant(Id, Difficulty);
			V.Mesh = TSoftObjectPtr<UStaticMesh>(AssetPath(Folder6A, Mesh));
			return V;
		};
		D.Variants = {Gap(TEXT("RailsGone"), 2, TEXT("SM_P9_TrackPanel_GapRails")), Gap(TEXT("TrackGone"), 1, TEXT("SM_P9_TrackPanel_GapAll"))};
	}
	// 11. the booking office: window bars horizontal and the window numbers wrong
	{
		FP9AnomalyDef& D = Def(TEXT("TicketCounterWrong"), TEXT("Booking office bars horizontal, window numbers wrong"), EP9AnomalyCategory::Other, EP9AnomalyHandler::MaterialSwap, 1.0f,
			{EP9AnomalyLocation::TicketCounter});
		D.MaterialSlotMatch = TEXT("BookingCounter");
		D.Variants = {Material6A(TEXT("Bars231"), 2, TEXT("MI_P9_Anom6_BookingCounter_231")), Material6A(TEXT("Bars312"), 2, TEXT("MI_P9_Anom6_BookingCounter_312")),
			Material6A(TEXT("Bars174"), 1, TEXT("MI_P9_Anom6_BookingCounter_174")), Material6A(TEXT("BarsOnly"), 3, TEXT("MI_P9_Anom6_BookingCounter_123"))};
	}
	// 12. Stage 6C: the footbridge concourse's analog clock runs backwards (counterclockwise). Weight 0.8: the station already has
	//     one clock anomaly (ClockWrong), so the two clock anomalies together stay below a fifth of the anomalous days.
	{
		FP9AnomalyDef& D = Def(TEXT("AnalogClockReverse"), TEXT("The footbridge analog clock runs backwards"), EP9AnomalyCategory::Environment, EP9AnomalyHandler::AnalogClock, 0.8f,
			{EP9AnomalyLocation::AnalogClock});
		auto Rev = [](const TCHAR* Id, int32 Difficulty, float Rate, float Pause, float Slow, bool bWaitUntilSeen)
		{
			FP9AnomalyVariant V = Variant(Id, Difficulty);
			V.Param = Rate;                                            // x real time, backwards
			V.Scale = FVector(Pause, Slow, bWaitUntilSeen ? 1.0f : 0.0f);
			return V;
		};
		D.Variants = {Rev(TEXT("Backwards"), 1, 1.0f, 0.0f, 0.0f, false),          // runs backwards all day at real speed: watch the second hand
			Rev(TEXT("Unwinding"), 2, 12.0f, 1.5f, 6.0f, true),                       // seen -> stops -> second hand back -> minutes unwind
			Rev(TEXT("Rewinding"), 3, 60.0f, 2.5f, 3.0f, true)};                      // the same, then an hour a minute backwards
	}
	// 13. Stage 8A: one Platform 9 lamp burns red (its spot and fill lights and its tube) - the rest of the station stays normal
	{
		FP9AnomalyDef& D = Def(TEXT("RedLightPF9"), TEXT("A Platform 9 lamp burns red"), EP9AnomalyCategory::Environment, EP9AnomalyHandler::LightTint, 1.0f,
			{EP9AnomalyLocation::PlatformLamp});
		auto Red = [](const TCHAR* Id, int32 Difficulty, FLinearColor Color, float Intensity, float PulseDepth, float PulsePeriod)
		{
			FP9AnomalyVariant V = Variant(Id, Difficulty);
			V.Color = Color;
			V.Param = Intensity;
			V.Scale = FVector(PulseDepth, PulsePeriod, 0.0f);
			return V;
		};
		D.Variants = {Red(TEXT("Crimson"), 2, FLinearColor(1.0f, 0.035f, 0.02f), 1.35f, 0.12f, 4.5f),   // deep red, a slow uneasy swell
			Red(TEXT("BloodRed"), 3, FLinearColor(0.9f, 0.0f, 0.0f), 1.7f, 0.2f, 3.0f),                       // strong, pulsing
			Red(TEXT("DullRed"), 1, FLinearColor(1.0f, 0.16f, 0.07f), 1.1f, 0.05f, 6.0f)};                    // a sick red-orange, almost steady
	}
	// 14. Stage 8A: the Vashi local's seat upholstery is red (both cars; texture recoloured from the original, same UVs and mesh)
	{
		FP9AnomalyDef& D = Def(TEXT("TrainSeatsRed"), TEXT("The train's seats are red"), EP9AnomalyCategory::Train, EP9AnomalyHandler::TrainMaterial, 1.0f,
			{EP9AnomalyLocation::TrainSeats});
		D.MaterialSlotMatch = TEXT("Train_Seat");
		auto Seat = [](const TCHAR* Id, int32 Difficulty, const TCHAR* Material)
		{
			FP9AnomalyVariant V = Variant(Id, Difficulty);
			V.Material = TSoftObjectPtr<UMaterialInterface>(AssetPath(Materials8A, Material));
			return V;
		};
		D.Variants = {Seat(TEXT("Red"), 2, TEXT("MI_P9_Anom8_TrainSeat_Red")), Seat(TEXT("DarkRed"), 1, TEXT("MI_P9_Anom8_TrainSeat_DarkRed"))};
	}
	// 15. Stage 8A: one coach of the Vashi local is dark (which one comes from the day's seed through the point choice)
	{
		FP9AnomalyDef& D = Def(TEXT("CoachLightOff"), TEXT("One coach of the train is dark"), EP9AnomalyCategory::Train, EP9AnomalyHandler::TrainLightsOff, 1.0f,
			{EP9AnomalyLocation::TrainLights});
		auto Dark = [](const TCHAR* Id, int32 Difficulty, float Residual, bool bDiesOnApproach)
		{
			FP9AnomalyVariant V = Variant(Id, Difficulty);
			V.Param = Residual;                                                       // what is left of the coach's light
			V.Scale = FVector(1.0f, 1.0f, bDiesOnApproach ? 1.0f : 0.0f);
			return V;
		};
		D.Variants = {Dark(TEXT("Dark"), 3, 0.04f, false), Dark(TEXT("Dim"), 1, 0.14f, false), Dark(TEXT("DiesOnApproach"), 2, 0.04f, true)};
	}
	// 16. Stage 8A: the Lantern Man stands motionless in the darkest part of Platform 9; the lamp beside him flickers
	{
		FP9AnomalyDef& D = Def(TEXT("LanternMan"), TEXT("A man with a lantern stands at the dark end of Platform 9"), EP9AnomalyCategory::Supernatural,
			EP9AnomalyHandler::LanternFigure, 1.0f, {EP9AnomalyLocation::LanternSpot});
		D.Variants = {Variant(TEXT("Standing"), 2)};
	}
}

// =====================================================================================================================
// generation
// =====================================================================================================================
int32 AP9CampaignDirector::ScheduledDay(const FP9AnomalyDef& D, int32 InRunSeed) const
{
	if (!D.bScheduled)
	{
		return INDEX_NONE;
	}
	const int32 First = FMath::Clamp(D.ScheduleFirstDay, 1, NumDays);
	const int32 Last = FMath::Clamp(D.ScheduleLastDay, First, NumDays);
	const uint32 H = HashCombine(GetTypeHash(InRunSeed), GetTypeHash(D.Id.ToString() + TEXT("#6A")));
	return First + static_cast<int32>(H % static_cast<uint32>(Last - First + 1));
}

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
	// Stage 6A: a scheduled anomaly (Kanchana) owns its day outright: that day is anomalous and she is its only anomaly
	int32 ScheduledDef = INDEX_NONE;
	for (int32 d = 0; d < Anomalies.Num() && ScheduledDef == INDEX_NONE; ++d)
	{
		const FP9AnomalyDef& D = Anomalies[d];
		if (D.bEnabled && D.bScheduled && !D.Variants.IsEmpty() && ScheduledDay(D, InRunSeed) == InDay)
		{
			ScheduledDef = d;
		}
	}
	if (ScheduledDef != INDEX_NONE)
	{
		Plan.bAnomalous = true;
	}
	if (!Plan.bAnomalous)
	{
		return Plan;
	}
	FRandomStream Rng(Plan.Seed);
	const int32 Want = ScheduledDef != INDEX_NONE ? 1 : FMath::Max(1, PickWeighted(Rng, AnomalyCountWeights) + 1);

	TSet<int32> UsedPoints;
	TSet<FString> UsedTargets;
	TArray<FName> DayTags;
	TArray<int32> UsedDefs;
	auto VariantFits = [this](const FP9AnomalyVariant& V, int32 p)
	{
		return V.PointMatch.IsEmpty() || SpawnPoints[p]->PointId.ToString().Contains(V.PointMatch);
	};
	auto FreePoints = [&](const FP9AnomalyDef& D)
	{
		TArray<int32> Out;
		for (int32 p = 0; p < SpawnPoints.Num(); ++p)
		{
			const AP9AnomalySpawnPoint* Point = SpawnPoints[p];
			if (!Point || !D.LocationTypes.Contains(Point->LocationType) || UsedPoints.Contains(p) || UsedTargets.Contains(Point->GetTargetKey()) || !IsPointAllowed(Point))
			{
				continue;
			}
			bool bAnyVariant = false;
			for (const FP9AnomalyVariant& V : D.Variants)
			{
				bAnyVariant |= VariantFits(V, p);
			}
			if (bAnyVariant)
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
		if (ScheduledDef != INDEX_NONE)
		{
			if (!FreePoints(Anomalies[ScheduledDef]).IsEmpty())
			{
				Candidates.Add(ScheduledDef);
				Weights.Add(1.0f);
			}
		}
		else
		{
			for (int32 d = 0; d < Anomalies.Num(); ++d)
			{
				const FP9AnomalyDef& D = Anomalies[d];
				if (!D.bEnabled || D.bScheduled || D.Variants.IsEmpty() || UsedDefs.Contains(d) || SharesTag(D.ExcludeTags, DayTags) || FreePoints(D).IsEmpty())
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
			VariantWeights.Add(VariantFits(V, PointIndex) ? V.Weight * (V.Difficulty == FMath::Min(Plan.Difficulty, 3) ? 2.0f : 1.0f) : 0.0f);
		}
		const int32 VariantIndex = FMath::Max(0, PickWeighted(Rng, VariantWeights));

		FP9PlannedAnomaly& A = Plan.Anomalies.AddDefaulted_GetRef();
		A.DefIndex = DefIndex;
		A.VariantIndex = VariantIndex;
		A.PointIndex = PointIndex;
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
	if (!Stalker)
	{
		for (TActorIterator<AP9SilhouetteStalker> It(GetWorld()); It; ++It) { Stalker = *It; break; }
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
		if (AP9MainMenu::HoldsCampaign(GetWorld()))
		{
			return;                                                 // Stage 9B: the main menu is up - the campaign starts when the player chooses
		}
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
		Active += FString::Printf(TEXT(" %s:%s@%s"), *A.Plan.DefId.ToString(), *A.Plan.VariantId.ToString(), *A.Plan.PointId.ToString());
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
	case EP9AnomalyHandler::HideActor: bOk = ActivateHideActor(V, Point); break;
	case EP9AnomalyHandler::PAAnnouncement: bOk = V.Sound.LoadSynchronous() != nullptr; break;
	case EP9AnomalyHandler::DynamicClock: bOk = ActivateDynamicClock(A, Def, V, Point); break;
	case EP9AnomalyHandler::MeshSwap: bOk = ActivateMeshSwap(A, V, Point); break;
	case EP9AnomalyHandler::LoopTeleport: bOk = Point->LoopTrigger.IsValid && !Point->LoopOffset.IsNearlyZero(); break;
	case EP9AnomalyHandler::PosterEyes: bOk = ActivatePosterEyes(A, Def, V, Point); break;
	case EP9AnomalyHandler::Stalker: bOk = ActivateStalker(Point); break;
	case EP9AnomalyHandler::AnalogClock: bOk = ActivateAnalogClock(V, Point); break;
	case EP9AnomalyHandler::LightTint: bOk = ActivateLightTint(A, V, Point); break;
	case EP9AnomalyHandler::TrainMaterial: bOk = ActivateTrainMaterial(Def, V, Point); break;
	case EP9AnomalyHandler::TrainLightsOff: bOk = ActivateTrainLightsOff(A, V, Point); break;
	case EP9AnomalyHandler::LanternFigure: bOk = ActivateLanternFigure(A, Point); break;
	case EP9AnomalyHandler::EndlessBridge: bOk = ActivateEndlessBridge(A, Point); break;
	case EP9AnomalyHandler::ReverseEscalator: bOk = ActivateReverseEscalator(A, Point); break;
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
		if (A.Target)
		{
			A.Target->OnCanvasRenderTargetUpdate.RemoveAll(this);
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

bool AP9CampaignDirector::ActivateHideActor(const FP9AnomalyVariant& V, AP9AnomalySpawnPoint* Point)
{
	// Stage 6A: the target with everything listed on the point (a closed stair head: its panel, the blocking volume behind it, a tarp)
	AActor* Target = Point->TargetActor;
	if (!Target || Target->IsHidden())
	{
		return false;
	}
	TArray<AActor*> Group = {Target};
	for (AActor* Extra : Point->ExtraActors)
	{
		if (Extra) Group.AddUnique(Extra);
	}
	if (V.Param >= 1.0f && Point->SecondaryActor)
	{
		Group.AddUnique(Point->SecondaryActor);                      // e.g. the CLOSED - NO ENTRY sign goes too
	}
	for (AActor* Actor : Group)
	{
		RecordActor(Actor);
		Actor->SetActorHiddenInGame(true);
		Actor->SetActorEnableCollision(false);
	}
	return true;
}

bool AP9CampaignDirector::ActivateAnalogClock(const FP9AnomalyVariant& V, AP9AnomalySpawnPoint* Point)
{
	// Stage 6C: the clock keeps its own time; the anomaly only changes the direction it runs. CleanupDay puts it back.
	AP9AnalogClock* Clock = Cast<AP9AnalogClock>(Point->TargetActor);
	if (!Clock || !Clock->HasAllParts() || Clock->IsReversed())
	{
		return false;
	}
	Clock->BeginReverse(V.Param, V.Scale.X, V.Scale.Y, V.Scale.Z > 0.5f);
	return true;
}

// ---------------------------------------------------------------------------------------------------------------- Stage 8A
bool AP9CampaignDirector::ActivateLightTint(FP9ActiveAnomaly& A, const FP9AnomalyVariant& V, AP9AnomalySpawnPoint* Point)
{
	// the lamp's own lights (spot on the target, fill among the extras) take the colour; its tube glows the same colour
	TArray<ULightComponent*> Lights;
	if (Point->TargetActor) Point->TargetActor->GetComponents<ULightComponent>(Lights);
	for (AActor* Extra : Point->ExtraActors)
	{
		TArray<ULightComponent*> More;
		if (Extra) Extra->GetComponents<ULightComponent>(More);
		Lights.Append(More);
	}
	if (Lights.IsEmpty())
	{
		return false;
	}
	for (ULightComponent* L : Lights)
	{
		RecordLight(L);
		A.Lights.Add(L);
		A.LightBase.Add(L->Intensity * FMath::Max(0.1f, V.Param));
		L->SetLightColor(V.Color);
		L->SetIntensity(L->Intensity * FMath::Max(0.1f, V.Param));
	}
	if (UStaticMeshComponent* Tube = Point->SecondaryActor ? Point->SecondaryActor->FindComponentByClass<UStaticMeshComponent>() : nullptr)
	{
		for (int32 i = 0; i < Tube->GetNumMaterials(); ++i)
		{
			const UMaterialInterface* M = Tube->GetMaterial(i);
			if (M && M->GetName().Contains(TEXT("LampTube")))
			{
				RecordMesh(Tube);
				if (UMaterialInstanceDynamic* MID = Tube->CreateDynamicMaterialInstance(i))
				{
					MID->SetVectorParameterValue(TEXT("TubeColor"), V.Color);
					A.MIDs.Add(MID);
				}
				break;
			}
		}
	}
	return true;
}

void AP9CampaignDirector::TickLightTint(FP9ActiveAnomaly& A, const FP9AnomalyVariant& V, float DeltaSeconds)
{
	// a slow swell, never off: the area stays readable, it just is not right
	const float Period = FMath::Max(0.5f, V.Scale.Y);
	const float S = 1.0f + V.Scale.X * FMath::Sin(2.0f * PI * A.Time / Period) + 0.25f * V.Scale.X * FMath::Sin(2.0f * PI * A.Time / (Period * 0.37f));
	for (int32 i = 0; i < A.Lights.Num(); ++i)
	{
		if (A.Lights[i]) A.Lights[i]->SetIntensity(A.LightBase[i] * S);
	}
}

bool AP9CampaignDirector::ActivateTrainMaterial(const FP9AnomalyDef& Def, const FP9AnomalyVariant& V, AP9AnomalySpawnPoint* Point)
{
	// every matching slot on every mesh of the train (both cars' interiors), so it is the same in whichever car the player boards
	AP9Train* T = Cast<AP9Train>(Point->TargetActor);
	UMaterialInterface* Material = V.Material.LoadSynchronous();
	if (!T || !Material)
	{
		return false;
	}
	int32 Changed = 0;
	TArray<UStaticMeshComponent*> Meshes;
	T->GetComponents<UStaticMeshComponent>(Meshes);
	for (UStaticMeshComponent* M : Meshes)
	{
		bool bRecorded = false;
		for (int32 i = 0; i < M->GetNumMaterials(); ++i)
		{
			const UMaterialInterface* Cur = M->GetMaterial(i);
			if (Cur && Cur->GetName().Contains(Def.MaterialSlotMatch))
			{
				if (!bRecorded) { RecordMesh(M); bRecorded = true; }
				M->SetMaterial(i, Material);
				++Changed;
			}
		}
	}
	UE_LOG(LogP9, Log, TEXT("Campaign: %s - %d train seat slots now %s"), *Def.Id.ToString(), Changed, *Material->GetName());
	return Changed > 0;
}

void AP9CampaignDirector::SetCarLights(FP9ActiveAnomaly& A, float Scale)
{
	for (int32 i = 0; i < A.Lights.Num(); ++i)
	{
		if (A.Lights[i]) A.Lights[i]->SetIntensity(A.LightBase[i] * Scale);
	}
	for (UMaterialInstanceDynamic* MID : A.MIDs)
	{
		if (MID) MID->SetScalarParameterValue(TEXT("Intensity"), 3.0f * FMath::Max(0.01f, Scale));
	}
}

bool AP9CampaignDirector::ActivateTrainLightsOff(FP9ActiveAnomaly& A, const FP9AnomalyVariant& V, AP9AnomalySpawnPoint* Point)
{
	// the point names the car ("Car0" / "Car1"): its two interior lights and its ceiling tubes' glow go down to the residual
	AP9Train* T = Cast<AP9Train>(Point->TargetActor);
	const FString Car = Point->TargetComponent.ToString();
	if (!T || Car.IsEmpty())
	{
		return false;
	}
	for (int32 L = 0; L < 2; ++L)
	{
		if (ULightComponent* Light = P9Campaign::FindNamedComponent<ULightComponent>(T, FString::Printf(TEXT("%s_InteriorLight%d"), *Car, L)))
		{
			RecordLight(Light);
			A.Lights.Add(Light);
			A.LightBase.Add(Light->Intensity);
		}
	}
	if (UStaticMeshComponent* Interior = P9Campaign::FindNamedComponent<UStaticMeshComponent>(T, Car + TEXT("_Interior")))
	{
		for (int32 i = 0; i < Interior->GetNumMaterials(); ++i)
		{
			const UMaterialInterface* M = Interior->GetMaterial(i);
			if (M && M->GetName().Contains(TEXT("Train_Tube")))
			{
				RecordMesh(Interior);
				if (UMaterialInstanceDynamic* MID = Interior->CreateDynamicMaterialInstance(i)) A.MIDs.Add(MID);
				break;
			}
		}
	}
	if (A.Lights.Num() < 2)
	{
		return false;
	}
	A.Stage = V.Scale.Z > 0.5f ? 0 : 2;
	SetCarLights(A, A.Stage == 2 ? V.Param : 1.0f);
	return true;
}

void AP9CampaignDirector::TickTrainLightsOff(FP9ActiveAnomaly& A, const FP9AnomalyVariant& V, const AP9AnomalySpawnPoint* Point, float DeltaSeconds)
{
	if (A.Stage == 2 || !Point || !Point->TargetActor)
	{
		return;
	}
	const USceneComponent* CarRoot = P9Campaign::FindNamedComponent<USceneComponent>(Point->TargetActor, Point->TargetComponent.ToString());
	const ACharacter* P = PlayerCharacter();
	if (A.Stage == 0)
	{
		// dies as the player comes to it: within 12 m of the car while it is in the station, or aboard
		const bool bNear = P && CarRoot && !Point->TargetActor->IsHidden() && FVector::Dist(P->GetActorLocation(), CarRoot->GetComponentLocation()) < 1200.0f;
		if (bNear || (Train && Train->IsPlayerAboard()))
		{
			A.Stage = 1;
			A.StageTime = 0.0f;
			UE_LOG(LogP9, Log, TEXT("Campaign: %s - %s lights failing"), *A.Plan.DefId.ToString(), *Point->TargetComponent.ToString());
		}
		return;
	}
	A.StageTime += DeltaSeconds;
	const float N = FMath::PerlinNoise1D(A.StageTime * 11.0f);
	const float Level = A.StageTime < 1.6f ? (N > 0.05f ? 1.0f : (N > -0.25f ? 0.35f : V.Param)) : V.Param;
	SetCarLights(A, Level);
	if (A.StageTime >= 1.6f)
	{
		A.Stage = 2;
	}
}

bool AP9CampaignDirector::ActivateLanternFigure(FP9ActiveAnomaly& A, AP9AnomalySpawnPoint* Point)
{
	UStaticMesh* Mesh = LanternManMesh.LoadSynchronous();
	if (!Mesh || !Point->TargetActor)
	{
		return false;
	}
	FVector Spot = Point->GetActorLocation();
	FHitResult Floor;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(P9LanternSpot), false);
	if (GetWorld()->LineTraceSingleByChannel(Floor, Spot + FVector(0.0f, 0.0f, 150.0f), Spot - FVector(0.0f, 0.0f, 400.0f), ECC_Visibility, Params))
	{
		Spot = Floor.ImpactPoint;
	}
	// the lamp beside him: its lights (target spot + the extras' fill) and its tube (secondary)
	TArray<ULightComponent*> Lamp;
	Point->TargetActor->GetComponents<ULightComponent>(Lamp);
	for (AActor* Extra : Point->ExtraActors)
	{
		TArray<ULightComponent*> More;
		if (Extra) Extra->GetComponents<ULightComponent>(More);
		Lamp.Append(More);
	}
	for (ULightComponent* L : Lamp) RecordLight(L);
	UStaticMeshComponent* Tube = Point->SecondaryActor ? Point->SecondaryActor->FindComponentByClass<UStaticMeshComponent>() : nullptr;
	if (Tube) RecordMesh(Tube);
	const FTransform Xf(FRotator(0.0f, Point->GetActorRotation().Yaw, 0.0f), Spot + FVector(0.0f, 0.0f, 95.0f));
	AP9LanternMan* Man = GetWorld()->SpawnActorDeferred<AP9LanternMan>(AP9LanternMan::StaticClass(), Xf, this);
	if (!Man)
	{
		return false;
	}
	Man->Tags.Add(P9Campaign::SpawnedTag);
	Man->FinishSpawning(Xf);
	RecordSpawned(Man);
	A.Extras.Add(Man);
	// a lantern, not a torch: a little warm light round his hand (measured in PIE: 14 cd washed his own body out)
	Man->LanternIntensity = 3.5f;
	Man->LanternRadius = 300.0f;
	Man->Setup(Mesh, Lamp, Tube, static_cast<int32>(HashCombine(HashCombine(GetTypeHash(RunSeed), GetTypeHash(Day)), GetTypeHash(Point->PointId))));
	return true;
}

bool AP9CampaignDirector::ActivateEndlessBridge(FP9ActiveAnomaly& A, AP9AnomalySpawnPoint* Point)
{
	UStaticMesh* South = BridgeSouthMesh.LoadSynchronous();
	UStaticMesh* End = BridgeEndMesh.LoadSynchronous();
	UStaticMesh* Bay = BridgeBayMesh.LoadSynchronous();
	UStaticMesh* Lead = BridgeLeadInMesh.LoadSynchronous();
	if (!South || !End || !Bay || !Lead || Point->ExtraActors.Num() < 2 || !Point->LoopTrigger.IsValid)
	{
		return false;
	}
	for (AActor* Extra : Point->ExtraActors)
	{
		if (Extra) RecordActor(Extra);                                    // whatever the helper hides comes back at day end
	}
	AP9EndlessFootbridge* Bridge = GetWorld()->SpawnActorDeferred<AP9EndlessFootbridge>(AP9EndlessFootbridge::StaticClass(), FTransform::Identity, this);
	if (!Bridge)
	{
		return false;
	}
	Bridge->Tags.Add(P9Campaign::SpawnedTag);
	Bridge->FinishSpawning(FTransform::Identity);
	RecordSpawned(Bridge);
	A.Extras.Add(Bridge);
	return Bridge->Setup(Point, South, End, Bay, Lead, BridgeTubeMesh.LoadSynchronous());
}

bool AP9CampaignDirector::ActivateReverseEscalator(FP9ActiveAnomaly& A, AP9AnomalySpawnPoint* Point)
{
	UStaticMesh* Upper = EscalatorUpperMesh.LoadSynchronous();
	UStaticMesh* Step = EscalatorStepMesh.LoadSynchronous();
	if (!Upper || !Step || !Point->TargetActor || Point->TargetActor->IsHidden())
	{
		return false;
	}
	RecordActor(Point->TargetActor);
	AP9ReverseEscalator* Esc = GetWorld()->SpawnActorDeferred<AP9ReverseEscalator>(AP9ReverseEscalator::StaticClass(), Point->TargetActor->GetActorTransform(), this);
	if (!Esc)
	{
		return false;
	}
	Esc->Tags.Add(P9Campaign::SpawnedTag);
	Esc->FinishSpawning(Point->TargetActor->GetActorTransform());
	RecordSpawned(Esc);
	A.Extras.Add(Esc);
	return Esc->Setup(Point->TargetActor, Upper, Step);
}

bool AP9CampaignDirector::IsAnyAnomalyNoticed() const
{
	for (const FP9ActiveAnomaly& A : ActiveAnomalies)
	{
		if (A.bNoticed) return true;
	}
	return false;
}

FString AP9CampaignDirector::GetAnomalyHelperStatus() const
{
	FString S;
	for (const FP9ActiveAnomaly& A : ActiveAnomalies)
	{
		for (const AActor* E : A.Extras)
		{
			if (const AP9EndlessFootbridge* B = Cast<AP9EndlessFootbridge>(E)) S += TEXT(" bridge[") + B->GetStatus() + TEXT("]");
			if (const AP9ReverseEscalator* R = Cast<AP9ReverseEscalator>(E)) S += TEXT(" escalator[") + R->GetStatus() + TEXT("]");
			if (const AP9LanternMan* L = Cast<AP9LanternMan>(E)) S += FString::Printf(TEXT(" lantern[seen=%d at %s]"), L->HasBeenSeen() ? 1 : 0, *L->GetActorLocation().ToCompactString());
		}
		S += FString::Printf(TEXT(" %s noticed=%d stage=%d"), *A.Plan.DefId.ToString(), A.bNoticed ? 1 : 0, A.Stage);
	}
	return S;
}

void AP9CampaignDirector::TickNotice(FP9ActiveAnomaly& A, const FP9AnomalyDef& Def, const AP9AnomalySpawnPoint* Point, float DeltaSeconds)
{
	// "seen / met" only drives the anomaly ambience (user choice: never before it is found); every handler has its own test
	if (A.bNoticed || !Point)
	{
		return;
	}
	bool bNow = false;
	switch (Def.Handler)
	{
	case EP9AnomalyHandler::PAAnnouncement:
		bNow = ConnectingCalls > 0 && bConnectingCalled;                          // the wrong announcement has been played
		break;
	case EP9AnomalyHandler::EndlessBridge:
		for (const AActor* E : A.Extras) if (const AP9EndlessFootbridge* B = Cast<AP9EndlessFootbridge>(E)) bNow |= B->IsEngaged();
		break;
	case EP9AnomalyHandler::ReverseEscalator:
		for (const AActor* E : A.Extras) if (const AP9ReverseEscalator* R = Cast<AP9ReverseEscalator>(E)) bNow |= R->IsEngaged();
		break;
	case EP9AnomalyHandler::LanternFigure:
		for (const AActor* E : A.Extras) if (const AP9LanternMan* L = Cast<AP9LanternMan>(E)) bNow |= L->HasBeenSeen();
		break;
	case EP9AnomalyHandler::Stalker:
		bNow = Stalker && !Stalker->IsHidden() && IsInView(Stalker->GetActorLocation() + FVector(0.0f, 0.0f, 120.0f), 0.8f, 2500.0f, Stalker, true);
		break;
	case EP9AnomalyHandler::TrainMaterial:
	case EP9AnomalyHandler::TrainLightsOff:
	{
		const AActor* T = Point->TargetActor;
		const ACharacter* P = PlayerCharacter();
		bNow = (Train && Train->IsPlayerAboard()) || (T && P && !T->IsHidden() && FVector::Dist2D(P->GetActorLocation(), T->GetActorLocation()) < 2400.0f
			&& IsInView(T->GetActorLocation() + FVector(0.0f, 0.0f, 200.0f), 0.5f, 2600.0f, T, false));
		break;
	}
	default:
	{
		const AActor* T = Point->TargetActor;
		const FVector Where = T ? T->GetComponentsBoundingBox(true).GetCenter() : Point->GetActorLocation();
		bNow = IsInView(Where, 0.85f, 3500.0f, T, true);
		break;
	}
	}
	A.NoticeTime = bNow ? A.NoticeTime + DeltaSeconds : 0.0f;
	if (A.NoticeTime >= P9Campaign::NoticeSeconds || (bNow && (Def.Handler == EP9AnomalyHandler::PAAnnouncement || Def.Handler == EP9AnomalyHandler::EndlessBridge
		|| Def.Handler == EP9AnomalyHandler::ReverseEscalator || Def.Handler == EP9AnomalyHandler::LanternFigure)))
	{
		A.bNoticed = true;
		UE_LOG(LogP9, Log, TEXT("Campaign: %s noticed by the player (anomaly ambience may play)"), *A.Plan.DefId.ToString());
	}
}

bool AP9CampaignDirector::ActivateDynamicClock(FP9ActiveAnomaly& A, const FP9AnomalyDef& Def, const FP9AnomalyVariant& V, AP9AnomalySpawnPoint* Point)
{
	UStaticMeshComponent* Mesh = Point->TargetActor ? Point->TargetActor->FindComponentByClass<UStaticMeshComponent>() : nullptr;
	if (!Mesh || !ClockDigits.LoadSynchronous())
	{
		return false;
	}
	int32 Slot = INDEX_NONE;
	for (int32 i = 0; i < Mesh->GetNumMaterials() && Slot == INDEX_NONE; ++i)
	{
		const UMaterialInterface* Current = Mesh->GetMaterial(i);
		Slot = Current && Current->GetName().Contains(Def.MaterialSlotMatch) ? i : INDEX_NONE;
	}
	if (Slot == INDEX_NONE)
	{
		return false;
	}
	UCanvasRenderTarget2D* Face = UCanvasRenderTarget2D::CreateCanvasRenderTarget2D(this, UCanvasRenderTarget2D::StaticClass(), P9Campaign::ClockFaceW, P9Campaign::ClockFaceH);
	if (!Face)
	{
		return false;
	}
	RecordMesh(Mesh);
	UMaterialInstanceDynamic* MID = Mesh->CreateDynamicMaterialInstance(Slot);
	if (!MID)
	{
		return false;
	}
	MID->SetTextureParameterValue(TEXT("PanelTexture"), Face);
	A.MIDs.Add(MID);
	A.Target = Face;
	// a running clock starts at the station's own time and drifts; a fixed one shows the variant text as it is
	A.ClockMinutes = P9Campaign::ParseClock(V.Text);
	ClockTextNow = V.Param > 0.0f ? P9Campaign::FormatClock(A.ClockMinutes) : V.Text;
	Face->OnCanvasRenderTargetUpdate.AddDynamic(this, &AP9CampaignDirector::DrawClock);
	Face->UpdateResource();
	return true;
}

void AP9CampaignDirector::DrawClock(UCanvas* Canvas, int32 Width, int32 Height)
{
	using namespace P9Campaign;
	// black LED face, then each character's dot-matrix cell from the atlas, added (cells overlap; their backgrounds are black)
	Canvas->K2_DrawTexture(nullptr, FVector2D::ZeroVector, FVector2D(Width, Height), FVector2D::ZeroVector, FVector2D::UnitVector, FLinearColor::Black, BLEND_Opaque);
	UTexture2D* Atlas = ClockDigits.Get();
	if (!Atlas)
	{
		return;
	}
	const float Sx = Width / static_cast<float>(ClockFaceW), Sy = Height / static_cast<float>(ClockFaceH);
	for (int32 k = 0; k < 5 && k < ClockTextNow.Len(); ++k)
	{
		const TCHAR C = ClockTextNow[k];
		const int32 Cell = C == TEXT(':') ? 10 : (FChar::IsDigit(C) ? C - TEXT('0') : INDEX_NONE);
		if (Cell == INDEX_NONE)
		{
			continue;
		}
		Canvas->K2_DrawTexture(Atlas, FVector2D(ClockSlotX[k] * Sx, 0.0f), FVector2D(ClockCellW * Sx, ClockCellH * Sy),
			FVector2D(Cell * ClockCellW / ClockAtlasW, 0.0f), FVector2D(ClockCellW / ClockAtlasW, ClockCellH / ClockAtlasH), FLinearColor::White, BLEND_Additive);
	}
}

bool AP9CampaignDirector::ActivateMeshSwap(FP9ActiveAnomaly& A, const FP9AnomalyVariant& V, AP9AnomalySpawnPoint* Point)
{
	// the placed track is static (its mesh cannot change at runtime): hide it and stand a movable copy with the variant mesh in its place
	AActor* Target = Point->TargetActor;
	const UStaticMeshComponent* Template = Target ? Target->FindComponentByClass<UStaticMeshComponent>() : nullptr;
	UStaticMesh* Mesh = V.Mesh.LoadSynchronous();
	if (!Template || !Mesh || Target->IsHidden())
	{
		return false;
	}
	const FTransform Xf = Template->GetComponentTransform();
	AStaticMeshActor* Copy = GetWorld()->SpawnActorDeferred<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Xf, this);
	if (!Copy)
	{
		return false;
	}
	UStaticMeshComponent* C = Copy->GetStaticMeshComponent();
	C->SetMobility(EComponentMobility::Movable);
	C->SetStaticMesh(Mesh);
	for (int32 i = 0; i < Template->GetNumMaterials(); ++i)
	{
		C->SetMaterial(i, Template->GetMaterial(i));
	}
	C->SetCollisionProfileName(Template->GetCollisionProfileName());
	C->SetCollisionEnabled(Template->GetCollisionEnabled());
	C->bNeverDistanceCull = true;
	Copy->Tags.Add(P9Campaign::SpawnedTag);
	Copy->FinishSpawning(Xf);
	Copy->SetActorTransform(Xf);
	RecordSpawned(Copy);
	A.Extras.Add(Copy);
	RecordActor(Target);
	Target->SetActorHiddenInGame(true);
	Target->SetActorEnableCollision(false);
	return true;
}

bool AP9CampaignDirector::ActivatePosterEyes(FP9ActiveAnomaly& A, const FP9AnomalyDef& Def, const FP9AnomalyVariant& V, AP9AnomalySpawnPoint* Point)
{
	UStaticMesh* Disc = PupilMesh.LoadSynchronous();
	UMaterialInterface* Dark = PupilMaterial.LoadSynchronous();
	if (!Disc || !Dark || !Point->TargetActor || !ActivateMaterialSwap(A, Def, V, Point))
	{
		return false;
	}
	// two flattened discs just off the paper over the painted eye whites; TickPosterEyes turns them towards the camera
	const FTransform Stand = Point->TargetActor->GetActorTransform();
	for (const FVector& Eye : P9Campaign::EyeCentres)
	{
		const FVector Where = Stand.TransformPosition(Eye + FVector(P9Campaign::PupilLift, 0.0f, 0.0f));
		AActor* Pupil = SpawnFigure(Where, Stand.Rotator().Yaw, Disc, P9Campaign::PupilScale, Dark);
		if (!Pupil)
		{
			return false;
		}
		Pupil->GetRootComponent()->SetWorldRotation(Stand.GetRotation());
		Pupil->SetActorEnableCollision(false);
		if (UStaticMeshComponent* C = Pupil->FindComponentByClass<UStaticMeshComponent>())
		{
			C->SetCastShadow(false);
		}
		Pupil->SetActorHiddenInGame(V.bChangeAfterLookAway);       // the "later" variant shows them only once the poster has changed
		A.Extras.Add(Pupil);
	}
	return true;
}

bool AP9CampaignDirector::ActivateStalker(AP9AnomalySpawnPoint* Point)
{
	if (!Stalker)
	{
		return false;
	}
	// she stands on the floor under the point, facing along it until she turns to watch the player
	FVector Spot = Point->GetActorLocation();
	FHitResult Floor;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(P9StalkerSpot), false, Stalker);
	if (GetWorld()->LineTraceSingleByChannel(Floor, Spot + FVector(0.0f, 0.0f, 150.0f), Spot - FVector(0.0f, 0.0f, 400.0f), ECC_Visibility, Params))
	{
		Spot = Floor.ImpactPoint;
	}
	return Stalker->BeginCampaignSighting(Spot, Point->GetActorRotation().Yaw);
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
	C->bNeverDistanceCull = true;                          // the level's cull distances would hide distant anomalies
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
	const AP9AnomalySpawnPoint* Point = SpawnPoints.IsValidIndex(A.Plan.PointIndex) ? SpawnPoints[A.Plan.PointIndex].Get() : nullptr;
	if (A.PendingMesh && A.PendingMaterial && Point)
	{
		// "it changed after you passed it": the player must have looked at it (0.5 s), then away (0.4 s)
		// (bounds include non-colliding components: the signs have no collision)
		const AActor* Target = Point->TargetActor;
		const FVector Where = Target ? Target->GetComponentsBoundingBox(true).GetCenter() : Point->GetActorLocation();
		if (IsInView(Where, 0.9f, 3500.0f, Target, false))
		{
			A.SeenTime += DeltaSeconds; A.AwayTime = 0.0f; A.bSeen |= A.SeenTime > 0.5f;
		}
		else if (A.bSeen && !IsInView(Where, 0.45f, 100000.0f, Target, false) && (A.AwayTime += DeltaSeconds) > 0.4f)
		{
			A.PendingMesh->SetMaterial(A.PendingSlot, A.PendingMaterial);
			A.PendingMesh = nullptr;
			if (Def.Handler == EP9AnomalyHandler::PosterEyes)
			{
				for (AActor* Extra : A.Extras) if (Extra) Extra->SetActorHiddenInGame(false);
			}
			UE_LOG(LogP9, Log, TEXT("Campaign: %s changed while the player looked away"), *A.Plan.DefId.ToString());
		}
	}
	switch (Def.Handler)
	{
	case EP9AnomalyHandler::DynamicClock: TickClock(A, V, DeltaSeconds); break;
	case EP9AnomalyHandler::LoopTeleport: TickLoop(A, Point, DeltaSeconds); break;
	case EP9AnomalyHandler::PosterEyes: TickPosterEyes(A, Point, DeltaSeconds); break;
	case EP9AnomalyHandler::LightTint: TickLightTint(A, V, DeltaSeconds); break;
	case EP9AnomalyHandler::TrainLightsOff: TickTrainLightsOff(A, V, Point, DeltaSeconds); break;
	default: break;
	}
	TickNotice(A, Def, Point, DeltaSeconds);
}

void AP9CampaignDirector::TickClock(FP9ActiveAnomaly& A, const FP9AnomalyVariant& V, float DeltaSeconds)
{
	if (V.Param <= 0.0f || !A.Target)
	{
		return;                                                       // a fixed wrong time never moves
	}
	A.ClockAccum += DeltaSeconds;
	const float Step = FMath::Max(0.05f, V.Scale.X);
	if (A.ClockAccum < Step)
	{
		return;
	}
	const int32 Steps = FMath::FloorToInt(A.ClockAccum / Step);
	A.ClockAccum -= Steps * Step;
	A.ClockMinutes += V.Param >= 2.0f ? Steps : -Steps;             // 1 = backwards, 2 = racing forwards
	ClockTextNow = P9Campaign::FormatClock(A.ClockMinutes);
	A.Target->UpdateResource();
}

void AP9CampaignDirector::TickLoop(FP9ActiveAnomaly& A, const AP9AnomalySpawnPoint* Point, float DeltaSeconds)
{
	A.LoopCooldown = FMath::Max(0.0f, A.LoopCooldown - DeltaSeconds);
	ACharacter* Player = PlayerCharacter();
	if (!Point || !Player || A.LoopCooldown > 0.0f || Phase != EP9DayPhase::Exploring || (Train && Train->IsPlayerAboard()))
	{
		return;
	}
	// only while moving on along the stretch (walking back the other way is the way out)
	const FVector Along = Point->LoopDirection.GetSafeNormal();
	if (!Point->LoopTrigger.IsInsideOrOn(PlayerFeet()) || FVector::DotProduct(Player->GetVelocity(), Along) < 20.0f)
	{
		return;
	}
	Player->SetActorLocation(Player->GetActorLocation() + Point->LoopOffset, false, nullptr, ETeleportType::TeleportPhysics);
	++A.Loops;
	A.LoopCooldown = 0.6f;
	// the station lights seem to dip for a moment - that is when it happens
	FadeAlpha = FMath::Max(FadeAlpha, P9Campaign::LoopBlinkAlpha);
	FadeTarget = 0.0f;
	FadeSpeed = P9Campaign::LoopBlinkAlpha / P9Campaign::LoopBlinkSeconds;
	UE_LOG(LogP9, Log, TEXT("Campaign: %s - player sent back by %s (loop %d)"), *A.Plan.DefId.ToString(), *Point->LoopOffset.ToCompactString(), A.Loops);
}

void AP9CampaignDirector::TickPosterEyes(FP9ActiveAnomaly& A, const AP9AnomalySpawnPoint* Point, float DeltaSeconds)
{
	using namespace P9Campaign;
	const APlayerController* PC = PlayerController();
	if (!Point || !Point->TargetActor || !PC || !PC->PlayerCameraManager || A.Extras.Num() < 2)
	{
		return;
	}
	// where the camera is, seen from the poster: the pupils turn towards it (clamped inside the painted eye whites)
	const FTransform Stand = Point->TargetActor->GetActorTransform();
	const FVector Cam = Stand.InverseTransformPosition(PC->PlayerCameraManager->GetCameraLocation());
	const FVector Mid = (EyeCentres[0] + EyeCentres[1]) * 0.5f;
	const FVector To = Cam - Mid;
	FVector2D Want = FVector2D::ZeroVector;
	if (To.X > 1.0f)
	{
		Want.X = FMath::Clamp(FMath::Atan2(To.Y, To.X) / FMath::DegreesToRadians(55.0f), -1.0f, 1.0f) * PupilTravelY;
		Want.Y = FMath::Clamp(FMath::Atan2(To.Z, FVector2D(To.X, To.Y).Size()) / FMath::DegreesToRadians(40.0f), -1.0f, 1.0f) * PupilTravelZ;
	}
	A.EyeOffset = FMath::Vector2DInterpTo(A.EyeOffset, Want, DeltaSeconds, 6.0f);
	for (int32 i = 0; i < 2; ++i)
	{
		if (AActor* Pupil = A.Extras[i])
		{
			const FVector Local = EyeCentres[i] + FVector(PupilLift, A.EyeOffset.X, A.EyeOffset.Y);
			Pupil->SetActorLocation(Stand.TransformPosition(Local));
		}
	}
}

void AP9CampaignDirector::TickPresentation(float DeltaSeconds)
{
	FadeAlpha = FMath::FInterpConstantTo(FadeAlpha, FadeTarget, DeltaSeconds, FadeSpeed);
	FlashAlpha = FMath::Max(0.0f, FlashAlpha - DeltaSeconds * 2.2f);
	TitleTime += DeltaSeconds;
	HintTime += DeltaSeconds;
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
		P9Captions::NotifySound(this, Sound);                       // Stage 9B: captions (only sounds listed in DT_P9_Captions)
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
		P9Captions::NotifySound(this, Sound);                       // Stage 9B: captions
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
	if (IsValid(Stalker))
	{
		Stalker->EndCampaignSighting();                              // Stage 6A: Kanchana leaves with her day
	}
	for (TActorIterator<AP9AnalogClock> It(GetWorld()); It; ++It)
	{
		It->EndReverse();                                            // Stage 6C: clockwise again, showing the correct time
	}
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
		if (const AP9AnalogClock* Clock = Cast<AP9AnalogClock>(Actor))
		{
			Snap.Add(Actor->GetName() + TEXT("/clock"), Clock->IsReversed() ? TEXT("reversed") : TEXT("clockwise"));
		}
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
		Active += FString::Printf(TEXT(" %s:%s@%s"), *A.Plan.DefId.ToString(), *A.Plan.VariantId.ToString(), *A.Plan.PointId.ToString());
		if (A.Loops > 0) Active += FString::Printf(TEXT("(loops %d)"), A.Loops);
	}
	FString KanchanaDay = TEXT("-");
	for (const FP9AnomalyDef& D : Anomalies)
	{
		if (D.bScheduled && D.bEnabled) KanchanaDay = FString::Printf(TEXT("%s on day %d"), *D.Id.ToString(), ScheduledDay(D, RunSeed));
	}
	return FString::Printf(TEXT("day=%d/%d run=%d seed=%d phase=%d status=%d token=%d dayTime=%.1f locked=%d zoneArmed=%d history=[%s] baselineRecords=%d scheduled=%s active=%d:%s"),
		Day, NumDays, RunId, RunSeed, static_cast<int32>(Phase), static_cast<int32>(Status), DayToken, DayTime, bDecisionLocked ? 1 : 0, bZoneArmed ? 1 : 0,
		*FString::Join(History, TEXT(",")), Baseline.Num(), *KanchanaDay, ActiveAnomalies.Num(), *Active);
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
