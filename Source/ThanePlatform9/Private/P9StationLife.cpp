#include "P9StationLife.h"

#include "P9FriendNPC.h"
#include "ThanePlatform9.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Sound/SoundBase.h"

namespace P9Life
{
	static FAutoConsoleCommandWithWorldAndArgs MeetCommand(
		TEXT("p9.Friends.Meet"),
		TEXT("p9.Friends.Meet [conversation id] - start a friends' meet-up now (test)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			for (TActorIterator<AP9StationLife> It(World); It; ++It)
			{
				const bool bOk = It->StartMeet(Args.Num() ? FName(*Args[0]) : NAME_None);
				UE_LOG(LogP9, Log, TEXT("p9.Friends.Meet: %s"), bOk ? TEXT("started") : TEXT("no conversation possible now"));
				It->LogStatus();
			}
		}));
}

AP9StationLife::AP9StationLife()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void AP9StationLife::BeginPlay()
{
	Super::BeginPlay();
	Rng.Initialize(Seed);
	State = EState::Waiting;
	Timer = FirstMeetDelay;
	UE_LOG(LogP9, Log, TEXT("StationLife: %d conversations, %d greetings, %d meet spots, %d friends"), Conversations.Num(), Greetings.Num(), MeetSpots.Num(), Friends().Num());
}

TArray<AP9FriendNPC*> AP9StationLife::Friends() const
{
	TArray<AP9FriendNPC*> Out;
	for (TActorIterator<AP9FriendNPC> It(GetWorld()); It; ++It)
	{
		if (!It->FriendKey.IsNone() && It->NPCRole != EP9NPCRole::Vendor && It->IsNPCAvailable()) Out.Add(*It);   // Stage 5B: hidden friends never meet
	}
	return Out;
}

AP9FriendNPC* AP9StationLife::FindFriend(FName Key) const
{
	for (AP9FriendNPC* F : Friends()) if (F->FriendKey == Key) return F;
	return nullptr;
}

bool AP9StationLife::StartMeet(FName ConversationId)
{
	if (State != EState::Waiting && ConversationId.IsNone()) return false;
	if (State != EState::Waiting) Finish();
	// candidate conversations: shuffled, not the last one again
	TArray<int32> Order;
	for (int32 i = 0; i < Conversations.Num(); ++i)
	{
		if ((ConversationId.IsNone() && Conversations[i].Id != LastConversation) || Conversations[i].Id == ConversationId) Order.Add(i);
	}
	for (int32 i = Order.Num() - 1; i > 0; --i) Order.Swap(i, Rng.RandRange(0, i));
	for (const int32 Ci : Order)
	{
		TArray<FName> Keys;
		for (const FP9NPCLine& L : Conversations[Ci].Lines) Keys.AddUnique(L.Speaker);
		TArray<AP9FriendNPC*> Who;
		for (const FName K : Keys)
		{
			AP9FriendNPC* F = FindFriend(K);
			if (!F || !F->IsAvailableForMeet()) { Who.Reset(); break; }
			Who.Add(F);
		}
		if (Who.Num() < 2) continue;
		for (const FP9NPCMeetSpot& Spot : MeetSpots)
		{
			bool bAll = true;
			for (const AP9FriendNPC* F : Who)
			{
				// reachable via its route, and close enough (walking distance along the route) to arrive well inside the gather time
				const float Eta = F->RouteDistanceTo(Spot.Center) / FMath::Max(50.0f, F->WalkSpeed);
				bAll &= F->RouteIndexNear(Spot.Center, Spot.ReachRadius) != INDEX_NONE && Eta < GatherTimeout * 0.7f;
			}
			if (!bAll) continue;
			ConvIndex = Ci; bGreetingOnly = false; Center = Spot.Center; LineIndex = 0;
			Participants.Reset();
			const float A0 = Rng.FRandRange(0.0f, 360.0f);
			for (int32 i = 0; i < Who.Num(); ++i)
			{
				const float A = FMath::DegreesToRadians(A0 + 360.0f * i / Who.Num() + Rng.FRandRange(-15.0f, 15.0f));
				const float R = Spot.CircleRadius * Rng.FRandRange(0.9f, 1.15f);
				Who[i]->BeginMeet(Spot.Center + FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, 0.0f), Spot.Center);
				Participants.Add(Who[i]);
			}
			State = EState::Gathering; Timer = GatherTimeout;
			UE_LOG(LogP9, Log, TEXT("StationLife: meet-up %s at %s with %d friends"), *Conversations[Ci].Id.ToString(), *Spot.Id.ToString(), Who.Num());
			return true;
		}
	}
	// no scripted conversation fits: two friends who share a spot just greet each other
	if (ConversationId.IsNone())
	{
		TArray<AP9FriendNPC*> All = Friends();
		for (const FP9NPCMeetSpot& Spot : MeetSpots)
		{
			TArray<AP9FriendNPC*> Who;
			for (AP9FriendNPC* F : All)
			{
				const float Eta = F->RouteDistanceTo(Spot.Center) / FMath::Max(50.0f, F->WalkSpeed);
				if (F->IsAvailableForMeet() && F->RouteIndexNear(Spot.Center, Spot.ReachRadius) != INDEX_NONE && Eta < GatherTimeout * 0.7f) Who.Add(F);
			}
			if (Who.Num() < 2) continue;
			for (int32 i = Who.Num() - 1; i > 0; --i) Who.Swap(i, Rng.RandRange(0, i));
			Who.SetNum(2);
			ConvIndex = INDEX_NONE; bGreetingOnly = true; Center = Spot.Center; LineIndex = 0;
			Participants.Reset();
			for (int32 i = 0; i < 2; ++i)
			{
				const float A = FMath::DegreesToRadians(Rng.FRandRange(0.0f, 360.0f) + 180.0f * i);
				Who[i]->BeginMeet(Spot.Center + FVector(FMath::Cos(A), FMath::Sin(A), 0.0f) * Spot.CircleRadius * 0.8f, Spot.Center);
				Participants.Add(Who[i]);
			}
			State = EState::Gathering; Timer = GatherTimeout;
			UE_LOG(LogP9, Log, TEXT("StationLife: greeting meet at %s (%s, %s)"), *Spot.Id.ToString(), *Who[0]->FriendName, *Who[1]->FriendName);
			return true;
		}
	}
	return false;
}

void AP9StationLife::Finish()
{
	for (int32 i = 0; i < Participants.Num(); ++i)
	{
		if (Participants[i]) Participants[i]->EndMeet(0.3f + Rng.FRandRange(0.0f, 2.5f) + i * 0.4f);   // they leave one by one
	}
	if (ConvIndex != INDEX_NONE) LastConversation = Conversations[ConvIndex].Id;
	Participants.Reset();
	State = EState::Waiting;
	Timer = Rng.FRandRange(MinGap, MaxGap);
}

void AP9StationLife::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Timer -= DeltaSeconds;
	// Stage 5B: a participant hidden by the campaign (new run) ends the meet at once
	if (State != EState::Waiting)
	{
		for (const AP9FriendNPC* P : Participants)
		{
			if (!P || !P->IsNPCAvailable())
			{
				UE_LOG(LogP9, Log, TEXT("StationLife: a participant became unavailable - meet ended"));
				Finish();
				return;
			}
		}
	}
	switch (State)
	{
	case EState::Waiting:
		if (Timer <= 0.0f && !StartMeet()) Timer = 10.0f;
		break;
	case EState::Gathering:
	{
		int32 Arrived = 0;
		for (const AP9FriendNPC* P : Participants) Arrived += P && P->HasArrivedAtMeet() ? 1 : 0;
		if (Arrived == Participants.Num() || (Timer <= 0.0f && Arrived >= 2))
		{
			Participants.RemoveAll([](const TObjectPtr<AP9FriendNPC>& P) { return !P || !P->HasArrivedAtMeet(); });   // late ones go back on EndMeet
			State = EState::Talking; Timer = 0.6f; LineIndex = 0;
			UE_LOG(LogP9, Log, TEXT("StationLife: %d friends gathered, talking"), Participants.Num());
		}
		else if (Timer <= 0.0f) { UE_LOG(LogP9, Log, TEXT("StationLife: gathering timed out")); Finish(); }
		break;
	}
	case EState::Talking:
	{
		if (Timer > 0.0f) break;
		// next line whose speaker is present
		AP9FriendNPC* Speaker = nullptr; USoundBase* Sound = nullptr; FString Text;
		if (bGreetingOnly)
		{
			if (LineIndex < Participants.Num())
			{
				Speaker = Participants[LineIndex];
				if (const TSoftObjectPtr<USoundBase>* G = Greetings.Find(Speaker->FriendKey)) Sound = G->LoadSynchronous();
			}
		}
		else
		{
			const TArray<FP9NPCLine>& Lines = Conversations[ConvIndex].Lines;
			while (LineIndex < Lines.Num() && !Speaker)
			{
				for (AP9FriendNPC* P : Participants) if (P->FriendKey == Lines[LineIndex].Speaker) Speaker = P;
				if (!Speaker) ++LineIndex;
			}
			if (Speaker) { Sound = Lines[LineIndex].Sound.LoadSynchronous(); Text = Lines[LineIndex].Text; }
		}
		if (!Speaker) { UE_LOG(LogP9, Log, TEXT("StationLife: conversation over")); Finish(); break; }
		for (AP9FriendNPC* P : Participants)
		{
			if (P != Speaker) P->FaceTowards(Speaker->GetActorLocation());        // listeners turn to the speaker
		}
		Speaker->FaceTowards(Center);
		const float Len = Sound ? Speaker->Say(Sound) : 1.5f;
		UE_LOG(LogP9, Log, TEXT("StationLife: %s says \"%s\" (%.1fs)"), *Speaker->FriendName, *Text, Len);
		++LineIndex;
		Timer = Len + Rng.FRandRange(0.35f, 0.9f);                                  // natural pauses between lines
		break;
	}
	default:
		break;
	}
}

void AP9StationLife::LogStatus() const
{
	static const TCHAR* S[] = {TEXT("Waiting"), TEXT("Gathering"), TEXT("Talking"), TEXT("Dispersing")};
	UE_LOG(LogP9, Log, TEXT("StationLife: %s timer=%.1f conversation=%s participants=%d"), S[(int32)State], Timer,
		ConvIndex != INDEX_NONE ? *Conversations[ConvIndex].Id.ToString() : (bGreetingOnly ? TEXT("greeting") : TEXT("-")), Participants.Num());
}
