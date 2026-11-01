// Development-only console commands for the Stage 4A anomaly campaign: inspect / force days, simulate generation over many
// seeds, route decisions through the real validation, check baseline restoration and save/load. Logs only (never on the HUD).

#include "ThanePlatform9.h"

#if !UE_BUILD_SHIPPING

#include "P9CampaignDirector.h"
#include "P9AnomalySpawnPoint.h"
#include "P9StationAnnouncer.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

namespace P9CampaignDebug
{
	AP9CampaignDirector* FindDirector(UWorld* World)
	{
		for (TActorIterator<AP9CampaignDirector> It(World); It; ++It)
		{
			return *It;
		}
		UE_LOG(LogP9, Warning, TEXT("p9.Campaign: no AP9CampaignDirector in this world"));
		return nullptr;
	}

	int32 ArgInt(const TArray<FString>& Args, int32 Index, int32 Default)
	{
		return Args.IsValidIndex(Index) ? FCString::Atoi(*Args[Index]) : Default;
	}

	void Register(const TCHAR* Name, const TCHAR* Help, TFunction<void(AP9CampaignDirector*, const TArray<FString>&)> Fn)
	{
		IConsoleManager::Get().RegisterConsoleCommand(Name, Help,
			FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([Fn](const TArray<FString>& Args, UWorld* World)
			{
				if (AP9CampaignDirector* Director = FindDirector(World))
				{
					Fn(Director, Args);
				}
			}), ECVF_Cheat);
	}

	/** Checks the selection rules on one plan; returns the number of violations (logged). */
	int32 CheckPlan(const AP9CampaignDirector* D, const FP9DayPlan& Plan)
	{
		int32 Bad = 0;
		TSet<int32> Points, Defs;
		TSet<FString> Targets;
		for (const FP9PlannedAnomaly& A : Plan.Anomalies)
		{
			const FP9AnomalyDef& Def = D->Anomalies[A.DefIndex];
			const AP9AnomalySpawnPoint* P = D->SpawnPoints[A.PointIndex];
			auto Fail = [&](const TCHAR* Why) { ++Bad; UE_LOG(LogP9, Warning, TEXT("p9.Campaign: RULE VIOLATION %s in %s"), Why, *Plan.ToString()); };
			if (Points.Contains(A.PointIndex)) Fail(TEXT("point reused"));
			if (Targets.Contains(P->GetTargetKey())) Fail(TEXT("target controlled twice"));
			if (Defs.Contains(A.DefIndex)) Fail(TEXT("anomaly twice"));
			if (!Def.LocationTypes.Contains(P->LocationType)) Fail(TEXT("incompatible location"));
			if (!D->IsPointAllowed(P)) Fail(TEXT("point outside the footbridge / Platform 9 anomaly zone"));
			for (const FP9PlannedAnomaly& B : Plan.Anomalies)
			{
				if (&A != &B)
				{
					for (const FName& Tag : Def.ExcludeTags)
					{
						if (D->Anomalies[B.DefIndex].Tags.Contains(Tag)) { Fail(TEXT("exclusion broken")); break; }
					}
				}
			}
			Points.Add(A.PointIndex);
			Targets.Add(P->GetTargetKey());
			Defs.Add(A.DefIndex);
		}
		if (Plan.bAnomalous == Plan.Anomalies.IsEmpty())
		{
			++Bad;
			UE_LOG(LogP9, Warning, TEXT("p9.Campaign: RULE VIOLATION anomalous flag / count mismatch in %s"), *Plan.ToString());
		}
		return Bad;
	}

	struct FRegistrar
	{
		FRegistrar()
		{
			Register(TEXT("p9.Campaign.Status"), TEXT("p9.Campaign.Status - log day, seed, phase and the active anomalies"),
				[](AP9CampaignDirector* D, const TArray<FString>&) { UE_LOG(LogP9, Log, TEXT("p9.Campaign.Status: %s"), *D->GetStatusString()); });

			Register(TEXT("p9.Campaign.New"), TEXT("p9.Campaign.New [seed] - start a new run from Day 1 (seed 0/omitted = random)"),
				[](AP9CampaignDirector* D, const TArray<FString>& Args) { D->StartNewRun(ArgInt(Args, 0, 0)); });

			Register(TEXT("p9.Campaign.Restart"), TEXT("p9.Campaign.Restart - regenerate and restart the current day"),
				[](AP9CampaignDirector* D, const TArray<FString>&) { D->RestartDay(); });

			Register(TEXT("p9.Campaign.Day"), TEXT("p9.Campaign.Day <n> - jump to day n of the current run (regenerated)"),
				[](AP9CampaignDirector* D, const TArray<FString>& Args) { D->SetDay(ArgInt(Args, 0, 1)); });

			Register(TEXT("p9.Campaign.Decide"), TEXT("p9.Campaign.Decide <pf1|board> - submit a decision through the normal validation"),
				[](AP9CampaignDirector* D, const TArray<FString>& Args)
				{
					const bool bBoard = Args.IsValidIndex(0) && Args[0].Equals(TEXT("board"), ESearchCase::IgnoreCase);
					D->SubmitDecision(bBoard ? EP9Decision::BoardTrain : EP9Decision::ReturnToPlatform1);
				});

			Register(TEXT("p9.Campaign.Cleanup"), TEXT("p9.Campaign.Cleanup - restore every change of the current day's anomalies (test)"),
				[](AP9CampaignDirector* D, const TArray<FString>&) { D->CleanupDay(); });

			Register(TEXT("p9.Campaign.Verify"), TEXT("p9.Campaign.Verify - compare anomaly targets with the BeginPlay snapshot"),
				[](AP9CampaignDirector* D, const TArray<FString>&) { D->VerifyBaseline(true); });

			Register(TEXT("p9.Campaign.Save"), TEXT("p9.Campaign.Save - write the save slot now"),
				[](AP9CampaignDirector* D, const TArray<FString>&) { D->SaveProgress(); });

			Register(TEXT("p9.Campaign.Load"), TEXT("p9.Campaign.Load - read the save slot and regenerate that day"),
				[](AP9CampaignDirector* D, const TArray<FString>&)
				{
					if (D->LoadProgress())
					{
						UE_LOG(LogP9, Log, TEXT("p9.Campaign.Load: run %d seed %d day %d"), D->GetRunId(), D->GetRunSeed(), D->GetDay());
						D->RestartDay();
					}
					else
					{
						UE_LOG(LogP9, Warning, TEXT("p9.Campaign.Load: no usable save"));
					}
				});

			Register(TEXT("p9.Campaign.DeleteSave"), TEXT("p9.Campaign.DeleteSave - remove the campaign save slot"),
				[](AP9CampaignDirector* D, const TArray<FString>&) { UE_LOG(LogP9, Log, TEXT("p9.Campaign.DeleteSave -> %s"), D->DeleteSave() ? TEXT("deleted") : TEXT("nothing to delete")); });

			Register(TEXT("p9.Campaign.Scare"), TEXT("p9.Campaign.Scare <reflection|apparition> - play a jumpscare now"),
				[](AP9CampaignDirector* D, const TArray<FString>& Args)
				{
					const bool bReflection = Args.IsValidIndex(0) && Args[0].Equals(TEXT("reflection"), ESearchCase::IgnoreCase);
					D->DebugJumpscare(bReflection ? EP9JumpscareType::Reflection : EP9JumpscareType::Apparition);
				});

			Register(TEXT("p9.Campaign.Plan"), TEXT("p9.Campaign.Plan <seed> <day> [attempt] - log the plan generation gives (no history)"),
				[](AP9CampaignDirector* D, const TArray<FString>& Args)
				{
					const FP9DayPlan Plan = D->GenerateDayPlan(ArgInt(Args, 0, 1), ArgInt(Args, 1, 1), ArgInt(Args, 2, 0), {});
					UE_LOG(LogP9, Log, TEXT("p9.Campaign.Plan: %s"), *Plan.ToString());
				});

			Register(TEXT("p9.Campaign.Force"), TEXT("p9.Campaign.Force normal | <Def>[:Variant][@Point][!] ... - restart the day with exactly these anomalies ('!' = with jumpscare)"),
				[](AP9CampaignDirector* D, const TArray<FString>& Args)
				{
					FP9DayPlan Plan;
					Plan.Seed = 4242;
					TSet<int32> Used;
					for (const FString& Arg : Args)
					{
						if (Arg.Equals(TEXT("normal"), ESearchCase::IgnoreCase)) continue;
						FString Spec = Arg, DefName, VariantName, PointName;
						const bool bScare = Spec.EndsWith(TEXT("!"));
						Spec.RemoveFromEnd(TEXT("!"));
						if (!Spec.Split(TEXT("@"), &Spec, &PointName)) PointName.Reset();
						if (!Spec.Split(TEXT(":"), &DefName, &VariantName)) DefName = Spec;
						const int32 DefIndex = D->FindAnomalyDef(FName(*DefName));
						if (DefIndex == INDEX_NONE) { UE_LOG(LogP9, Warning, TEXT("p9.Campaign.Force: unknown anomaly '%s'"), *DefName); return; }
						const FP9AnomalyDef& Def = D->Anomalies[DefIndex];
						int32 VariantIndex = VariantName.IsEmpty() ? 0 : Def.Variants.IndexOfByPredicate([&](const FP9AnomalyVariant& V) { return V.Id == FName(*VariantName); });
						if (VariantIndex == INDEX_NONE) { UE_LOG(LogP9, Warning, TEXT("p9.Campaign.Force: unknown variant '%s'"), *VariantName); return; }
						int32 PointIndex = PointName.IsEmpty() ? INDEX_NONE : D->FindSpawnPoint(FName(*PointName));
						if (PointIndex == INDEX_NONE)
						{
							for (int32 p = 0; p < D->SpawnPoints.Num(); ++p)
							{
								if (D->SpawnPoints[p] && !Used.Contains(p) && Def.LocationTypes.Contains(D->SpawnPoints[p]->LocationType)) { PointIndex = p; break; }
							}
						}
						if (PointIndex == INDEX_NONE) { UE_LOG(LogP9, Warning, TEXT("p9.Campaign.Force: no spawn point for '%s'"), *Arg); return; }
						Used.Add(PointIndex);
						FP9PlannedAnomaly& A = Plan.Anomalies.AddDefaulted_GetRef();
						A.DefIndex = DefIndex;
						A.VariantIndex = VariantIndex;
						A.PointIndex = PointIndex;
						A.bJumpscare = bScare;
						A.DefId = Def.Id;
						A.VariantId = Def.Variants[VariantIndex].Id;
						A.PointId = D->SpawnPoints[PointIndex]->PointId;
					}
					Plan.bAnomalous = !Plan.Anomalies.IsEmpty();
					D->SetForcedPlan(Plan);
					D->RestartDay();
				});

			// ---------------- Stage 4D: automated anomaly audit ----------------
			Register(TEXT("p9.Anomaly.Audit"), TEXT("p9.Anomaly.Audit - every enabled anomaly variant: assets load, an allowed spawn point exists, a visible change exists"),
				[](AP9CampaignDirector* D, const TArray<FString>&)
				{
					int32 Bad = 0, Variants = 0;
					for (const FP9AnomalyDef& Def : D->Anomalies)
					{
						if (!Def.bEnabled) { UE_LOG(LogP9, Log, TEXT("p9.Anomaly.Audit: %-18s disabled (replaced)"), *Def.Id.ToString()); continue; }
						TArray<const AP9AnomalySpawnPoint*> Points;
						for (const AP9AnomalySpawnPoint* P : D->SpawnPoints) if (P && Def.LocationTypes.Contains(P->LocationType) && D->IsPointAllowed(P)) Points.Add(P);
						for (const FP9AnomalyVariant& V : Def.Variants)
						{
							++Variants;
							TArray<FString> Problems;
							if (Points.IsEmpty()) Problems.Add(TEXT("no allowed spawn point"));
							if (!V.Material.IsNull() && !V.Material.LoadSynchronous()) Problems.Add(TEXT("material missing"));
							if (!V.Sound.IsNull() && !V.Sound.LoadSynchronous()) Problems.Add(TEXT("sound missing"));
							if (!V.Mesh.IsNull() && !V.Mesh.LoadSynchronous()) Problems.Add(TEXT("mesh missing"));
							if (Def.Handler == EP9AnomalyHandler::MaterialSwap)
							{
								bool bSlot = false;
								for (const AP9AnomalySpawnPoint* P : Points)
								{
									const UStaticMeshComponent* M = P->TargetActor ? P->TargetActor->FindComponentByClass<UStaticMeshComponent>() : nullptr;
									for (int32 i = 0; M && i < M->GetNumMaterials(); ++i) bSlot |= M->GetMaterial(i) && M->GetMaterial(i)->GetName().Contains(Def.MaterialSlotMatch);
								}
								if (!bSlot) Problems.Add(TEXT("no target slot matches - no visible change"));
								if (V.Material.IsNull()) Problems.Add(TEXT("no replacement material - no visible change"));
							}
							if (Def.Handler == EP9AnomalyHandler::StaticProp && V.Mesh.IsNull()) Problems.Add(TEXT("no mesh - nothing appears"));
							Bad += Problems.Num() ? 1 : 0;
							UE_LOG(LogP9, Log, TEXT("p9.Anomaly.Audit: %-18s %-18s diff %d points %d %s %s"), *Def.Id.ToString(), *V.Id.ToString(), V.Difficulty, Points.Num(),
								Problems.Num() ? TEXT("FAIL") : TEXT("ok"), *FString::Join(Problems, TEXT("; ")));
						}
					}
					UE_LOG(LogP9, Log, TEXT("p9.Anomaly.Audit: %d variants, %d with problems"), Variants, Bad);
				});
			Register(TEXT("p9.Campaign.ScareVolume"), TEXT("p9.Campaign.ScareVolume <0..1.5> - volume of scare stings and drones"),
				[](AP9CampaignDirector* D, const TArray<FString>& Args) { if (Args.Num()) D->ScareVolume = FMath::Clamp(FCString::Atof(*Args[0]), 0.0f, 1.5f); UE_LOG(LogP9, Log, TEXT("ScareVolume %.2f"), D->ScareVolume); });

			// ---------------- Stage 4B story ----------------
			Register(TEXT("p9.Story.Status"), TEXT("p9.Story.Status - opening/epilogue state, the Vashi and Dadar locals, the announcer"),
				[](AP9CampaignDirector* D, const TArray<FString>&) { UE_LOG(LogP9, Log, TEXT("p9.Story.Status: %s"), *D->GetStoryStatus()); });
			Register(TEXT("p9.Story.Validate"), TEXT("p9.Story.Validate - check trains, boards, indicators, announcements and anomaly zones agree with the story"),
				[](AP9CampaignDirector* D, const TArray<FString>&) { D->ValidateStory(); });
			Register(TEXT("p9.Story.Zones"), TEXT("p9.Story.Zones - list every anomaly spawn point and whether the zone rules allow it"),
				[](AP9CampaignDirector* D, const TArray<FString>&)
				{
					for (const AP9AnomalySpawnPoint* P : D->SpawnPoints)
					{
						FString Why;
						const bool bOk = D->IsPointAllowed(P, &Why);
						UE_LOG(LogP9, Log, TEXT("p9.Story.Zones: %-20s %-18s %s %s"), P ? *P->PointId.ToString() : TEXT("null"),
							P ? *StaticEnum<EP9AnomalyLocation>()->GetNameStringByValue(static_cast<int64>(P->LocationType)) : TEXT(""), bOk ? TEXT("allowed") : TEXT("REJECTED"), *Why);
					}
				});
			Register(TEXT("p9.Story.Opening"), TEXT("p9.Story.Opening - play the Dadar -> Thane opening again (test)"),
				[](AP9CampaignDirector* D, const TArray<FString>&) { D->DebugStartOpening(); });
			Register(TEXT("p9.Story.Epilogue"), TEXT("p9.Story.Epilogue - jump to the post-Day-10 epilogue (marks the campaign completed)"),
				[](AP9CampaignDirector* D, const TArray<FString>&) { D->DebugStartEpilogue(); });
			Register(TEXT("p9.Story.Announce"), TEXT("p9.Story.Announce <id> - play a station announcement through the announcer (test)"),
				[](AP9CampaignDirector* D, const TArray<FString>& Args)
				{
					static int32 N = 0;
					if (D->Announcer && Args.Num()) D->Announcer->Play(FName(*Args[0]), FString::Printf(TEXT("debug-%d"), ++N));
				});

			Register(TEXT("p9.Campaign.Sim"), TEXT("p9.Campaign.Sim <firstSeed> <runs> - generate full 10-day runs (all correct) and report distribution, rule checks and determinism"),
				[](AP9CampaignDirector* D, const TArray<FString>& Args)
				{
					const int32 Seed0 = ArgInt(Args, 0, 1000), Runs = FMath::Clamp(ArgInt(Args, 1, 200), 1, 100000);
					int32 Normal = 0, Anomalous = 0, Violations = 0, Mismatch = 0, Scares = 0, MaxAnomalyStreak = 0, MaxNormalStreak = 0;
					TMap<int32, int32> CountHist;
					TMap<FName, int32> PerDef;
					TMap<int32, int32> PerCategory;
					TArray<int32> AnomalousByDay;
					AnomalousByDay.SetNumZeroed(D->NumDays + 1);
					int32 RepeatsNextDay = 0;
					for (int32 r = 0; r < Runs; ++r)
					{
						TArray<FString> History;
						int32 StreakA = 0, StreakN = 0;
						for (int32 Day = 1; Day <= D->NumDays; ++Day)
						{
							const FP9DayPlan Plan = D->GenerateDayPlan(Seed0 + r, Day, 0, History);
							Mismatch += Plan.ToString() == D->GenerateDayPlan(Seed0 + r, Day, 0, History).ToString() ? 0 : 1;
							Violations += CheckPlan(D, Plan);
							Plan.bAnomalous ? ++Anomalous : ++Normal;
							StreakA = Plan.bAnomalous ? StreakA + 1 : 0;
							StreakN = Plan.bAnomalous ? 0 : StreakN + 1;
							MaxAnomalyStreak = FMath::Max(MaxAnomalyStreak, StreakA);
							MaxNormalStreak = FMath::Max(MaxNormalStreak, StreakN);
							AnomalousByDay[Day] += Plan.bAnomalous ? 1 : 0;
							CountHist.FindOrAdd(Plan.Anomalies.Num())++;
							TArray<FString> Ids;
							for (const FP9PlannedAnomaly& A : Plan.Anomalies)
							{
								PerDef.FindOrAdd(A.DefId)++;
								PerCategory.FindOrAdd(static_cast<int32>(D->Anomalies[A.DefIndex].Category))++;
								Scares += A.bJumpscare ? 1 : 0;
								Ids.Add(A.DefId.ToString());
								if (!History.IsEmpty() && History.Last().Contains(A.DefId.ToString())) ++RepeatsNextDay;
							}
							History.Add(FString::Join(Ids, TEXT("|")));
						}
					}
					const int32 Days = Normal + Anomalous;
					UE_LOG(LogP9, Log, TEXT("p9.Campaign.Sim: %d runs x %d days = %d days | normal %d (%.1f%%) anomalous %d | violations %d | non-deterministic %d | jumpscares %d | longest streak anomalous %d normal %d | same anomaly on consecutive days %d"),
						Runs, D->NumDays, Days, Normal, 100.0f * Normal / Days, Anomalous, Violations, Mismatch, Scares, MaxAnomalyStreak, MaxNormalStreak, RepeatsNextDay);
					FString Hist, ByDay, Cats;
					for (int32 n = 0; n <= 3; ++n) Hist += FString::Printf(TEXT(" %d:%d"), n, CountHist.FindRef(n));
					for (int32 Day = 1; Day <= D->NumDays; ++Day) ByDay += FString::Printf(TEXT(" d%d:%.0f%%"), Day, 100.0f * AnomalousByDay[Day] / Runs);
					for (const TPair<int32, int32>& P : PerCategory) Cats += FString::Printf(TEXT(" %s:%d"), *StaticEnum<EP9AnomalyCategory>()->GetNameStringByValue(P.Key), P.Value);
					UE_LOG(LogP9, Log, TEXT("p9.Campaign.Sim: anomalies per day%s | anomalous share by day%s | categories%s"), *Hist, *ByDay, *Cats);
					FString Defs;
					for (const FP9AnomalyDef& Def : D->Anomalies) Defs += FString::Printf(TEXT(" %s:%d"), *Def.Id.ToString(), PerDef.FindRef(Def.Id));
					UE_LOG(LogP9, Log, TEXT("p9.Campaign.Sim: per anomaly%s"), *Defs);
				});
		}
	};
	static FRegistrar GRegistrar;
}

#endif
