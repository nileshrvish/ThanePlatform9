# Unreal Engine MCP Feature Research

**Research Date:** 2026-02-01
**Purpose:** Identify features from other UE5 MCP projects that could enhance our blueprint-to-C++ conversion workflow

---

## Current MCP Server Capabilities

Our current implementation (`ClaudeUnrealMCP`) provides:

### Blueprint Reading
- `read_blueprint` - Overview of blueprint (name, parent, counts)
- `read_variables` - All variables with types and defaults
- `read_class_defaults` - CDO properties including inherited ones
- `read_components` - Component hierarchy
- `read_event_graph` - Event graph nodes and connections
- `read_event_graph_detailed` - Event graph with pin default values (paginated)
- `read_function_graphs` - Function graphs with nodes and connections (paginated)
- `read_timelines` - Timeline templates, tracks, and keys
- `read_interface` - Blueprint Interface function signatures
- `read_user_defined_struct` - User Defined Struct fields
- `read_user_defined_enum` - User Defined Enum entries

### Blueprint Writing/Modification
- `add_component` - Add component to blueprint
- `set_component_property` - Set component property values
- `add_input_mapping` - Add key mapping to input mapping context
- `reparent_blueprint` - Change blueprint parent class
- `compile_blueprint` - Compile a blueprint
- `delete_interface_function` - Remove function from Blueprint Interface
- `delete_function_graph` - Remove function graph from blueprint
- `clear_event_graph` - Remove all nodes from event graph
- `refresh_nodes` - Reconstruct all nodes to fix stale pins
- `break_orphaned_pins` - Remove orphaned pins aggressively
- `delete_user_defined_struct` - Delete struct asset
- `modify_struct_field` - Change struct field types
- `set_blueprint_compile_settings` - Modify compilation settings
- `modify_function_metadata` - Set function flags (BlueprintThreadSafe, BlueprintPure)
- `remove_error_nodes` - Auto-identify and remove nodes causing compilation errors
- `clear_animation_blueprint_tags` - Remove AnimBlueprintExtension_Tag objects
- `clear_anim_graph` - Delete all nodes from AnimGraph

### Asset Management
- `save_asset` - Save single asset to disk
- `save_all` - Save all modified assets
- `list_blueprints` - List all blueprints in project
- `list_actors` - List all actors in current level

### Utilities
- `capture_screenshot` - Capture viewport screenshot
- `ping` - Test connection
- `reload_mcp_server` - Reload MCP server

---

## Other UE5 MCP Projects Analysis

### 1. chongdashu/unreal-mcp
**GitHub:** https://github.com/chongdashu/unreal-mcp
**Focus:** Natural language control of Unreal Engine through AI assistants

**Key Features We Don't Have:**

#### Actor Management
- **Create actors** - Spawn cubes, spheres, lights, cameras, etc.
- **Delete actors** - Remove actors from level
- **Set actor transforms** - Position, rotation, scale
- **Query actor properties** - Get detailed actor information
- **Find actors by name** - Search for specific actors

#### Blueprint Node Graph Manipulation
- **Add event nodes** - BeginPlay, Tick, custom events
- **Create function call nodes** - Call any blueprint function
- **Connect nodes** - Wire pins together
- **Add variables to graphs** - Create variable nodes
- **Create references** - Component refs, self refs
- **Navigate graphs** - Find and manage nodes

#### Blueprint Class Creation
- **Create new Blueprint classes** - Generate from scratch
- **Add custom components** - Attach components during creation
- **Spawn Blueprint actors** - Instantiate blueprints in level

#### Editor Control
- **Focus viewport** - Center on actors or locations
- **Control viewport camera** - Orientation and distance

**Architecture:**
- Native TCP server plugin in C++
- Node.js MCP server communicating via TCP
- Integration with Unreal Editor subsystems

---

### 2. flopperam/unreal-engine-mcp
**GitHub:** https://github.com/flopperam/unreal-engine-mcp
**Focus:** AI-powered world building and architectural creation

**Key Features (50+ tools):**

#### Blueprint Visual Scripting
- `add_node` - Add any node type to blueprint
- `connect_nodes` - Wire pins between nodes
- `delete_node` - Remove nodes from graph
- `set_node_property` - Modify node properties
- `create_variable` - Add blueprint variables
- `set_blueprint_variable_properties` - Configure variable settings
- `create_function` - Add new function to blueprint
- `add_function_input` - Add function parameter
- `add_function_output` - Add return value
- `delete_function` - Remove function
- `rename_function` - Change function name

#### Blueprint Analysis
- `read_blueprint_content` - Extract blueprint information
- `analyze_blueprint_graph` - Pattern detection and analysis
- `get_blueprint_variable_details` - Variable metadata
- `get_blueprint_function_details` - Function signatures and metadata

#### World Building (High-Level Commands)
- `create_town` - Generate entire town layouts
- `construct_house` / `construct_mansion` - Parametric architecture
- `create_tower` / `create_arch` / `create_staircase` - Structural elements
- `create_castle_fortress` - Complex fortifications
- `create_suspension_bridge` / `create_aqueduct` - Infrastructure
- `create_maze` / `create_pyramid` / `create_wall` - Level design

#### Physics & Materials
- `spawn_physics_blueprint_actor` - Instantiate with physics
- `set_physics_properties` - Configure physics settings
- `get_available_materials` - List project materials
- `apply_material_to_actor` - Assign materials
- `set_mesh_material_color` - Modify material colors

#### Advanced Features
- **Autonomous maze generation** - Recursive backtracking algorithm
- **Parametric architecture** - Multiple architectural styles
- **Multi-component structures** - Integrated complex buildings
- **Reasoning model support** - Claude Opus 4.5 integration for multi-step builds

**Architecture:**
- Supports UE 5.5, 5.6, 5.7
- Embedded browser in Unreal Editor (The Flop Agent)
- Fully autonomous AI agent capability

---

### 3. ChiR24/Unreal_mcp
**GitHub:** https://github.com/ChiR24/Unreal_mcp
**Focus:** Ultra-high-performance automation via C++ Automation Bridge

**Key Features:**
- Native C++ Automation Bridge plugin
- TypeScript + Rust (WebAssembly) implementation
- High-performance game development automation
- Comprehensive AI assistant control

**Unique Approach:**
- Uses Unreal's native automation framework
- WebAssembly for performance-critical operations
- Multi-language architecture (TypeScript + C++ + Rust)

---

### 4. ayeletstudioindia/unreal-analyzer-mcp
**GitHub:** https://github.com/ayeletstudioindia/unreal-analyzer-mcp
**Focus:** Source code analysis for Unreal Engine codebases

**Key Features:**

#### C++ Code Analysis
- **Class inspection** - Extract methods, properties, inheritance, access levels
- **Hierarchy mapping** - Visualize class inheritance chains
- **Code searching** - Context-aware pattern matching
- **Reference finding** - Find all usages of functions/classes/variables
- **Pattern detection** - Identify common UE patterns

#### Advanced Analysis
- **Subsystem analysis** - Study Physics, Rendering, Input systems
- **Best practices guidance** - UPROPERTY, UFUNCTION, Components, Events, Replication
- **API documentation queries** - Search official docs with examples

#### Scope
- Official Unreal Engine source code analysis
- Custom C++ codebase analysis
- Applicable to any C++ project

**Use Cases for Our Workflow:**
- Analyze C++ classes before/after blueprint conversion
- Find references to blueprint functions being converted
- Understand inheritance hierarchies
- Validate converted C++ follows UE patterns

---

### 5. runreal/unreal-mcp
**GitHub:** https://github.com/runreal/unreal-mcp
**Focus:** Python-based MCP using built-in Python remote execution

**Key Features:**
- No custom plugin required
- Uses Unreal's built-in Python remote execution protocol
- Lighter weight approach

---

### 6. prajwalshettydev/UnrealGenAISupport
**GitHub:** https://github.com/prajwalshettydev/UnrealGenAISupport
**Focus:** LLM/GenAI integration with multiple AI providers

**Key Features:**
- Multi-provider support (GPT, Deepseek, Claude, Gemini, Qwen, Kimi, Grok)
- Automatic scene generation from AI
- Audio TTS, ElevenLabs integration (planned)
- OpenRouter, Groq, Dashscope support (planned)

---

## Feature Priority for Our Use Case

### High Priority - Blueprint Workflow Enhancements

**1. Blueprint Node Graph Manipulation** ⭐⭐⭐
- Add/delete/connect nodes programmatically
- Set node properties
- Create variables in graphs
- **Value:** Build blueprints programmatically, not just read/delete
- **Impact:** Enable automated refactoring during C++ conversion
- **Complexity:** High (requires deep UE Blueprint API knowledge)

**2. Create Blueprint Functions** ⭐⭐⭐
- Create/delete/rename functions
- Add function inputs/outputs
- Modify function signatures
- **Value:** Currently can only delete functions, not create
- **Impact:** Enable hybrid C++/Blueprint workflows
- **Complexity:** Medium

**3. Blueprint Graph Analysis** ⭐⭐⭐
- Analyze graph patterns
- Detect complexity metrics
- Identify conversion candidates
- **Value:** Better understand blueprints before converting
- **Impact:** Smarter conversion decisions
- **Complexity:** Medium

### Medium Priority - Actor & Level Management

**4. Actor Manipulation** ⭐⭐
- Create/delete actors
- Set transforms
- Query properties
- Find by name
- **Value:** General UE workflow improvement
- **Impact:** Broader MCP utility beyond blueprint conversion
- **Complexity:** Low-Medium

**5. Material Management** ⭐⭐
- Get available materials
- Apply to actors
- Set colors
- **Value:** Complete actor setup workflows
- **Impact:** Useful for testing converted blueprints
- **Complexity:** Low

**6. Viewport Control** ⭐
- Focus on actors
- Control camera
- **Value:** Nice to have for debugging
- **Impact:** Minor convenience
- **Complexity:** Low

### Advanced - Code Analysis

**7. C++ Code Analysis** ⭐⭐
- Class inspection
- Reference finding
- Pattern detection
- **Value:** Complement blueprint→C++ workflow
- **Impact:** Validate converted C++ code
- **Complexity:** High (separate MCP server recommended)

---

## Implementation Strategy

### Phase 1: Foundation (Current Sprint)
**Status:** ✅ Complete
- All blueprint reading capabilities
- Basic blueprint modification (delete, clear, refresh)
- Asset management
- Compilation and saving

### Phase 2: Blueprint Creation (Recommended Next)
**Features to Add:**
1. Create blueprint functions with inputs/outputs
2. Add nodes to blueprint graphs
3. Connect nodes together
4. Set node properties
5. Create blueprint variables

**Implementation Approach:**
- Study `flopperam/unreal-engine-mcp` node manipulation code
- Study `chongdashu/unreal-mcp` graph building code
- Use `FBlueprintEditorUtils` for function creation
- Use `UK2Node_*` classes for node creation
- Use `UEdGraphSchema_K2` for pin connections

**Estimated Effort:** 2-3 days

### Phase 3: Blueprint Analysis
**Features to Add:**
1. Analyze graph complexity (node count, depth, branching)
2. Detect common patterns (loops, branches, function calls)
3. Identify conversion candidates (simple vs complex)
4. Generate conversion recommendations

**Implementation Approach:**
- Graph traversal algorithms
- Pattern matching on node types
- Complexity scoring metrics

**Estimated Effort:** 1-2 days

### Phase 4: Actor Management
**Features to Add:**
1. Create/delete actors (static mesh, light, camera, etc.)
2. Set actor transforms
3. Query actor properties
4. Find actors by name/tag/class

**Implementation Approach:**
- Use `UWorld::SpawnActor` for creation
- Use `AActor::Destroy()` for deletion
- Use `UGameplayStatics` for queries

**Estimated Effort:** 1 day

### Phase 5: Material & Physics
**Features to Add:**
1. Get available materials in project
2. Apply materials to actors/components
3. Set material parameters
4. Configure physics properties

**Implementation Approach:**
- Use `UAssetManager` for material discovery
- Use `UMeshComponent::SetMaterial()` for application
- Use `UPrimitiveComponent` for physics settings

**Estimated Effort:** 1 day

---

## Technical Considerations

### Blueprint Graph Manipulation Challenges

**1. Node Creation:**
- Each node type has specific requirements
- Pin types must match for connections
- Execution flow must be valid
- Variable types must be compatible

**2. Pin Connections:**
- Must validate pin compatibility
- Must handle type conversions
- Must maintain execution order
- Must prevent circular dependencies

**3. Blueprint Compilation:**
- Graphs must be valid before compile
- All pins must be properly connected or have defaults
- Node positions should be set for readability
- Blueprint must be marked dirty for save

### UE API Classes to Use

**Blueprint Classes:**
- `UBlueprint` - Main blueprint asset
- `UBlueprintGeneratedClass` - Generated C++ class
- `FBlueprintEditorUtils` - Blueprint editing utilities
- `UEdGraph` - Graph structure
- `UEdGraphNode` - Base node class
- `UEdGraphPin` - Pin on a node
- `UEdGraphSchema_K2` - Blueprint schema

**Node Classes:**
- `UK2Node_Event` - Event nodes (BeginPlay, Tick, etc.)
- `UK2Node_CallFunction` - Function call nodes
- `UK2Node_VariableGet/Set` - Variable access nodes
- `UK2Node_FunctionEntry/Result` - Function entry/exit
- `UK2Node_IfThenElse` - Branch nodes
- `UK2Node_MacroInstance` - Macro nodes

**Actor Classes:**
- `UWorld` - World/level container
- `AActor` - Base actor class
- `UActorComponent` - Base component class
- `UStaticMeshComponent` - Static mesh component
- `USceneComponent` - Transform component

---

## Code References from Other Projects

### Node Creation Example (from flopperam/unreal-engine-mcp)
```typescript
// Simplified concept - actual implementation would be in C++
function addNode(blueprintPath, nodeType, positionX, positionY) {
  const blueprint = LoadObject(blueprintPath);
  const graph = blueprint.GetEventGraph();
  const node = CreateNode(nodeType);
  node.NodePosX = positionX;
  node.NodePosY = positionY;
  graph.AddNode(node);
  return node;
}
```

### Pin Connection Example (from chongdashu/unreal-mcp)
```typescript
// Simplified concept
function connectNodes(sourceNode, sourcePinName, targetNode, targetPinName) {
  const sourcePin = sourceNode.FindPin(sourcePinName);
  const targetPin = targetNode.FindPin(targetPinName);

  if (sourcePin && targetPin && arePinsCompatible(sourcePin, targetPin)) {
    sourcePin.MakeLinkTo(targetPin);
    return true;
  }
  return false;
}
```

---

## Success Metrics

### For Blueprint Conversion Workflow:
- ✅ Reduce manual blueprint editing by 80%
- ✅ Automate node deletion/cleanup during conversion
- ✅ Generate C++ stubs from blueprint function signatures
- ✅ Analyze blueprint complexity before conversion
- ✅ Validate blueprint state after modifications

### For General MCP Utility:
- ✅ Create test blueprints programmatically
- ✅ Build prototype levels via AI commands
- ✅ Automate repetitive blueprint tasks
- ✅ Enable blueprint→blueprint refactoring

---

## Conclusion

The research identified **3 high-priority features** that would significantly enhance our blueprint-to-C++ conversion workflow:

1. **Blueprint Node Graph Manipulation** - Build/modify blueprint graphs programmatically
2. **Blueprint Function Creation** - Create functions with proper signatures
3. **Blueprint Graph Analysis** - Understand blueprint complexity before conversion

These features align with our core mission of converting blueprints to C++ incrementally and safely. Other features (actor management, materials, viewport control) would be valuable additions but are lower priority for our specific use case.

**Recommended Next Steps:**
1. Implement blueprint function creation tools
2. Add node manipulation capabilities
3. Build graph analysis features
4. Expand to actor management as time permits

---

## References

- [chongdashu/unreal-mcp](https://github.com/chongdashu/unreal-mcp) - Natural language UE control
- [flopperam/unreal-engine-mcp](https://github.com/flopperam/unreal-engine-mcp) - World building focus
- [ChiR24/Unreal_mcp](https://github.com/ChiR24/Unreal_mcp) - C++ Automation Bridge
- [ayeletstudioindia/unreal-analyzer-mcp](https://github.com/ayeletstudioindia/unreal-analyzer-mcp) - Code analysis
- [runreal/unreal-mcp](https://github.com/runreal/unreal-mcp) - Python remote execution
- [prajwalshettydev/UnrealGenAISupport](https://github.com/prajwalshettydev/UnrealGenAISupport) - Multi-LLM integration
- [MCP Unreal Engine Integrations](https://glama.ai/mcp/servers/integrations/unreal-engine) - Integration directory
