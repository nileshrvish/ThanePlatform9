# ThanePlatform9 — Progress

_Last updated: 2026-10-17_

## What this project is

ThanePlatform9 is an **Unreal Engine 5.7, Android-first mobile game**: a first-person walkable greybox
of a railway station (platforms, tracks, footbridge and stairs), set on Platform 9 at Thane.
It is a **Blueprint-only** project (no C++ `Source/` folder). All game content lives under `Content/P9/`.

## Done so far (committed)

| Commit | Step | What it did |
|---|---|---|
| `287b98a` | Setup | Git LFS tracking for Unreal binary assets |
| `7df480a` | Setup | Android-first mobile rendering settings |
| `991e07f` | 1A | Core classes (`BP_P9_GameMode`, `BP_P9_Character`, `BP_P9_PlayerController`), Enhanced Input assets (`IA_Move`, `IA_Look`, `IA_Interact`, `IMC_Default`), blockout map `L_Station_Blockout` |
| `cd9bbca` | 1B | First-person movement and look through Enhanced Input |
| `bbea23c` | 1C | Station greybox: platforms, tracks, footbridge, stairs |

## In progress (not committed yet)

- `Config/DefaultInput.ini`: `DefaultTouchInterface` changed from the engine's default virtual joysticks to `None`,
  so custom touch UI can replace it.
- `Content/P9/Core/BP_P9_PlayerController.uasset` has been modified.
- New, untracked: `Content/P9/UI/WBP_MobileControls.uasset`, `Content/P9/UI/WBP_TouchJoystick.uasset`.

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
  Its event graph is empty, so it has **no logic**, and **there's no Jump button**.
- **WBP_TouchJoystick:** a SizeBox containing an Overlay with `Base` and `Thumb` images, plus `ActivePointer`, `Radius` and `DeadZone` variables.
  Its event graph is empty, so it has **no logic**.
- **DefaultInput.ini:** `DefaultTouchInterface=None` (uncommitted). The engine's virtual joysticks are turned off.
- **Climbing system:** this project doesn't contain one. The downloaded one is at `Documents/Unreal Projects/ClimbingSystem-main 5.7`
  (DanialKama's ClimbingSystem, Blueprint-only and third-person). It has `BP_PlayerCharacter` (LedgeForwardTracer → LedgeHeightTracer →
  GrabLedge → ClimbLedge, plus knobs), `ABP_Player` with IK, Mixamo montages (`AM_IdleToBracedHang`, `AM_ClimbStand`, ...)
  and `SKM_Manny_Simple`.

### MCP limitation
The MCP can't add new Blueprint graph nodes, such as function calls, branches or events. It can only connect, delete or duplicate
nodes that already exist and set their defaults. It *can* add widgets and components, set properties, reparent Blueprints,
add input mappings, duplicate assets and compile.

### Environment
There's no Android SDK or NDK installed, so testing on a device and packaging an Android build both need that set up later.
