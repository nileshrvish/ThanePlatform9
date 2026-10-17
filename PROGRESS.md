# ThanePlatform9 — Progress

_Last updated: 2026-10-17_

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

## Current status (uncommitted, awaiting review; as of 2026-10-17)

- **Modified:** `BP_P9_Character` (`LedgeClimb` component), `IMC_Default` (SpaceBar → `IA_Jump`),
  `WBP_MobileControls` (C++ parent, circular ↑ `JumpButton`, transparent `LookArea`),
  `WBP_TouchJoystick` (C++ parent, thumb rewire), `ThanePlatform9.uproject` (module and EnhancedInput entries), this file.
- **New:** `IA_Jump`, `L_LedgeTest` (test map), `Source/` (12 files).
- **Not changed:** `Config/`, `L_Station_Blockout`, `Plugins/`, `.cursor/`.
- **Passed in PIE:** movement, jump, look, all six ledge heights plus the low ceiling, joystick capture and reset, look and Jump
  conflict tests, two-finger use, and the shutdown cleanup. The Win64 Shipping compile passed, and the test tooling isn't in that build.
- **Still outstanding:** real Android multi-touch on a device; packaged Android and desktop builds (no Android SDK or NDK installed);
  the mid-mantle obstruction case; a platform-edge mantle with no barrier; the one-off joystick-reset anomaly after a screenshot;
  and the pressed-state visual. Details are below.

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

### Environment
There's no Android SDK or NDK installed, so testing on a device and packaging an Android build both need that set up later.
