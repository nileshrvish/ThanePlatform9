# Stage 5B — Anomaly Inventory and Audit

Date: 2026-11-01. Read-only audit; no anomaly was changed.

## Sources and evidence
- **Code catalogue:** `AP9CampaignDirector::BuildDefaultCatalogue()`, `Source/ThanePlatform9/Private/P9CampaignDirector.cpp:191-372`.
  - Handlers: `ActivateAnomaly` (line 886 on) and `TickAnomaly` (lines 1289-1529).
- **Placed director `S4A_CampaignDirector`:** read from the map with editor Python (`Tools/ClaudeHarness/ue_py/inspect_5b.py`).
  - Its `Anomalies` array is identical to the code catalogue: 21 definitions and 49 variants, no instance overrides.
  - It lists 36 spawn points.
- **Story events:** `P9CampaignStory.cpp` (opening, Vashi local, epilogue).
- **Silhouette:** `P9SilhouetteStalker.cpp`.
- **PIE checks (2026-11-01):**
  - `p9.Anomaly.Audit` on a forced normal day: **48 variants, 0 problems**; PhantomHorn is reported disabled.
  - `p9.Campaign.Verify`: 88 entries, 0 differences.
  - `p9.Story.Validate`: 0 problems.
  - `p9.Story.Zones`: 36 of 36 points allowed.
  - `p9.Campaign.Sim 1000 200` (200 runs × 10 days = 2000 days):
    - 40.2 % normal days, 0 rule violations, 0 non-deterministic plans, 85 jumpscares;
    - each day 1–10 was anomalous on 56–65 % of runs.

## Rules shared by every catalogue anomaly
- **Days:** any of days 1–10.
  - Each day is anomalous with chance 0.6.
  - An anomalous day gets 1–3 anomalies, with weights 0.62 / 0.28 / 0.10.
  - Extreme days (tier 4) always get at least 2.
- **Seeding:** everything is **seeded**. The day type and difficulty depend on (run seed, day); the anomaly, variant and point picks also depend on the attempt number and the run's history.
  - Difficulty tiers 1–4 are rolled per day and never fixed to a day number.
  - A repetition penalty (×0.2 / 0.5 / 0.8) applies to anomalies seen 1, 2 or 3 days ago.
- **Locations:** only at spawn points inside `AnomalyZones` (footbridge and concourse; Platform 9/10 and track 9). Never on Platforms 1–8.
- **Activation:** happens when the day starts (`StartDay`). Everything is restored at day end through the baseline record (`RestoreBaseline`).
- **Jumpscares:** only on tier ≥ 2 days, rolled per anomaly. The chance is ×1.5 on tier 4. There is a 45 s cooldown.

## Inventory

Columns:
- **Diff** is variant difficulty.
- **Train / Env / NPC** show whether the anomaly affects the train, the environment or NPCs.
- Every row is seeded and possible on days 1–10 unless noted.

### 1. Visual anomalies (signage; MaterialSwap handler)
| Anomaly (id) | Code category | Location / spawn point(s) | What the player sees | Variants (diff) | Train/Env/NPC | Assets | Status |
|---|---|---|---|---|---|---|---|
| PlatformSignWrong | Signage | PF9/10 platform signs `PF910_SignW`, `PF910_SignE` (targets `S3B1_PlatformSign_PF9_10_W/E`, slot `Sign_PF9_10`) | Platform sign shows 13, or is misspelt | Number13 (2), **ChangesBehindYou** (3; changes after the player looks away), Misspelt (1) | –/Env/– | `MI_P9_Anom_SignPF9_10_A`, `_C` | Working |
| FOBSignWrong | Signage | Footbridge sign PF2/3 `FOB_SignPF23` (`S3A7_FOBSign_PF2_3`) | Arrows reversed / says Platform 9 | ArrowsReversed (2), Platform9 (2) | –/Env/– | `MI_P9_Anom_FOBSignPF2_3_A/_B` | Working |
| FOBSignPF9Wrong | Signage | Footbridge sign PF9/10 `FOB_SignPF910` (`S3B1_FOBSign_PF9_10`) | Arrows reversed / Platform 13 | ArrowsReversed (2), Platform13 (2) | –/Env/– | `MI_P9_Anom_FOBSignPF9_10_A/_B` | Working |
| IndicatorWrong | Signage | PF9/10 train indicator `PF910_Indicator` (`S3B1_TrainIndicator_PF9_10_W`) | Impossible time / DO NOT BOARD / NO RETURN | ImpossibleTime (1), DoNotBoard (3), NoReturn (2), **NoReturnLater** (3, look-away) | –/Env/– | `MI_P9_Anom_IndicatorPF9_10_A/_B/_C` | Working. The audit falsely reports it as FAIL when it runs while this anomaly is active (see the issues section) |
| ClockWrong | Environment | PF9/10 clock `PF910_Clock` (`S3B1_StationClock_PF9_10_W`) | Time half an hour back / impossible / midnight | HalfHourBack (1), Impossible (2), Midnight (2), **StopsAtMidnight** (3, look-away) | –/Env/– | `MI_P9_Anom_Clock_A/_B/_C` | Working |
| PosterWrong | Signage | Ad stands `POSTER_FOB` (`S4D_AdStand_FOB`), `POSTER_PF9` (`S4D_AdStand_PF9`) | The tea advert's man is watching, or someone stands behind him | Watching (1), SomeoneBehind (2), **WatchingLater** (3, look-away) | –/Env/– | `MI_P9_Anom_AdPoster_Watching` (texture `_v3`), `MI_P9_Anom_AdPoster_Behind` | Working |

### 2. Environmental anomalies
| Anomaly | Code category | Location | What the player sees | Variants | Train/Env/NPC | Assets / classes | Status |
|---|---|---|---|---|---|---|---|
| LampFault | Environment | PF9/10 lamps `PF910_LampE/W1/W2`. Target = `S2C_Lamp_PF9_10_*` spot light; secondary = `S3B3_TubeLight_PF9_10_*` (tube MID `Intensity`/`TubeColor`); extra = `S3C_Fill_PF9_10_*` | Lamp off, flickering, or red | Off (2), Flicker (1), Red (3) | –/Env/– | LampFault handler. It finds the first `ULightComponent` on the target and refuses Static lights | Working. **Depends on the lamp actors that the lighting overhaul will retune** |
| ImpossibleShadow | Supernatural | `SHADOW_PF9` (−1500, 7150), `SHADOW_FOB` (0, 6300, 600) | A person's shadow on the floor with nobody there | Standing (2), Long (1) | –/Env/– | `SM_P9_FloorShadow`, `MI_P9_Anom_FloorShadow` (StaticProp) | Working |

### 3. Spatial and architectural anomalies
| Anomaly | Code category | Location | What the player sees | Variants | Train/Env/NPC | Assets | Status |
|---|---|---|---|---|---|---|---|
| BenchMissing | Environment | 4 PF9/10 benches `PF910_BenchW_N/S`, `PF910_BenchE_N/S` | A bench is gone (hidden, collision off) | Gone (2) | –/Env/– | HideActor | Working |
| ExtraBench | Other | `PF910_BenchSlotA` (−300, 7448), `PF910_BenchSlotB` (600, 7700) | An extra bench, aligned or turned 90° | Aligned (1), Turned (2) | –/Env/– | ExtraProp (copy of the target's mesh, checked for overlaps) | Working |
| WallSilhouette | Supernatural | End walls `WALL_PF9W`, `WALL_PF9E` | A human shape pressed into, or emerging from, the wall | Pressed (3), Emerging (2) | –/Env/– | `SM_P9_Entity_Hero` (flattened), `MI_P9_Anom_WallStain` (StaticProp) | Working |

### 4. Audio anomalies
| Anomaly | Code category | Location | What the player hears | Variants | Train/Env/NPC | Assets | Status |
|---|---|---|---|---|---|---|---|
| AnnouncementWrong | Audio | `PA_Station`. Plays when the Vashi local is called in (`TickConnectingTrain`) | Marathi PF9 arrival announcement altered: Platform 13 / "do not turn" / slowed | Platform13 (2), DoNotTurn (3), Slowed (1; the normal announcement at pitch 0.78) | Train-linked (tied to the call-in) | `SW_P9_PA_PF9_Anom_Platform13`, `_DoNotTurn`, `SW_P9_PA_PF9_VashiArrival` | Working |
| Footsteps | Audio | `FOLLOW_Player`; only while the player walks inside the anomaly zone and is not aboard | Footsteps 3.8 m behind the player every 22–38 s | Behind (2) | –/–/– | `SW_P9_Anom_Footsteps` | Working |
| PhantomHorn | Audio | `HORN_North` (0, 9500), `HORN_West` (−9500, 2500) | A horn from where no track runs | Distant (2) | – | `SW_P9_TrainHorn` | **Disabled** (`bEnabled=false`, replaced by GhostTrain in 4D). 0 of 2000 simulated days |

### 5. Train anomalies
| Anomaly | Code category | Location | What the player sees | Variants | Train/Env/NPC | Assets | Status |
|---|---|---|---|---|---|---|---|
| TrainDestination | Train | `TRAIN_Destination`. Vashi local `S4B_VashiLocal_PF9`, component `Shell` | Destination board reads PLATFORM 9 / NO RETURN / VAHSI S | Platform9 (2), NoReturn (3), Misspelt (1) | **Train** | Canvas render target (`DrawDestination`) | Working |
| TrainSeats | Train | `TRAIN_Seats` (Vashi local `Interior`) | Seats red, or bloodless pale | Red (2), Bloodless (1) | **Train** | `MI_P9_Anom_TrainSeat_Red` (TrainSeatTint) | Working |
| TrainLights | Train | `TRAIN_LightsCar0/Car1` | One car's interior lights dark / flickering / red | Dark (2), Flicker (1), Red (3) | **Train** | Interior light components `CarN_InteriorLightM` | Working |
| GhostTrain | Train | `GHOST_PF10` (−1200, 8080, −75), seen from the zone | A dark cab on track 10 that should not exist, with its own sound every 30–50 s | Waiting (2) | Env (it is a prop, not an `AP9Train`) | `SM_P9_LocalTrain_CabShell`, `SW_P9_Scare_GhostTrain` (StaticProp) | Working |
| WindowFigure (+ **Jumpscare B**) | Supernatural | `TRAIN_WindowCar0/Car1` | Someone standing in the Vashi local: far end / at a door / face to the window. Scare B (FarEnd only, chance 0.5): seen from inside the car, lights fail, the figure is 2.3 m away when they return, brown-out | FarEnd (2), AtDoor (2), FacingWindow (3) | **Train** + NPC-like figure | `SM_P9_Entity_Hero`, `SW_P9_Scare_Drone`, `SW_P9_Scare_StingCarriage` | Working. Known limit: the brown-out is too bright under auto-exposure (CLAUDE.md §5) |

### 6. NPC / silhouette / entity anomalies
| Anomaly | Code category | Location | What the player sees | Trigger | Days | Random/seeded | Assets / classes | Status |
|---|---|---|---|---|---|---|---|---|
| DistantFigure (+ **Jumpscare A**) | Supernatural | PF9/10 ends `PF910_EndW/E` | A figure at the platform end, facing the player or turned away; it vanishes within 11 m. Scare A (chance 0.45): a drone, then 1.8 s later the figure stands 1.65 m in front of the player with a sting | Catalogue | 1–10 | Seeded | `SM_P9_Entity_Hero`, `SW_P9_Scare_Drone/StingClose/ClothRush` | Working |
| WatchingFigure (+ **Jumpscare C**) | Supernatural | `FOB_FigureN` (0, 7700, 600), `FOB_FigureMid` (−120, 5200, 600) | A tall figure on the footbridge that is gone after the player looks away. Approaching variant: it comes 40 % closer each time the player looks back. Scare C (Approaching only, chance 0.5): it lunges after two steps | Catalogue | 1–10 | Seeded | `SM_P9_Entity_Tall`, `SW_P9_Scare_StingFootbridge` | Working |
| Silhouette stalker (not in the catalogue) | — | Behind the player, inside `AnomalyZones` | A woman in a red saree follows; when noticed she speaks one of 5 lines, then vanishes when the player looks away | Phase `Exploring` and ≥ 1 active anomaly, seeded 35 % per (run, day), at most 2 per day, 50 s cooldown, player moving | 1–10, anomalous days only | Seeded day roll | `AP9SilhouetteStalker`, `BP_P9Silhouette`, `SK_P9_Silhouette`, `SW_P9_NPC_SH_1..5` | Working (not counted as an anomaly by the director: it reads the director only). **After the Stage 5B decision (locked until the campaign is completed) she never appears; see the issues section** |
| Reflection jumpscare | — (`EP9JumpscareType::Reflection`) | 2D | Sting and white flash | Only from the debug command `p9.Campaign.Scare reflection` | none | — | `SW_P9_Anom_StingReflection` | **Implemented, never triggered in play** (no catalogue entry uses it) |
| Legacy apparition jumpscare (`TriggerJumpscare(Apparition)`) | — | 1.25 m in front of the player | Stage 4A close apparition | Only from `p9.Campaign.Scare apparition` | none | — | `SM_P9_Apparition`, `SW_P9_Anom_StingApparition` | **Debug only**; superseded by the staged Scare A in 4D |

### 7. Story-driven (scripted) anomalies — epilogue only (`P9CampaignStory.cpp:549-731`)
These run once, after Day 10 is answered correctly. They are not random, never appear on days 1–10, and are not in the catalogue.

| Event | When (seconds after the Vashi local leaves Platform 9) | What the player observes | Train/Env/NPC | Assets | Status |
|---|---|---|---|---|---|
| Altered announcement "the train will not stop at Airoli" | 21 s | Marathi PA with one word changed | Train ride | `SW_P9_PA_PF9_Epilogue_NoAiroli` | Working |
| Failing carriage lights + passenger | 25–33 s | The car's lights flicker; a passenger at the far end, visible only while the player is not looking straight at it | **Train** | `SpawnFigure` (FigureMesh `SM_P9_Apparition`) | Working |
| Final apparition | 33–35.4 s | Dark; the camera settles on the aisle; lights snap back with the figure 1.4 m away; sting; blackout | **Train** | `SW_P9_Anom_StingApparition` | Working; saves `EpilogueStage=2` |
| Final screen "DAY 11 / 10" | 37.5 s | Ending title | — | — | Working |

The epilogue chaiwala wake-up is story, not an anomaly. It uses `SM_P9_Chaiwala_VG` through the `ChaiwalaMesh` soft reference.

### 8. Gameplay and interaction anomalies
There are no separate gameplay anomalies. The interaction mechanics live inside the anomalies above:
- **Look-away changes:** ChangesBehindYou, NoReturnLater, StopsAtMidnight and WatchingLater. The player must look at the target for 0.5 s, then away for 0.4 s.
- **Gaze-reactive figures:** WatchingFigure (vanish, approach), DistantFigure (vanishes on approach), the epilogue passenger.
- **Decisions:** Platform 1 zone = "something was wrong"; riding the Vashi local = "all normal". This is the game rule, not an anomaly.

## Totals
- 21 catalogue definitions and 49 variants; 20 definitions / **48 variants are enabled**.
- 3 staged jumpscares (A, B, C).
- 1 separate stalker system.
- 3 scripted epilogue events, plus the final screen.
- 2 debug-only jumpscare paths.
- By code category, enabled definitions only: Signage 5, Environment 3, Train 4, Audio 2, Supernatural 5 (incl. ImpossibleShadow and WallSilhouette), Other 1 (total 20).

Per-anomaly counts in 200 simulated runs (2000 days):

| Anomaly | Days |
|---|---|
| AnnouncementWrong | 122 |
| PlatformSignWrong | 115 |
| BenchMissing | 106 |
| PosterWrong | 104 |
| FOBSignWrong | 102 |
| ClockWrong | 100 |
| TrainDestination | 100 |
| LampFault | 99 |
| FOBSignPF9Wrong | 91 |
| IndicatorWrong | 91 |
| WatchingFigure | 91 |
| TrainLights | 89 |
| Footsteps | 89 |
| GhostTrain | 89 |
| ExtraBench | 87 |
| ImpossibleShadow | 87 |
| TrainSeats | 85 |
| WallSilhouette | 80 |
| WindowFigure | 80 |
| DistantFigure | 68 |
| PhantomHorn | 0 |

## Issues found

### Unused
- **PhantomHorn** is disabled, and spawn points **`HORN_North`, `HORN_West`** are used only by it.
- **Assets no catalogue entry uses.** They are kept alive only by the director's `PreloadAssets` hard-reference list, so they still get cooked:
  - `MI_P9_Anom_SignPF2_3_A/B/C` and `MI_P9_Anom_IndicatorPF2_3_A/B/C`: the Stage 4A PF2/3 versions, superseded when 4B moved anomalies to PF9/10;
  - `MI_P9_Anom_SignPF9_10_B` (no variant uses it);
  - `SW_P9_Anom_PA_DoNotTurn`, `SW_P9_Anom_PA_Normal`, `SW_P9_Anom_PA_Platform13`: the 4A PA lines, superseded by `/Game/P9/Announcements/SW_P9_PA_PF9_Anom_*`;
  - `MI_P9_Anom_GhostTrain`, already on the pending-delete list;
  - `SM_P9_Chaiwala` (old epilogue mesh; the director now points to `SM_P9_Chaiwala_VG`).
- **Asset referenced by nothing at all:** `T_P9_Anom_AdPoster_Watching` (v1 texture; the material uses `_v3`).

### Duplicate implementations
- **Apparition scare, three code paths:**
  - legacy `TriggerJumpscare(Apparition)` (debug only);
  - staged Scare A (DistantFigure);
  - the epilogue final apparition.
- **Carriage scare, twice:** Scare B (WindowFigure) and the epilogue light-failure sequence are separate code with the same pattern.
- **Shared materials (by design):** four look-away variants reuse their non-look-away sibling's material: ChangesBehindYou/Number13, NoReturnLater/NoReturn, StopsAtMidnight/Midnight, WatchingLater/Watching.
- **"13" theme on three targets (distinct, not duplicates):** PlatformSignWrong:Number13, FOBSignPF9Wrong:Platform13, AnnouncementWrong:Platform13.

### Incomplete or limited
- **LampFault** covers only the three PF9/10 lamps. Footbridge and concourse lamps have no spawn points.
- **ClockWrong, IndicatorWrong, FOBSignWrong and FOBSignPF9Wrong** each have a single spawn point.
- **Scare B's brown-out** reads too bright (auto-exposure).
- **`p9.Anomaly.Audit` false positive:** if it runs while an IndicatorWrong variant is active, the slot no longer contains `TrainIndicator_PF9_10`, so all 4 variants report "no target slot matches". Observed on run 18 day 1; on a forced normal day it reports 0 problems.

### Implemented but never triggered
- PhantomHorn (disabled).
- The Reflection jumpscare and the legacy Apparition path (debug commands only).
- The **Silhouette stalker** after the Stage 5B unlock decision. She is locked until the campaign is completed, but her existing gating needs `Exploring` days with active anomalies, which do not exist after Day 10.

### Broken or missing references
- None. On a normal day the audit loads every variant's assets, finds an allowed spawn point and a visible change for each.
- Not verified: whether the soft reference `ChaiwalaMesh → SM_P9_Chaiwala_VG` cooks into an APK. It is not in `PreloadAssets`; the soft reference stored on the map should be followed by the cooker, but no APK has been built since 4C.
