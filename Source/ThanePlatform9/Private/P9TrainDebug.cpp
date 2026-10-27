// Development-only console commands for exercising the Stage 3E.2/3E.3 train (doors, departure, loop speed, destination)
// and the interaction prompt in PIE without completing any game flow.

#include "ThanePlatform9.h"

#if !UE_BUILD_SHIPPING

#include "P9Train.h"
#include "P9InteractableComponent.h"
#include "P9InteractionSubsystem.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

namespace P9TrainDebug
{
	void ForEachTrain(UWorld* World, TFunctionRef<void(AP9Train*)> Fn)
	{
		int32 Count = 0;
		for (TActorIterator<AP9Train> It(World); It; ++It) { Fn(*It); ++Count; }
		if (Count == 0) UE_LOG(LogP9, Warning, TEXT("p9.Train: no AP9Train in this world (is PIE running?)"));
	}

	void Register(const TCHAR* Name, const TCHAR* Help, TFunction<void(AP9Train*, const TArray<FString>&)> Fn)
	{
		IConsoleManager::Get().RegisterConsoleCommand(Name, Help,
			FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([Name, Fn](const TArray<FString>& Args, UWorld* World)
			{
				ForEachTrain(World, [&](AP9Train* Train)
				{
					Fn(Train, Args);
					UE_LOG(LogP9, Log, TEXT("%s: %s"), Name, *Train->GetStatusString());
				});
			}), ECVF_Cheat);
	}

	struct FRegistrar
	{
		FRegistrar()
		{
			Register(TEXT("p9.Train.State"), TEXT("p9.Train.State - log train/door state, safety and player status"), [](AP9Train*, const TArray<FString>&) {});
			Register(TEXT("p9.Train.Open"), TEXT("p9.Train.Open - open the platform-side doors (only when stopped at a stop)"),
				[](AP9Train* T, const TArray<FString>&) { UE_LOG(LogP9, Log, TEXT("p9.Train.Open -> %s"), T->RequestOpenDoors() ? TEXT("accepted") : TEXT("refused")); });
			Register(TEXT("p9.Train.Close"), TEXT("p9.Train.Close - warn and close the doors"),
				[](AP9Train* T, const TArray<FString>&) { UE_LOG(LogP9, Log, TEXT("p9.Train.Close -> %s"), T->RequestCloseDoors() ? TEXT("accepted") : TEXT("refused")); });
			Register(TEXT("p9.Train.Depart"), TEXT("p9.Train.Depart - close the doors if needed and depart once safe"),
				[](AP9Train* T, const TArray<FString>&) { UE_LOG(LogP9, Log, TEXT("p9.Train.Depart -> %s"), T->RequestDeparture() ? TEXT("accepted") : TEXT("refused")); });
			Register(TEXT("p9.Train.Speed"), TEXT("p9.Train.Speed <cm/s> - cruise speed for the loop"),
				[](AP9Train* T, const TArray<FString>& A) { if (A.Num()) T->MaxSpeed = FMath::Max(10.0f, FCString::Atof(*A[0])); });
			Register(TEXT("p9.Train.Auto"), TEXT("p9.Train.Auto <0|1> - auto dwell/close/depart cycle"),
				[](AP9Train* T, const TArray<FString>& A) { T->bAutoCycle = A.Num() == 0 || FCString::Atoi(*A[0]) != 0; });
			Register(TEXT("p9.Train.Dwell"), TEXT("p9.Train.Dwell <seconds> - door dwell time"),
				[](AP9Train* T, const TArray<FString>& A) { if (A.Num()) T->DwellTime = FMath::Max(1.0f, FCString::Atof(*A[0])); });
			Register(TEXT("p9.Train.Dest"), TEXT("p9.Train.Dest <text...> - set the LED destination board"),
				[](AP9Train* T, const TArray<FString>& A) { T->SetDestinationText(FString::Join(A, TEXT(" "))); });
		}
	};
	static FRegistrar GRegistrar;
}

static FAutoConsoleCommandWithWorldAndArgs GP9Interact(
	TEXT("p9.Interact"),
	TEXT("p9.Interact - press the interaction prompt (same as the touch button / E)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		UP9InteractionSubsystem* Subsystem = World ? World->GetSubsystem<UP9InteractionSubsystem>() : nullptr;
		if (!Subsystem)
		{
			UE_LOG(LogP9, Warning, TEXT("p9.Interact: no interaction subsystem (is PIE running?)"));
			return;
		}
		const UP9InteractableComponent* Current = Subsystem->GetCurrentInteractable();
		const FString Prompt = Current ? Current->PromptText.ToString() : TEXT("<none>");
		const bool bAvailable = Subsystem->IsActionAvailable();
		const bool bTriggered = Subsystem->TriggerInteract();
		UE_LOG(LogP9, Log, TEXT("p9.Interact: prompt '%s' available=%s triggered=%s"), *Prompt, bAvailable ? TEXT("yes") : TEXT("no"), bTriggered ? TEXT("yes") : TEXT("no"));
	}));

#endif
