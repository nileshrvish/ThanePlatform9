# CLAUDE.md — Platform No. 9 (ThanePlatform9)

This is the hand-off file for new Claude Code sessions. Read it first, then `PROGRESS.md` (the older history up to Stage 3A; **never edit `PROGRESS.md`**).
It was last updated 2026-11-01, at the end of Stage 5D (scanned station NPCs, animation fixes, epilogue train and chaiwala). Stages 4A–5D were committed and pushed on 2026-11-01 (see `git log`).

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
