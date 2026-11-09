# CLAUDE.md — Platform No. 9 (ThanePlatform9)

This is the hand-off file for new Claude Code sessions. Read it first, then `PROGRESS.md` (the older history up to Stage 3A; **never edit `PROGRESS.md`**).
It was last updated 2026-11-09, at the end of Stage 9C (earlier: 2026-11-08, end of Stage 8A (anomalies 13–16, seamless endless footbridge, reverse-escalator stair, Lantern Man, the Greeting, NPC box bodies, original audio pack; see §3). Stages 4A–5F (and 5G, Dhanraj on his Mixamo rig) are committed (see `git log`); 6A, 6B, 6C, 7A, 7B, 7C (APK only) and 8A are not committed.

## 1. Project at a glance
- **Game:** "Platform No. 9" is a first-person horror game set at Thane station (Mumbai), inspired by Exit 8 and P.T.
  - The player spots anomalies over 10 days.
  - If anything is wrong, the player goes back to Platform 1; if all is normal, they board the Vashi local at Platform 9.
- **Platform:** Android first.
- **Engine and tools:**
  - Unreal Engine 5.7.4 at `C:\Program Files\Epic Games\UE_5.7`.
  - Project: `C:\Users\niles\Documents\Unreal Projects\ThanePlatform9\ThanePlatform9.uproject`.
  - Blender 4.5.3 LTS at `C:\Program Files\Blender Foundation\Blender 4.5\blender.exe` (run in the background with `-b --factory-startup`; the Blender MCP addon also works).
- **Main map:** `/Game/P9/Maps/L_Station_Redesign`. Do not change the startup or default map.
- **Git:**
  - Branch `mobile-controls-ledge-climb`, remote `origin` (GitHub). Git LFS handles `.uasset`, `.umap`, `.fbx`, `.blend`, `.png`, `.jpg`.
  - Never use or print the GitHub PAT stored in `.git/config`.
  - `Tools/ClaudeHarness/.gitignore` keeps test captures and per-run command files out of git.
- **Source module:** `Source/ThanePlatform9` (C++).
- **Content:** `Content/P9/`.
- **Blender sources:** `C:\Users\niles\Documents\Blender Projects\P9_Assets\` (not in git).
- **Backups:** `C:\Users\niles\Documents\Unreal Projects\_backups\ThanePlatform9\`.

## 2. Standing rules (from the user; these always apply)
1. **Explain before changing.** Work one step at a time. Show toolchain commands (build, import, package) before running them.
2. **Never delete files or assets without explicit approval, including files Claude created.** Never overwrite existing assets or backups; write new names (`_v2`, `_v3`, `A3`) instead. Moving superseded outputs into a subfolder is fine; say so.
3. **Git:** never stage, commit, push, merge, reset or revert unless the user explicitly asks. Ask first before any destructive git operation.
4. **Protected systems.** Do not modify any of these unless the user explicitly asks:
   - mobile controls;
   - ledge climbing;
   - campaign logic and the anomaly director (`P9CampaignDirector*`, `P9CampaignStory`, `P9CampaignDebug`, `P9CampaignTypes`, `P9CampaignSave`);
   - the storm controller (`P9StormController`);
   - the train systems (`P9Train*`);
   - project settings, global config and `Config/*.ini`. Close the editor before touching an `.ini`.
5. **Map edits:** before any map change, make a timestamped backup of `L_Station_Redesign.umap` and record its SHA-256. Back up `Source/` (with a SHA256SUMS file) before C++ changes.
6. **Forbidden runs:** do not run ES3.1 preview, full station traversal tests or Build Lighting. Do not build an APK unless the user asks.
7. **Audio:** use original or properly licensed audio only. TTS (edge-tts) lines are temporary prototypes; keep them separate from the original sound effects.
8. **Fairness:** normal days must stay normal. No UI or outline that reveals an anomaly.
9. **Honest reporting:** report results honestly. A log line is not proof of visuals; PIE is not Android performance. List what remains incomplete.

## 3. Stage history (what exists)

**Stages 1–3 (see `PROGRESS.md`):** greybox and redesign; mobile controls and ledge climbing (C++); storm sky; canopies, signage and lighting; closed-loop railway with functional local trains (`AP9Train`).

**4A – anomaly campaign:** `AP9CampaignDirector` (10-day seeded campaign), anomaly catalogue (`FP9AnomalyDef`/`FP9AnomalyVariant`) and spawn points, baseline record/restore, save version 2, `p9.Campaign.*`.

**4B – story:** Dadar → Thane opening, Marathi PA (`AP9StationAnnouncer`, edge-tts), chaiwala epilogue. Random anomalies only on the footbridge (FOB) and Platform 9/10.

**4C – polish:** CR livery, Vashi local hidden at its hold stop, final scare. APK 4C at `..\_builds\ThanePlatform9_Android_4C\` (never installed on a device).

**4D – harder anomalies:** ten anomaly improvements, four difficulty tiers, three staged jumpscares, entities in `/Game/P9/Entities`. `p9.Anomaly.Audit`: 48 variants, 0 problems.

**5A – station life (v1 and v2):** friend NPCs, chai stall "Chai Ka Vishwaguru" on the FOB concourse, routines, meet-ups with Hindi/Marathi TTS conversations, the chaiwala's rally speech, the silhouette stalker (red saree). The 5A v1/v2 low-poly characters are now superseded by 5C but still on disk.

**5B – rules and lighting:**
- Anomaly inventory: `Stage5B_AnomalyInventory.md`.
- Friends F1–F5 are **hidden on Days 1–10** and available from the epilogue (`bHideUntilCampaignComplete`); the silhouette is locked on Days 1–10 and allowed in the epilogue (`bAppearInEpilogue`). CVar `p9.Friends.ForceUnlock -1|0|1`.
- Chaiwala's rally line once per campaign day (`bOncePerCampaignDay`, reset on `OnDayStarted`).
- Vashi local reset each new day (`AP9Train::ResetToStop`, `ResetConnectingTrainForNewDay`).
- 41 station lights retuned to 4200 K in place (no baked lighting; the map has `bForceNoPrecomputedLighting`). Tools: `ue_py/light_5b_apply.py`, `light_5b_restore_original.py`.

**5C – scanned NPCs (user's own 3D models):**
- Seven 1M-triangle scans from `C:\Users\niles\Downloads\3d model\<name>\` replace all NPCs: dhanraj → F1 Curly, Nilesh → F2 Stripe, shivshankar → F3 Specs, Rohit → F4 Selfie, rupesh → F5 Backpack, Modi → Chaiwala, kanchana → Silhouette. Slot heights 1.77/1.72/1.69/1.74/1.80/1.70/1.64 m.
- Optimised to 15k tris with colour/normal/roughness baked from the full scan (2048²), rigged on the 5A 23-bone skeleton, 3 LODs (100 % / 33 % / 10 %; screen sizes 1.0/0.35/0.12).
- Crossfading clip player `UP9NPCAnimInstance` (no Anim Blueprint); NPC walking eases in/out, turns smoothly, walk clip rate follows speed.
- Specs' bench stop fixed: the PF1 steel bench's **seats face north (+Y, towards the tracks)**, backrest on the south edge (y ≈ −1300). Sit stop (1800, −1250) facing 90.
- Epilogue: up to 3 friends near the Dadar local board while the player is seated, sit near the player, and get off as soon as the player stands up or leaves.

**5D – animation and chaiwala fixes (current):**
- Clip set **A3** (current): arms clear the body (hang angle measured along the whole arm, 13–18°), elbows folded ~22°; seated hands rest on the thighs via arm IK (elbows always bend); walk with both legs solved by IK and a smooth, reduced hip bob (planted foot ≤ 0.5 cm vertical drift). The chaiwala got a Walk clip.
- Epilogue chaiwala: the stall chaiwala NPC (animated, Modi) plays the wake-up by mirroring the director's now-hidden static proxy, then walks off the train, along Platform 1, up the PF1 stairs and the footbridge back behind his counter (`EpilogueReturnPath`) and resumes vending. The director used to stop moving him at 15 s (he froze in the doorway); fixed.
- The chaiwala holds a ketli (right hand) and a cutting-chai glass (left hand): `/Game/P9/Props/ChaiProps/`.
- Friends getting off: react within ~1 s of the player standing, hurry out; if the doors already shut they slip out only while unseen.

**5E – Unreal retargeting, name boards, studio framework (uncommitted):**
- Motion-captured idle + walk from Unreal's Mannequin, retargeted in-engine (IK Rig + IK Retargeter) onto all seven scans. Clips: `Station3D/<n>/MX/SK_P9_NPC3D_<n>_MX_<Idle|Walk>` (in use for idle/walk; sit, chat, gestures, pour, menace, turn stay A3).
- The Mannequin faces +Y and the scans face +X; the retargeter needs both facing the same way, so the Mannequin was exported and re-imported rotated −90°: `/Game/P9/Characters/MannyX/` (`SKM_MannyX`, skeleton, `Anims/SKM_MannyX_Idle|Walk`). FBX in `P9_Assets/MannyX/Export/`.
- Mannequin source copied (not modified) from the engine template into `/Game/Characters/Mannequins/` (meshes, materials, textures, rigs, unarmed idle + walk).
- IK rigs and retargeters: `Station3D/Retarget/` (`IK_MannyX`, `IK_NPC3D_<n>`, `RTG_MannyX_to_<n>`). Scripts: `ue_py/rt_5e_retarget_mx.py` (all), `rt_5e_pilot_mx.py`, `npc3d_5e_apply_mx.py` (RESTORE=True → A3 idle/walk), `npc3d_5e_walkspeed.py`.
- NPC `WalkSpeed` per instance = the clip's own speed (≈127 cm/s × hip height / 0.89 m): dhanraj 127, Nilesh 123.3, shivshankar 121.1, Rohit 124.9, rupesh 129.3, Modi 121.9; silhouette `WalkClipSpeed` 117.4.
- Unused test assets (approval needed to delete): `Retarget/IK_Manny`, `RTG_Manny_to_dhanraj`, `RTG_Manny_to_dhanraj_tA/_tB`, `RTG_MannyX_to_Modi_MXa/_MXb`, `dhanraj/MC/*`, `Modi/MXa`, `Modi/MXb`. A retarget-pose arm splay had no measurable effect.
- Thane name boards (`S4B_NameBoard_Thane_PF1/PF9_E/W`, a hanging sign stood 3.25 m up in mid-air): two 7 cm steel posts each (`S5E_NameBoardPost_*`, `MI_P9_SteelBlue`, BlockAll), script `ue_py/board_5e_posts.py`.
- Map SHA after 5E starts `a90e48b6f374`. Backups `pre5E_*`, `pre5E2_*`.

**5F – Mannequin skeleton per character, hanging boards (uncommitted, current):**
- All seven scans re-rigged on **Unreal's Mannequin skeleton** (88 bones: spine_01-05, neck_01-02, twist bones, 5-finger hands, ik_* bones), fitted to each person's joints and height with the Mannequin's bone orientations kept, so Unreal animation applies without axis distortion. Blender: `StationNPCs3D/fit_manny_npc3d.py` (scan posed into the Mannequin rest pose, bones fitted, voxel-proxy heat weights) → `<n>/v002/P9_NPC3D_<n>_MQ2_v001.blend`, `Export_MQ2/`. Imported with yaw −90 (like MannyX) → `Station3D/<n>/MQ2/SK_P9_NPC3D_<n>_MQ2` (+ skeleton, physics asset), materials `MI_P9_NPC3D_<n>`, LODs.
- Clips on MQ2 (all in use): `MQ2/…_MQ2_Idle|Walk` = Unreal mocap retargeted from MannyX; `MQ2/…_MQ2_Chat|LookAround|TurnInPlace|Gesture|Pour|Menace|TalkAngry` = moved from the old rig (A4/A3) by IK Retargeter; `MQ2S/…_MQ2S_SitDown|SitIdle|StandUp` = built on the MQ2 rig (`sit_mq.py`: hips onto the 46 cm seat with per-body calibration, feet planted, hands palm-down on the thighs, Mannequin rest fingers). Unreal ships no sit/talk/gesture mocap; the user chose to keep these clips.
- Retarget assets: `Retarget/IK_NPC3D_MQ2_<n>`, `RTG_MannyX_to_MQ2_<n>`, `RTG_Old_to_MQ2_<n>`. Scripts: `ue_py/mq_5f_setup.py`, `mq_5f_apply.py`, `npc3d_5d_props_apply.py` (kettle/glass offsets re-measured for the MQ2 hands).
- Epilogue chaiwala: the director walks him at a steady 100 cm/s with 0.5 s ease (was a smoothstep peaking ~140 cm/s), and his walk clip rate is measured in the carriage frame (the moving train used to count as walking). `P9CampaignStory.cpp` (protected; user request) and `P9FriendNPC.cpp`.
- Thane boards hang from the canopy ceiling on two 3 cm rods each (`S5F_NameBoardHanger_*`, script `ue_py/board_5f_hang.py`); the 5E floor posts were removed (user request); PF1_W moved to x −1925 and PF9_E to x 1930 (they were in the canopy gaps above the stairs).
- Superseded/unused (approval needed to delete): `Station3D/<n>/MQ/` (v1 import with a broken 0.0001-scale skeleton) and `Retarget/IK_NPC3D_MQ_dhanraj`, `RTG_MannyX_to_MQ_dhanraj`, `RTG_Old_to_MQ_dhanraj`; `Station3D/<n>/A4/` (source of the moved clips, keep while MQ2 clips are regenerated from it), `MX/`, `MXa/MXb`, `MC/`; Blender `Export_MQ/`, `*_MQ_v001.blend`, `_MQ2S_try1/`.
- Map SHA after 5F starts `ce76c5e8ba4a`. Backups `pre5F_*`, `pre5F2_*`; save `SaveGames_pre5F_*`.

**6A – new anomaly set (2026-11-07, uncommitted, current):**
- The user asked to remove every old anomaly and build eleven new ones. The 21 old catalogue entries and their handlers (lamp, extra prop, train board/seats/lights, footsteps, horn, figures, static props) and the jumpscares A/B/C were **deleted from C++** (user choice). The epilogue's own scares and `RecordLight` remain.
- **Rules:** an anomalous day has **exactly one** anomaly (`AnomalyCountWeights {1}`). **Kanchana** is scheduled (`bScheduled`, days 3–5): exactly one seeded day per run, and that day she is its only anomaly. The other ten are random (seeded, repetition penalty as before). A variant's `PointMatch` limits it to points whose id contains the text.
- **The eleven** (`BuildDefaultCatalogue`; handler in brackets):
  1. `ClockWrong` (DynamicClock: canvas render target with the dot-matrix glyph atlas `T_P9_Anom6_ClockDigits`): RunsBackwards / Racing / WrongHour 03:47 / Impossible 27:63.
  2. `IndicatorWrong` (MaterialSwap): PANVEL / DADAR / THANE / PanvelLater.
  3. `EndlessFootbridge` (LoopTeleport, `LOOP_FOB`): going north past y 6900 on deck B sends the player back 1480 cm; walking south is free.
  4. `Kanchana` (Stalker): `AP9SilhouetteStalker::BeginCampaignSighting` at one of 6 `KANCHANA_*` points. She waits; once seen she follows and speaks a line every 10 s (within 18 m).
  5. `AnnouncementWrong` (PAAnnouncement): Platform13, DoNotTurn, Slowed (4B) plus new Platform10 and Panvel (edge-tts, `SW_P9_PA_PF9_Anom6_*`).
  6. `SignboardWrong` (MaterialSwap, slot `Sign_`): arrows reversed or wrong number on the FOB signs PF2/3, 4/5, 6/7, 8, 9/10 and the PF9/10 island signs.
  7. `ClosedEntryOpen` (HideActor): the gate / hoarding / barricade, its S2 collision block (+ PF6/7 tarp) and the CLOSED plate disappear; the stairs down work.
  8. `PosterBoy` (PosterEyes): the ad becomes the "Shakti Doodh" boy; two pupil discs (engine sphere, `MI_P9_Anom6_PosterPupil`) track the camera. Variants BoyWatching / Later.
  9. `EndlessStair` (LoopTeleport, `LOOP_Stair`): 3 steps from the bottom of the PF9/10 lower flight → the same step above the landing (−660, 0, +300); climbing up is free.
  10. `TrackMissing` (MeshSwap: hides the static track, spawns a movable copy): `SM_P9_TrackPanel_GapRails` / `_GapAll`, 10 m gap at x −1400..−400. Only `TRACK_T10` is listed: track 9 is under the Vashi local whenever the player reaches PF9 (`TRACK_T9` point kept, unlisted).
  11. `TicketCounterWrong` (MaterialSwap, slot `BookingCounter`): horizontal bars with windows 2-3-1 / 3-1-2 / 1-7-4, or bars only.
- Loops dip the screen (0.45 s fade) at the jump; they are not seamless.
- **Zone rule change** (`IsPointAllowed`, P9CampaignStory.cpp): Track points check only the point; ClosedEntry points skip their extra actors.
- **Assets:** `/Game/P9/Anomalies6A/` (Textures, Materials, track meshes). Sources in `P9_Assets/Anomalies6A/` (`gen_6a_textures.py`, `build_track_gap_6a.py`, `P9_TrackGap_6A_v001.blend`); `Announcements/Scripts/announcements_mr.json` (+2 items; old copy `_pre6A.json`). Import template `import_templates/import_6a.json`. Map scripts `ue_py/a6_materials.py`, `a6_setup.py`, `a6_drop_t9.py`.
- **Map:** 17 new points in `P9/Campaign/AnomalyPoints6A`. The director lists 25 points; the 27 old points are still in the level, unlisted. Map SHA after 6A starts `c1caced7f47d`. Backups `pre6A_*`, `pre6A2_*`, `src_pre6A_*`, `SaveGames_pre6A_*` (the live save was restored from it after testing).
- **Checks (PIE):** Audit 37 variants / 0 problems, Story.Validate 0, Verify 0 diffs, Sim 500 runs: 0 violations, Kanchana once per run on days 3/4/5. Evidence screenshots: `production/qa/evidence/stage6a/`.
- `Stage5B_AnomalyInventory.md` describes the old (deleted) set.

**6B – Kanchana navigation, announcement check, cleanup (2026-11-07, uncommitted, current):**
- **Kanchana walks the player's own path** (`P9SilhouetteStalker`, no navmesh): the player's feet are recorded every 50 cm (up to ~70 m; reset by jumps > 4 m such as teleports and loops, and on trains). She joins the trail only by a straight walk proven clear (capsule sweep on ECC_Pawn + floor probes every 60 cm, max 55 cm step). If she can't join, she waits; once far behind (path > 17 m, or > 11 m without a trail) and unseen, she reappears on the trail about 8–15 m of path behind the player, out of the camera's view. Used for both her scheduled day and the epilogue appearances. `p9.Silhouette.Status` shows trail/cursor/joins/catchUps. Tested in PIE: platform → both stair flights and the landing → corner → footbridge; catch-up after the player runs; seen from the footbridge through the grille she is not "seen" (the grille blocks the line of sight).
- **Announcements validated** (`production/qa/evidence/stage6a/6b_announcement_validation.txt`): the format and levels match the normal one, the chime is present, sentences 1/3/4/5 match the normal announcement (corr ≥ 0.997), and Whisper (medium, mr) hears "दहावर" (platform ten) and "पन्वेल" (Panvel). Both play in PIE.
- **Cleanup (user-approved):** removed 28 obsolete spawn points (27 old + `TRACK_T9`) and the director's 39 PreloadAssets entries for them; deleted 56 unused assets from `/Game/P9/Anomalies` and `/Game/P9/Entities` (folders `Audio4D`, `Entities` gone). Kept 21: SM_P9_Apparition and its materials/textures + the apparition sting (epilogue), SM_P9_AdStand + the chai poster (the level's ad stands), the 4D sign MIs/textures SignboardWrong uses, T_P9_Apparition_Body (pupil material). Method: C++ name grep + asset-registry referencers after the map was saved without the preload entries (`ue_py/a6b_cleanup_plan.py`, `a6b_cleanup_map.py`, `a6b_cleanup_delete.py`). Backups `L_Station_Redesign_pre6B_*`, `content_pre6B_*` (all 78 files + SHA256SUMS), `src_pre6B_*`.
- Map SHA after 6B starts `6b205bc53fa5`. Regression after the cleanup: Audit 37/0, Story.Validate 0, Verify 0, Sim 300 runs 0 violations, all 11 anomalies activate when forced.

**6C – analog railway clock, anomaly 12 "AnalogClockReverse" (2026-11-07, uncommitted, current):**
- **Model** (Blender 4.5.3 via Blender MCP; script `P9_Assets/AnalogClock6C/build_6c_clock.py`, blend `P9_AnalogClock_6C_v001.blend`, FBX in `Export/`; first mirrored attempt kept in `_v001_mirrored/`): double-faced hanging drum clock after the user's reference photo. Ø 60.2 cm drum, 16.6 cm deep, 85.1 cm high with the pole. `SM_P9_AnalogClock_Body` (drum + bezels, both dials, centre pins, pole + flange, rivets; LOD0/1/2 = 2949/1536/948 tris, screen sizes 1.0/0.25/0.10, one box collision), `_Glass` (480 tris, translucent), `_HourHand` 76, `_MinuteHand` 76, `_SecondHand` 132 (red); every pivot at the clock centre, hands point at 12 (+Z) and turn about Y. Dial `T_P9_AnalogClock_Dial` 1024² (`gen_6c_dial.py`, Inter font, SIL OFL). Materials `/Game/P9/Anomalies6C/Materials/` (`M_P9_AnalogClock` texture×tint/metallic/roughness, `M_P9_AnalogClockGlass` translucent, 6 MIs). Import template `import_templates/import_6c.json`.
- **C++** `P9AnalogClock.h/.cpp` (new, not protected): `AP9AnalogClock` – one station time drives 6 hand components (3 per face, front +Y, back −Y mirrored). Each day starts at `StartTime` 23:47:00 (via the director's `OnDayStarted`), `TimeScale` 1. `BeginReverse(rate, pause, slow, bWaitUntilSeen)` / `EndReverse()` (back to the correct time). Seen = within 26 m, view cone 0.86, not edge-on, unobstructed trace (checked every 0.2 s). Sequence: runs 0.8 s → second hand stops and twitches back for the pause → backwards at real speed for `slow` s (the second hand visibly first) → ramps (3 s) to rate× backwards. Hands are only re-posed when the body was rendered in the last 0.25 s. Console `p9.Clock.Status`, `p9.Clock.Set HH:MM[:SS]`.
- **Campaign** (protected files, on the user's request): `EP9AnomalyLocation::AnalogClock` and `EP9AnomalyHandler::AnalogClock` appended; catalogue #12 `AnalogClockReverse`, weight 0.8 (≈ 8 % of random anomalous days vs ≈ 9 % for each weight-1 anomaly); variants Backwards (diff 1: reverse all day at 1×), Unwinding (diff 2: on sight, pause 1.5 s, slow 6 s, 12×), Rewinding (diff 3: pause 2.5 s, slow 3 s, 60×). `CleanupDay` calls `EndReverse` on every analog clock; the Verify snapshot records `clockwise/reversed`; the audit checks the target is a complete clock. The digital `ClockWrong` and Kanchana's schedule are untouched. One anomaly per day means the two clock anomalies can never coincide (no exclusion tag needed).
- **Map**: `S6C_AnalogClock_Concourse` at (−250, 3150, 845) yaw 0 (dials face north/south along the route), flange on the concourse ceiling z 900, bottom 2.15 m above the deck, in front of the booking office; folder `P9/Environment/AnalogClock6C`. Point `S6C_Point_ANALOG_Clock` (`ANALOG_Clock`). Director: 12 anomalies, 26 points. Scripts `ue_py/c6_assets.py`, `c6_lods.py`, `c6_setup.py`, `inspect_6c*.py`. Map SHA after 6C starts `33142b34572d`. Backups `L_Station_Redesign_pre6C_*`, `src_pre6C_*`, `SaveGames_pre6C_*` (live save restored after the tests).
- **Checks (PIE)**: Audit 40 variants / 0 problems; Story.Validate 0; Verify 59 entries / 0 differences; Map Check 0/0; Sim 500 runs (seeds 777, 4242): 0 violations, 0 non-deterministic, same seed → identical output, Kanchana once per run on days 3–5, exactly one anomaly per anomalous day. Both faces read 23:47 → clockwise on normal days; all three anomaly variants run counterclockwise; cleanup and load restore clockwise + correct time; the other 11 anomalies (incl. digital ClockWrong, shown 27:63) leave the analog clock normal. A jump under the clock bumps its collision (no mantle). Evidence `production/qa/evidence/stage6c/`.
- Limits: the digital PF9/10 clock is a fixed 23:47 texture on normal days, so after a few minutes the two clocks disagree by the elapsed minutes; second hand has a tangent warning (no normal map, harmless); no Android test; the clock does not tick in steps (smooth sweep).

**7A – Mixamo rigs and clips for every station NPC (2026-11-07, uncommitted, current):**
- Same workflow as 5G Dhanraj (`ue_py/mx5g_dhanraj_setup.py` / `_apply.py`, Blender `StationNPCs3D/mixamo_npc3d.py` – both unchanged). Sources: `Downloads/new_3d_model/<Name>/` (one `*_normal_walking.fbx` with skin each; the rest are skinless clips). Blender: `StationNPCs3D/mixamo_npc3d_7a.py` (copy + `sit` mode: seated clip's hips moved to the MQ2S distance behind the root, feet kept on the floor), runners `run_7a_mesh.ps1`, `run_7a_anims.ps1`; outputs `<name>/mixamo/` (`P9_NPC3D_<name>_MX_v001.blend`, `Export_MX/`, first sit try in `_7a_sit_try1/`).
- Mesh = the existing 15k-tri optimised scan (v002, baked 2048² textures) on the Mixamo skeleton with weights transferred from the Mixamo skin (0 unweighted verts). **Unreal strips the `mixamorig:` prefix** – bones are `Hips`, `RightHand`, ... Bones: Nilesh 41, shivshankar 33, Rohit/rupesh/Modi/kanchana 65 (Dhanraj 33). Import scale = character height / Mixamo skin height (1.90 m): Nilesh 0.9043, shivshankar 0.8890, Rohit 0.9168, rupesh 0.9444, Modi 0.8952, kanchana 0.8636 (heights 172/169/174/180/170/164 cm), yaw −90. Template `import_templates/import_7a_mixamo.json`.
- Assets `Station3D/<name>/MX7A/`: `SK_P9_NPC3D_<name>_MX` (+ `_Skeleton`, `_PhysicsAsset`), `Anims/SK_P9_NPC3D_<name>_MX7A_<Clip>`. Mixamo: Walk, StairsUp (in place), SitIdle (F2–F5), Chat (Modi talking), Idle (kanchana). Retargeted in Unreal (`ue_py/mx7a_setup.py`; `Retarget/IK_NPC3D_MX7A_<name>`, `RTG_MannyX_to_MX7A_<name>`, `RTG_MQ2_to_MX7A_<name>`): Idle (MannyX), LookAround, TurnInPlace, SitDown/StandUp (MQ2S), Chat (MQ2), Gesture/Pour (Modi).
- One material slot each (existing `MI_P9_NPC3D_<name>`); LOD settings `LODS_P9_NPC3D_MX7A` (copy of the shared one; screen 1.0/0.35/0.12; finger chains removed from LOD1/LOD2). Normal + Roughness of Nilesh/shivshankar/Rohit/rupesh/Modi capped at 1024 (BaseColor 2048; Kanchana and Dhanraj untouched); backup `textures_pre7A_*`.
- Map (`ue_py/mx7a_apply.py`, `mx7a_fix.py`): F2 Stripe, F3 Specs, F4 Selfie, F5 Backpack and the Chaiwala use MX7A meshes/clips. Clip speeds = Mixamo speed × scale (walk 140.1/142.8/92.5/103.4/88.2; stairs along the slope 43.9/44.8/43.3/48.4/41.3); WalkSpeed moved towards the clip pace by ≤ 15 % (140.1/139.3/106.2/109.9/103.6); StairsUpSpeed 60 as Dhanraj. Chaiwala props on sockets `RightHand`/`LeftHand`, offsets re-measured (`pie_7a_handpose.py`). **Kanchana (stalker) is not changed** (protected) – her MX7A assets exist, unused. Map SHA after 7A starts `2665bbab87ba`. Backups `L_Station_Redesign_pre7A_*`, `SaveGames_pre7A_*` (restored).
- Checks (PIE): line-ups of all six NPCs per clip (idle/walk/stairs/sit) vs Dhanraj; routines sampled 100 s – clips follow activity (Walk→Walk, Sit→SitIdle, climbing→StairsUp, Idle/Look/Turn/Chat); Specs on the PF1 bench, rupesh on the PF1 stairs, chaiwala at the stall with the ketli, Specs/Rohit in the epilogue train seats; no animation/skeleton warnings; Map Check 0/0; Audit 40/0; Story 0. Evidence `production/qa/evidence/stage7a/`.
- Limits: no Mixamo descending clip – stairs down use the walk; the station stairs are shallower than Mixamo's stair clip, so steps do not line up exactly; no Android measurement.

**7B – stair clips, Kanchana on Mixamo, NPC bodies, natural locomotion, new idles (2026-11-07, uncommitted, current):**
- **Stair clips fitted to the real stairs** (rise 16.667, tread 30 cm; PF1 and PF9/10: 18 steps + 150 cm landing + 18, step tops at whole rises): Blender `StationNPCs3D/stairs_npc3d_7b.py` builds one 2-tread cycle per character from its own Mixamo walk (the supplied ascending clips lean back with the head up), plants on tread centres, toe/heel clearance over nosings, IK legs; up 1.0 s (60 cm/s), down 0.9 s (66.7 cm/s), 7° lean going up. Kanchana: one cycle (frames 19–43) of her uneven walk + a gait time-warp. Runner `run_7b_stairs.sh`, checks `render_7b_stairs.py` (sheets `<n>/mixamo/MX7B_<n>_Stairs*.png`), `inspect_7b_gait.py`; first Kanchana tries in `kanchana/mixamo/_7b_try1..3/`. Assets `Station3D/<n>/MX7B/Anims/SK_P9_NPC3D_<n>_MX7B_StairsUp|StairsDown` (Dhanraj too), template `import_7b_stairs.json`.
- **C++** (`P9NPCLocomotion.h/.cpp` new): `ProbeFloor` gives the stair ramp (nosing found by bisection) + direction; `StairPhase` = frac(±z / 2 rise) locks the clip time to the height on the flight (`bStairClipsFitted`). Friends: `StairsDownAnim`, `StairsDownSpeed`, walk / stairs-up / stairs-down chosen from the floor; root follows the ramp (no riser jumps). Edge guard: no step drops > 45 cm; the old step-round-the-player point is validated (it used to land beyond stair walls).
- **Bodies:** capsule per person (`BodyRadius`/`BodyHalfHeight`: F1 27/88.5, F2 28/86, F3 27/84.5, F4 25/87, F5 28/90, CW 28/85, Kanchana 25/82), query-only Pawn overlapping pawns only, no overlap events (no project-settings channel). Separation in code (body registry): walkers steer round bodies and the player (keep-left head-on, eased), slow/hold behind (hysteresis), squeeze past after 2.5 s, hard no-overlap push; standing NPCs ease apart; seated/train/scripted/vendor never moved; held next to an occupied stop for > 1 s = arrived.
- **Natural locomotion (user: "like GTA 5", chose natural locomotion only):** pure-pursuit path following (smooth arcs, wall-checked), eased steering, ±5 % slow pace drift (clip rate follows).
- **Idles:** the user's new Mixamo idles for all six incl. Dhanraj (user choice) → `MX7B/Anims/..._MX7B_Idle` (`run_7b_idles.sh`, `import_7b_idles.json`). `Greeting/greeting_idle.fbx` ignored (user).
- **Kanchana:** MX7A mesh/Idle/Walk + MX7B Menace/TalkAngry (retargeted, `mx7b_kanchana_setup.py`, `RTG_MQ2_to_MX7A_kanchana`) + stair clips; navigation unchanged — only her mesh settles onto the steps and side-steps friends; `WalkAnimSpeed` 129.5 drives the walk rate, `WalkClipSpeed` (her pace) unchanged.
- Map (`ue_py/mx7b_apply.py`): instance properties only. Map SHA after 7B starts `cc0d2671183b`. Backups `L_Station_Redesign_pre7B_2026-11-07_203708.umap`, `src_pre7B_2026-11-07_202749`, `SaveGames_pre7B_*` (restored).
- **Test gotcha:** setting actor properties from Python in PIE reruns construction (body → single-node, NPC clips freeze). Use `set_editor_property(name, value, unreal.PropertyAccessChangeNotifyMode.NEVER)`; `StartRoutine` now re-creates the clip player if needed.
- Checks: stair probe (`pie_7b_stairprobe.py`): planted toe vs tread within ±2.5 cm, planted-toe drift median 7–17 cm/s; flat crossing/overtaking test (`pie_7b_gaps.py`): 0 overlaps NPC–NPC and NPC–player in 60 s; epilogue boarding/seating and chaiwala return OK; Audit 40/0, Story 0, Map Check 0/0. Evidence `production/qa/evidence/stage7b/`.

**8A – anomalies 13–16, seamless endless footbridge and stair, creatures, NPC boxes, original audio (2026-11-08, uncommitted, current):**
- **Catalogue now 16** (49 variants). New handlers appended to `EP9AnomalyHandler` (LightTint, TrainMaterial, TrainLightsOff, LanternFigure, EndlessBridge, ReverseEscalator) and locations (PlatformLamp, LanternSpot). One anomaly per day, seeded, everything baseline-recorded and restored.
  13. `RedLightPF9` (LightTint): one PF9 lamp's spot + fill go red (Crimson / BloodRed / DullRed, slow pulse), its tube glows red (MID `TubeColor`). Points `REDLIGHT_W1/W2/E`.
  14. `TrainSeatsRed` (TrainMaterial, slot `Train_Seat`, both cars): `MI_P9_Anom8_TrainSeat_Red/_DarkRed` (textures recoloured from `T_P9_TrainSeat`, `gen_8a_seat_textures.py`). Point `TRAINSEATS_Vashi`.
  15. `CoachLightOff` (TrainLightsOff): the point's car (`TRAINLIGHT_Car0/Car1`, TargetComponent) → both interior lights × residual (Dark 0.04 / Dim 0.14 / DiesOnApproach flickers 1.6 s within 12 m) and its tube MID `Intensity` 3 → 3×residual.
  16. `LanternMan` (LanternFigure, `AP9LanternMan`): static mesh (truly motionless), warm lantern light 3.5 cd r300 + emissive glass, at `LANTERN_PF9East` (2870, 7700, 0) yaw 190; only lamp E (spot, fill, tube) flickers in seeded bursts (0.8× between). Sounds on him.
  - `EndlessFootbridge` → **EndlessBridge** (`AP9EndlessFootbridge`, no teleport): walking north into `LOOP_FOB` (y 6300–6550) swaps the real north end (shell + floor, sign, tube, light, CCTV, end wall, parapet blocks = point extras) for movable copies; the end then recedes at the player's pace (never closer than at the swap), 600 cm bays (seamless UVs) with code-built box collision are laid behind it, a 1.2 % larger lead-in bay hides the joint. Walking back stops it; south of y 5900 looking away it collapses back unseen; walking north re-engages.
  - `EndlessStair` → **ReverseEscalator** (`AP9ReverseEscalator`): the PF9/10 finish is replaced by `SM_P9_StairPF910_Upper8A` + 19 moving `SM_P9_EscalatorStep8A` (rise out of the floor, slide under the landing); Pawn-only ramp 1 cm over the real cube nosings. Walking down: progress taken back each frame (held); standing: carried up at 55 cm/s; walking up the flight stops it; heading down again restarts it.
  - Kanchana's variant `PointMatch = KANCHANA_PF9` (starts on Platform 9; points checked clear: no overlap, 98–124 cm to the nearest column). Her code unchanged.
- **Greeting** (`AP9GreetingScare`, `P9CampaignStory.cpp`): replaces only the aisle figure at ride 34.2 s – stands 0.5 s, lunge pose + scream, 0.22 s ease-in to face 64 cm from the camera (FOV → 62, hands stay 18 cm in front of the camera), impact + shake, `Finish()` at the 35.4 s blackout. The passenger figure during the flicker is unchanged; without her meshes the old figure is used.
- **Creatures** from `Downloads/new_3d_model` (user's choice): `StationNPCs`-style bake, `P9_Assets/Creatures8A/optimize_creature_8a.py` (rest pose = sculpt; Lantern Man's idle frame tears the lantern). `SM_P9_LanternMan` 30k tris (embedded shaded texture; the download folder's PBR maps do not fit its UVs), `SM_P9_Greeting_rest` / `_lunge` 40k (PBR maps fit). Material `M_P9_Creature8A_v2` (v1 never compiled – sRGB defaults on Masks samplers). FBX face +Y → actors use mesh yaw −90.
- **Pieces**: `P9_Assets/Footbridge/Endless8A/build_endless_8a.py`, `build_escalator_8a.py` → `/Game/P9/Anomalies8A/Endless/`. The finish has no tread over x 860–890 (pre-existing; the grid cube shows there on normal days).
- **NPC hybrid body**: `UBoxComponent BodyBox` (radius × 0.6 deep, × 0.95 wide, query-only overlap pawns, no events) + registry boxes; hard no-overlap and standing separation use a 2D SAT box test; a step that cannot be resolved is held (deadlock guard `PassThroughAfter` 8 s). `p9.Friends.Overlaps`. Dhanraj: `DrunkChance` 0.
- **Audio**: 55 procedural WAVs (`P9_Assets/StationAudio/OriginalAudioPack8A/gen_8a_audio.py`, numpy only) → `/Game/P9/Audio/OriginalAudioPack/{Music,Ambience,SFX,Creatures}`, sound classes `SC_P9_*`, attenuation `ATT_P9_Positional_Near/Far`. `AP9AudioDirector` (`S8A_AudioDirector`): station / Platform 9 / anomaly-after-seen / epilogue layers, player footsteps, Kanchana. Docs `docs/audio/`. `p9.Audio.Status`, `p9.Anomaly.Helpers`.
- Map SHA after 8A starts `c97e2fa1ec87`. Backups `L_Station_Redesign_pre8A_2026-11-07_234003.umap` (SHA `2f1179b2…`, the editor's own save-on-close of 21:57 after 7C), `src_pre8A_*`, `SaveGames_pre8A_*` (restored).
- Checks (PIE): Audit 49/0, Story 0, Sim 1000 runs 0 violations / 0 non-deterministic, Verify 0 after every cleanup, save/load OK, Map Check 0/0, NPC box overlaps 0 in 180 polls. Evidence `production/qa/evidence/stage8a/`.

**9A – visual polish, audio, NPC transitions and stuck fix (2026-11-08, uncommitted, current):**
- **Lantern Man** point `S8A_Point_LANTERN_PF9East` (id unchanged) moved to the dark WEST end by the only PF9/10 train indicator (x −2100): (−2860, 7440, 0) yaw −10; his flickering lamp is now W1 (`S2C_Lamp_PF9_10_W1` + tube + fill). The request said "east … near the indicator"; the indicator is at the west end, so the indicator was used — confirm with the user.
- **RedLightPF9**: only `REDLIGHT_W1` (lamp beside the indicator) is listed; `REDLIGHT_W2`/`_E` unlisted from the director (actors kept). Director 31 points. Shares lamp W1 with the Lantern Man (never on the same day).
- **Signs**: `M_P9_SignFace` got an emissive (texture^`EmissiveContrast` 1.6 × Tint × `EmissiveStrength` 1.0) — all sign MIs incl. the anomaly variants glow alike; `MI_P9_SignClosed` EmissiveStrength 0. 18 `S9A_SignGlow_*` point lights (6 cd, r 260, unshadowed, warm red) inside the platform / FOB sign boxes (folder `P9/Lighting/SignGlow9A`).
- **NPC lighting**: cause = scan albedo with baked flash (linear mean 0.45–0.48, p90 0.75–0.79 for dhanraj / Modi / Rohit). `M_P9_NPC3D` got a Custom-node albedo calibration (`AlbedoKnee`, `AlbedoMax`, `AlbedoSaturation`, identity defaults) + `SpecularLevel`; per-character values on the existing MIs (Kanchana untouched). Exposure / lights unchanged.
- **NPC stuck (root cause)**: Specs' sit stop (1800,−1250) was inside the PF1 bench collision (y −1311..−1239) and the chaiwala's return path cut through `S3D_SteelBench_Conc_3425`; the 7B floor guard then held them for ever. Data fixed (Specs stop y −1230; path via (300,3335)→(470,3350)). Code (`P9FriendNPC`): route-target validation at BeginPlay (points inside furniture moved to clear floor, legs crossing furniture logged), `RecoverBlocked` (arrive / back out / detour / skip), 4 s progress watchdog, arrival before floor checks, a vendor still in the epilogue scene is returned to his counter when a day starts.
- **NPC transitions**: stop = 0.45 s blend with the outgoing stride slowing to rest (`UP9NPCAnimInstance::SetOutgoingRateDecay`), set-off 0.35 s, arrival eases to 12 cm/s, walk rate clamp 0.12; on-the-spot turns accelerate / decelerate (320°/s²) with the turn clip rate following the turn speed; paths turning back > 110° → slow to a stop along the facing, turn on the spot, walk on; yielding to the player decelerates instead of freezing.
- **Audio** (`gen_9a_audio.py`, 55 WAVs → `/Game/P9/Audio/OriginalAudioPack9A/`): footsteps per surface walk/run L/R (cadence from speed, settling step, silent when still), thunder after lightning (moon-light flash detection, delay = distance/343 m/s, storm controller untouched; `p9.Audio.Thunder`), slower deeper wind (0.20). **`AP9TrainAudio`** (new, `S9A_TrainAudio`): roll / traction / brake / idle layers from speed + state, clatter per axle per 13 m joint, brake application, standstill vent, approach horn; trains' RunLoopSound / BrakeSound cleared on the instances, HornSound → `SFX_Train9A_Horn_Long`. `p9.TrainAudio.Status`. Docs in `docs/audio/`.
- Map SHA after 9A starts `21b3c6a74fff`. Backups `L_Station_Redesign_pre9A_2026-11-08_131648.umap` (c97e2fa1…), `pre9A2_2026-11-08_135749` (b2973b70…), `src_pre9A_*`, `content_pre9A_*` (materials), `SaveGames_pre9A_*` (restored). Scripts `ue_py/a9_map.py`, `a9_audio_setup.py`, `a9_signglow.py`, `pie_9a_*`. Evidence `production/qa/evidence/stage9a/`.
- Checks (PIE): Audit 49/0, Story 0, Sim 500 runs 0 violations / 0 non-deterministic, Verify 0 after cleanup (Lantern, red light), Map Check 0/0, NPC overlaps 0, Specs full bench cycle, chaiwala epilogue return to the stall. Audio NOT listened to by Claude.

**9B – main menu, How to Play, settings, captions, cinematic letterbox (2026-11-08, uncommitted, current):**
- **`AP9MainMenu`** (`S9B_MainMenu`, folder `P9/UI`; Slate built in code, every screen in a ScaleToFit box inside the safe zone): main (Continue only with a valid campaign save / New Game / How to Play / Settings / Quit, hidden on iOS), How to Play board (yellow signboard header, 4 sections, joystick / look / jump icons, red IMPORTANT band; Back + Start or Continue), Settings, New-game confirmation when a save exists. Background: slow drifting camera at the east end of PF9 (instance props CameraFrom/To/Rotation). Menu music `BGM_Menu_9B` (fades in 2.5 s, out 1.8 s) + UI click / confirm / back. While up it HOLDS the director at its first-tick start (`AP9MainMenu::HoldsCampaign`, a 3-line check in `P9CampaignDirector::Tick` — protected file, user-requested), hides the mobile controls, disables the audio director's layers. New Game lowers `Director->bUseSaveGame` for the first tick only (fresh run + opening; raised again at once). First New Game shows How to Play once (`bSeenHowToPlay`). Console `p9.Menu.Status|Show|Start new|continue|Screen main|howto|settings|confirm`.
- **`UP9SettingsSubsystem`** (`P9GameSettings.h/.cpp`, slot `P9Settings`): master (audio-device primary volume; mute = 0), music (SC_P9_Music), effects (SC_P9_Player/Railway/NPC/Creature/HorrorFX + the default class = announcements / voices / old SFX), ambience (SC_P9_StationAmbience) via one runtime sound mix; captions on/off. `p9.Settings.Status|Set`.
- **Captions** (`P9Captions.h/.cpp`, `UP9CaptionSubsystem`, `DT_P9_Captions` / `FP9CaptionRow`, 31 rows): bottom-centre, white on a 58 % dark rounded box, fades, wrap 1080, min 1.2 s per line, above the interaction prompt. Timings measured from the WAVs (`P9_Assets/Captions9B/build_captions_9b.py` → `captions_9b.csv`; copies in `docs/audio/`). Covered: opening train sound, PF1 arrival PA (opening + epilogue), chaiwala wake + call, epilogue "no Airoli" PA, Kanchana SH_1–5, Greeting scream, Lantern Man stinger, strong / close thunder, approach horns. Gameplay PA variants are NOT captioned (rule 8). Hooks (one line each): announcer, director PlaySound2D/PlaySoundAt (protected), stalker line, Greeting, Lantern Man, audio director thunder, train audio horn. The chaiwala's centre title was replaced by the caption (`P9CampaignStory.cpp`, protected). `p9.Captions.Test|Status`.
- **Letterbox**: eases in (≈1.7 s) while the player is seated in the opening / epilogue-wake train until its doors open, then out.
- Audio: `gen_9b_audio.py` → `/Game/P9/Audio/OriginalAudioPack9B/{Music,UI}` (4 files). `p9.Audio.Status` shows the last footstep asset.
- Map SHA after 9B starts `78a478e708bf`. Backups `L_Station_Redesign_pre9B_2026-11-08_141935.umap` (21b3c6a7…), `src_pre9B_2026-11-08_141321`, `SaveGames_pre9B_*` (campaign save restored; `P9Settings.sav` is new, seenHowTo = 1 from testing). Evidence `production/qa/evidence/stage9b/`.

**9C – footsteps per surface, bench sitting, material polish (2026-11-09, uncommitted, current):**
- **Footsteps** (`AP9AudioDirector`, `P9AudioDirector.h/.cpp`): six surfaces `EP9StepSurface` (Platform, Footbridge, Concourse, Stairs, Metal, Train) from the floor the movement component stands on (AP9Train → Train, AP9ReverseEscalator → Metal, the actor found under `ConcourseProbe` (0,3000,650) → Concourse, deck level → Footbridge, any level between platform and deck → Stairs, else Platform; the walkable collision is all WorldGrid cubes, so materials cannot be used). Sets `Steps9C_<Surface>` (`FP9StepSet`: WalkL/R ×4, RunL/R ×3, Scuff ×2, Gain) default to `/Game/P9/Audio/OriginalAudioPack9C/Player/` (CDO defaults – no map change). Played in 3D at the stepping foot (`FootSpacing` 11 cm, `PlaySoundAtLocation` + a transient near-field attenuation: full level within 4 m, non-spatialized blend 260→90 cm); `bSpatialFootsteps` false = 2D. Scuff only on a stop from a run (> RunSpeed, decel > 1400 cm/s²) or a pivot > 320°/s at running pace, 0.7 s cooldown. 9A/8A sets remain the fallback. `p9.Audio.Status` adds `surface N scuffs N spatial N`.
- **Audio**: `P9_Assets/StationAudio/OriginalAudioPack9C/gen_9c_audio.py` (heel force pulse → surface modes + slap + shoe tock, roll friction, ball plant, toe-off, texture, swish, room) → `Out_v4/` imported (v1 `Out/`, `Out_v2/` = copy of v1, `Out_v3/` kept). `analyse_9c.py` = objective checks. Template `import_templates/import_9c.json`. Docs `docs/audio/` (9C sections, `manifest_9c.json`). **Not listened to by Claude.**
- **Bench sitting** (`P9FriendNPC`): on arrival at a Sit stop `BeginSitAlign` probes the seat behind the stop along the sit facing (`ProbeSeat`: first hit 30–62 cm above the floor, edge refined to 1 cm) and computes the stand point that puts the pelvis `SitSeatInset` (20 cm) behind the seat front after the clip's hip shift (`SitHipShift`, 0 = 22 cm × height/1.75 m; measured for Specs 20.4 vs 21.2). New SitPhases: 5 short walk (> 22 cm), 0 turn + shuffle onto the spot (22–30 cm/s, hidden by the turn clip's steps), 4 settle `SitSettleTime` 0.35 s, 1 sit, 2 seated (`MeasureSeated` logs pelvis / feet once, only when rendered), 3 stand up, 6 settle `StandSettleTime` 0.5 s, then walk. Blends: →SitDown 0.3, SitDown→ 0.25, →StandUp 0.25, StandUp→ 0.35 s (were 0.15). Epilogue train seats: a settled stand before sitting. Test helper `p9.Friends.Sit <name|key>`, CVar `p9.Friends.SitInset`. Specs: pelvis 20.0 cm behind the seat front, 5.5–5.8 cm above the seat, feet 20 cm ahead (was 1.8 cm in FRONT of the seat edge – perched / floating since the 9A stop move).
- **Materials**: `M_P9_TexturedLit` (master of ~40 station MIs) extended with identity-default params: `BaseBrightness`, `BaseSaturation`, `RoughnessVariation` (roughness from texture luminance), `Specular`, `NormalMap` + `NormalStrength` behind static switch `UseNormalMap` (off = no extra sample). New textures `/Game/P9/Environment/Materials9C/Textures/` (`P9_Assets/Materials9C/gen_9c_textures.py`): `T_P9_FOBChequer_9C` (1024², aged ivory instead of paper-white, grout lines, traffic dirt) + normal maps `T_P9_<FOBChequer|StairTread|IslandSurface|Plaster|PrecastWall|CeilingPanel|SteelPerforated|Granite>_N9C` (DirectX green, from luminance relief + explicit joints). MIs tuned: FOBChequer, SteelPerforated / SteelSatin (benches 0.5 / 0.55 brightness), Plaster (0.8), CeilingPanel (0.82), StairTread (desaturated 0.55), IslandSurface, PrecastWall, Granite. NPC materials (9A calibration) unchanged – checked under the concourse tubes.
- **Note:** the 9B caption-wrap and menu-focus fixes had been applied by Live Coding only; the 9C full build includes them (captions confirmed full-width). Always finish with a full Build.bat.
- **No map change in 9C** (map SHA still `78a478e708bf`). Backups `src_pre9C_2026-11-09_151613` (66 files + SHA256SUMS), `content_pre9C_2026-11-09_154038` (19 material assets + SHA256SUMS), `SaveGames_pre9C_2026-11-09_151220` (restored after testing). Scripts `ue_py/c9_*.py`, `pie_9c_sitpose.py`. Evidence `production/qa/evidence/stage9c/`.
- Checks (PIE): all six surfaces detected with the right set (walk / run, L/R alternation, scuff on a run stop); Specs sit-down / stand-up bursts (10 fps); Audit 49/0, Story 0, Sim 300 runs 0 violations / 0 non-deterministic, Verify 0, overlaps 0 (3 polls), save/load round trip, Map Check 0/0. A PIE start once crashed in the Python plugin right after the material-graph script (editor restart fixed it).

## 4. Station NPCs — key facts (5C/5D)

**C++ (non-protected unless noted):**
- `P9FriendNPC.h/.cpp`: station NPC (roles Group, Vendor, Commuter, BenchSitter, StallVisitor, Wanderer; routes of `FP9NPCRoutePoint`).
  - Activities include `Train` (epilogue boarding) and `Scripted` (vendor epilogue wake-up).
  - Properties added in 5C/5D: `bRideEpilogueTrain`, `BoardRadius`, `HeldMeshRight/Left`, `HeldSocketRight/Left`, `HeldOffsetRight/Left` (hand-bone space), `EpilogueReturnPath`.
  - Hand props are created **at runtime only** when a mesh is set (empty static mesh components trip Map Check).
  - The floor trace never climbs more than 35 cm in a step (it used to step onto bench seats). Inside the train, path Z is explicit.
  - Train layout constants mirror the story's: car floor 110 cm, car centres ±1066.5, doors at −690/0/690 car-local.
  - The capsule has NO collision; beyond 30 m NPCs tick at 5 Hz.
- `P9NPCAnimInstance.h/.cpp`: crossfade clip player (proxy evaluates two `UAnimSequence`s and blends).
- `P9StationLife.h/.cpp`: meet-ups and conversations (excludes unavailable NPCs).
- `P9SilhouetteStalker.h/.cpp`: stalker; crossfades; walk rate follows her follow speed (`WalkClipSpeed` 105).
- **Protected, changed on the user's explicit request (5D):** `P9CampaignStory.cpp` — the epilogue chaiwala movement runs in every epilogue step; if a vendor NPC can play the wake-up (`CanPlayEpilogueWake`), the static chaiwala becomes an invisible proxy and the NPC mirrors it (`BeginEpilogueWake` / `EndEpilogueWake`). Without a vendor NPC the old static chaiwala is shown.
- Console: `p9.Friends.Status`, `p9.Friends.Meet [C1..C6]`, `p9.Friends.ForceUnlock -1|0|1`, `p9.Silhouette.Status`, `p9.Silhouette.Force 0|1`, `p9.Story.Epilogue`.

**Unreal assets (current):**
- `/Game/P9/Characters/Station3D/<dhanraj|Nilesh|shivshankar|Rohit|rupesh|Modi|kanchana>/`: `SK_P9_NPC3D_<n>`, `_Skeleton`, `_PhysicsAsset`; clip sets `A3/SK_P9_NPC3D_<n>_A3_<Clip>` (**in use**), `A2/` and the first `SK_P9_NPC3D_<n>_Anim_<Clip>` (superseded).
- `Station3D/Textures/T_P9_NPC3D_<n>_<BaseColor|Normal|Roughness>` (normal: green flipped; roughness: masks); `Station3D/Materials/M_P9_NPC3D` + `MI_P9_NPC3D_<n>`; `Station3D/LODS_P9_NPC3D`.
- `Station3D/Static/SM_P9_NPC3D_Modi`: static epilogue chaiwala (director `ChaiwalaMesh`; now only an invisible proxy).
- `/Game/P9/Props/ChaiProps/`: `SM_P9_Ketli`, `SM_P9_ChaiGlass`, `MI_P9_ChaiProps`, `T_P9_ChaiProps_*`.
- Superseded (on disk): `/Game/P9/Characters/Station/*` (5A v2), `/Game/P9/Characters/Friends/F*` (5A v1).

**Map actors (folder `P9/Characters/Station`):** `S5B_Friend_F1_Curly` … `S5B_Friend_F5_Backpack`, `S5B_Chaiwala` (512, 3650, 690) facing 180, `S5B_ChaiStall_Vishwaguru` (475, 3650, 600) facing 180 (footprint x 396–590, y 3543–3771), `S5B_StationLife`, `S5B_Silhouette`. Routes, roles, assets and props are set **on the instances**.

**Routines:**

| Friend | Role | Route |
|---|---|---|
| F1 Curly (dhanraj) | StallVisitor | Concourse ↔ stall front (330, 3640, 600) ↔ PF1 stairs ↔ Platform 1 |
| F2 Stripe (Nilesh) | Commuter | PF1 wait (1100, −1050) ↔ PF1 stairs ↔ FOB ↔ PF9/10 stairs ↔ PF9 wait (1700, 7100) |
| F3 Specs (shivshankar) | BenchSitter | PF1 steel bench middle seat: stop (1800, −1250) facing 90 (seats face the tracks); approach (1690, −1200) |
| F4 Selfie (Rohit) | Wanderer | West end of Platform 1 |
| F5 Backpack (rupesh) | Wanderer | FOB and concourse ↔ Platform 1 |

- Chaiwala `EpilogueReturnPath`: (−1450,−1150,0) → (−1350,−1525,0) → (−50,−1525,600) → (0,−1250,600) → (0,2380,600) → (0,3000,600) → (300,3350,600) → (520,3460,600) → (512,3650,600).
- Geometry: PF1 stairs y −1650..−1400 from x −1400 (z 0) to x −200 (z 600); PF9/10 stairs y 7300..7525 from x 300 to x 1400; FOB deck x ±195, z 600. Dadar local at PF1: actor y −720, doors open to −Y, floor at world z 35.

**Blender (`P9_Assets`):**
- `StationNPCs3D/`:
  - `optimize_npc3d.py` (decimate + bake, outputs in `<name>/v002/`), `rig_npc3d.py` (joints from the scan, heat weights with a voxel-proxy fallback, T→A pose, centimetre FBX), `reanim_npc3d.py` (clip generation + checks + FBX, tag `A2`/`A3`), `clips_npc3d.py` (A3 clip generator; `clips_npc3d_a2.py` = A2 record), `rigcheck_npc3d.py`, `fbxcheck_npc3d.py`, `export_static_npc3d.py`.
  - Rig blends: dhanraj/shivshankar/Rohit/kanchana `rig_v001`; Nilesh/rupesh/Modi `rig_v002` (proxy weights). Current exports: `<name>/v002/Export_A3/`.
- `ChaiProps/build_chai_props.py` (ketli, glass, atlas textures).
- `StationNPCs/build_station_npcs.py`: the 5A skeleton and 5A clip functions (imported by the NPC3D scripts — keep it).

**Backups and hashes:**
- Current map SHA starts `0a28cb078ecd` (end of 5D). Map backups `L_Station_Redesign_pre5B_*`, `pre5C_*`, `pre5C2_*`, `pre5D_*`, `pre5D2_*`.
- Source backups `src_pre5B_*`, `src_pre5C2_*`, `src_pre5D_*`.
- PIE save backups `SaveGames_pre5B_firstrun_*`, `SaveGames_pre5C2_*`. The live PIE save is at the epilogue stage.

## 5. Open items and decisions waiting for the user
- **Delete requests pending** (nothing deleted yet):
  - 5A v1 friend assets (`/Game/P9/Characters/Friends/F*/`, wrong-scale clips, `Anims/`) and their metre-scale FBX files;
  - 5A v2 station NPC assets (`/Game/P9/Characters/Station/*`), now unused;
  - NPC3D superseded clips (`SK_P9_NPC3D_<n>_Anim_*`, the `A2/` folders) and the static `SM_P9_NPC3D_Modi` once the proxy is no longer needed;
  - Blender attempt folders: `dhanraj/v001`, `_A2_try1`, `_A2_try2`, `_A2_pre_armik`, `_A2_armik_try1`, `_A3_try1`, `Modi/v002/_A3_nowalk`, rig `v001` files of Nilesh/rupesh/Modi;
  - older: `T_P9_ChaiStall_Sign.png` v1, `T_P9_Anom_AdPoster_Watching_v2.png`, `MI_P9_Anom_GhostTrain`.
  - 8A: `/Game/P9/Creatures8A/Materials/M_P9_Creature8A` (never compiled, unused), `P9_Assets/Footbridge/Endless8A/Export/_superseded/` (first stair cuts), test captures `Tools/ClaudeHarness/_8a_first_burst/`, `_8a_greeting_whitemat/`.
- **Not done or not verified:**
  - No Android performance measurement (7 × 15k-tri skinned NPCs, 21 × 2048² textures ≈ 60 MB).
  - The walk was checked in Blender frame sheets and logs, not watched up close in game.
  - TTS lines not listened to; licensing not cleared. The scanned people are real (incl. a sitting prime minister): get consent / check store rules before any release.
- **Known limits:**
  - No finger bones: props sit in open hands; during the rally gesture the ketli tilts with the raised hand.
  - Station lamps are unshadowed, so NPCs have no ground contact shadow (can look floating).
  - No foot IK on stairs. Scare B's brown-out too bright (auto-exposure).
  - If the player leaves the train extremely fast while looking at a seated friend, the friend may stay seated until unseen.
- **No APK since 4C.** Build one only when asked; the command is in §6.

## 6. Workflow and tooling (proven recipes)

**Helper scripts** are in `Tools/ClaudeHarness/`. Run them from Git Bash; the editor must be running with the ClaudeUnrealMCP plugin.

| Script | What it does |
|---|---|
| `./runpy.sh <script in ue_py/> <tag> [ms]` | Runs editor Python; prints log lines prefixed `P9PY:`. Use `unreal.log("P9PY: ...")`. |
| `./runseq.sh <tag> "cmd\|ms" ...` | Runs a console command sequence; `@start` starts PIE, `@assert` asserts PIE is running. |
| `./shots.sh "name\|x y z yaw\|pitch yaw\|0"` | Teleports the player (`p9.Test.Teleport`) and captures the PIE window. |
| `./snap.sh name "pitch yaw"` | Captures the PIE window without moving. |

- PIE control: `c307.json` stops PIE; `c705.json` quits the editor (`node mcp_call.mjs <index.js> <tag> c705.json`).
- Live Coding: `c4a_lc.json`, for function bodies only (not constructors, headers or UPROPERTYs).
- Map Check: `mc.json`. Saving the map: `ue_py/save_map_5b.py` (refuses if anything else is dirty).
- `ue_py/dirty_check.py` lists dirty packages; `pie_slow.py` / `pie_fast.py` set PIE time dilation.
- 5C/5D map scripts (not saving): `npc3d_apply.py` / `npc3d_restore.py` (5A ↔ NPC3D meshes), `npc3d_apply_a2.py` / `npc3d_restore_a2.py` (A2 clips + Specs stop), `npc3d_5d_setup.py` (A3 clips, props material), `npc3d_5d_props.py` + `npc3d_5d_props_apply.py` (props, return path), `npc3d_setup.py` / `npc3d_lod_screens.py` (materials, LODs).
- PIE test helpers: `pie_5d_stand.py` (player stands up via `UP9SeatComponent::Stand`), `pie_5d_handpose.py` (prop offsets from the evaluated pose), `pie_5d_freeze_idle.py`, `pie_5d_propdiag.py`.

**Build** (the editor must be closed; use PowerShell with `&` because of the space in the path):
`& "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat" ThanePlatform9Editor Win64 Development "-Project=C:\Users\niles\Documents\Unreal Projects\ThanePlatform9\ThanePlatform9.uproject" -WaitMutex -NoHotReloadFromIDE`

**Import** (editor closed; uses the legacy FBX path):
`& "...\UnrealEditor-Cmd.exe" "<uproject>" -run=ImportAssets "-importsettings=<json>" -nosourcecontrol -unattended -nop4 "-ini:Engine:[ConsoleVariables]:Interchange.FeatureFlags.Import.FBX=0"`
- Templates in `Tools/ClaudeHarness/import_templates/` (`import_5c_npc3d.json` meshes/textures, `import_5c_npc3d_a2.json` / `import_5d.json` animation-only onto existing skeletons + props).
- Animation-only import: `MeshTypeToImport: FBXIT_Animation`, `bImportMesh: false`, `Skeleton: <path>`; assets are named `<fbx name>_<take>`.
- Expected and harmless: "Invalid Destination Path ()", the BINKA ensure, exit code 1.
- **Never** import from editor Python: Interchange crashed the editor when called from the MCP thread.

**Blender (background):** `& "C:\Program Files\Blender Foundation\Blender 4.5\blender.exe" -b --factory-startup [<file.blend>] --python <script> -- <args>`.

**Launch the editor:**
`Start-Process "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe" -ArgumentList '"<uproject>"','/Game/P9/Maps/L_Station_Redesign'` — wait for "Total Editor Startup" or "LogLoad: Took".

**APK (only if asked):**
`RunUAT.bat BuildCookRun -project=<uproject> -platform=Android -cookflavor=ETC2 -clientconfig=Development -build -cook -stage -pak -package -archive -archivedirectory="C:\Users\niles\Documents\Unreal Projects\_builds\ThanePlatform9_Android_<stage>" -map=/Game/P9/Maps/L_Station_Redesign -cmdline=/Game/P9/Maps/L_Station_Redesign -nop4 -utf8output -unattended -nocompileeditor`

**Useful console commands:**
- Campaign: `p9.Campaign.Status|Force <Def>[:Variant][@Point][!]|Force normal|Sim <seed> <runs>|Verify|Cleanup|Save|Load`
- Audit and story: `p9.Anomaly.Audit`, `p9.Story.Validate|Status|Zones|Opening|Epilogue`
- Trains and storm: `p9.Train.State`, `p9.Storm.State|Flash 0 15`
- Test inputs: `p9.Test.Teleport x y z yaw`, `p9.Test.Move x y sec`, `p9.Test.Jump`, `p9.Test.State`, `p9.Test.Touch Start|End <ptr> <x01> <y01>`

## 7. Gotchas learned the hard way
- **Blender → UE skeletal meshes:** export in centimetres (scene unit 0.01, rig ×100 applied, location keys ×100, `FBX_SCALE_ALL`, armature renamed `Armature`, NLA strips become takes).
- **Blender 4.4+ action slots:** NLA strips do not evaluate in background renders — assign the action (and `action_slot`) directly for checks. While generating clips, set `animation_data.use_nla = False`, or depsgraph updates re-apply earlier strips over the pose being keyed.
- **Scan meshes:** `delete_loose` clears the edit selection (re-select before `smart_project`, or the unwrap silently does nothing). Heat weighting often fails on scans: weight a voxel-remeshed proxy and transfer weights.
- **Scanned rigs:** the knees are not under the hips at rest — leg solvers must include the rest thigh/shin tilt. These scans have short arms: seated hand targets must stay within ~90 % of arm reach.
- **Python and Blueprints:** set properties on placed instances (not just the CDO); static mesh slots via `sm.set_material(i, mi)`; skeletal slots via a rebuilt `unreal.SkeletalMaterial` list; bool `bFoo` is `foo` in Python; `PerPlatformFloat.default` is read-only (construct `unreal.PerPlatformFloat(default=x)`); `SkeletalMeshLODSettings` is created with `DataAssetFactory`.
- **UE-exported FBX in Blender:** the armature sits under an empty scaled 0.01 — unparent and apply before editing/exporting, or the exported skeleton gets a 0.0001 scale (bones explode in UE). Leaf-bone tails from FBX import are arbitrary (don't measure with them). Never quit the editor right after a refused save (PIE running) — unsaved map edits are lost.
- **IK Retargeter facing:** source and target skeletons must face the same way (Mannequin +Y vs scans +X gave walks that strode sideways). In PIE, `EditorAssetLibrary` is blocked: use `unreal.load_asset`; off-screen NPC poses do not refresh unless `visibility_based_anim_tick_option` is ALWAYS_TICK_POSE_AND_REFRESH_BONES.
- **Editor-world bone transforms** read from Python are the reference (A-)pose, not the clip pose — measure attachment offsets in PIE.
- **Comments:** a `//` comment inside a single-line `{ ... }` or a ternary breaks the code after it.
- **Platform 1 is the "go back" decision zone** (y −1690..−900; −900 is inside). Teleporting there resolves the day. Shoot from y ≤ −760 or from Platform 2/3.
- **Teleporting a seated player** (seat disables the capsule collision) drops them through the world — stand up first (`pie_5d_stand.py`).
- **Epilogue tests** write the save slot (it resumes in the epilogue); back up `Saved/SaveGames` first.
- **Blueprint recompiles dirty the map.** Back up the map before saving. The map was once saved from the editor UI by someone else during a session — always re-check the map SHA against the last recorded value.
- **Map Check** after map edits must report 0 errors and 0 warnings (an empty `StaticMeshComponent` is a warning).
- **Shell:** heredocs containing apostrophes can fail in the Bash tool — write scripts to files instead.

## 8. Claude Code Game Studios framework (installed 2026-11-01)

Installed from https://github.com/donchitos/claude-code-game-studios (release 1.2.0, MIT): `.claude/` (49 agents incl. `unreal-specialist`, 74 skills, 15 hooks, 14 rules, docs, scripts, `settings.json`, statusline), `project.yaml`, `docs/` (engine references, workflow guide), `design/`, `production/`.
- **Precedence:** the project rules in §2 (protected systems, backups, no deletes, no commits without asking, honest reporting) always win over the studio's defaults.
- Hooks load only when Claude Code is **started in this folder**. They need Bash (Git for Windows) and Python 3; `jq` is optional. `update-check.sh` asks the GitHub API for new releases (turn off: `/settings --local updates.check=false`).
- First use: restart Claude Code here, then run `/setup-engine unreal 5.7` (the shipped reference is written for 5.8; the project is on **5.7.4**) and `/adopt` or `/start` (this is an existing project: Stages 1–5D).

**Studio context (imported):**

@.claude/docs/directory-structure.md

@docs/engine-reference/unreal/VERSION.md

@.claude/docs/coordination-rules.md

@.claude/docs/coding-standards.md

- Collaboration protocol: Question → Options → Decision → Draft → Approval; ask before writing files; show drafts first; no commits without instruction. `production/session-state/active.md` is the studio's session checkpoint.

## 9. Resume checklist for a new session
1. Read this file and `Tools/ClaudeHarness/` (scripts may need `chmod +x`).
2. Check `git status` (should be clean apart from new work) and the map SHA against §4.
3. Ask the user what to do next. Pending decisions: the deletions in §5, an APK, Android performance, the next stage.
4. For any work, follow the rules in §2: backups and SHA first, show commands first, and do not touch protected systems without an explicit request.
