# ThanePlatform9 — Progress

_Last updated: 2026-10-24 (Stage 3A.13: documentation of Stages 3A.4–3A.12 and the T10 investigation)_

## What this project is

ThanePlatform9 is an **Unreal Engine 5.7, Android-first mobile game**: a first-person walkable greybox
of a railway station (platforms, tracks, footbridge and stairs), set on Platform 9 at Thane.
It started as a Blueprint-only project. It now also has a small C++ game module (`Source/ThanePlatform9`, added 2026-10-17)
for the mobile controls and ledge climbing. All game content lives under `Content/P9/`.

## Done so far (committed)

| Commit | Step | What it did |
|---|---|---|
| `287b98a` | Setup | Git LFS tracking for Unreal binary assets |
| `7df480a` | Setup | Android-first mobile rendering settings |
| `991e07f` | 1A | Core classes (`BP_P9_GameMode`, `BP_P9_Character`, `BP_P9_PlayerController`), Enhanced Input assets (`IA_Move`, `IA_Look`, `IA_Interact`, `IMC_Default`), blockout map `L_Station_Blockout` |
| `cd9bbca` | 1B | First-person movement and look through Enhanced Input |
| `bbea23c` | 1C | Station greybox: platforms, tracks, footbridge, stairs |
| `017f857` | Baseline | Mobile controls WIP (`WBP_MobileControls`, `WBP_TouchJoystick`, `DefaultTouchInterface=None`, controller adds the widget) and the ClaudeUnrealMCP plugin with the Cursor config |
| `e636d7a` | Controls | Mobile touch controls (joystick, touch look, circular ↑ Jump) and automatic ledge climbing in the C++ module, `IA_Jump`, `L_LedgeTest`. Branch `mobile-controls-ledge-climb`, local only (not pushed) |

## Current status (as of 2026-10-18)

- **Committed (`e636d7a`, local, not pushed):** the mobile controls and ledge-climbing work described below.
- **Uncommitted, awaiting review:** the station redesign: `L_Station_Redesign`, `L_Mod_IslandPlatform`,
  `Content/P9/Environment/Sky/` (`MI_P9_StormSky`, `MI_P9_LightningBolt`), `AP9StormController` (2 source files) and this file.
  Stages 1, 2A and 2B are approved. Stage 2C (lighting) and its stabilization pass, and Stage 3A.3 (one canopy module on PF2/3), are awaiting review.
  An Android Development APK of the redesign is packaged in `Saved\Builds\Android_Redesign`. It has not been installed on a phone.
  (That APK predates Stages 3A.3–3A.12.)
- **Update 2026-10-24:** Stages 3A.4–3A.12 have been carried out and reported, with no approval recorded here yet
  (see *Stages 3A.4–3A.12* below). They are all in `L_Station_Redesign` and new assets under `Content/P9/Environment/`,
  all uncommitted. OHE now covers T1–T9 and the goods line TG; **T10 is not covered** (see *Stage 3A.13*).
- **Not changed:** `Config/`, the default game map (`L_Station_Blockout`), `L_Station_Blockout` and `L_LedgeTest`
  (SHA-256 checked), gameplay code, mobile controls, `Plugins/`, `.cursor/`.
- **Android toolchain:** installed and working (see *Environment* at the end). A Development Android APK has been packaged.
  It has **not** been run on a device yet: `adb devices` has never listed one.
- **Still outstanding:** real Android multi-touch and performance on a device; the mid-mantle obstruction case; a platform-edge
  mantle with no barrier; the one-off joystick-reset anomaly after a screenshot; and the pressed-state visual. Details are below.

## Current step: Unreal MCP plugin for Cursor

Goal: install [ClaudeUnrealMCP](https://github.com/AgustinJimenez/ClaudeUnrealMCP) so Cursor can inspect and edit
Blueprints. Then use it to build mobile controls: a movement joystick, one Jump button, and drag-to-look camera.

### Environment check (2026-10-17)

| Requirement | Status |
|---|---|
| Unreal Engine 5.7 | Installed at `C:\Program Files\Epic Games\UE_5.7` |
| Node.js 18+ | v20.18.1 |
| npm | 10.8.2 |
| Git / Git LFS | 2.49.0 / 3.6.1 |
| Visual Studio | VS 2026 (v18) Community at `C:\Program Files\Microsoft Visual Studio\18\Community`, MSVC 14.51.36231 (compiler 19.51.36260) |
| Windows SDK | 10.0.22621.0 (used by UBT) and 10.0.26100.0 |
| UnrealBuildTool | `C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe`, run through `Engine\Build\BatchFiles\Build.bat` |
| UE 5.7.4 toolchain rules (`Engine\Config\Windows\Windows_SDK.json`) | Accepts VS2026 (18.0 or later) and MSVC 14.38 or later. 14.51 is allowed but not "preferred" (14.44), so UBT only warns. |
| Project-level `.cursor/mcp.json` | Didn't exist, so it was created new. The global `~/.cursor/mcp.json` was not changed. |

### Plugin facts (repo commit `400ff58`)

- It ships as **C++ source only, with no prebuilt binaries**, so it has to be compiled before the editor can load it.
- It is an Editor-type module that starts a TCP server on port 9877. A Node.js bridge (`MCPServer/index.js`) connects Cursor to it.
- It enables these plugins as dependencies: EnhancedInput, Chooser, StructUtils, EditorScriptingUtilities, Niagara, StateTree, ProceduralMeshComponent.
- The README warns that it is in active development and not production-ready.

### Status: INSTALLED AND VERIFIED (2026-10-17)

1. Cloned the plugin into `Plugins/ClaudeUnrealMCP` at commit `400ff58`, the upstream HEAD.
2. `npm install` in `MCPServer/` installed `@modelcontextprotocol/sdk@0.5.0`. npm audit reports one advisory
   (DNS-rebinding protection is off by default). It only affects the SDK's HTTP transports, not the stdio transport used here.
3. Built with the editor closed:
   `Build.bat -TargetType=Editor Win64 Development -Project=...ThanePlatform9.uproject`.
   **Result: Succeeded** (54 actions, about 142 s, only C4996 deprecation warnings).
   Output: `Plugins/ClaudeUnrealMCP/Binaries/Win64/UnrealEditor-ClaudeUnrealMCP.dll`.
4. Created `.cursor/mcp.json` with an `unreal-engine` stdio server that runs `node .../Plugins/ClaudeUnrealMCP/MCPServer/index.js`.
5. Reopened the editor. The plugin is listening on TCP port 9877.
6. Tested the bridge over stdio the way Cursor does: `initialize` succeeded, `tools/list` returned **169 tools**,
   `ping` returned `pong`, and `list_blueprints /Game/P9` returned BP_P9_Character, BP_P9_GameMode and BP_P9_PlayerController.

Nothing in `.uproject`, Config or gameplay assets was changed by the install. The uncommitted mobile-UI work is untouched.

### Notes

- `Plugins/ClaudeUnrealMCP` is its own Git clone, nested inside the project repo. We still need to decide how to track it
  in the project repo: as a submodule, as vendored files, or ignored.
- Some tools are destructive (`delete_asset`, `run_python`, `clear_event_graph`, `save_all` and others).
  Use read tools first, and commit before any edit session.
- `list_blueprints` didn't return the widget blueprints (`WBP_*`). Use `search_assets` or `read_widget_tree` for UI assets.

Cursor shows `unreal-engine` as connected, with its tools available (confirmed by the user).

## Mobile controls and ledge climbing: inspection (2026-10-17, read-only)

### What already exists
- **Input:** `IA_Move` (WASD and the left gamepad stick), `IA_Look` (mouse and the right stick, with a Scalar modifier) and
  `IA_Interact` (E and gamepad face-bottom), all in `IMC_Default`. **There is no `IA_Jump` and no jump mapping.**
- **BP_P9_Character** (parent: Character). It has a `FirstPersonCamera` component and **no mesh**. `IA_Move` drives AddMovementInput
  along the actor's forward and right vectors. `IA_Look` drives yaw and pitch input. **There is no jump logic.**
- **BP_P9_PlayerController** (uncommitted change): BeginPlay adds a mapping context, then creates `WBP_MobileControls` and adds it to the viewport.
- **WBP_MobileControls:** a CanvasPanel with a `WBP_TouchJoystick` instance and a `LookArea` (Border), plus a `LastLookPosition` variable.
  **There's no Jump button.** Look logic lives in the `LookArea` mouse-event bindings
  (`On_LookArea_MouseButtonDown`, `On_LookArea_MouseMove` and `On_LookArea_MouseButtonUp`). The drag delta in screen pixels × 0.005 goes
  to yaw and pitch. **Bug:** the `Y × -1` result isn't connected to Add Controller Pitch Input (`Val` stays at 0), so vertical look does nothing.
  It also doesn't track which finger is which and doesn't capture the pointer.
- **WBP_TouchJoystick:** a SizeBox containing an Overlay with `Base` and `Thumb` images, plus `ActivePointer` (-1), `Radius` (100) and `DeadZone` (0.1).
  It overrides `OnTouchStarted`, `OnTouchMoved` and `OnTouchEnded`, and has an `UpdateStick` function (the center is hard-coded at 125,125).
  It drives `IA_Move` through Enhanced Input's Start/Update/Stop Continuous Input Injection. **Bugs:**
  1. `OnTouchMoved` overwrites the thumb's translation with the normalized value, so the thumb barely moves on screen.
  2. The pointer is never captured. If the finger slides off the joystick, it stops updating. If the finger is lifted outside it,
     `OnTouchEnded` never fires, so the movement injection never stops (the character keeps running).
- *(Correction: an earlier version of this file said both widgets had no logic. That was wrong, because only their event graphs had been read.)*
- **DefaultInput.ini:** `DefaultTouchInterface=None` (uncommitted). The engine's virtual joysticks are turned off.
- **Climbing system:** this project doesn't contain one. The downloaded one is at `Documents/Unreal Projects/ClimbingSystem-main 5.7`
  (DanialKama's ClimbingSystem, Blueprint-only and third-person). It has `BP_PlayerCharacter` (LedgeForwardTracer → LedgeHeightTracer →
  GrabLedge → ClimbLedge, plus knobs), `ABP_Player` with IK, Mixamo montages (`AM_IdleToBracedHang`, `AM_ClimbStand`, ...)
  and `SKM_Manny_Simple`.

### MCP limitation
The MCP can't add new Blueprint graph nodes, such as function calls, branches or events. It can only connect, delete or duplicate
nodes that already exist and set their defaults. It *can* add widgets and components, set properties, reparent Blueprints,
add input mappings, duplicate assets and compile.

### Implementation progress (2026-10-17)
Approved plan: a C++ layer wired up with the MCP. Look approach (b): C++ look, with the old `LookArea` bindings kept but no longer firing.
- **Baseline commit** `017f857` (local, not pushed). The plugin's nested `.git` was moved to `Unreal Projects\_backups\ClaudeUnrealMCP.git`.
- **IA_Jump** was duplicated from `IA_Interact` (a true/false action), and SpaceBar is mapped to it in `IMC_Default`.
- **Joystick thumb fix (MCP):** `OnTouchMoved` now goes from `Update Stick` straight to `Update Value Of Continuous Input Injection`.
  The node that overwrote the thumb position is still in the graph, unconnected.
- **C++ module `ThanePlatform9`** (added to the `.uproject` along with the EnhancedInput plugin):
  - `UP9TouchJoystickWidget`: captures the finger, and if capture is lost it calls the Blueprint's `OnTouchEnded`.
  - `UP9MobileControlsWidget`: look with per-finger tracking and capture, `LookSensitivity`, `LookVerticalScale`, `bInvertLookY`
    and `LookSmoothingSpeed` settings; holds `IA_Jump` while the button is pressed; hides itself when there's no touch input.
  - `UP9LedgeClimbComponent`: binds `IA_Jump` (Jump / StopJumping), then does automatic ledge detection and a validated mantle,
    with a timeout and abort-to-falling.
  - `p9.Test.*` console commands (development builds only).
- **Build fixes:** `ThanePlatform9.h` moved to `Public/` (fixed C1083). The private member was renamed to `JumpButtonWidget`,
  which fixed a Blueprint duplicate-property clash with the designer widget `JumpButton`.
- **WBP_TouchJoystick:** reparented to `P9TouchJoystickWidget`. All four graphs are intact, and it compiles with 0 errors and 0 warnings.
- **WBP_MobileControls:** reparented to `P9MobileControlsWidget`. Added one `JumpButton` (with a "JUMP" label, anchored bottom-right,
  200×200, ZOrder 10, not focusable). `LookArea` is set to HitTestInvisible. `LookAction` is `IA_Look` and `JumpAction` is `IA_Jump`.
  It compiles with 0 errors and 0 warnings, and `check_all_blueprints /Game/P9` reports 0 errors and 0 warnings across 5 Blueprints.
- **Note:** the MCP's `add_widget` and `set_widget_property` tools save the asset immediately after each edit.
- **Ledge component** `LedgeClimb` added to `BP_P9_Character` with `JumpAction` set to `IA_Jump`. It compiles, and `check_all_blueprints` is clean.
- **Test map** `L_LedgeTest` is a copy of the station map. The original isn't edited. It has test ledges `LT_Ledge_60/120/180/240/300/400`
  and `LT_Ceiling_Ledge_120` with `LT_Ceiling_Slab` (150 cm of clearance), placed at Y ≈ 3500.
- **Development-only test tooling** (`P9TestCommands.cpp`, excluded from Shipping builds): `p9.Test.Touch`, `TouchDrag`, `ShowTouchControls`,
  `WidgetState`, an extended `State`, and `EditorThrottle` (an in-memory toggle of "Use Less CPU when in Background").

### Test results (2026-10-17, PIE at 60 fps, throttling off during the run and then restored to 1)
Passed:
- Normal jump.
- Look through `IA_Look`: +X turns right, +Y looks down. A touch drag up looks up.
- Ledges at 120, 180, 240 and 300 cm are grabbed and mantled (0.80–1.12 s) and end in Walking mode.
- The 60 cm ledge is cleared by a normal jump. The 400 cm ledge is rejected and the character lands normally.
- The low-ceiling grab is rejected ("no room to stand"). The platform-edge grab is rejected because of the 2 m `GB_EdgeBarrier_P1/P9`.
- Touch: the joystick captures its finger, and lifting the finger outside the stick stops movement. Look keeps tracking across
  the joystick and the button without triggering either. Jump press, hold and release work. Two fingers at once, plus a third
  finger on Jump, all work.

Observations:
- 300 cm ledges are reachable: up to 216 cm above the feet, which is the capsule height plus `MaxReachAboveHead` (40).
- When a ledge is too high, the log message says "ledge is under an overhang", which is misleading.
- In the ceiling case the character can perch on the ledge corner with its head against the ceiling. It recovers by walking back.
- The Jump button stays pressed when the finger slides off it until the finger is lifted. This has no gameplay effect, because `JumpMaxHoldTime` is 0.
- The controls show in desktop PIE because the editor preference "Use Mouse for Touch" is on
  (`Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini`).

Untested:
- An obstruction appearing mid-mantle (the MCP actor tools only act on the editor world).
- A successful platform-edge mantle (the level has barriers along both platform edges).
- Real-device multi-touch (only synthetic Slate touch events were used).
- A Shipping build compile, and a packaged desktop build. (The Shipping compile was done later; see below.)

### UI fixes and cleanup (2026-10-17, second pass)
- **White panel root cause:** the legacy `LookArea` Border in `WBP_MobileControls` is anchored over the right half of the
  screen (Min 0.5,0 to Max 1,1). Its `Background` brush has no texture and its `BrushColor` was solid white (A=1),
  so it drew a white rectangle. `ContentColorAndOpacity` A=0 only fades a Border's children, and `HitTestInvisible` doesn't affect drawing.
  **Fix:** `LookArea.BrushColor` alpha set to 0. The widget is still there (bindings intact, still non-hit-testable).
  A PIE capture confirms the 3D view now fills the viewport.
- **Jump button redesign** (same `JumpButton` widget, same `IA_Jump` path through `UP9MobileControlsWidget`):
  - Size is 150 Slate units, which is 100 px at the current preview scale (0.668 px/unit). It stays anchored bottom-right at offset -80.
  - Style is a circle (RoundedBox, HalfHeightRadius) with a black 40% fill and a white 70% 2 px outline.
    The pressed state is lighter and more opaque with a 3 px white outline. Content padding is 0.
  - `JumpLabel` now shows "↑" in Roboto Bold 48, white, with a black 60% shadow, set to `HitTestInvisible`.
    The label's centre matches the button's centre exactly.
- **Joystick centre:** `JoystickSize` has a 250×250 override and the widget's local geometry is 250×250. `UpdateStick` uses
  `AbsoluteToLocal(MyGeometry)`, so the hard-coded centre (125,125) is correct and **no fix was needed**.
  A touch at the exact centre gave a thumb offset of (0.005, 0.001) and no movement.
- **Ledge diagnostic:** the log message "ledge is under an overhang" is now
  "ledge top is above reach (solid geometry at max reach height)". Only the message changed.
- **MaxReachAboveHead** is unchanged at 40 cm. The highest reachable ledge top is capsule height (176) + 40 = 216 cm above the feet,
  so with a 90 cm jump a 300 cm ledge can be grabbed.
- **Shipping check:** the `ThanePlatform9 Win64 Shipping` game target compiled and linked successfully. It was not packaged
  or run, and no packaged desktop or Android build has been made. A scan of the Shipping exe finds no `p9.Test.*`,
  `EditorPerformanceSettings` or `DumpControls` strings, while known module strings (e.g. `P9LedgeClimbInput`) are found,
  which shows the scan works.
- **Added a dev-only tool:** `p9.Test.DumpControls` (inside `#if !UE_BUILD_SHIPPING`), a read-only widget layout, brush and geometry dump.

### Validation after the UI fixes (clean run E, PIE 60 fps, throttling off then restored and confirmed =1)
Passed:
- Joystick: an exact-centre touch gives no movement; pushing moves the character; dragging off the stick keeps tracking;
  lifting outside the stick resets it and stops movement. This passed 4 of 4 times in clean runs.
- Look drag, including across the joystick and the new button, with no side effects.
- Jump press, hold and release on the new button.
- Joystick, look and Jump together.
- Ledges at 120 cm (0.77 s), 180 (0.90 s), 240 (1.12 s) and 300 (0.95 s) mantle successfully. 400 is rejected with the new message.
- The low ceiling is rejected and the character recovers by walking back.

Intermittent or inconclusive:
- In one run (D), after a screenshot capture, a joystick lift outside the stick did **not** reset (the character kept walking).
  This didn't reproduce in 4 clean attempts. In run E, a capture changed `jumpHeld` and the camera with no test input.
  With "Use Mouse for Touch" on, window activation and real mouse focus can interfere with synthetic pointer 0.
  **This needs checking on a real device.**
- The pressed-state visual wasn't captured while pressed, because the capture itself released the press.

Found, not fixed (fixed later; see "Joystick shutdown cleanup fix" below):
- If the joystick is still held when PIE stops, `UP9TouchJoystickWidget::NativeDestruct → ForceRelease → BP OnTouchEnded`
  logs "Accessed None … GetLocalPlayerSubSystem" because the local player has already gone. It only happens at teardown.

### Joystick shutdown cleanup fix (2026-10-17)
- **Call path:** `UP9TouchJoystickWidget::NativeDestruct` → `ForceRelease()` → Blueprint `OnTouchEnded` → `Get Owning Player` →
  `Enhanced Input Local Player Subsystem` → `Stop Continuous Input Injection for Action`.
- **Baseline reproduced before the fix:** stopping PIE with the joystick held logged
  "Accessed None … CallFunc_GetLocalPlayerSubSystemFromPlayerController_ReturnValue … Node: Stop Continuous Input Injection for Action
  Graph: OnTouchEnded … WBP_TouchJoystick". It appeared in the same frame as, and just before, `BeginTearingDown`.
- **Invalid reference (measured after the fix):** the widget's **owning player controller is already null** during teardown, so the
  local player and the Enhanced Input subsystem are null too.
- **Fix (in `P9TouchJoystickWidget.cpp`, `ForceRelease()` only):** before calling the Blueprint `OnTouchEnded`, check that the owning
  player, its local player and its Enhanced Input subsystem are all valid. If any is missing, the C++ capture state is still cleared,
  the Blueprint call is skipped, and one Log-level line records which reference was null. In normal gameplay all three are valid, so the
  normal touch-end, capture-lost recovery and the Blueprint logic don't change. Nothing is suppressed globally.
- **Results:** the editor build succeeded. `WBP_TouchJoystick` and `WBP_MobileControls` compile with 0 errors and 0 warnings, and
  `check_all_blueprints /Game/P9` reports 0 errors and 0 warnings across 5 Blueprints. Throttling was off for the test and was then
  restored to 1, which a separate read confirmed.
  - PASS: normal release inside the stick. The stick reset to (0,0) and velocity went to 0.
  - PASS: release after dragging off the stick and lifting outside it. The stick reset and velocity went to 0.
  - PASS: stopping PIE with the joystick idle (twice). No errors and no cleanup log line.
  - PASS: stopping PIE while the joystick was held. The new log line appeared
    ("owning player null, local player null, input subsystem null") and **no Accessed None or Blueprint Runtime Error was logged**.
  - PASS: a new PIE session after the held stop. The stick was idle, velocity was 0 (nothing stuck), and joystick move and release worked.
  - Observation: in that next session, the first synthetic touch logged
    `LogSlate: Error: SlateUser [0] notified of a touch starting for pointer [0] without finding out it ever ended`.
    The test tool stopped PIE while simulated pointer 0 was still down and never sent its touch-end, and the editor's Slate application
    persists across PIE sessions. This is attributed to the test harness (a real device's OS ends touches itself) but isn't proven.
    Behaviour was correct afterwards. (Resolved in the test harness; see the next section.)

### Test-harness touch cleanup and final review (2026-10-17)
- **Harness fix (scratchpad `mcp_call.mjs` only; no project code changed):**
  - It tracks every simulated `p9.Test.Touch` Start, Move and End, plus `TouchDrag` targets, and saves the active fingers to `active_touches.json`.
  - It ends every active finger before any `play_in_editor stop`, and again on abort, error or normal finish (`finally`).
  - If PIE has already ended, the fingers are kept and released at the start of the next PIE session, before any new touch.
- **Scenario results (each checked over its own log window):**
  - S1 control (old behaviour: stop with a finger down, then a new touch) reproduced the Slate error once.
  - S2, a normal stop with a finger down: the finger was released before the stop. **0 Slate errors.**
  - S3, PIE ending with a finger down: the finger was released in the next session. **0 Slate errors.**
  - S4, the abort path: the finger was kept, then released in the next run. **0 Slate errors.**
  - In every case the joystick moved and reset normally afterwards, and the state file ended empty. Throttling was off during the
    scenarios and was then restored to 1, which a separate read confirmed.
- **Final review:**
  - The editor build is up to date. `WBP_MobileControls`, `WBP_TouchJoystick`, `BP_P9_Character` and `BP_P9_PlayerController`
    compile with 0 errors and 0 warnings, and `check_all_blueprints /Game/P9` reports 0 errors and 0 warnings across 5 Blueprints.
  - Reading back the saved assets confirmed: `LookArea.BrushColor` A=0; `JumpButton` is a circle at 150 units (100 px) with an
    `HitTestInvisible` ↑ label; both widgets have their C++ parents; the thumb rewire is intact; `LedgeClimb.JumpAction` is `IA_Jump`;
    and `MaxReachAboveHead` is **40 (unchanged)**.
  - The test tooling is entirely inside `#if !UE_BUILD_SHIPPING`, and the throttle code is also inside `#if WITH_EDITOR`.
  - A PIE session with no touches logged 0 errors, and a capture showed a clear viewport with the circular ↑ button.

## Station redesign (working level `L_Station_Redesign`, uncommitted)

Context (Stage 1, 2026-10-17): `L_Station_Redesign` is a copy of `L_Station_Blockout`. Its 104 old `GB_` cubes are hidden in game and in
the editor, with collision off; nothing was deleted. A compact two-platform blockout (`RD_` actors) was built and PIE-tested.
`L_Station_Blockout`, `L_LedgeTest` and the default game map (`L_Station_Blockout`) are unchanged.

### Stage 2A: ten-platform blockout (2026-10-17)
- **New module level** `/Game/P9/Maps/Modules/L_Mod_IslandPlatform` (created empty by the user, then filled through the MCP with 56 pieces):
  a 60 × 10 m island deck, a canopy (with a gap over the stair) at 5 m, 7 central columns, 3 benches, end fences, and the two-flight stair
  (18 × 16.67 cm risers, 30 cm treads, 1.5 m landing, railings).
- **`L_Station_Redesign` layout** (Y across the station, cm):
  - PF1 side platform at −1700..−890 (Stage 1, kept).
  - Island platforms 2/3, 4/5, 6/7 and 9/10 at Y 430 / 2250 / 4070 / 7410, as **4 Level Instances** of the module.
  - PF8 single-face platform at 5390..6090, with a 2 m fence against the goods line.
  - Tracks T1–T10 plus goods line TG, 1.7 m from the platform edge to the track centre and 4.8 m between paired tracks,
    with rails visually extended to ±120 m.
  - Low (1.2 m) track-pair fences at Y −480 / 1340 / 3160 / 4980 / 6500, each with two 3 m gaps.
- **Footbridge and concourse:**
  - A 96 m main footbridge, 4 m wide with the deck at +6 m, with stair openings at every platform.
  - A "Ticket on Bridge" concourse (14 × 15 m) over T5/T6, with a 2.5 m ticket booth (can't be climbed), benches, a stall
    and a departure board (no collision).
- **Closed areas:**
  - On the footbridge: a gate at the PF4/5 stair head, a scaffold and tarp at PF6/7, and a barricade at PF8.
  - An old footbridge at X = 24 m with a missing middle section, no stairs and barricaded ends.
- **Boundaries:** an east wall, track-end fences at X ±36 m, and invisible blockers (hidden in game).
- **Superseded Stage 1 pieces:** 104 `RD_` actors are hidden with collision off (each read back as `NoCollision`); none deleted.
- **PIE results (60 fps; throttling off during the tests and then restored to 1): all passed, with 0 errors:**
  - "Loaded 4 levels", and the character stands on and collides with instanced geometry.
  - PF1 stairs up (Z 90 → 690). The footbridge ↔ concourse seams work in both directions, and the footbridge runs to its north end (Y 7876).
  - The PF2/3 and PF9/10 stairs go down to their platforms.
  - The gate, scaffold and barricade each stop the player at X 170.9.
  - The closed platforms can be reached across the tracks through the fence gaps (island 4/5 at Y 2132, PF8 at Y 5933).
  - The PF8 stair dead-ends at its barricade. The track-end fence, east wall, concourse booth and old footbridge are blocked or out of reach.
- **Not verified:** Android performance (no device connected at the time). The blockout draws about 443 separate meshes;
  instancing and merged kit meshes are planned for Stage 3.
- **Status: approved.**

### Stage 2B: storm sky, faint moon, distant lightning (2026-10-17). Status: approved
- **New C++ actor `AP9StormController`** (`Public/P9StormController.h`, `Private/P9StormController.cpp`). It is optional:
  without it the sky simply stays still and dark.
  - It finds its targets by tag (`P9SkyDome`, `P9MoonLight`, and `P9Bolt*` groups).
  - It drives a dynamic material instance on the sky dome and briefly raises the moon light's intensity.
    It shows one bolt group for 0.14 s.
  - Timing is configurable: first flash after 6 s, then every 12–40 s; 0.45 s flashes with a 55 % chance of a double flash.
    A `bEnableLightning` switch turns random flashes off.
  - Dev-only console commands `p9.Storm.Flash [Bolt] [Hold]` and `p9.Storm.State` sit inside `#if !UE_BUILD_SHIPPING`.
    A string scan of the Win64 Shipping exe confirmed they aren't in that build.
- **Sky and bolt materials:**
  - `MI_P9_StormSky` is an instance of the engine `M_Sky_Panning_Clouds2`, used on an `SM_SkySphere` dome
    (`S2B_SkyDome`, about 2 km, no collision, no shadows).
  - `MI_P9_LightningBolt` (engine emissive additive material) is used on 22 hidden-in-game segments that form 2 distant bolt groups.
- **Moon:** the existing directional light, at 2 lux with a pale blue colour and tag `P9MoonLight`.
- **Not used:** volumetric clouds, sky atmosphere, Lumen and ray tracing.
- **PIE results:**
  - Targets were found, and the flash interval stayed within 12–40 s.
  - Peak values were applied, then restored exactly after each flash.
  - 0 errors. The Editor and Shipping builds compile.

### Stage 2C: night lighting (2026-10-18). Status: awaiting review
- **Why everything is dynamic (no lightmap bake):**
  - The islands are Level Instances. UE 5.7 loads each one as its own instanced copy of the one module package
    (`LoadLevelInstance` with a unique `_LevelInstance_<hash>` package suffix), and all four copies share one `LevelBuildDataId`.
  - So baked lightmaps can't be stored for each island. Static lights would leave the islands unlit, and Stationary lights
    would leave them unbuilt.
  - Every light added in Stage 2C is therefore **Movable**.
  - As a guard, the level's World Settings has `bForceNoPrecomputedLighting = True`. Build Lighting then skips this map with a
    warning and can't write lightmaps for it, so lightning flashes can never be baked. This setting is per-map, not a project setting.
  - If Stage 3 turns the islands into real or packed meshes, a baked path (Static pools plus a Stationary moon) can be reconsidered.
- **Moon (`DirectionalLight_0`):**
  - Stays Movable, because the storm controller changes its intensity at runtime.
  - Shadows went from 4 CSM cascades out to 400 m down to **1 cascade out to 30 m** (fade-out fraction 0.25),
    with no inset shadows and no volumetric shadow. Beyond 30 m the moonlight is unshadowed.
- **Exposure and grade:** an unbound post-process volume `S2C_PostProcess` with
  - manual exposure at −1.0 EV (physical-camera exposure off);
  - bloom 0.5, vignette 0.5 and saturation 0.85.
- **Sky:** `MI_P9_StormSky` *Overall Color* changed from 1.0 to 2.5, so the approved storm sky keeps its brightness under the new
  manual exposure.
  - The placed controller now uses `SkyFlashStrength` 1.0 (was 0.55) and `FlashCloudColor` (1.6, 1.75, 2.2).
  - These are instance values only. No code changed in Stage 2C.
- **Light pools:** 36 Movable, **unshadowed** local lights (candela units), all tagged `P9LightPool`.
  - Each has a short attenuation radius (450–900 cm), no effect on translucency, and draw-distance culling at 60 m (fading over 15 m).
  - **Platforms:** spot lights under the canopies in a cool, slightly green fluorescent tone.
    - PF1 has 3 and the open islands 2/3 and 9/10 have 3 each.
    - The closed platforms get fewer or dimmer lights: 4/5 has 2, 6/7 has 1 dim, PF8 has 2 dim.
    - Each pool reaches the platform edges on both sides.
  - **Stairs:** a sodium point light over the landing of each of the 6 stairs.
  - **Footbridge:** 7 cool-white lights under the roof. The scaffold and barricade stretches stay dim on purpose.
  - **Concourse:** 3 warm lights plus one fluorescent light at the ticket booth.
  - **Track crossings:** 5 dim amber lights at the west fence gaps.
  - Values are kept in a regeneration script: base intensities × 2.5 lamp scale, and one exposure value.
- **Fog:** one `ExponentialHeightFog`, kept restrained and not volumetric.
  - Density 0.035, falloff 0.5, starting at 8 m, maximum opacity 0.8, cut off at 600 m (so the sky and distant bolts are not fogged).
  - Colour (0.05, 0.04, 0.035), matching the horizon glow.
  - On mobile it renders as one per-pixel pass (`r.Mobile.PixelFogQuality` defaults to 1). It can be turned off or deleted
    without affecting anything else.
- **Validation (desktop PIE, which renders with *PC D3D SM6*, not the mobile renderer):**
  - **Traversal:** the full Stage 2A suite passed again with identical results.
    - Main route and stairs; closed heads stop the player at X 170.9; fence gaps to the closed platforms; PF8 dead end; boundaries.
    - 60 fps on the PC.
  - **Map check:** 0 errors and 0 warnings.
  - **Storm controller:** finds its targets. A flash sets the moon to 8.0 against its base of 2.0, then restores it exactly.
  - **Log:** no new errors apart from MCP tool noise.
  - **Captures:** platform, concourse, footbridge, track level, open sky and flash.
- **Known issues and limits:**
  - **Lighting build was not run.** The MCP has no tool for it. With the guard on, Build Lighting is expected to log
    "bForceNoPrecomputedLighting is true, Skipping Lighting Build!". That hasn't been observed yet.
  - **Lightning is weak under covered areas:** *Superseded by the stabilization pass below.* Part of this finding was a
    test-harness timing bug: every harness call took about 60 s to return, so most "flash" captures were taken after the
    flash had ended.
  - **Editor crash during testing:** reloading the open map through the MCP `load_level` call (with Level Instances loaded)
    hit an engine assertion: `!LevelList.Contains(TickTaskLevel)`, TickTaskManager.cpp:1987.
    The save made just before it had already completed. The test tooling no longer reloads the level.
  - **Android not verified:** nothing has been checked on a device or in the mobile renderer. Not measured:
    - 36 dynamic local lights plus one CSM cascade plus fog plus bloom;
    - about 480 meshes in total.

    Mobile limitations that apply:
    - Movable point lights cast no shadows on mobile. None are shadowed here anyway.
    - Local-light cost depends on the mobile forward light grid (`r.Mobile.Forward.EnableLocalLights=1`).
    - Low-end GPUs may need fewer pools or a lower `MaxDrawDistance`.

### Stage 2C stabilization (2026-10-18). Status: awaiting review
- **Lightning (1a, placed instance `P9StormController_0` only):**
  - `SkyFlashStrength` 0.25 (was 1.0). The HDR sky saturates, so this is what keeps the peak a stormy grey and not white.
  - `FlashHorizonColor` (0.9, 0.95, 1.2), `FlashZenithColor` (0.45, 0.5, 0.75).
  - `MoonFlashIntensity` 8 (was 6). `FlashCloudColor` is unchanged at (1.6, 1.75, 2.2).
- **Lightning (1b, C++ `AP9StormController`):** a new optional sky-light flash for covered areas.
  - New properties `bFlashSkyLight` (default **false** in code), `SkyLightFlashIntensity` (default 4) and `SkyLightTag` (`P9SkyLight`).
  - The base intensity is cached in `ResolveTargets`. `ApplyFlash` sets base + amount × flash, and amount 0 writes the exact base back.
  - `DescribeState` now also prints horizon, zenith and skylight values, with intensities to 4 decimals.
  - The placed instance enables it with +12. `SkyLight_0` is tagged `P9SkyLight`.
  - Timing, envelope, randomness, bolts and the existing sky and moon paths are unchanged.
- **Why the skylight flash is cheap on mobile (checked in engine source before enabling it):**
  - On a Movable skylight with real-time capture off, `USkyLightComponent::SetIntensity` calls `UpdateLimitedRenderingStateFast()`.
    That is one small render command updating the proxy colour. There is **no recapture**.
  - The renderer writes `View.SkyLightColor` from the proxy every frame (`SceneRendering.cpp:1917`), and the mobile base pass
    multiplies the sky ambient by it (`MobileLightingCommon.ush:764`, `MobileBasePassPixelShader.usf:870`).
  - The controller only calls `SetIntensity` while a flash lasts (about 0.45 s).
- **R1 (mobile cost):** all 36 `P9LightPool` lights now use `MaxDrawDistance` 4000 and `MaxDistanceFadeRange` 1000
  (were 6000 and 1500). The light count is unchanged (R2 deferred), and the moon shadow stays 1 cascade to 30 m (R3 kept).
- **Builds:** Editor (Development) and Win64 Shipping both succeeded with the C++ change. The editor was closed by the user
  for the rebuild. The map was never reloaded through the MCP.
- **Validation (desktop PIE, PC D3D SM6):**
  - **Matched captures with a held flash:** the camera was re-applied before each frame. Mean luminance 0–255, normal → peak:

    | View | Region | Normal | Peak |
    |---|---|---|---|
    | Open (track level) | sky | 13 | 135 |
    | | middle | 14 | 60 |
    | | floor | 12 | 40 |
    | Under the PF2/3 canopy | middle | 33 | 73 |
    | | lamp-pool floor | 135 | 139 |

    In play the peak lasts 0.03 s, followed by a 0.09 s decay.
  - **Restore:** after every flash, cloud, horizon and zenith equal their cached bases (6 decimals), the moon is 2.0000,
    the skylight is 1.0000 and `activeBolt=-1`. Next-flash times stay within 12–40 s.
  - **Traversal:** the full suite passed with Stage 2A results.
  - **Map Check:** 0 errors and 0 warnings.
  - **Original maps:** SHA-256 unchanged for `L_Station_Blockout`, `L_LedgeTest` and `L_Mod_IslandPlatform`.
- **Force No Precomputed Lighting (option A, no build run):**
  - It read back `True` after a fresh editor start.
  - No `*_BuiltData` asset exists anywhere under `Content/`.
  - The log has no "needs rebuild" message, and the editor viewport capture shows no "LIGHTING NEEDS TO BE REBUILT" overlay.
  - The World Settings checkbox has not been confirmed by eye yet.
  - **Note:** Build Lighting is not harmless here. Before checking that flag, the engine scans visible levels for duplicate
    `LevelBuildDataId`s. The four island instances share one, so it would assign new IDs and mark the instanced island
    packages dirty. Don't run it, and never save the island module if prompted after one.
- **Android packaging of the redesign:** see *Environment*.

### Stage 3A.3: one-module canopy integration (2026-10-18). Status: awaiting review
- **Context:** the canopy prototype was modelled in Blender, imported correctly scaled into `/Game/P9/Environment/Canopy_v2/`,
  and given mobile materials in Stage 3A.2. Those materials are `M_P9_Weathered_Master` plus three instances, using `T_P9_WeatherMask`.
- **What was added:** exactly one actor, directly in the persistent level of `L_Station_Redesign`. It is not inside the shared
  island module, which is unchanged.
  - **Label:** `S3A_Canopy_PF2_3_W` (`StaticMeshActor_452`).
  - **Transform:** location (−2700, 430, 0) cm, rotation 0/0/0, scale 1. This is island PF2/3's west end, X −3000…−2400.
  - **Mesh:** `/Game/P9/Environment/Canopy_v2/SM_P9_Canopy_Bay6m` (600 × 1036 × 524 cm, 4 imported `UCX_` convex hulls). Mobility Static.
  - **Collision:** `BlockAll`, query and physics.
  - **Per-actor material overrides:** slot 0 `MI_P9_Concrete`, slot 1 `MI_P9_PaintedSteel`, slot 2 `MI_P9_RoofMetal`.
    The mesh's default slots are unchanged (the placeholder materials).
  - **Method:** placed with editor Python (`spawn_actor_from_object`, `set_material`). It saved only after checking that the map was
    the only dirty package. The editor was relaunched directly on the map, with no MCP map load or reload.
- **Recovery backup:** made with the editor fully closed, before any edit:
  `C:\Users\niles\Documents\Unreal Projects\_backups\ThanePlatform9\L_Station_Redesign_pre3A3_2026-10-18.umap`.
  - SHA-256 `bda45ca54891cdb976a2f99746ed12993927b572fe22c5641315085ecf825a43`, matching the source at the time. The hash is also
    stored beside the backup as `.sha256`.
  - The map after the change is SHA-256 `4a7929ceedc72e2e0e3633618a92815ee7d2ba77996845f80e67e2b73e0cba0a` (900,200 bytes).
  - **To restore:** close the editor and copy the backup over the map. That is an overwrite, so it needs approval.
- **Static checks (pass):**
  - exactly 1 actor with the label, and 737 actors in total;
  - transform, mesh, mobility, materials and collision read back exactly;
  - the 4 hulls are intact (plinth Z 0–30, column 30–480, two roof slabs 436–525);
  - Map Check: **0 errors, 0 warnings**, the same as the pre-change baseline.
- **Clearance, measured from the placed actor's real bounds:**
  - **old blockout column at X −2400:** 18.2 cm below the roof sheet;
  - **old blockout canopy:** abuts exactly at X −2400, no overlap;
  - **end fence:** 250 cm below the roof;
  - **stair gap, footbridges, signs and lamps:** none over this section.
- **PIE traversal (pass):** the full Stage 2A suite gave the same results.
  - The PF9/10 stair descent ended at X 1482.7 instead of 1492.7, still on the platform. That is timing variance.
- **New tests (pass):**

  | Test | Expected | Result |
  |---|---|---|
  | Centreline walk west from X −2470 | stop at the new column, X ≈ −2646 | **X −2645.90** |
  | Walk west 2.5 m off-centre | pass the column, stop at the end fence ≈ −2961 | **X −2960.90** |
  | Cross under the roof, and walk along the edge under the gutter line | no roof contact | Z 90.15 throughout |
  | Drop onto the roof | land at Z ≈ 587 | **Z 587.68** |
  | Platform-level walk under the X −2400 joint | no snag | none |
  | Walk along the roof across the joint | — | steps up onto the old blockout canopy (Z 610). The roof is not reachable in normal play. |

  - The first centreline attempt started at (−2000, 430), which overlaps a bench, so the teleport was rejected. That run was
    invalid, not failed, and it was redone from X −2470.
- **Seams:** no gap at the X −2400 joint during a flash. There is the expected hard step: the old flat blockout canopy is at
  500–520 cm, while the new roof slopes from a 450 cm eave to a 524 cm ridge.
- **Storm (pass):** captures under ambient night light and held `p9.Storm.Flash` flashes, at platform level, track level and under
  the joint. After every flash the moon read 2.0000, the skylight 1.0000, and the cloud colour equalled its base.
- **ES3.1 preview (pass):** the redesign was launched with `-featureleveles31` and `-ExecCmds="p9.Test.Teleport -1950 260 100 180"`.
  The teleport ran after the map loaded. The module rendered with no shader or material errors or warnings in the log.
  This is not a device test: no Android FPS or performance has been measured.
- **Unchanged:** verified by hashing 49 files and by a size/timestamp snapshot of `Content`, `Config`, `Source`, the MCP plugin
  source and the `.uproject`. Only `L_Station_Redesign.umap` changed. Also confirmed:
  - `L_Station_Blockout`, `L_LedgeTest`, `L_Mod_IslandPlatform`, `Test/L_Test_CanopyImport` and the stray root `L_Test_CanopyImport.umap`;
  - all `Canopy_v2`, `Canopy` (old 6 cm) and Sky assets;
  - Config, the default map, gameplay code, mobile controls, ledge climbing and the storm controller;
  - the Blender `.blend` files and the FBX;
  - no Build Lighting was run, and no BuiltData exists.
- **Known limitation:** the west end has no dedicated lamp. The module reads only by ambient moon and sky light and lightning flashes.
  (Resolved by Stage 3A.5.)
- **Git:** nothing was staged, committed or pushed.

## Stages 3A.4–3A.12 (2026-10-24). Status: done and reported; no approval recorded here

All of these changed only `L_Station_Redesign` (persistent level) plus new assets, unless stated. Every map change was preceded by a
timestamped backup in `C:\Users\niles\Documents\Unreal Projects\_backups\ThanePlatform9\` with a `.sha256` file beside it, was placed
with editor Python that saved only when the map was the single dirty package, and ended with Map Check **0 errors, 0 warnings**.
Not done in any of them: Build Lighting, changes to the shared island module, the default map, Config, mobile controls, movement,
ledge climbing or the storm controller, and any Git staging or commit. After every lightning flash used in testing, the moon read
2.0000 and the skylight 1.0000, and the editor throttle was restored to 1.

Blender sources are under `C:\Users\niles\Documents\Blender Projects\P9_Assets\<family>\`, each with `Export\` (FBX, unit scale 100)
and `Textures\` folders. Text is English only, in the Inter font shipped with Blender (SIL OFL).

| Stage | Map hash before (backup) → after | Actors |
|---|---|---|
| 3A.4 | map not changed by the stage (see note) | 737 |
| 3A.5 | `cc68ecd6…` (`pre3A5_2026-10-24_124636`) → `0d37243c…` | 737 → 739 |
| 3A.6 | `0d37243c…` (`pre3A6_2026-10-24_130433`) → `9ef61813…` | 739 → 740 |
| 3A.7 | `9ef61813…` (`pre3A7_2026-10-24_132320`) → `2998fb9c…` | 740 → 757 |
| 3A.8 | `2998fb9c…` (`pre3A8_2026-10-24_133957`) → `f2789cdb…` | 757 → 763 |
| 3A.9 | `f2789cdb…` (`pre3A9_2026-10-24_135548`) → `b5cd7f5f…` | 763 → 793 |
| 3A.10 | `b5cd7f5f…` (`pre3A10_2026-10-24_140353`) → `8268bfbb…` | 793 → 789 |
| 3A.11 | `8268bfbb…` (`pre3A11_2026-10-24_140911`) → `a9d9f01e…` | 789 → 821 |
| 3A.12 | `a9d9f01e…` (`pre3A12_2026-10-24_141553`) → `cbd9065a…` | 821 → 900 |

Actor counts are from the editor (they include actors inside the island Level Instances).

### Stage 3A.4: canopy review and material tuning
- Read-only visual review of the 3A.3 canopy (night and held-flash captures). A second canopy bay at the same end was judged unsafe (no clear space).
- Approved material-instance change, values only (`/Game/P9/Environment/Canopy_v2/Materials/`):
  `MI_P9_Concrete` `ColorA` (0.330, 0.318, 0.290) → (0.420, 0.410, 0.380); `MI_P9_PaintedSteel` `ColorA` (0.052, 0.066, 0.058) →
  (0.0676, 0.0858, 0.0754); `MI_P9_RoofMetal` `MacroTiling` 0.30 → 0.25 and `StreakAmount` 0.85 → 0.70.
  Shader cost unchanged (339 pixel / 153 vertex instructions). ES3.1 preview clean.
- **Out-of-band map save (2026-10-18 14:16:18 local):** the map changed from the recorded `4a7929ce…` to `cc68ecd6…` (same 900,200 bytes)
  outside any task. An offline comparison against the pre-3A.3 backup (`PkgInfo`, `DiffAssets`) found only the 3A.3 canopy actor added and no
  other exported-property change. Byte identity with `4a7929ce…` can't be proven (no copy survives). Decision on accepting it: not recorded.

### Stage 3A.5: west-end tube light
- New `SM_P9_TubeLight` (64 tris, 130 × 7 × 62.9 cm) and unlit `M_P9_LampTube` in `/Game/P9/Environment/StationLighting/`; batten slot reuses `MI_P9_PaintedSteel`.
  Source `StationLighting\P9_TubeLight_v001.blend`.
- Actors: `S3A5_TubeLight_PF2_3_W` at (−2850, 575, 489.7), Static, no collision, no shadow; `S3A5_Lamp_PF2_3_W` spot light at
  (−2850, 575, 425), Movable, unshadowed, 750 cd, radius 900, tagged `P9LightPool`. Lamp count 36 → 37.
- Full traversal suite and ES3.1 preview passed.

### Stage 3A.6: PF2/3 platform sign
- New `SM_P9_PlatformSign` (92 tris), `M_P9_SignFace`, `MI_P9_Sign_PF2_3`, `T_P9_Sign_PF2_3` in `/Game/P9/Environment/StationSignage/`.
  Source `StationSignage\P9_PlatformSign_v001.blend`.
- Actor `S3A6_PlatformSign_PF2_3_W` at (−2950, 430, 505.8), Static, no collision. ES3.1 and the full suite were not run (as instructed).

### Stage 3A.7: reference-driven details for the PF2/3 west end
- New meshes: `SM_P9_TrainIndicator`, `SM_P9_StationClock`, `SM_P9_FOBSign`, `SM_P9_IslandSkin_60m`, `SM_P9_CanopyValance_Bay6m`,
  `SM_P9_HoopFence_3m`; shared masters `M_P9_TexturedLit`, `M_P9_TexturedLitMasked`, `M_P9_LEDPanel` in `/Game/P9/Environment/Shared/`;
  25 new assets in total across `StationSignage`, `PlatformSurface`, `CanopyDetails`, `TrackFence` and `Shared`.
- 17 actors: `S3A7_IslandSkin_PF2_3` (floor zoning and painted edge, overlay 1 cm above the module deck), `S3A7_CanopyValance_PF2_3_W`,
  `S3A7_TrainIndicator_PF2_3_W`, `S3A7_StationClock_PF2_3_W`, `S3A7_FOBSign_PF2_3`, and 12 hoop-top fence panels on Y −480 and 1340.
- Changed existing actors: the sign's slot 1 now uses `MI_P9_Sign_PF2_3_Red`; `S2_TrackFence_-480_1` and `S2_TrackFence_1340_1` are
  hidden in game with their collision kept (nothing deleted).
- Full suite and ES3.1 preview passed.

### Stage 3A.8: first OHE masts and the footbridge floor
- New `SM_P9_OHEMast_9m` (42 tris, 50 × 50 × 903 cm, lattice shaft, number plate in slot 3), `MI_P9_MastLattice`, plates
  `MI_P9_MastPlate_34_21…29` (`/Game/P9/Environment/OHEMasts/`); `SM_P9_FOBFloor_PF2_3` and `MI_P9_FOBChequer` (`FootbridgeFloor/`).
- Actors: `S3A8_FOBFloor_PF2_3` at (0, 0, 600), no collision; `S3A8_OHEMast_W_21/23/25/27/29` at X −3800 (beyond the west blockers),
  Z −90, on Y −480 / 1340 / 3160 / 4980 / 6500, no collision.

### Stage 3A.9: OHE over T1–T4 (west), red/white platform edge, coach-watering pipeline
- New in `OHEMasts/`: `SM_P9_OHECantilever_Double` (368 tris; registration arms at ±2.42 m), `SM_P9_OHEWireSpan_10m` (contact wire +
  catenary with 1.40 m support height and 0.95 m sag, droppers; scaled in X per span), `SM_P9_OHEMast_9m_Col` (same mast plus one UCX box),
  `MI_P9_Insulator`, plates 22/24/26/30/32/34. Source `OHEMasts\P9_OHECantilever_v001.blend`.
- New `MI_P9_PlatformFace_RedWhite` (`PlatformSurface/`), applied as a per-actor override on slot 1 of `S3A7_IslandSkin_PF2_3`.
- OHE conventions set here and reused since: contact wire at Z 475 (5.50 m above rail top −75); catenary at the supports Z 615;
  masts on the track-pair centre lines, 2.40 m from each track centre; inner masts use the collision mesh (`BlockAll`); end masts beyond the
  blockers, cantilevers and wires have no collision; wires cast no shadow.
- Placed 6 masts (X −2775, −1400, −575 on Y −480 and 1340), 8 cantilevers, 12 spans, plus the pipeline (removed in 3A.10).

### Stage 3A.10: coach-watering pipeline removed
- Removed the four `S3A9_CoachWater_PF2_3_*` actors only (they were the only users of the pipeline mesh). Nothing else changed.
- Pipeline assets were **kept, unused**: `/Game/P9/Environment/TrackServices/SM_P9_CoachWateringPipe_6m`, `MI_P9_YellowPaint`,
  `T_P9_YellowPaint`, and `P9_Assets\TrackServices\` (`P9_CoachWatering_v001.blend`, FBX, texture). Deleting them needs approval.
- Re-checked 2026-10-24 (Stage 3A.13): **0 pipeline actors** in the map.
- No next stage was documented at that point, so none was started.

### Stage 3A.11: OHE over T1–T4 east of the footbridge
- Masts at X 575, 1725, 3075, 3800 on Y −480 and 1340 (plates 36–50), mirroring the west; they avoid the S2 fence gap X 1350–1650 and
  the old-footbridge piers (X 2250, 2550). End masts at X 3800 (beyond the east track-end fence at 3600) have no collision.
- 8 cantilevers and 16 spans, including `S3A11_OHEWire_T1…T4_FOB` (X −575 → 575) joining the west section under the main footbridge.
- Clearances (catenary / contact wire below deck underside Z 570): main footbridge 38.5 / 95 cm; old footbridge 41.7 / 95 cm
  (the 1725–3075 span is centred on it so the catenary sags under).

### Stage 3A.12: OHE over the remaining track pairs
- Line Y 3160 (T5 2920, T6 3400): masts at X −2775, −1400, 1725, 3075, 3800 (the concourse piers stand on this line at X ±600), so one
  31.25 m span (X −1400 → 1725) passes under the concourse; catenary clearance 21.1 cm, the tightest in the station.
- Lines Y 4980 (T7 4740, T8 5220) and Y 6500 (goods line TG 6260, T9 6740): same mast X positions as T1–T4.
- 19 masts (plates 52–88), 22 cantilevers (including on the 3A.8 end masts), 38 spans.
- Traversal: T5–T9 and TG walk unobstructed to both track-end fences (X ±3560.9); the fence gap at X −1800 still crosses;
  PF6/7, PF8 and PF9/10 edge walks and the PF9/10 stair unchanged.

### OHE coverage (as of 2026-10-24)

| Mast line (Y) | Tracks | Mast X positions | Status |
|---|---|---|---|
| −480 | T1 (−720), T2 (−240) | −3800, −2775, −1400, −575, 575, 1725, 3075, 3800 | covered |
| 1340 | T3 (1100), T4 (1580) | same | covered |
| 3160 | T5 (2920), T6 (3400) | −3800, −2775, −1400, 1725, 3075, 3800 | covered |
| 4980 | T7 (4740), T8 (5220) | as −480 | covered |
| 6500 | TG (6260), T9 (6740) | as −480 | covered |
| none | **T10 (8080)** | — | **not covered** (see Stage 3A.13) |

Every covered track has continuous wire from X −3800 to 3800. Total OHE actors: 5 (3A.8) + 26 (3A.9) + 32 (3A.11) + 79 (3A.12) = 142.

### Other notes from 2026-10-24
- **Jump button:** a read-only investigation found that hiding it would trap Android players on the tracks (90 cm platform edge, 45 cm
  step height). Option 2 (walk-up ledge climb) was started and interrupted before any file was edited. Decision pending.
- **Manual map save after 3A.12:** at 14:24:26 local the editor saved `L_Station_Redesign` through its own save prompt (not a tool script),
  then a play session ran and the editor was quit at 14:29. The map hash is now `5c9cc994…` (1,299,308 bytes) instead of `cbd9065a…`.
  A read-only check in a headless commandlet (Stage 3A.13) found all 147 Stage 3A.5–3A.12 actors exactly at their recorded design values,
  37 `P9LightPool` lamps and no pipeline actors. Changes outside the `S3A*` actors were not checked.

## Stage 3A.13: T10 investigation (2026-10-24). Status: proposal, awaiting approval; no map change

- **Geometry (measured):** T10 rails at Y 7992–8000 and 8160–8168 (centre 8080); track bed to Y 8400; `S2_EastWall` at Y 8400–8430,
  Z −90…310; track-end fences at X ±3600 and blockers at X ±3650 run to Y 8400. No mast line exists north of Y 6500.
- **Nearby structures:** PF9/10 deck ends at Y 7910 and its canopy at Y 7950 (Z 500–520); main footbridge deck B ends at Y 7910
  (north parapet 7910–7920), so it does **not** cross T10; old-footbridge deck 2 (X 2200–2600) ends at Y 8000 with a barricade at
  Y 7995–8005, Z 600–850. No fences, piers or lamps between T10 and the wall.
- **Why no existing line works:** line 6500 is 1,580 cm away. A mast on PF9/10 (Y ≈ 7840) would stand on the platform edge and run
  through the canopy (Z 500–520), so it is rejected.
- **Proposed arrangement:** a single-track mast line at **Y 8320** (T10 + 240, the project-wide offset), at the usual X positions
  (−3800, −2775, −1400, −575, 575, 1725, 3075, 3800).
  - **New asset needed:** `SM_P9_OHECantilever_Single`, the −Y half of the existing double cantilever (same tubes, insulators and
    registration arm). The double cannot be reused: its north arm would reach Y 8590, over and beyond the boundary wall, serving no track.
  - **Wires:** 7 spans of `SM_P9_OHEWireSpan_10m` at Y 8080, Z 475, as on the other tracks.
  - **Collision/shadow:** inner masts `SM_P9_OHEMast_9m_Col` (`BlockAll`), X ±3800 end masts `SM_P9_OHEMast_9m` (no collision),
    cantilevers and wires no collision, wires no shadow. Plates `34|90` … `34|104`.
  - **Clearances (from geometry):** mast faces 127 cm from the north rail and 55 cm from the wall; wire 130 cm from the PF9/10 canopy
    edge, 80 cm from the old-footbridge deck edge and 75 cm from its barricade; nothing above the wire.
  - **Player path:** the 127 cm gap between rail and mast stays walkable; the 55 cm gap behind each inner mast is narrower than the
    68 cm capsule, so players pass on the track side.
- **Open decisions:** approve the new single-cantilever mesh; accept the Y 8320 line and plate numbers; note that the project-wide
  mast offset puts mast faces 2.15 m from track centres, below the 2.36 m that Indian Railways' Schedule of Dimensions is generally
  understood to require (not verified here; applies to all existing lines too).

## Environment

### Android toolchain (set up 2026-10-17; this replaces the older note that no SDK or NDK was installed)
- **Android SDK (per user):** `C:\Users\niles\AppData\Local\Android\Sdk`
  - platforms `android-34` (rev 3), build-tools 35.0.1, CMake 3.22.1
  - **NDK r27c (27.2.12479018)**, platform-tools r37.0.1, cmdline-tools 23.0
  - All existing licence files were kept.
- **JDK:** Android Studio's bundled JBR 17.0.11 (`C:\Program Files\Android\Android Studio\jbr`).
- **User environment variables:** `ANDROID_HOME`, `JAVA_HOME`, `NDKROOT`, `NDK_ROOT`, written by Epic's `SetupAndroid.bat`.
  The user `PATH` was not changed, so `platform-tools` is not on it.
  The separate Visual Studio SDK at `C:\Program Files (x86)\Android\android-sdk` was not touched.
- **Turnkey** reports Android as Valid (r27c).
- **Development Android build:** `RunUAT BuildCookRun -platform=Android -cookflavor=ASTC -clientconfig=Development` succeeded.
  - Output in `Saved\Builds\Android_Development`: `ThanePlatform9-arm64.apk` (97.4 MB) and the OBB (44.9 MB).
  - It was built before the station redesign and uses the default map `L_Station_Blockout`.
- **Redesign Android build (2026-10-18):** BuildCookRun (Development, ASTC) **succeeded**:
  `AutomationTool exiting with ExitCode=0 (Success)`.
  - The cook reported `Success - 0 error(s), 0 warning(s)`, and the cook and package log has no warnings or errors.
  - Command: `-map=/Game/P9/Maps/L_Station_Redesign+/Game/P9/Maps/Modules/L_Mod_IslandPlatform+/Game/P9/Maps/L_Station_Blockout`
    and `-cmdline=/Game/P9/Maps/L_Station_Redesign`, archived to `Saved\Builds\Android_Redesign`.
  - Output: `ThanePlatform9-arm64.apk` (97,428,149 bytes) and `main.1.com.YourCompany.ThanePlatform9.obb` (46,913,682 bytes),
    plus install, uninstall and symbol files.
  - Cooked maps: `Entry`, `L_Station_Blockout`, `L_Station_Redesign`, `L_Mod_IslandPlatform`.
  - **Launch override:** this APK contains `assets/UECommandLine.txt` =
    `-project="../../../ThanePlatform9/ThanePlatform9.uproject" /Game/P9/Maps/L_Station_Redesign`.
    The original APK's file has only `-project=…`, so it starts on the default map.
  - The override is per-package: UAT regenerates the staged command line every run, and UBT deletes `assets/UECommandLine.txt`
    when none is staged (`UEDeployAndroid.cs`). `Config/` and `GameDefaultMap` are unchanged.
  - The original `Saved\Builds\Android_Development` build is untouched: all 8 files SHA-256-identical before and after.
  - Regenerated intermediates (approved): `Saved\StagedBuilds\Android_ASTC`, `Saved\Cooked\Android_ASTC`, `Intermediate\Android`.
- **Not done yet:** no device has been connected (`adb devices` is empty). Neither APK has been installed or run on a phone,
  and no Android appearance or performance has been verified.

