#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "P9StationLife.generated.h"

class AP9FriendNPC;
class USoundBase;

USTRUCT(BlueprintType)
struct FP9NPCLine
{
	GENERATED_BODY()
	/** FriendKey of the speaker (F1..F5). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Conversation") FName Speaker;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Conversation") TSoftObjectPtr<USoundBase> Sound;
	/** Reference wording (Roman) for logs; the audio is the source of truth. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Conversation") FString Text;
};

USTRUCT(BlueprintType)
struct FP9NPCConversation
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Conversation") FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Conversation") TArray<FP9NPCLine> Lines;
};

USTRUCT(BlueprintType)
struct FP9NPCMeetSpot
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Conversation") FName Id;
	/** Floor point the friends gather around. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Conversation") FVector Center = FVector::ZeroVector;
	/** A friend can come here if one of its route points is this close. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Conversation", meta = (ClampMin = "50.0")) float ReachRadius = 500.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Conversation", meta = (ClampMin = "40.0")) float CircleRadius = 80.0f;
};

/**
 * Stage 5A: occasional meet-ups of the station friends. Every now and then it picks a conversation whose speakers can all
 * reach a common meet spot (via their own routes), gathers them, plays the lines one speaker at a time (listeners face the
 * speaker), then lets them walk back to their routines. One conversation at a time; lines are audible only near the camera
 * (the NPC decides), otherwise the meet still happens silently. Greetings are used when two friends meet without a script.
 * Purely ambient: no link to the campaign, anomalies or trains. Data (lines, sounds, spots) is editable on the instance.
 */
UCLASS()
class THANEPLATFORM9_API AP9StationLife : public AActor
{
	GENERATED_BODY()

public:
	AP9StationLife();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StationLife") TArray<FP9NPCConversation> Conversations;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StationLife") TMap<FName, TSoftObjectPtr<USoundBase>> Greetings;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StationLife") TArray<FP9NPCMeetSpot> MeetSpots;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StationLife", meta = (ClampMin = "5.0")) float MinGap = 35.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StationLife", meta = (ClampMin = "5.0")) float MaxGap = 80.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StationLife", meta = (ClampMin = "5.0")) float FirstMeetDelay = 25.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StationLife", meta = (ClampMin = "5.0")) float GatherTimeout = 90.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StationLife") int32 Seed = 5;

	/** Start a meet now (console: p9.Friends.Meet [conversation id]). */
	bool StartMeet(FName ConversationId = NAME_None);
	void LogStatus() const;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	enum class EState : uint8 { Waiting, Gathering, Talking, Dispersing };
	TArray<AP9FriendNPC*> Friends() const;
	AP9FriendNPC* FindFriend(FName Key) const;
	void Finish();

	FRandomStream Rng;
	EState State = EState::Waiting;
	float Timer = 0.0f;
	int32 ConvIndex = INDEX_NONE;
	int32 LineIndex = 0;
	bool bGreetingOnly = false;
	FVector Center = FVector::ZeroVector;
	FName LastConversation;
	UPROPERTY(Transient) TArray<TObjectPtr<AP9FriendNPC>> Participants;
};
