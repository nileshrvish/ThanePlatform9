# ClaudeUnrealMCP — Implementation Plan & Roadmap

**Created:** 2026-02-01
**Last Updated:** 2026-05-01
**Repo:** https://github.com/AgustinJimenez/ClaudeUnrealMCP

---

## Current Status: 109 tools

### Completed Sprints

| Sprint | Date | Features | Status |
|--------|------|----------|--------|
| 1 | 2026-02-01 | Blueprint function creation (create, add inputs/outputs, rename) | DONE |
| 2 | 2026-02-03 | Level actor properties (read/set actor props, components) | DONE |
| 3 | 2026-02-05 | Component map manipulation, replace component class | DONE |
| 4 | 2026-02-07 | CDO property manipulation, interface management | DONE |
| 5 | 2026-02-09 | Node manipulation (connect, disconnect, add struct nodes, delete) | DONE |
| 6 | 2026-02-11 | Input system reading (IMC) | DONE |
| 7 | 2026-02-13 | Struct migration (14 migration/fix tools) | DONE |
| 8 | 2026-02-15 | Enum migration, pin fixes | DONE |
| 9 | 2026-02-21 | Chooser table migration with nested walker | DONE |
| 10 | 2026-04-16 | Asset ops (duplicate, inspect, set_property) via C++ EditorAssetLibrary | DONE |
| 11 | 2026-05-01 | Actor spawning, level management (6 tools) | DONE |
| 12 | 2026-05-01 | Material system (5 tools) | DONE |
| 13 | 2026-05-01 | Widget Blueprint / UMG (4 tools) | DONE |
| 14 | 2026-05-01 | Behavior Trees (4 tools) | DONE |
| 15 | 2026-05-01 | Quality of Life — search/rename/delete assets (3 tools) | DONE |

### Unique Strengths (no competitor has these)
- **14 BP-to-C++ migration tools** (struct/enum/interface/chooser migration)
- **Animation BP specialization** (clear_anim_graph, clear_animation_blueprint_tags)
- **Fine-grained node repair** (reconstruct_node, break_orphaned_pins, fix_pin_enum_type)
- **PropertyAccess path fixing**
- **Blueprint reparenting** with full tooling

---

## Competitive Gap Analysis (May 2026)

### Competitors Analyzed
- **[StraySpark](https://www.strayspark.studio/products/unreal-mcp-server)** — commercial, 100+ tools
- **[remiphilippe/mcp-unreal](https://github.com/remiphilippe/mcp-unreal)** — Go + HTTP, PIE/console/Niagara/mesh
- **[GenOrca/unreal-mcp](https://github.com/GenOrca/unreal-mcp)** — Python UE module, material graphs/BT/widgets
- **[Monolith](https://github.com/tumourlove/monolith)** — 1,286 actions across 16 modules
- **[unrealmcp (PyPI)](https://pypi.org/project/unrealmcp/)** — 280 commands, 13 categories
- **[VibeUE](https://github.com/kevinpbuckley/VibeUE)** — 950 methods, 29 services
- **[chongdashu/unreal-mcp](https://github.com/chongdashu/unreal-mcp)** — Python, socket-based

### Reference repos cloned locally:
- `E:/repo/unreal-mcp-genorca/` — GenOrca (material graphs, log reading)
- `E:/repo/mcp-unreal-remi/` — remiphilippe (PIE, console, Niagara, mesh, logs)
- `E:/repo/unreal-mcp-chong/` — chongdashu (Python socket pattern)

---

## Remaining Gaps — Prioritized Backlog

### Sprint 16 — PIE + Editor Workflow (HIGH priority, LOW effort)

**Reference:** remiphilippe `internal/editor/editor_utils.go` + `internal/editor/utilities.go`

- [ ] `play_in_editor` — start/stop/query PIE sessions
  - UE API: `GEditor->PlayInEditor(GWorld, ...)` / `GEditor->RequestEndPlayMap()`
  - Remi uses HTTP endpoint `/api/editor/pie_control` with `start`/`stop`/`status` ops
- [ ] `execute_console_command` — run UE console commands (CVar, stat, etc.)
  - UE API: `GEngine->Exec(GWorld, *Command)` or `UKismetSystemLibrary::ExecuteConsoleCommand`
  - Remi: tries plugin `/api/editor/console_command`, falls back to Remote Control API
- [ ] `read_log` — read editor output log with filtering
  - UE API: Read `FPaths::ProjectLogDir()` + glob for .log file, filter by keyword/regex/line count
  - GenOrca: `unreal.Paths.project_log_dir()`, reads file, filters by keyword
  - Remi: HTTP endpoint with category/verbosity/regex/max_lines/since_seconds filters
- [ ] `get_engine_version` — return UE version string
  - UE API: `FEngineVersion::Current().ToString()`
- [ ] `set_viewport_camera` — set editor viewport camera position/rotation
  - UE API: `GEditor->GetActiveViewport()->GetViewportClient()->SetViewLocation/SetViewRotation`

### Sprint 17 — Material Graph Authoring (HIGH priority, MEDIUM effort)

**Reference:** GenOrca `Content/Python/UnrealMCPython/material_actions.py`

- [ ] `add_material_expression` — add expression node to material graph
  - UE API: `UMaterialEditingLibrary::CreateMaterialExpression(Material, ExpressionClass, X, Y)`
  - GenOrca: `unreal.MaterialEditingLibrary.create_material_expression(material, expression_class, x, y)`
  - Common expressions: TextureSample, Multiply, Lerp, Add, Constant, VectorParameter, ScalarParameter, TextureCoordinate
- [ ] `connect_material_expressions` — wire expression outputs to inputs
  - UE API: `UMaterialEditingLibrary::ConnectMaterialExpressions(FromExpr, OutputName, ToExpr, InputName)`
  - GenOrca: `unreal.MaterialEditingLibrary.connect_material_expressions(from_expr, out_name, to_expr, in_name)`
- [ ] `disconnect_material_expression` — disconnect a material expression input
  - UE API: `UMaterialEditingLibrary::DeleteMaterialExpression`
- [ ] `recompile_material` — recompile after graph changes
  - UE API: `UMaterialEditingLibrary::RecompileMaterial(Material)`
- [ ] `list_material_expressions` — list all expression nodes in a material graph
  - UE API: iterate `Material->GetExpressionCollection().Expressions`

### Sprint 18 — Sequencer / Cinematics (HIGH priority, MEDIUM effort)

**Reference:** StraySpark (12 tools), Monolith (118 actions)

- [ ] `create_level_sequence` — create new LevelSequence asset
  - UE API: `IAssetTools::CreateAsset` with `ULevelSequenceFactoryNew`
- [ ] `add_sequence_track` — add transform/animation/audio/event track
  - UE API: `UMovieScene::AddTrack`, bind actor via `FMovieSceneSequenceID`
- [ ] `add_keyframe` — add keyframe at time with value
  - UE API: `UMovieSceneSection`, `AddKey` on channels (float, bool, transform)
- [ ] `set_sequence_playback` — set range, rate, loop
  - UE API: `ULevelSequence::GetMovieScene()->SetPlaybackRange`
- [ ] `read_level_sequence` — inspect sequence structure (tracks, sections, keys)
  - UE API: iterate `MovieScene->GetTracks()`, sections, channels

### Sprint 19 — Niagara VFX (HIGH priority, MEDIUM effort)

**Reference:** remiphilippe `internal/editor/niagara.go`

- [ ] `create_niagara_system` — create Niagara system asset
  - UE API: factory-based creation
- [ ] `spawn_niagara_system` — spawn Niagara system actor in level
  - Remi: `/api/niagara/ops` with op=`spawn_system`, system_path, location
- [ ] `set_niagara_parameter` — set parameter on Niagara component
  - Remi: ops `set_parameter` with param name, type (float/vector/color), value
- [ ] `get_niagara_info` — inspect system/emitter structure
  - Remi: ops `get_system_info` returns emitters, parameters, renderers
- [ ] `niagara_activate` / `niagara_deactivate` — control system playback

### Sprint 20 — Animation Authoring (HIGH priority, HIGH effort)

**Reference:** Monolith (118 actions), VibeUE (89 methods)

- [ ] `add_anim_state` — add state to AnimGraph state machine
- [ ] `add_anim_transition` — add transition rule between states
- [ ] `create_montage` — create AnimMontage from AnimSequence
- [ ] `add_montage_section` — add section to montage
- [ ] `create_blend_space` — create 1D/2D blend space
- [ ] `add_blend_space_sample` — add animation sample point
- [ ] `read_anim_graph` — read AnimGraph state machine structure (states, transitions, nodes)
- [ ] `add_anim_notify` — add notify/notify state to animation

### Sprint 21 — State Trees (MEDIUM priority, MEDIUM effort)

- [ ] `create_state_tree` — create StateTree asset
- [ ] `add_state_tree_state` — add state with tasks
- [ ] `add_state_tree_transition` — add transition with conditions
- [ ] `read_state_tree` — inspect StateTree structure

### Sprint 22 — Data Tables & Data Assets (MEDIUM priority, LOW effort)

**Reference:** unrealmcp (8 commands), VibeUE (18 methods)

- [ ] `create_data_table` — create DataTable with row struct
- [ ] `add_data_table_row` — add/edit row by name
- [ ] `read_data_table` — list all rows with values
- [ ] `delete_data_table_row` — remove row
- [ ] `import_data_table` — import from CSV/JSON string

### Sprint 23 — Audio / MetaSounds (MEDIUM priority, MEDIUM effort)

**Reference:** Monolith (86 actions), StraySpark (9 tools)

- [ ] `create_sound_cue` — create SoundCue asset
- [ ] `add_sound_cue_node` — add expression node (wave player, mixer, attenuation)
- [ ] `create_metasound` — create MetaSound source asset
- [ ] `add_metasound_node` — add MetaSound graph node
- [ ] `connect_metasound_nodes` — wire MetaSound nodes

### Sprint 24 — Landscape & Foliage (MEDIUM priority, MEDIUM effort)

**Reference:** StraySpark (7 tools), VibeUE (90 methods)

- [ ] `create_landscape` — create landscape actor with dimensions
- [ ] `paint_landscape_layer` — apply material layer weights
- [ ] `add_foliage_type` — create/configure foliage type
- [ ] `paint_foliage` — place foliage instances at locations

### Sprint 25 — PCG (MEDIUM priority, MEDIUM effort)

**Reference:** remiphilippe (7 commands)

- [ ] `create_pcg_component` — add PCG component to actor
- [ ] `set_pcg_parameter` — set PCG graph parameters
- [ ] `execute_pcg_graph` — run PCG generation
- [ ] `read_pcg_results` — get generated instances

### Sprint 26 — GAS (MEDIUM priority, HIGH effort)

**Reference:** Monolith (135 actions), StraySpark (8 tools)

- [ ] `create_gameplay_ability` — create ability BP
- [ ] `create_gameplay_effect` — create GE with modifiers
- [ ] `create_attribute_set` — create attribute set
- [ ] `add_gameplay_tag` — create/manage gameplay tags

### Sprint 27 — Physics & Collision (LOW priority, LOW effort)

- [ ] `set_physics_simulation` — enable/disable physics on component
- [ ] `set_collision_profile` — set collision preset/channels
- [ ] `create_physics_constraint` — add constraint between components

### Sprint 28 — Networking (LOW priority, MEDIUM effort)

- [ ] `set_replication` — configure actor/component replication
- [ ] `add_rpc` — set up RPC functions

### Sprint 29 — Spline Tools (LOW priority, LOW effort)

- [ ] `create_spline_component` — add spline to actor
- [ ] `add_spline_point` — add point with position/tangent
- [ ] `read_spline_data` — get all points and metadata

---

## Architecture Notes

### Adding a new command

1. Declare handler in `Source/ClaudeUnrealMCP/Public/MCPServer.h`
2. Register in `CommandHandlers` map in `MCPServerCore.cpp`
3. Implement in appropriate `MCPServer*.cpp` file
4. Add tool definition in `MCPServer/toolDefinitions.js`
5. Rebuild plugin, restart editor

### File organization
- `MCPServerCore.cpp` — command routing, ping, list_structs
- `MCPServerRead*.cpp` — read-only commands (blueprints, components, actors, etc.)
- `MCPServerComponent*.cpp` — component manipulation
- `MCPServerNode*.cpp` — blueprint graph node operations
- `MCPServerMigration*.cpp` — BP-to-C++ migration tools
- `MCPServerPython.cpp` — generic asset ops (duplicate, inspect, set_property)
- `MCPServerLevelCommands.cpp` — actor spawn/destroy/duplicate, level load/list
- `MCPServerMaterialCommands.cpp` — material create/instance/params/assign
- `MCPServerWidgetCommands.cpp` — widget BP create/add/set/read
- `MCPServerBehaviorTreeCommands.cpp` — BT/BB create/read/add keys
- `MCPServerAssetCommands.cpp` — search/rename/delete assets
- `MCPServerHelpers.cpp/.h` — shared utility functions

### Key constraints
- **DO NOT** add `PythonScriptPlugin` dependency — crashes editor on startup (PostLoad assertion)
- Use `UEditorAssetLibrary` (from `EditorScriptingUtilities` module) for asset ops instead
- All commands run on the game thread via `AsyncTask(ENamedThreads::GameThread, ...)`
- TCP server on port 9877, JSON protocol, newline-terminated messages

### Reference repos (cloned locally for implementation reference)
- `E:/repo/unreal-mcp-genorca/` — GenOrca: material graph authoring via Python unreal module
- `E:/repo/mcp-unreal-remi/` — remiphilippe: PIE, console, Niagara, procedural mesh via HTTP/RC API
- `E:/repo/unreal-mcp-chong/` — chongdashu: Python socket-based, simple spawn/transform
