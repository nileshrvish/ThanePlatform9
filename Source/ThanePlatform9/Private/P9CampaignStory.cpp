// Stage 4B story layer of AP9CampaignDirector: the Dadar -> Thane opening, the connecting Vashi local at Platform 9, the
// anomaly zone rules and the post-Day-10 epilogue (chaiwala wake-up and the last ride). Kept apart from the campaign core.

#include "P9CampaignDirector.h"
#include "ThanePlatform9.h"
#include "P9AnomalySpawnPoint.h"
#include "P9SeatComponent.h"
#include "P9StationAnnouncer.h"
#include "P9Train.h"
#include "P9FriendNPC.h"
#include "EngineUtils.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/LightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "Engine/Texture.h"
#include "Misc/Paths.h"
#include "Sound/SoundBase.h"

namespace P9Story
{
	constexpr float TrainFloor = 110.0f;                 // AP9Train car floor above rail top (P9TrainLayout::Floor)
	constexpr float DoorX[3] = {-690.0f, 0.0f, 690.0f};  // AP9Train door centres, car-local

	bool TrainAtStop(const AP9Train* T, int32 Index)
	{
		if (!T || !T->StopDistances.IsValidIndex(Index) || T->IsMoving() || T->GetRouteLength() <= 0.0f)
		{
			return false;
		}
		float D = FMath::Fmod(T->StopDistances[Index], T->GetRouteLength());
		D = D < 0.0f ? D + T->GetRouteLength() : D;
		return FMath::Abs(T->GetRouteDistance() - D) < 5.0f;
	}

	template <typename C>
	C* Named(const AActor* Actor, const FString& Name)
	{
		TArray<C*> All;
		if (Actor) Actor->GetComponents<C>(All);
		for (C* Comp : All) if (Comp->GetName() == Name) return Comp;
		return nullptr;
	}

	bool InAny(const TArray<FBox>& Boxes, const FVector& P)
	{
		for (const FBox& B : Boxes) if (B.IsInsideOrOn(P)) return true;
		return false;
	}

	/** The car (0/1) of a train whose centre is nearest to a location. */
	int32 NearestCar(const AP9Train* T, const FVector& P)
	{
		const USceneComponent* C0 = Named<USceneComponent>(T, TEXT("Car0"));
		const USceneComponent* C1 = Named<USceneComponent>(T, TEXT("Car1"));
		if (!C0 || !C1) return 0;
		return FVector::DistSquared(C0->GetComponentLocation(), P) <= FVector::DistSquared(C1->GetComponentLocation(), P) ? 0 : 1;
	}
}

// =====================================================================================================================
// zones
// =====================================================================================================================
bool AP9CampaignDirector::IsInAnomalyZone(const FVector& Location) const
{
	return P9Story::InAny(AnomalyZones, Location) && !P9Story::InAny(NormalZones, Location);
}

bool AP9CampaignDirector::IsPointAllowed(const AP9AnomalySpawnPoint* Point, FString* Why) const
{
	if (!Point)
	{
		if (Why) *Why = TEXT("missing");
		return false;
	}
	const EP9AnomalyLocation T = Point->LocationType;
	const bool bTrain = T == EP9AnomalyLocation::TrainDestination || T == EP9AnomalyLocation::TrainSeats || T == EP9AnomalyLocation::TrainLights || T == EP9AnomalyLocation::TrainWindow;
	TArray<FVector> Checks = {Point->GetActorLocation()};
	if (!bTrain)                                           // a train moves; its points stand at its Platform 9 stop
	{
		if (Point->TargetActor) Checks.Add(Point->TargetActor->GetActorLocation());
		if (Point->SecondaryActor) Checks.Add(Point->SecondaryActor->GetActorLocation());
		for (const AActor* Extra : Point->ExtraActors) if (Extra) Checks.Add(Extra->GetActorLocation());
	}
	for (const FVector& P : Checks)
	{
		if (P9Story::InAny(NormalZones, P))
		{
			if (Why) *Why = FString::Printf(TEXT("inside a normal zone (Platforms 1-8) at %s"), *P.ToCompactString());
			return false;
		}
	}
	if (T == EP9AnomalyLocation::HornSource || T == EP9AnomalyLocation::DistantView)
	{
		return true;                                       // distant source / sight-line: only perceived from inside the zone, never on Platforms 1-8
	}
	for (const FVector& P : Checks)
	{
		if (!P9Story::InAny(AnomalyZones, P))
		{
			if (Why) *Why = FString::Printf(TEXT("outside the footbridge / Platform 9 anomaly zone at %s"), *P.ToCompactString());
			return false;
		}
	}
	return true;
}

// =====================================================================================================================
// shared helpers
// =====================================================================================================================
void AP9CampaignDirector::ShowHint(const FText& Text, float Duration)
{
	HintText = Text;
	HintTime = 0.0f;
	HintDuration = Duration;
}

bool AP9CampaignDirector::SitPlayerInTrain(AP9Train* InTrain, bool bPlatformSide)
{
	ACharacter* C = PlayerCharacter();
	if (!InTrain || !C)
	{
		return false;
	}
	if (UP9SeatComponent* Old = UP9SeatComponent::GetSeatOf(C))
	{
		Old->Stand();
	}
	// a free window seat on the platform side, as near the middle of the train as possible
	const float Side = InTrain->PlatformSideWorldY >= 0 ? 1.0f : -1.0f;
	const FVector Want = InTrain->GetActorLocation() + FVector(-600.0f, Side * 140.0f, 0.0f);
	TArray<UP9SeatComponent*> Seats;
	InTrain->GetComponents<UP9SeatComponent>(Seats);
	UP9SeatComponent* Best = nullptr;
	for (UP9SeatComponent* S : Seats)
	{
		const bool bSide = (S->GetComponentLocation().Y - InTrain->GetActorLocation().Y) * Side > 100.0f;
		if (S->IsOccupied() || (bPlatformSide && !bSide)) continue;
		if (!Best || FVector::DistSquared(S->GetComponentLocation(), Want) < FVector::DistSquared(Best->GetComponentLocation(), Want)) Best = S;
	}
	if (!Best)
	{
		UE_LOG(LogP9, Warning, TEXT("Story: no free seat on %s"), *InTrain->GetName());
		return false;
	}
	C->TeleportTo(Best->GetComponentLocation() + FVector(0.0f, 0.0f, 60.0f), C->GetActorRotation(), false, true);
	const bool bSat = Best->Sit(C);
	if (APlayerController* PC = PlayerController())
	{
		// along the seat, turned towards the window as far as a seated head comfortably turns
		const float SeatYaw = Best->GetComponentRotation().Yaw;
		const float Out = Side > 0.0f ? 90.0f : -90.0f;
		const float Turn = FMath::Clamp(FRotator::NormalizeAxis(Out - SeatYaw), -60.0f, 60.0f);
		PC->SetControlRotation(FRotator(-4.0f, SeatYaw + Turn, 0.0f));
	}
	UE_LOG(LogP9, Log, TEXT("Story: player seated in %s (%s) -> %s"), *InTrain->GetName(), *Best->GetName(), bSat ? TEXT("ok") : TEXT("FAILED"));
	return bSat;
}

void AP9CampaignDirector::SetOpeningTrainShown(bool bShown)
{
	if (OpeningTrain && OpeningTrain->IsHidden() == bShown)
	{
		OpeningTrain->SetActorHiddenInGame(!bShown);
		OpeningTrain->SetActorEnableCollision(bShown);
		UE_LOG(LogP9, Log, TEXT("Story: Dadar local %s"), bShown ? TEXT("shown") : TEXT("hidden (resumed game - the opening is over)"));
	}
}

void AP9CampaignDirector::HideConnectingAtHold()
{
	if (Train && P9Story::TrainAtStop(Train, ConnectingHoldStop))
	{
		Train->SetActorHiddenInGame(true);
	}
}

void AP9CampaignDirector::ResetConnectingTrainForNewDay(bool bPlayerMoved)
{
	if (!Train)
	{
		return;
	}
	const bool bIdleAtHold = P9Story::TrainAtStop(Train, ConnectingHoldStop) && Train->GetDoorState() == EP9DoorState::Closed
		&& Train->GetTrainState() == EP9TrainState::StoppedAtPlatform;
	if (!bIdleAtHold && Train->IsPlayerAboard() && !bPlayerMoved)
	{
		// never move a train out from under the player (only reachable if a day starts without the teleport)
		UE_LOG(LogP9, Warning, TEXT("Story: Vashi local NOT reset for the new day - the player is aboard"));
		return;
	}
	if (!bIdleAtHold)
	{
		Train->ResetToStop(ConnectingHoldStop);
	}
	if (bConnectingFast || SavedMaxSpeed > 0.0f)
	{
		bConnectingFast = true;                                     // force a restore of the saved service speeds
		SetConnectingFast(false);
	}
	Train->SetActorHiddenInGame(true);                               // waits out of sight; shown again only when called in
	const bool bHadState = bConnectingCalled || bConnectingDepartRequested || ConnectingOpenAt > 0.0f;
	bConnectingCalled = false;
	bConnectingDepartRequested = false;
	ConnectingOpenAt = -1.0f;
	ConnectingDwell = 0.0f;
	ConnectingAboardTime = 0.0f;
	UE_LOG(LogP9, Log, TEXT("Story: new day - Vashi local %s at its hold stop (hidden, idle%s)"),
		bIdleAtHold ? TEXT("already waiting") : TEXT("RESET"), bHadState ? TEXT(", call-in state cleared") : TEXT(""));
}

void AP9CampaignDirector::PlaceChaiwala(const TArray<FVector>& Path, float Dist, float Yaw, float Pitch)
{
	if (!Chaiwala || Path.Num() == 0)
	{
		return;
	}
	// position along the polyline (train-local), plus a light gait: two small rises and a sway per stride (0.7 m)
	FVector P = Path.Last();
	float Left = Dist;
	for (int32 i = 1; i < Path.Num(); ++i)
	{
		const float Seg = FVector::Dist(Path[i - 1], Path[i]);
		if (Left <= Seg) { P = FMath::Lerp(Path[i - 1], Path[i], Seg > 0.0f ? Left / Seg : 1.0f); break; }
		Left -= Seg;
	}
	const float Stride = Dist / 70.0f * PI;
	const float Moving = Dist > 0.0f ? 1.0f : 0.0f;
	P.Z += FMath::Abs(FMath::Sin(Stride)) * 2.5f * Moving;
	Chaiwala->GetRootComponent()->SetRelativeLocationAndRotation(P, FRotator(Pitch, Yaw, FMath::Sin(Stride) * 1.5f * Moving));
}

void AP9CampaignDirector::SaveEpilogueStage(int32 Stage)
{
	EpilogueStage = Stage;
	SaveProgress();
}

// =====================================================================================================================
// opening: Dadar -> Thane, arrival at Platform 1
// =====================================================================================================================
void AP9CampaignDirector::StartOpening()
{
	++DayToken;
	CleanupDay();
	Phase = EP9DayPhase::Opening;
	bDecisionLocked = true;
	bOpeningDone = false;
	StoryTime = 0.0f;
	StoryCue = 0;
	OpeningDoorsAt = -1.0f;
	bOpeningDoorsClosed = false;
	SetOpeningTrainShown(true);
	SitPlayerInTrain(OpeningTrain, true);
	FadeAlpha = 1.0f;
	FadeTarget = 1.0f;
	ShowTitle(NSLOCTEXT("P9", "OpeningTitle", "DADAR  →  THANE"),
		NSLOCTEXT("P9", "OpeningSub", "23:41  ·  slow local\nAnother long day at the office. Almost home."), 7.0f);
	if (USoundBase* Run = TrainRunAmbience.LoadSynchronous())
	{
		PlaySound2D(Run, 0.55f, 0.95f);
		AmbienceAudio = CampaignSounds.Num() ? CampaignSounds.Last() : nullptr;
	}
	UE_LOG(LogP9, Log, TEXT("Story: OPENING - %s -> %s, arriving at Platform %d (train %s)"), *Story.OriginStation, *Story.ArrivalStation, Story.ArrivalPlatform,
		OpeningTrain ? *OpeningTrain->GetName() : TEXT("missing"));
}

void AP9CampaignDirector::TickOpening(float DeltaSeconds)
{
	StoryTime += DeltaSeconds;
	if (StoryCue == 0 && StoryTime >= 1.5f)
	{
		++StoryCue;
		const bool bAtApproach = P9Story::TrainAtStop(OpeningTrain, 0);
		if (bAtApproach && OpeningTrain->RequestDeparture())
		{
			UE_LOG(LogP9, Log, TEXT("Story: the Dadar local pulls into Thane"));
		}
		else
		{
			UE_LOG(LogP9, Log, TEXT("Story: the Dadar local is already at Platform 1 - arrival without the run-in"));
			HandleOpeningArrived(OpeningArrivalStop);
		}
	}
	if (StoryCue == 1 && StoryTime >= 6.0f)
	{
		++StoryCue;
		FadeTarget = 0.0f;
		FadeSpeed = 1.0f / 2.5f;
	}
	if (OpeningDoorsAt > 0.0f && WorldTimeNow >= OpeningDoorsAt)
	{
		OpeningDoorsAt = -1.0f;
		if (OpeningTrain) OpeningTrain->RequestOpenDoors();
		ShowHint(NSLOCTEXT("P9", "ChangeHint", "Change here for the Vashi local · Platform 9\nYou get off at Airoli."), 9.0f);
	}
	// once the player has left it, the Dadar local shuts its doors and stays at Platform 1
	if (OpeningTrain && !bOpeningDoorsClosed && OpeningTrain->GetDoorState() == EP9DoorState::Open && !OpeningTrain->IsPlayerAboard()
		&& PlayerCharacter() && FVector::Dist2D(PlayerCharacter()->GetActorLocation(), OpeningTrain->GetActorLocation()) > 1500.0f)
	{
		bOpeningDoorsClosed = OpeningTrain->RequestCloseDoors();
	}
	// the campaign starts on the footbridge
	if (StoryCue >= 2 && PlayerCharacter() && !OpeningTrain->IsPlayerAboard() && CampaignStartZone.IsInsideOrOn(PlayerFeet()))
	{
		UE_LOG(LogP9, Log, TEXT("Story: player reached the footbridge - Day 1 begins"));
		bOpeningDone = true;
		StartDay(false);
	}
}

void AP9CampaignDirector::HandleOpeningArrived(int32 StopIndex)
{
	if (StopIndex != OpeningArrivalStop || (Phase != EP9DayPhase::Opening && Phase != EP9DayPhase::Epilogue))
	{
		return;
	}
	if (IsValid(AmbienceAudio))
	{
		AmbienceAudio->FadeOut(2.5f, 0.0f);
	}
	const bool bOpening = Phase == EP9DayPhase::Opening;
	if (Announcer)
	{
		Announcer->Play(TEXT("PA_PF1_ThaneArrival"), bOpening ? TEXT("PF1-arrival-opening") : TEXT("PF1-arrival-epilogue"));
	}
	if (bOpening)
	{
		OpeningDoorsAt = WorldTimeNow + 3.0f;
	}
	UE_LOG(LogP9, Log, TEXT("Story: Dadar local at Thane Platform %d (%s)"), Story.ArrivalPlatform, bOpening ? TEXT("opening") : TEXT("epilogue"));
}

// =====================================================================================================================
// the connecting Vashi local (Platform 9)
// =====================================================================================================================
void AP9CampaignDirector::HandleTrainArrived(int32 StopIndex)
{
	if (StopIndex == ConnectingPlatformStop)
	{
		ConnectingOpenAt = WorldTimeNow + 1.5f;
		ConnectingDwell = 0.0f;
		ConnectingAboardTime = 0.0f;
		UE_LOG(LogP9, Log, TEXT("Story: Vashi local arrived at Platform %d"), Story.ConnectingPlatform);
	}
	else if (StopIndex == ConnectingHoldStop)
	{
		bConnectingFast = true;                                     // force a restore even if only the top speed was lifted
		SetConnectingFast(false);
		if (Train) Train->SetActorHiddenInGame(true);                // waits out of sight; shown again only when called in
		UE_LOG(LogP9, Log, TEXT("Story: Vashi local waiting (hidden) at its hold stop"));
	}
}

void AP9CampaignDirector::SetConnectingFast(bool bFast)
{
	if (!Train || bFast == bConnectingFast)
	{
		return;
	}
	if (bFast)
	{
		if (SavedMaxSpeed <= 0.0f)                                  // speeds may already be saved by the early top-speed raise
		{
			SavedMaxSpeed = Train->MaxSpeed;
			SavedAccel = Train->Acceleration;
			SavedDecel = Train->ServiceDeceleration;
		}
		Train->MaxSpeed = ConnectingFastSpeed;
		Train->Acceleration = ConnectingFastAccel;
		Train->ServiceDeceleration = ConnectingFastAccel;
		Train->SetActorHiddenInGame(true);
	}
	else
	{
		if (SavedMaxSpeed > 0.0f)
		{
			Train->MaxSpeed = SavedMaxSpeed;
			Train->Acceleration = SavedAccel;
			Train->ServiceDeceleration = SavedDecel;
			SavedMaxSpeed = 0.0f;
		}
		Train->SetActorHiddenInGame(false);
	}
	bConnectingFast = bFast;
	UE_LOG(LogP9, Log, TEXT("Story: Vashi local %s"), bFast ? TEXT("out of sight - fast, hidden return to the hold stop") : TEXT("back to service speed"));
}

void AP9CampaignDirector::TickConnectingTrain(float DeltaSeconds)
{
	if (!Train || Phase == EP9DayPhase::Inactive)
	{
		return;
	}
	const bool bCampaign = Phase == EP9DayPhase::Exploring && !bDecisionLocked;
	const bool bEpilogueWalk = Phase == EP9DayPhase::Epilogue && EpilogueStep == 1;
	ACharacter* Player = PlayerCharacter();

	// 1) call it in from the hold stop when the player approaches Platform 9
	if (!bConnectingCalled && (bCampaign || bEpilogueWalk) && Player && Platform9ApproachZone.IsInsideOrOn(PlayerFeet())
		&& P9Story::TrainAtStop(Train, ConnectingHoldStop) && Train->GetDoorState() == EP9DoorState::Closed)
	{
		bConnectingCalled = true;
		++ConnectingCalls;
		const FString Key = FString::Printf(TEXT("PF9-arrival-%d"), ConnectingCalls);
		bool bAnomalous = false;
		if (bCampaign && Announcer)
		{
			for (const FP9ActiveAnomaly& A : ActiveAnomalies)
			{
				const FP9AnomalyDef& Def = Anomalies[A.Plan.DefIndex];
				if (Def.Handler == EP9AnomalyHandler::PAAnnouncement)
				{
					const FP9AnomalyVariant& V = Def.Variants[A.Plan.VariantIndex];
					bAnomalous = Announcer->PlaySound(V.Sound.LoadSynchronous(), Story.ConnectingPlatform, FName(*(TEXT("PA_PF9_Anom_") + V.Id.ToString())), Key,
						0.8f, V.Param > 0.0f ? V.Param : 1.0f, false);
				}
			}
		}
		if (!bAnomalous && Announcer)
		{
			Announcer->Play(TEXT("PA_PF9_VashiArrival"), Key);
		}
		Train->RequestDeparture();
		UE_LOG(LogP9, Log, TEXT("Story: Vashi local called in (call %d, %s announcement)"), ConnectingCalls, bAnomalous ? TEXT("anomalous") : TEXT("normal"));
	}
	// called in: it becomes visible once it runs onto track 9 (already moving, well short of the platform)
	if (bConnectingCalled && Train->IsHidden() && Train->IsMoving() && Train->GetRouteLength() > 0.0f)
	{
		const float Platform = Train->StopDistances.IsValidIndex(ConnectingPlatformStop) ? Train->StopDistances[ConnectingPlatformStop] : 0.0f;
		const float Along = FMath::Fmod(Train->GetRouteDistance(), Train->GetRouteLength());
		if (Along >= ConnectingVisibleFrom && Along <= Platform)
		{
			Train->SetActorHiddenInGame(false);
			UE_LOG(LogP9, Log, TEXT("Story: Vashi local comes into view (%.0f m before the platform stop)"), (Platform - Along) / 100.0f);
		}
	}

	// 2) at Platform 9: open, wait for the player, then go
	if (ConnectingOpenAt > 0.0f && WorldTimeNow >= ConnectingOpenAt)
	{
		ConnectingOpenAt = -1.0f;
		Train->RequestOpenDoors();
	}
	if (P9Story::TrainAtStop(Train, ConnectingPlatformStop) && Train->GetDoorState() == EP9DoorState::Open && !bConnectingDepartRequested)
	{
		ConnectingDwell += DeltaSeconds;
		const bool bAboard = Train->IsPlayerAboard();
		ConnectingAboardTime = bAboard ? ConnectingAboardTime + DeltaSeconds : 0.0f;
		if (bAboard && ConnectingAboardTime >= BoardedDepartDelay && (bCampaign || bEpilogueWalk))
		{
			bConnectingDepartRequested = Train->RequestDeparture();
			UE_LOG(LogP9, Log, TEXT("Story: player aboard the Vashi local - doors closing"));
		}
		else if (!bAboard && Phase != EP9DayPhase::Epilogue && ConnectingMaxDwell > 0.0f && ConnectingDwell >= ConnectingMaxDwell)
		{
			bConnectingDepartRequested = Train->RequestDeparture();
			UE_LOG(LogP9, Log, TEXT("Story: Vashi local leaves without the player after %.0fs"), ConnectingDwell);
		}
	}

	// 3) out of sight and empty: run back fast and hidden; show it again shortly before the hold stop
	if (Train->IsMoving() && !Train->IsPlayerAboard())
	{
		const float D = Train->GetRouteDistance();
		// the train only gains speed while it is still accelerating away, so lift its top speed right after it leaves Platform 9
		// (normal acceleration: what the player can still see looks unchanged); out of sight it is hidden and accelerates hard
		const float Platform = Train->StopDistances.IsValidIndex(ConnectingPlatformStop) ? Train->StopDistances[ConnectingPlatformStop] : 0.0f;
		const float Hold = Train->StopDistances.IsValidIndex(ConnectingHoldStop) ? Train->StopDistances[ConnectingHoldStop] : 0.0f;
		const float HoldEnd = Hold > Platform ? Hold : Hold + Train->GetRouteLength();   // unwrapped distance of the hold stop after Platform 9
		if (SavedMaxSpeed <= 0.0f && !bConnectingFast && D > Platform + 50.0f && D < HoldEnd - ConnectingShowBeforeHold
			&& Train->GetTrainState() == EP9TrainState::Accelerating)
		{
			SavedMaxSpeed = Train->MaxSpeed;
			SavedAccel = Train->Acceleration;
			SavedDecel = Train->ServiceDeceleration;
			Train->MaxSpeed = ConnectingFastSpeed;
		}
		if (!bConnectingFast && D > ConnectingFastFromDistance && D < HoldEnd - ConnectingShowBeforeHold)
		{
			SetConnectingFast(true);                                  // stays hidden until the next call-in
		}
	}
}

// =====================================================================================================================
// epilogue: the chaiwala, the walk to Platform 9 and the last ride
// =====================================================================================================================
void AP9CampaignDirector::StartEpilogue()
{
	++DayToken;
	CleanupDay();
	if (Announcer) Announcer->StopAll(0.5f);
	Phase = EP9DayPhase::Epilogue;
	Status = EP9CampaignStatus::Completed;
	bDecisionLocked = true;
	bFinalEnding = false;
	DayLabelOverride = FText::GetEmpty();
	EpilogueStep = 0;
	StoryTime = 0.0f;
	StoryCue = 0;
	RideTime = -1.0f;
	OpeningDoorsAt = -1.0f;
	SetEndingInput(false);
	SaveEpilogueStage(1);
	FadeAlpha = 1.0f;
	FadeTarget = 1.0f;
	ShowTitle(FText::GetEmpty(), FText::GetEmpty(), 0.1f);

	const bool bArrived = P9Story::TrainAtStop(OpeningTrain, OpeningArrivalStop);
	SetOpeningTrainShown(true);
	SitPlayerInTrain(OpeningTrain, true);
	if (IsValid(Chaiwala)) Chaiwala->Destroy();                     // never two (a repeated start)
	Chaiwala = nullptr;
	if (IsValid(EpilogueScare)) EpilogueScare->Destroy();
	EpilogueScare = nullptr;
	if (OpeningTrain && !bArrived && P9Story::TrainAtStop(OpeningTrain, 0))
	{
		OpeningTrain->RequestDeparture();                          // still pulling in as the player wakes
	}
	// the chaiwala walks up the aisle to the player (all of his positions are train-local: he rides with a train still pulling in)
	ACharacter* C = PlayerCharacter();
	UStaticMesh* Mesh = ChaiwalaMesh.LoadSynchronous();
	if (OpeningTrain && C && Mesh)
	{
		const FTransform TrainXf = OpeningTrain->GetActorTransform();
		const FVector Seat = TrainXf.InverseTransformPosition(C->GetActorLocation());
		const float CarCentre = Seat.X < 0.0f ? -1066.5f : 1066.5f;
		const float Dir = CarCentre < Seat.X ? -1.0f : 1.0f;            // he comes from the middle of the carriage
		const float Side = OpeningTrain->PlatformSideWorldY >= 0 ? 1.0f : -1.0f;
		const FVector Stand(Seat.X + Dir * 80.0f, 0.0f, P9Story::TrainFloor);
		const FVector Start(Seat.X + Dir * 330.0f, 0.0f, P9Story::TrainFloor);
		float DoorLX = CarCentre;
		for (float DX : P9Story::DoorX)
		{
			if (FMath::Abs(CarCentre + DX - Stand.X) < FMath::Abs(DoorLX - Stand.X)) DoorLX = CarCentre + DX;
		}
		ChaiwalaIn = {Start, Stand};
		ChaiwalaOut = {Stand, FVector(DoorLX, 0.0f, P9Story::TrainFloor), FVector(DoorLX, Side * 125.0f, P9Story::TrainFloor), FVector(DoorLX, Side * 330.0f, 75.0f)};
		ChaiwalaFaceYaw = (Seat - Stand).GetSafeNormal2D().Rotation().Yaw;
		const FVector Loc = TrainXf.TransformPosition(Start);
		const float Yaw = TrainXf.GetRotation().Rotator().Yaw + (Stand - Start).Rotation().Yaw;
		const FTransform Xf(FRotator(0.0f, Yaw, 0.0f), Loc);
		AStaticMeshActor* A = GetWorld()->SpawnActorDeferred<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Xf, this);
		if (A)
		{
			UStaticMeshComponent* SM = A->GetStaticMeshComponent();
			SM->SetMobility(EComponentMobility::Movable);
			SM->SetStaticMesh(Mesh);
			SM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			A->Tags.Add(TEXT("P9Anomaly"));
			A->FinishSpawning(Xf);
			A->AttachToActor(OpeningTrain, FAttachmentTransformRules::KeepWorldTransform);
			RecordSpawned(A);
			Chaiwala = A;
			PlaceChaiwala(ChaiwalaIn, 0.0f, (Stand - Start).Rotation().Yaw, 0.0f);
			// Stage 5C: the station's own (animated) chaiwala plays the scene; the static one stays as an invisible path
			// proxy (positions, line sources). Without a vendor NPC the static chaiwala is shown as before.
			for (TActorIterator<AP9FriendNPC> It(GetWorld()); It; ++It)
			{
				if (It->CanPlayEpilogueWake())
				{
					A->SetActorHiddenInGame(true);
					It->BeginEpilogueWake(A);
					break;
				}
			}
			if (APlayerController* PC = PlayerController())              // wake up looking at where he stops
			{
				PC->SetControlRotation((TrainXf.TransformPosition(Stand) + FVector(0.0f, 0.0f, 150.0f) - C->GetPawnViewLocation()).Rotation());
			}
		}
	}
	UE_LOG(LogP9, Log, TEXT("Story: EPILOGUE - the chaiwala wakes the player (Dadar local %s, chaiwala %s); campaign stays completed"),
		bArrived ? TEXT("at Platform 1") : TEXT("pulling in"), Chaiwala ? TEXT("spawned") : TEXT("MISSING"));
}

void AP9CampaignDirector::DebugStartEpilogue()
{
	Day = NumDays;
	Status = EP9CampaignStatus::Completed;
	StartEpilogue();
}

void AP9CampaignDirector::DebugStartOpening()
{
	StartOpening();
}

void AP9CampaignDirector::TickEpilogue(float DeltaSeconds)
{
	StoryTime += DeltaSeconds;
	ACharacter* Player = PlayerCharacter();
	// Stage 5C: runs in every epilogue step - when step 0 ended at 15 s the chaiwala used to freeze in the doorway
	// the chaiwala: walks in (0.2-3.0 s), turns to the player, leans in while he speaks, then walks out through the door
	if (Chaiwala && ChaiwalaIn.Num() == 2)
	{
		const float InLen = FVector::Dist(ChaiwalaIn[0], ChaiwalaIn[1]);
		const float WalkYaw = (ChaiwalaIn[1] - ChaiwalaIn[0]).Rotation().Yaw;
		if (StoryTime < 12.0f)
		{
			const float A = FMath::Clamp((StoryTime - 0.2f) / 2.8f, 0.0f, 1.0f);
			const float Turn = FMath::Clamp((StoryTime - 3.0f) / 0.5f, 0.0f, 1.0f);
			const float Yaw = FMath::Lerp(WalkYaw, WalkYaw + FRotator::NormalizeAxis(ChaiwalaFaceYaw - WalkYaw), FMath::SmoothStep(0.0f, 1.0f, Turn));
			const float Lean = StoryTime > 3.4f && StoryTime < 5.2f ? -9.0f * FMath::Sin(PI * (StoryTime - 3.4f) / 1.8f) : 0.0f;
			PlaceChaiwala(ChaiwalaIn, A < 1.0f ? FMath::SmoothStep(0.0f, 1.0f, A) * InLen : InLen, Yaw, Lean);
		}
		else if (ChaiwalaOut.Num() > 1)
		{
			float OutLen = 0.0f;
			for (int32 i = 1; i < ChaiwalaOut.Num(); ++i) OutLen += FVector::Dist(ChaiwalaOut[i - 1], ChaiwalaOut[i]);
			const float Turn = FMath::Clamp((StoryTime - 12.0f) / 0.6f, 0.0f, 1.0f);
			const float A = FMath::Clamp((StoryTime - 12.5f) / (OutLen / 95.0f), 0.0f, 1.0f);
			const float Dist = FMath::SmoothStep(0.0f, 1.0f, A) * OutLen;
			// heading: along the path segment he is on (turning first towards the aisle)
			float Left = Dist; FVector Dir = ChaiwalaOut[1] - ChaiwalaOut[0];
			for (int32 i = 1; i < ChaiwalaOut.Num(); ++i)
			{
				const FVector Seg = ChaiwalaOut[i] - ChaiwalaOut[i - 1];
				if (Left <= Seg.Size() || i == ChaiwalaOut.Num() - 1) { Dir = Seg; break; }
				Left -= Seg.Size();
			}
			const float Target = Dir.IsNearlyZero() ? ChaiwalaFaceYaw : Dir.Rotation().Yaw;
			const float Yaw = A <= 0.0f ? FMath::Lerp(ChaiwalaFaceYaw, ChaiwalaFaceYaw + FRotator::NormalizeAxis(Target - ChaiwalaFaceYaw), FMath::SmoothStep(0.0f, 1.0f, Turn)) : Target;
			if (A < 1.0f) PlaceChaiwala(ChaiwalaOut, Dist, Yaw, 0.0f);
			else
			{
				// Stage 5C: out of the door - the vendor walks back to his stall (ignored unless he is playing the scene)
				for (TActorIterator<AP9FriendNPC> It(GetWorld()); It; ++It) It->EndEpilogueWake();
			}
		}
	}
	if (EpilogueStep == 0)
	{
		// 0) wake-up in the Dadar local at Platform 1
		if (StoryCue == 0 && StoryTime >= 3.0f)
		{
			++StoryCue;
			if (Chaiwala) PlaySoundAt(ChaiwalaWakeLine.LoadSynchronous(), Chaiwala->GetActorLocation() + FVector(0.0f, 0.0f, 160.0f), 1.0f, 1.0f, 2000.0f);
			ShowTitle(FText::GetEmpty(), NSLOCTEXT("P9", "ChaiwalaLine",
				"“Saheb, utha! Thane aala. Kuthe jaycha aahe tumhala?”\n(Sir, wake up! We've reached Thane. Where do you need to go?)"), 7.5f);
		}
		if (StoryCue == 1 && StoryTime >= 2.0f)
		{
			++StoryCue;
			FadeTarget = 0.0f;
			FadeSpeed = 1.0f / 4.0f;                                  // slow, dreamlike return
		}
		if (StoryCue == 2 && StoryTime >= 9.5f)
		{
			++StoryCue;
			ShowTitle(NSLOCTEXT("P9", "Dream", "It was only a dream."), FText::GetEmpty(), 4.5f);
			if (P9Story::TrainAtStop(OpeningTrain, OpeningArrivalStop))
			{
				HandleOpeningArrived(OpeningArrivalStop);              // announcement (once per event key)
			}
		}
		if (StoryCue == 3 && StoryTime >= 11.0f && P9Story::TrainAtStop(OpeningTrain, OpeningArrivalStop))
		{
			++StoryCue;
			OpeningTrain->RequestOpenDoors();
		}
		if (StoryCue == 4 && StoryTime >= 13.0f)
		{
			++StoryCue;
			if (Chaiwala) PlaySoundAt(ChaiwalaCallLine.LoadSynchronous(), Chaiwala->GetActorLocation() + FVector(0.0f, 0.0f, 160.0f), 1.0f, 1.0f, 2500.0f);
		}
		if (StoryCue == 5 && StoryTime >= 15.0f)
		{
			++StoryCue;
			EpilogueStep = 1;
			ShowHint(NSLOCTEXT("P9", "EpilogueHint", "Platform 9 · Vashi local\nYou get off at Airoli."), 10.0f);
			UE_LOG(LogP9, Log, TEXT("Story: epilogue - walk to Platform %d"), Story.ConnectingPlatform);
		}
	}
	// the Dadar local shuts its doors once the player has gone
	if (EpilogueStep >= 1 && OpeningTrain && OpeningTrain->GetDoorState() == EP9DoorState::Open && !OpeningTrain->IsPlayerAboard()
		&& Player && FVector::Dist2D(Player->GetActorLocation(), OpeningTrain->GetActorLocation()) > 1500.0f)
	{
		OpeningTrain->RequestCloseDoors();
	}
	if (EpilogueStep != 2)
	{
		return;
	}
	// 2) the last ride (time since the Vashi local left Platform 9)
	RideTime += DeltaSeconds;
	const int32 CarIdx = Player ? P9Story::NearestCar(Train, Player->GetActorLocation()) : 1;
	if (StoryCue == 0 && RideTime >= 21.0f)
	{
		++StoryCue;
		if (Announcer) Announcer->Play(TEXT("PA_PF9_Epilogue_NoAiroli"), TEXT("epilogue-no-airoli"), 1.0f, true);   // one word changed
	}
	if (StoryCue == 1 && RideTime >= 25.0f)
	{
		++StoryCue;
		for (int32 L = 0; L < 2; ++L)
		{
			if (ULightComponent* Light = P9Story::Named<ULightComponent>(Train, FString::Printf(TEXT("Car%d_InteriorLight%d"), CarIdx, L)))
			{
				RecordLight(Light);
				EpilogueLights.Add(Light);
			}
		}
		// a passenger at the far end of the carriage - there only while you do not look straight at it
		const USceneComponent* CarRoot = P9Story::Named<USceneComponent>(Train, FString::Printf(TEXT("Car%d"), CarIdx));
		if (CarRoot && Player)
		{
			const FVector Local = CarRoot->GetComponentTransform().InverseTransformPosition(Player->GetActorLocation());
			const FVector End = CarRoot->GetComponentTransform().TransformPosition(FVector(Local.X > 0.0f ? -800.0f : 800.0f, 0.0f, P9Story::TrainFloor));
			EpilogueFigure = SpawnFigure(End, (Player->GetActorLocation() - End).GetSafeNormal2D().Rotation().Yaw);
			if (EpilogueFigure) EpilogueFigure->AttachToComponent(Train->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
		}
		UE_LOG(LogP9, Log, TEXT("Story: epilogue - lights failing, a passenger at the end of car %d"), CarIdx);
	}
	if (StoryCue >= 2 && StoryCue < 3)
	{
		const float N = FMath::PerlinNoise1D(RideTime * 5.5f);
		for (ULightComponent* L : EpilogueLights) if (L) L->SetIntensity(N > -0.1f ? 90.0f : 4.0f);
		if (EpilogueFigure)
		{
			const FVector Head = EpilogueFigure->GetActorLocation() + FVector(0.0f, 0.0f, 165.0f);
			EpilogueFigure->SetActorHiddenInGame(IsInView(Head, 0.94f, 4000.0f, EpilogueFigure, false));
		}
	}
	const USceneComponent* ScareRoot = P9Story::Named<USceneComponent>(Train, FString::Printf(TEXT("Car%d"), ScareCar));
	if (StoryCue == 2 && RideTime >= 33.0f)
	{
		++StoryCue;                                                 // the carriage goes dark; the passenger is gone
		for (ULightComponent* L : EpilogueLights) if (L) L->SetIntensity(0.0f);
		if (EpilogueFigure) EpilogueFigure->SetActorHiddenInGame(true);
		// authored spot: in the aisle of the player's own carriage, 1.4 m from the player towards the middle of the car
		ScareCar = CarIdx;
		ScareRoot = P9Story::Named<USceneComponent>(Train, FString::Printf(TEXT("Car%d"), ScareCar));
		const FVector Local = ScareRoot && Player ? ScareRoot->GetComponentTransform().InverseTransformPosition(Player->GetActorLocation()) : FVector::ZeroVector;
		ScareSpotLocal = FVector(FMath::Clamp(Local.X + (Local.X > 0.0f ? -140.0f : 140.0f), -850.0f, 850.0f), 0.0f, P9Story::TrainFloor);
		if (APlayerController* PC = PlayerController()) ScareCamFrom = PC->GetControlRotation();
	}
	if (StoryCue == 3 && ScareRoot && Player)
	{
		// while it is dark the view settles on that spot (0.9 s), so the figure cannot appear off-screen or behind a seat
		const FVector Head = ScareRoot->GetComponentTransform().TransformPosition(ScareSpotLocal) + FVector(0.0f, 0.0f, 160.0f);
		const FRotator To = (Head - Player->GetPawnViewLocation()).Rotation();
		const float A = FMath::SmoothStep(0.0f, 1.0f, FMath::Clamp((RideTime - 33.0f) / 0.9f, 0.0f, 1.0f));
		if (APlayerController* PC = PlayerController()) PC->SetControlRotation(FQuat::Slerp(ScareCamFrom.Quaternion(), To.Quaternion(), A).Rotator());
	}
	if (StoryCue == 3 && RideTime >= 34.2f)
	{
		++StoryCue;                                                 // lights snap back on - it is standing in the aisle
		for (ULightComponent* L : EpilogueLights) if (L) L->SetIntensity(160.0f);
		if (ScareRoot && Player)
		{
			const FVector Spot = ScareRoot->GetComponentTransform().TransformPosition(ScareSpotLocal);
			EpilogueScare = SpawnFigure(Spot, (Player->GetActorLocation() - Spot).GetSafeNormal2D().Rotation().Yaw);
			if (EpilogueScare) EpilogueScare->AttachToComponent(Train->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
		}
		PlaySound2D(ApparitionSting.LoadSynchronous(), 1.0f, 1.0f);
		FlashAlpha = 0.25f;                                         // a short dim pulse, not a white flash
		ShakeTime = 0.3f;
		SaveEpilogueStage(2);                                       // reached: a resume shows the final screen, never this again
		UE_LOG(LogP9, Log, TEXT("Story: FINAL APPARITION in car %d aisle (car-local x %.0f)"), ScareCar, ScareSpotLocal.X);
	}
	if (StoryCue == 4 && RideTime >= 35.4f)
	{
		++StoryCue;                                                 // 1.2 s readable beat, then blackout
		for (ULightComponent* L : EpilogueLights) if (L) L->SetIntensity(0.0f);
		if (EpilogueScare) EpilogueScare->SetActorHiddenInGame(true);
		FadeTarget = 1.0f;
		FadeSpeed = 4.0f;
	}
	if (StoryCue == 5 && RideTime >= 37.5f)
	{
		++StoryCue;
		DayLabelOverride = NSLOCTEXT("P9", "Day11", "DAY 11 / 10");
		UE_LOG(LogP9, Log, TEXT("Story: epilogue finished - final screen"));
		EnterEnding();
	}
}

// =====================================================================================================================
// story checks
// =====================================================================================================================
int32 AP9CampaignDirector::ValidateStory() const
{
	int32 Problems = 0;
	auto Check = [&Problems](bool bOk, const FString& What)
	{
		if (!bOk) ++Problems;
		UE_LOG(LogP9, Log, TEXT("Story check: %s - %s"), bOk ? TEXT("ok  ") : TEXT("FAIL"), *What);
	};
	Check(OpeningTrain != nullptr, TEXT("Dadar -> Thane opening train assigned"));
	if (OpeningTrain)
	{
		Check(OpeningTrain->DestinationText.Contains(Story.OpeningTrainBoard), FString::Printf(TEXT("opening train board '%s' shows %s"), *OpeningTrain->DestinationText, *Story.OpeningTrainBoard));
		Check(OpeningTrain->IsRouteClosed(), TEXT("opening train route valid (closed)"));
		Check(OpeningTrain->StopDistances.IsValidIndex(OpeningArrivalStop), TEXT("opening train has its Platform 1 stop"));
		Check(OpeningTrain->PlatformSideWorldY < 0, TEXT("opening train opens towards Platform 1 (-Y)"));
	}
	Check(Train != nullptr, TEXT("connecting Vashi local assigned"));
	if (Train)
	{
		Check(Train->DestinationText.Contains(Story.ConnectingTrainBoard), FString::Printf(TEXT("connecting train board '%s' shows %s"), *Train->DestinationText, *Story.ConnectingTrainBoard));
		Check(Train->IsRouteClosed(), TEXT("connecting train route valid (closed)"));
		Check(Train->StopDistances.IsValidIndex(ConnectingPlatformStop) && Train->StopDistances.IsValidIndex(ConnectingHoldStop), TEXT("connecting train has Platform 9 and hold stops"));
		Check(Train->PlatformSideWorldY > 0, TEXT("connecting train opens towards Platform 9 (+Y)"));
		FVector P; float Yaw;
		if (Train->StopDistances.IsValidIndex(ConnectingPlatformStop))
		{
			Train->EvaluateRoute(Train->StopDistances[ConnectingPlatformStop], P, Yaw);
			Check(P9Story::InAny(AnomalyZones, P), FString::Printf(TEXT("Platform 9 stop %s lies on track 9 in the Platform 9 zone"), *P.ToCompactString()));
		}
	}
	Check(Announcer != nullptr, TEXT("station announcer assigned"));
	if (Announcer)
	{
		Check(Announcer->Validate(false) == 0, TEXT("announcements valid (sounds load, platforms allowed)"));
		Check(Announcer->AllowedPlatforms.Num() == 2 && Announcer->AllowedPlatforms.Contains(Story.ArrivalPlatform) && Announcer->AllowedPlatforms.Contains(Story.ConnectingPlatform),
			TEXT("train announcements limited to Platforms 1 and 9"));
		const FP9AnnouncementDef* A1 = Announcer->Find(TEXT("PA_PF1_ThaneArrival"));
		const FP9AnnouncementDef* A9 = Announcer->Find(TEXT("PA_PF9_VashiArrival"));
		const FP9AnnouncementDef* D9 = Announcer->Find(TEXT("PA_PF9_VashiDeparture"));
		Check(A1 && A1->Platform == Story.ArrivalPlatform, TEXT("Thane arrival announcement is a Platform 1 announcement"));
		Check(A9 && A9->Platform == Story.ConnectingPlatform && D9 && D9->Platform == Story.ConnectingPlatform, TEXT("Vashi arrival/departure are Platform 9 announcements"));
	}
	int32 Allowed = 0;
	for (const AP9AnomalySpawnPoint* SP : SpawnPoints)
	{
		FString Why;
		const bool bOk = IsPointAllowed(SP, &Why);
		Allowed += bOk ? 1 : 0;
		if (!bOk) Check(false, FString::Printf(TEXT("spawn point %s: %s"), SP ? *SP->PointId.ToString() : TEXT("null"), *Why));
	}
	Check(Allowed == SpawnPoints.Num(), FString::Printf(TEXT("%d/%d anomaly spawn points inside the footbridge / Platform 9 zone"), Allowed, SpawnPoints.Num()));
	for (const TPair<FString, FString>& P : Story.IndicatorTextures)
	{
		const UMaterialInterface* MI = LoadObject<UMaterialInterface>(nullptr, *P.Key);
		UTexture* Tex = nullptr;
		const bool bHas = MI && MI->GetTextureParameterValue(FMaterialParameterInfo(TEXT("PanelTexture")), Tex) && Tex;
		Check(bHas && Tex->GetName() == P.Value, FString::Printf(TEXT("%s shows %s (has %s)"), *FPaths::GetBaseFilename(P.Key), *P.Value, Tex ? *Tex->GetName() : TEXT("nothing")));
	}
	UE_LOG(LogP9, Log, TEXT("Story check: %d problems | facts: %s -> %s Platform %d; connection Platform %d to %s, getting off at %s"), Problems,
		*Story.OriginStation, *Story.ArrivalStation, Story.ArrivalPlatform, Story.ConnectingPlatform, *Story.ConnectingDestination, *Story.IntendedStop);
	return Problems;
}

FString AP9CampaignDirector::GetStoryStatus() const
{
	return FString::Printf(TEXT("phase=%d openingDone=%d epilogueStage=%d step=%d storyTime=%.1f ride=%.1f cue=%d | Vashi local: %s called=%d calls=%d fast=%d hidden=%d | Dadar local: %s | announcer: %s"),
		static_cast<int32>(Phase), bOpeningDone ? 1 : 0, EpilogueStage, EpilogueStep, StoryTime, RideTime, StoryCue,
		Train ? *Train->GetStatusString() : TEXT("-"), bConnectingCalled ? 1 : 0, ConnectingCalls, bConnectingFast ? 1 : 0, Train && Train->IsHidden() ? 1 : 0,
		OpeningTrain ? *OpeningTrain->GetStatusString() : TEXT("-"), Announcer && Announcer->IsPlaying() ? *Announcer->GetCurrentId().ToString() : TEXT("idle"));
}
