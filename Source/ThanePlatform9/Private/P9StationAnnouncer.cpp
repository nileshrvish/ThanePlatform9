#include "P9StationAnnouncer.h"
#include "P9Captions.h"
#include "ThanePlatform9.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"

namespace P9Announcer
{
	FP9AnnouncementDef Def(const TCHAR* Id, int32 Platform, const TCHAR* Script)
	{
		FP9AnnouncementDef D;
		D.Id = FName(Id);
		D.Platform = Platform;
		D.Sound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(FString::Printf(TEXT("/Game/P9/Announcements/SW_P9_%s.SW_P9_%s"), Id, Id)));
		D.Script = Script;
		return D;
	}
}

AP9StationAnnouncer::AP9StationAnnouncer()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetHidden(true);
	using P9Announcer::Def;
	Announcements = {
		Def(TEXT("PA_PF1_ThaneArrival"), 1, TEXT("Pravashanche laksh asu dya. Dadarhun Thanyala yenari local gaadi Thane sthanakachya platform kramank ekavar aali aahe. ...")),
		Def(TEXT("PA_PF9_VashiArrival"), 9, TEXT("Pravashanche laksh asu dya. Vashikade jaanari local gaadi kahi kshanat platform kramank nauvar yenar aahe. Ya gaadicha Airoli sthanakavar thamba aahe. ...")),
		Def(TEXT("PA_PF9_VashiDeparture"), 9, TEXT("Pravashanche laksh asu dya. Thane sthanakachya platform kramank nauvarun Vashikade jaanari local gaadi ata ravana hot aahe. ...")),
		Def(TEXT("PA_PF9_Epilogue_NoAiroli"), 9, TEXT("... Ya gaadicha Airoli sthanakavar thamba nahi. ... (epilogue, altered word)")),
	};
	Platform1Speakers = {FVector(-1800.0f, -1250.0f, 430.0f), FVector(0.0f, -1250.0f, 430.0f), FVector(1800.0f, -1250.0f, 430.0f)};
	Platform9Speakers = {FVector(-1800.0f, 7410.0f, 430.0f), FVector(0.0f, 7410.0f, 430.0f), FVector(1800.0f, 7410.0f, 430.0f)};
}

void AP9StationAnnouncer::BeginPlay()
{
	Super::BeginPlay();
	Attenuation = NewObject<USoundAttenuation>(this, TEXT("P9AnnouncerAttenuation"));
	Attenuation->Attenuation.bAttenuate = true;
	Attenuation->Attenuation.bSpatialize = true;
	Attenuation->Attenuation.AttenuationShapeExtents = FVector(InnerRadius);
	Attenuation->Attenuation.FalloffDistance = FalloffDistance;
	Validate(true);
}

void AP9StationAnnouncer::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopAll(0.0f);
	Super::EndPlay(EndPlayReason);
}

const FP9AnnouncementDef* AP9StationAnnouncer::Find(FName Id) const
{
	return Announcements.FindByPredicate([Id](const FP9AnnouncementDef& D) { return D.Id == Id; });
}

int32 AP9StationAnnouncer::Validate(bool bLog) const
{
	int32 Problems = 0;
	TSet<FName> Ids;
	for (const FP9AnnouncementDef& D : Announcements)
	{
		FString Why;
		if (Ids.Contains(D.Id)) Why = TEXT("duplicate id");
		else if (!AllowedPlatforms.Contains(D.Platform)) Why = FString::Printf(TEXT("platform %d has no announcements in this stage"), D.Platform);
		else if (!D.Sound.LoadSynchronous()) Why = TEXT("sound missing");
		Ids.Add(D.Id);
		if (!Why.IsEmpty())
		{
			++Problems;
			if (bLog) UE_LOG(LogP9, Warning, TEXT("Announcer: %s - %s"), *D.Id.ToString(), *Why);
		}
	}
	if (bLog)
	{
		UE_LOG(LogP9, Log, TEXT("Announcer: %d announcements, platforms allowed [%s], %d problems"), Announcements.Num(),
			*FString::JoinBy(AllowedPlatforms, TEXT(","), [](int32 P) { return FString::FromInt(P); }), Problems);
	}
	return Problems;
}

bool AP9StationAnnouncer::IsPlaying() const
{
	return IsValid(Current) && Current->IsPlaying();
}

void AP9StationAnnouncer::StopAll(float FadeOutSeconds)
{
	if (IsValid(Current) && Current->IsPlaying())
	{
		UE_LOG(LogP9, Log, TEXT("Announcer: stopping %s"), *CurrentId.ToString());
		FadeOutSeconds > 0.0f ? Current->FadeOut(FadeOutSeconds, 0.0f) : Current->Stop();
	}
	Current = nullptr;
	CurrentId = NAME_None;
}

bool AP9StationAnnouncer::Play(FName Id, const FString& EventKey, float Pitch, bool bTrainPA)
{
	const FP9AnnouncementDef* D = Find(Id);
	if (!D)
	{
		UE_LOG(LogP9, Warning, TEXT("Announcer: unknown announcement %s"), *Id.ToString());
		return false;
	}
	return PlaySound(D->Sound.LoadSynchronous(), D->Platform, Id, EventKey, D->Volume, Pitch, bTrainPA);
}

bool AP9StationAnnouncer::PlaySound(USoundBase* Sound, int32 Platform, FName LogId, const FString& EventKey, float Volume, float Pitch, bool bTrainPA)
{
	if (!Sound || !AllowedPlatforms.Contains(Platform))
	{
		UE_LOG(LogP9, Warning, TEXT("Announcer: %s refused (%s)"), *LogId.ToString(), Sound ? TEXT("platform not allowed") : TEXT("no sound"));
		return false;
	}
	if (PlayedKeys.Contains(EventKey))
	{
		UE_LOG(LogP9, Log, TEXT("Announcer: %s ignored - event '%s' already announced"), *LogId.ToString(), *EventKey);
		return false;
	}
	PlayedKeys.Add(EventKey);
	StopAll(0.6f);                                                   // one channel: never two announcements at once
	if (bTrainPA)
	{
		Current = UGameplayStatics::SpawnSound2D(this, Sound, Volume, Pitch);
	}
	else
	{
		const TArray<FVector>& Speakers = Platform == 1 ? Platform1Speakers : Platform9Speakers;
		const APlayerController* PC = GetWorld()->GetFirstPlayerController();
		const FVector Listener = PC && PC->GetPawn() ? PC->GetPawn()->GetActorLocation() : GetActorLocation();
		FVector Best = Speakers.Num() ? Speakers[0] : GetActorLocation();
		for (const FVector& S : Speakers)
		{
			if (FVector::DistSquared(S, Listener) < FVector::DistSquared(Best, Listener)) Best = S;
		}
		Current = UGameplayStatics::SpawnSoundAtLocation(this, Sound, Best, FRotator::ZeroRotator, Volume, Pitch, 0.0f, Attenuation);
	}
	CurrentId = LogId;
	PlayCounts.FindOrAdd(LogId)++;
	if (Current) P9Captions::NotifySound(this, Sound);              // Stage 9B: captions (cinematic announcements only are listed)
	UE_LOG(LogP9, Log, TEXT("Announcer: PLAY %s (platform %d, %s, pitch %.2f, %.1fs) event '%s'"), *LogId.ToString(), Platform,
		bTrainPA ? TEXT("train PA") : TEXT("station speaker"), Pitch, Sound->GetDuration() / FMath::Max(Pitch, 0.1f), *EventKey);
	return Current != nullptr;
}
