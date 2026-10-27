export const MCP_TOOL_DEFINITIONS = [
      {
        name: "ping",
        description: "Test connection to Unreal Engine",
        inputSchema: {
          type: "object",
          properties: {},
        },
      },
      {
        name: "reload_mcp_server",
        description: "Reload the MCP server (useful after code changes to index.js)",
        inputSchema: {
          type: "object",
          properties: {},
        },
      },
      {
        name: "list_blueprints",
        description: "List all blueprints in the project",
        inputSchema: {
          type: "object",
          properties: {
            path: {
              type: "string",
              description: "Filter by path prefix (e.g., /Game/Blueprints)",
            },
          },
        },
      },
      {
        name: "check_all_blueprints",
        description: "Compile all blueprints and return a list of those with errors or warnings. Useful for finding broken blueprints after code changes.",
        inputSchema: {
          type: "object",
          properties: {
            path: {
              type: "string",
              description: "Filter by path prefix (e.g., /Game/Blueprints). Defaults to /Game/",
            },
            include_warnings: {
              type: "boolean",
              description: "Include blueprints with warnings (not just errors). Default: false",
            },
          },
        },
      },
      {
        name: "read_blueprint",
        description: "Get overview of a blueprint (name, parent class, counts)",
        inputSchema: {
          type: "object",
          properties: {
            path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
          },
          required: ["path"],
        },
      },
      {
        name: "read_variables",
        description: "Read all variables defined in a blueprint",
        inputSchema: {
          type: "object",
          properties: {
            path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
          },
          required: ["path"],
        },
      },
      {
        name: "read_class_defaults",
        description: "Read Blueprint Class Default Object (CDO) properties, including inherited properties. IMPORTANT: This reads class defaults from the Blueprint asset (.uasset), NOT level instance property overrides. For actual working values from a placed actor, use read_actor_properties instead. CDO values are often placeholder/first-iteration values and may not match actual gameplay behavior.",
        inputSchema: {
          type: "object",
          properties: {
            path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
          },
          required: ["path"],
        },
      },
      {
        name: "read_components",
        description: "Read all components in a blueprint's component hierarchy",
        inputSchema: {
          type: "object",
          properties: {
            path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
          },
          required: ["path"],
        },
      },
      {
        name: "read_component_properties",
        description: "Read all properties of a specific component in a blueprint's CDO (Class Default Object). Use this to inspect component configuration including class references, default values, and settings.",
        inputSchema: {
          type: "object",
          properties: {
            path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
            component_name: {
              type: "string",
              description: "Name of the component to read properties from (e.g., CharacterMover, SkeletalMesh)",
            },
          },
          required: ["path", "component_name"],
        },
      },
      {
        name: "read_event_graph",
        description:
          "Read the event graph nodes and their connections from a blueprint",
        inputSchema: {
          type: "object",
          properties: {
            path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
          },
          required: ["path"],
        },
      },
      {
        name: "read_event_graph_detailed",
        description:
          "Read the event graph nodes, connections, and pin default values from a blueprint",
        inputSchema: {
          type: "object",
          properties: {
            path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
            max_nodes: {
              type: "integer",
              description: "Optional max nodes to return (for large graphs, use pagination)",
            },
            start_index: {
              type: "integer",
              description: "Optional start index into graph nodes (pagination)",
            },
          },
          required: ["path"],
        },
      },
      {
        name: "read_function_graphs",
        description:
          "Read function graph nodes, connections, and pin default values from a blueprint",
        inputSchema: {
          type: "object",
          properties: {
            path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
            name: {
              type: "string",
              description: "Optional function graph name to filter (exact match)",
            },
            max_nodes: {
              type: "integer",
              description: "Optional max nodes per graph (for large graphs)",
            },
            start_index: {
              type: "integer",
              description: "Optional start index into graph nodes (pagination)",
            },
          },
          required: ["path"],
        },
      },
      {
        name: "read_timelines",
        description: "Read timeline templates, tracks, and keys from a blueprint",
        inputSchema: {
          type: "object",
          properties: {
            path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
          },
          required: ["path"],
        },
      },
      {
        name: "read_interface",
        description: "Read function signatures from a Blueprint Interface",
        inputSchema: {
          type: "object",
          properties: {
            path: {
              type: "string",
              description: "Full path to the interface asset",
            },
          },
          required: ["path"],
        },
      },
      {
        name: "read_user_defined_struct",
        description: "Read fields from a User Defined Struct asset",
        inputSchema: {
          type: "object",
          properties: {
            path: {
              type: "string",
              description: "Full path to the struct asset",
            },
          },
          required: ["path"],
        },
      },
      {
        name: "read_user_defined_enum",
        description: "Read entries from a User Defined Enum asset",
        inputSchema: {
          type: "object",
          properties: {
            path: {
              type: "string",
              description: "Full path to the enum asset",
            },
          },
          required: ["path"],
        },
      },
      {
        name: "list_actors",
        description: "List all actors in the current level",
        inputSchema: {
          type: "object",
          properties: {},
        },
      },
      {
        name: "read_actor_components",
        description: "List components on a level actor instance (name, class, active state). Use this to discover component names for read_actor_component_properties.",
        inputSchema: {
          type: "object",
          properties: {
            actor_name: {
              type: "string",
              description: "Name of the actor in the level (e.g., LevelBlock_Traversable_C_24)",
            },
          },
          required: ["actor_name"],
        },
      },
      {
        name: "read_actor_component_properties",
        description: "Read EditAnywhere/BlueprintVisible properties from a specific component on a level actor instance.",
        inputSchema: {
          type: "object",
          properties: {
            actor_name: {
              type: "string",
              description: "Name of the actor in the level (e.g., LevelBlock_Traversable_C_24)",
            },
            component_name: {
              type: "string",
              description: "Component name to inspect (use read_actor_components to find names)",
            },
          },
          required: ["actor_name", "component_name"],
        },
      },
      {
        name: "find_actors_by_name",
        description: "Search for actors by name pattern (supports wildcards: * for any characters, ? for single character)",
        inputSchema: {
          type: "object",
          properties: {
            name_pattern: {
              type: "string",
              description: "Name pattern to search for (e.g., 'LevelBlock*', '*Traversable*', 'Player?')",
            },
            actor_class: {
              type: "string",
              description: "Optional: Filter by actor class (e.g., 'LevelBlock_C', 'StaticMeshActor')",
            },
          },
          required: ["name_pattern"],
        },
      },
      {
        name: "get_actor_material_info",
        description: "Get detailed material information from an actor's components (materials, textures, parameters)",
        inputSchema: {
          type: "object",
          properties: {
            actor_name: {
              type: "string",
              description: "Name of the actor in the level (e.g., LevelBlock_C_0)",
            },
          },
          required: ["actor_name"],
        },
      },
      {
        name: "get_scene_summary",
        description: "Get a comprehensive overview of the current level (actor counts by class, level info, performance stats)",
        inputSchema: {
          type: "object",
          properties: {
            include_details: {
              type: "boolean",
              description: "Include detailed breakdown of actor types (default: true)",
            },
          },
        },
      },
      // Write commands
      {
        name: "add_component",
        description: "Add a component to a blueprint",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
            component_class: {
              type: "string",
              description: "Class name of the component to add (e.g., CameraToggleComponent, StaticMeshComponent)",
            },
            component_name: {
              type: "string",
              description: "Name for the new component (optional, auto-generated if not provided)",
            },
          },
          required: ["blueprint_path", "component_class"],
        },
      },
      {
        name: "set_component_property",
        description: "Set a property value on a component in a blueprint",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
            component_name: {
              type: "string",
              description: "Name of the component to modify",
            },
            property_name: {
              type: "string",
              description: "Name of the property to set",
            },
            property_value: {
              type: "string",
              description: "Value to set (for object references, use full asset path)",
            },
          },
          required: ["blueprint_path", "component_name", "property_name", "property_value"],
        },
      },
      {
        name: "set_blueprint_cdo_class_reference",
        description: "Set a class reference property on a component in a blueprint CDO (Class Default Object). Use this to change blueprint class references to C++ classes (e.g., change BP_MovementMode_Falling to FallingMode C++ class).",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
            component_name: {
              type: "string",
              description: "Name of the component to modify (e.g., CharacterMover)",
            },
            property_name: {
              type: "string",
              description: "Name of the class property to set (e.g., FallingModeClass)",
            },
            class_name: {
              type: "string",
              description: "Full path or name of the C++ class to reference (e.g., /Script/UETest1.FallingMode or FallingMode)",
            },
          },
          required: ["blueprint_path", "component_name", "property_name", "class_name"],
        },
      },
      {
        name: "replace_component_map_value",
        description: "Replace an object instance in a component's map property with a new instance of a different class. Use this to replace blueprint movement mode instances with C++ class instances in the MovementModes map.",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
            component_name: {
              type: "string",
              description: "Name of the component to modify (e.g., CharacterMover)",
            },
            property_name: {
              type: "string",
              description: "Name of the map property (e.g., MovementModes)",
            },
            map_key: {
              type: "string",
              description: "The map key to modify (e.g., 'Falling', 'Walking')",
            },
            target_class: {
              type: "string",
              description: "Full path or name of the C++ class to instantiate (e.g., /Script/Mover.FallingMode or FallingMode)",
            },
          },
          required: ["blueprint_path", "component_name", "property_name", "map_key", "target_class"],
        },
      },
      {
        name: "replace_blueprint_array_value",
        description: "Replace an object instance in a blueprint CDO's array property with a new instance of a different class. Use this to replace blueprint transition instances with C++ class instances in the Transitions array.",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
            property_name: {
              type: "string",
              description: "Name of the array property (e.g., Transitions)",
            },
            array_index: {
              type: "integer",
              description: "Index of the array element to replace (0-based)",
            },
            target_class: {
              type: "string",
              description: "Full path or name of the C++ class to instantiate (e.g., /Script/Mover.BaseMovementModeTransition or BaseMovementModeTransition)",
            },
          },
          required: ["blueprint_path", "property_name", "array_index", "target_class"],
        },
      },
      {
        name: "add_input_mapping",
        description: "Add a key mapping to an input mapping context",
        inputSchema: {
          type: "object",
          properties: {
            context_path: {
              type: "string",
              description: "Full path to the input mapping context asset",
            },
            action_path: {
              type: "string",
              description: "Full path to the input action asset",
            },
            key: {
              type: "string",
              description: "Key name (e.g., O, SpaceBar, LeftMouseButton)",
            },
          },
          required: ["context_path", "action_path", "key"],
        },
      },
      {
        name: "remove_input_mapping",
        description: "Remove a key mapping from an input mapping context, via UInputMappingContext::UnmapKey/UnmapAllKeysFromAction - the counterpart to add_input_mapping, which has no built-in way to remove what it added. Useful when a mapping context was duplicated from an existing one (e.g. via run_python duplicate_asset) and inherited mappings that shouldn't be active in the new context.",
        inputSchema: {
          type: "object",
          properties: {
            context_path: {
              type: "string",
              description: "Full path to the input mapping context asset",
            },
            action_path: {
              type: "string",
              description: "Full path to the input action asset",
            },
            key: {
              type: "string",
              description: "Key name to unmap (e.g., LeftMouseButton). Omit to unmap ALL keys currently mapped to this action in this context.",
            },
          },
          required: ["context_path", "action_path"],
        },
      },
      {
        name: "reparent_blueprint",
        description: "Reparent a blueprint to a new parent class",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
            parent_class: {
              type: "string",
              description: "Parent class name or path (e.g., /Script/Engine.Actor or /Game/Blueprints/BP_MyBase.BP_MyBase_C)",
            },
          },
          required: ["blueprint_path", "parent_class"],
        },
      },
      {
        name: "compile_blueprint",
        description: "Compile a blueprint",
        inputSchema: {
          type: "object",
          properties: {
            path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
          },
          required: ["path"],
        },
      },
      {
        name: "save_asset",
        description: "Save an asset to disk",
        inputSchema: {
          type: "object",
          properties: {
            path: {
              type: "string",
              description: "Full path to the asset",
            },
          },
          required: ["path"],
        },
      },
      {
        name: "save_all",
        description: "Save all modified assets in the project",
        inputSchema: {
          type: "object",
          properties: {},
        },
      },
      {
        name: "delete_interface_function",
        description: "Delete a function from a Blueprint Interface",
        inputSchema: {
          type: "object",
          properties: {
            interface_path: {
              type: "string",
              description: "Full path to the interface asset",
            },
            function_name: {
              type: "string",
              description: "Name of the function to delete",
            },
          },
          required: ["interface_path", "function_name"],
        },
      },
      {
        name: "remove_implemented_interface",
        description: "Remove an implemented interface from a blueprint. Use this when a blueprint has interface function overrides that conflict with C++ parent implementations.",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
            interface_name: {
              type: "string",
              description: "Name of the interface to remove (e.g., 'BPI_SandboxCharacter_Pawn')",
            },
          },
          required: ["blueprint_path", "interface_name"],
        },
      },
      {
        name: "list_structs",
        description: "Debug command: List all registered UScriptStruct objects matching a pattern. Useful for discovering discoverable struct names.",
        inputSchema: {
          type: "object",
          properties: {
            pattern: {
              type: "string",
              description: "Pattern to filter struct names (e.g., 'FS_', 'CharacterProperties'). Default: 'FS_'",
            },
          },
        },
      },
      {
        name: "modify_interface_function_parameter",
        description: "Modify a parameter type in a Blueprint Interface function. Note: UE strips the 'F' prefix from C++ struct names in reflection (e.g., FS_PlayerInputState becomes S_PlayerInputState). Use list_structs to find discoverable names.",
        inputSchema: {
          type: "object",
          properties: {
            interface_path: {
              type: "string",
              description: "Full path to the interface asset",
            },
            function_name: {
              type: "string",
              description: "Name of the function to modify",
            },
            parameter_name: {
              type: "string",
              description: "Name of the parameter to modify (e.g., 'ReturnValue' for return type)",
            },
            new_type: {
              type: "string",
              description: "New type path (e.g., '/Script/UETest1.S_PlayerInputState' for C++ struct, '/Game/Blueprints/Data/S_PlayerInputState.S_PlayerInputState' for Blueprint struct)",
            },
            is_output: {
              type: "boolean",
              description: "True if modifying an output/return parameter, false for input parameters. Default: false",
            },
          },
          required: ["interface_path", "function_name", "parameter_name", "new_type"],
        },
      },
      {
        name: "delete_function_graph",
        description: "Delete a function graph from a Blueprint or Blueprint Function Library",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
            function_name: {
              type: "string",
              description: "Name of the function graph to delete",
            },
          },
          required: ["blueprint_path", "function_name"],
        },
      },
      {
        name: "clear_event_graph",
        description: "Clear all nodes from a blueprint's event graph (useful for full C++ conversions to remove blueprint logic)",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
          },
          required: ["blueprint_path"],
        },
      },
      {
        name: "empty_graph",
        description: "TEST: Empty all nodes from event graph (alias for clear_event_graph)",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
          },
          required: ["blueprint_path"],
        },
      },
      {
        name: "refresh_nodes",
        description: "Refresh/reconstruct all nodes in a blueprint to fix stale pin errors",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
          },
          required: ["blueprint_path"],
        },
      },
      {
        name: "break_orphaned_pins",
        description: "Aggressively remove orphaned pins and break their connections. Use when refresh_nodes doesn't fix orphaned pin errors.",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
          },
          required: ["blueprint_path"],
        },
      },
      {
        name: "delete_user_defined_struct",
        description: "Delete a user-defined struct asset. Useful when replacing blueprint structs with C++ versions.",
        inputSchema: {
          type: "object",
          properties: {
            struct_path: {
              type: "string",
              description: "Full path to the struct asset",
            },
          },
          required: ["struct_path"],
        },
      },
      {
        name: "modify_struct_field",
        description: "Modify a field type in a user-defined struct. Useful for updating blueprint structs to use C++ struct types.",
        inputSchema: {
          type: "object",
          properties: {
            struct_path: {
              type: "string",
              description: "Full path to the struct asset",
            },
            field_name: {
              type: "string",
              description: "Name of the field to modify (e.g., BlockColors_19_BD7B5F9248A47F4BA4AEE2BCADEEA20F)",
            },
            new_type: {
              type: "string",
              description: "New type for the field (e.g., FS_GridMaterialParams for C++ struct, S_GridMaterialParams for blueprint struct)",
            },
          },
          required: ["struct_path", "field_name", "new_type"],
        },
      },
      {
        name: "set_blueprint_compile_settings",
        description: "Modify blueprint compilation settings (e.g., thread-safe execution, construction script behavior)",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
            run_construction_script_on_drag: {
              type: "boolean",
              description: "Whether to run construction script when dragging in editor",
            },
            generate_const_class: {
              type: "boolean",
              description: "Whether to generate const class",
            },
            force_full_editor: {
              type: "boolean",
              description: "Whether to force full editor",
            },
          },
          required: ["blueprint_path"],
        },
      },
      {
        name: "modify_function_metadata",
        description: "Modify function metadata flags (e.g., BlueprintThreadSafe, BlueprintPure)",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
            function_name: {
              type: "string",
              description: "Name of the function to modify",
            },
            blueprint_thread_safe: {
              type: "boolean",
              description: "Whether the function is thread-safe (can be called from animation worker threads)",
            },
            blueprint_pure: {
              type: "boolean",
              description: "Whether the function is pure (no execution pins)",
            },
          },
          required: ["blueprint_path", "function_name"],
        },
      },
      {
        name: "capture_screenshot",
        description: "Capture a screenshot of the active Unreal Engine viewport",
        inputSchema: {
          type: "object",
          properties: {
            filename: {
              type: "string",
              description: "Optional filename (without extension). Defaults to 'MCP_Screenshot'. A timestamp will be appended automatically.",
            },
          },
        },
      },
      {
        name: "remove_error_nodes",
        description: "Automatically identify and remove nodes causing compilation errors in a blueprint. Useful for cleaning up after C++ conversions.",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
            auto_rewire: {
              type: "boolean",
              description: "Attempt to reconnect execution flow around deleted nodes (default: true)",
            },
          },
          required: ["blueprint_path"],
        },
      },
      {
        name: "clear_animation_blueprint_tags",
        description: "Remove AnimBlueprintExtension_Tag objects from an animation blueprint to fix 'cannot find referenced node with tag' errors. Use when tagged nodes have been removed but extensions still reference them. Use remove_extension: true to completely remove the tag extension (more aggressive fix).",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the animation blueprint asset",
            },
            remove_extension: {
              type: "boolean",
              description: "If true, completely removes the tag extension from the blueprint instead of just clearing its data. Use this for persistent tag errors that don't clear with the default behavior.",
            },
          },
          required: ["blueprint_path"],
        },
      },
      {
        name: "clear_anim_graph",
        description: "Delete all nodes from an animation blueprint's AnimGraph, leaving only the root output node. Use when rebuilding an AnimGraph from scratch or clearing corrupted graph state.",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the animation blueprint asset",
            },
          },
          required: ["blueprint_path"],
        },
      },
      // Sprint 1: Blueprint Function Creation Commands
      {
        name: "create_blueprint_function",
        description: "Create a new function in a blueprint with optional metadata flags (BlueprintThreadSafe, BlueprintPure, Const)",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
            function_name: {
              type: "string",
              description: "Name of the new function to create",
            },
            is_pure: {
              type: "boolean",
              description: "Whether the function is pure (no execution pins, returns value only). Default: false",
            },
            is_thread_safe: {
              type: "boolean",
              description: "Whether the function is thread-safe (can be called from animation worker threads). Default: false",
            },
            is_const: {
              type: "boolean",
              description: "Whether the function is const (doesn't modify object state). Default: false",
            },
          },
          required: ["blueprint_path", "function_name"],
        },
      },
      {
        name: "add_function_input",
        description: "Add an input parameter to an existing blueprint function",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
            function_name: {
              type: "string",
              description: "Name of the function to modify",
            },
            parameter_name: {
              type: "string",
              description: "Name of the input parameter to add",
            },
            parameter_type: {
              type: "string",
              description: "Type of the parameter (e.g., int, float, bool, string, FVector, FRotator, FTransform, or full path to object/struct type)",
            },
          },
          required: ["blueprint_path", "function_name", "parameter_name", "parameter_type"],
        },
      },
      {
        name: "add_function_output",
        description: "Add an output parameter (return value) to an existing blueprint function",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
            function_name: {
              type: "string",
              description: "Name of the function to modify",
            },
            parameter_name: {
              type: "string",
              description: "Name of the output parameter to add",
            },
            parameter_type: {
              type: "string",
              description: "Type of the parameter (e.g., int, float, bool, string, FVector, FRotator, FTransform, or full path to object/struct type)",
            },
          },
          required: ["blueprint_path", "function_name", "parameter_name", "parameter_type"],
        },
      },
      {
        name: "rename_blueprint_function",
        description: "Rename an existing blueprint function",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
            old_function_name: {
              type: "string",
              description: "Current name of the function",
            },
            new_function_name: {
              type: "string",
              description: "New name for the function",
            },
          },
          required: ["blueprint_path", "old_function_name", "new_function_name"],
        },
      },
      {
        name: "read_actor_properties",
        description: "Read all EditAnywhere properties from a level actor instance, including property overrides set in the Details panel. This returns ACTUAL working values from the level file (.umap), not Blueprint class defaults. Use this to: (1) Preserve actor configuration before blueprint reparenting, (2) Get correct reference values from a working project, (3) Read actual gameplay-tested property values. When copying reference data from another project, copy the level file (.umap) and use this command instead of read_class_defaults.",
        inputSchema: {
          type: "object",
          properties: {
            actor_name: {
              type: "string",
              description: "Name of the actor in the level (e.g., LevelVisuals_2, LevelBlock_3)",
            },
          },
          required: ["actor_name"],
        },
      },
      {
        name: "set_actor_properties",
        description: "Set EditAnywhere properties on a level actor instance. Use this to restore actor configuration after blueprint reparenting.",
        inputSchema: {
          type: "object",
          properties: {
            actor_name: {
              type: "string",
              description: "Name of the actor in the level (e.g., LevelVisuals_2, LevelBlock_3)",
            },
            properties: {
              type: "object",
              description: "Object containing property name-value pairs (as returned by read_actor_properties)",
            },
          },
          required: ["actor_name", "properties"],
        },
      },
      {
        name: "set_actor_component_property",
        description: "Set a property on a specific component of a level actor instance. Use for instance-specific component overrides (e.g., collision profile).",
        inputSchema: {
          type: "object",
          properties: {
            actor_name: {
              type: "string",
              description: "Name of the actor in the level (e.g., LevelBlock_Traversable_C_24)",
            },
            component_name: {
              type: "string",
              description: "Component name to modify (use read_actor_components to find names)",
            },
            property_name: {
              type: "string",
              description: "Property name to set (e.g., CollisionProfileName)",
            },
            property_value: {
              type: "string",
              description: "Value to set (as text, e.g., BlockAll)",
            },
          },
          required: ["actor_name", "component_name", "property_name", "property_value"],
        },
      },
      {
        name: "reconstruct_actor",
        description: "Trigger OnConstruction on a level actor by calling RerunConstructionScripts(). Use this after reparenting blueprints to C++ to apply C++ initialization logic (e.g., UpdateLevelVisuals, UpdateMaterials) to existing level instances.",
        inputSchema: {
          type: "object",
          properties: {
            actor_name: {
              type: "string",
              description: "Name of the actor in the level (e.g., LevelVisuals_C_6)",
            },
          },
          required: ["actor_name"],
        },
      },
      {
        name: "clear_component_map_value_array",
        description: "Clear an array property within an object stored in a component's map property. Use this to clear stale data from sub-objects (e.g., clear Transitions array in movement mode instances stored in CharacterMover's MovementModes map).",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
            component_name: {
              type: "string",
              description: "Name of the component (e.g., CharacterMover)",
            },
            map_property_name: {
              type: "string",
              description: "Name of the map property (e.g., MovementModes)",
            },
            map_key: {
              type: "string",
              description: "The key in the map (e.g., 'Walking', 'Sliding')",
            },
            array_property_name: {
              type: "string",
              description: "Name of the array property in the map value object (e.g., Transitions)",
            },
          },
          required: ["blueprint_path", "component_name", "map_property_name", "map_key", "array_property_name"],
        },
      },
      {
        name: "replace_component_class",
        description: "Replace a component's class with a different class (e.g., replace blueprint component class with C++ class). Use this to convert blueprint component references to C++ classes without losing the component configuration.",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
            component_name: {
              type: "string",
              description: "Name of the component to replace (e.g., AC_VisualOverrideManager, BP_VisualOverrideManager)",
            },
            new_class: {
              type: "string",
              description: "Full name of the new component class (e.g., AC_VisualOverrideManager for C++ class, or full path for blueprint class)",
            },
          },
          required: ["blueprint_path", "component_name", "new_class"],
        },
      },
      {
        name: "set_blueprint_cdo_property",
        description: "Set an object reference property on a blueprint's Class Default Object (CDO). Use this to set TObjectPtr properties like UInputAction or UInputMappingContext that are defined in C++ but need values assigned in the blueprint. This sets properties on the blueprint class itself, not on components.",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
            property_name: {
              type: "string",
              description: "Name of the property to set (e.g., IA_Sprint, IMC_Sandbox)",
            },
            property_value: {
              type: "string",
              description: "Full asset path of the object to assign (e.g., /Game/Input/IA_Sprint, /Game/Input/IMC_Sandbox)",
            },
          },
          required: ["blueprint_path", "property_name", "property_value"],
        },
      },
      // Sprint 5: Blueprint Node Manipulation
      {
        name: "connect_nodes",
        description: "Connect two nodes in a blueprint graph by wiring an output pin to an input pin. Use this to programmatically wire up blueprint logic. You must first use read_event_graph to get the node IDs and pin names.",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
            source_node_id: {
              type: "string",
              description: "GUID of the source node (from read_event_graph 'id' field)",
            },
            source_pin: {
              type: "string",
              description: "Name of the output pin on the source node (e.g., 'Triggered', 'ActionValue', 'ReturnValue')",
            },
            target_node_id: {
              type: "string",
              description: "GUID of the target node (from read_event_graph 'id' field)",
            },
            target_pin: {
              type: "string",
              description: "Name of the input pin on the target node (e.g., 'execute', 'Condition', 'Value')",
            },
            graph_name: {
              type: "string",
              description: "Name of the graph to modify (default: 'EventGraph'). Use for function graphs or other named graphs.",
            },
          },
          required: ["blueprint_path", "source_node_id", "source_pin", "target_node_id", "target_pin"],
        },
      },
      {
        name: "disconnect_pin",
        description: "Break all connections from/to a specific pin on a blueprint node. Use this to remove unwanted wiring before creating new connections.",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
            node_id: {
              type: "string",
              description: "GUID of the node (from read_event_graph 'id' field)",
            },
            pin_name: {
              type: "string",
              description: "Name of the pin to disconnect (e.g., 'Completed', 'ActionValue', 'WantsToSprint_1_xxx')",
            },
            graph_name: {
              type: "string",
              description: "Name of the graph (default: 'EventGraph')",
            },
          },
          required: ["blueprint_path", "node_id", "pin_name"],
        },
      },
      {
        name: "add_set_struct_node",
        description: "Add a new 'Set members in Struct' node to a blueprint event graph. Use this to create nodes that set struct field values.",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
            struct_type: {
              type: "string",
              description: "Name or path of the struct type (e.g., 'S_PlayerInputState' for blueprint structs)",
            },
            graph_name: {
              type: "string",
              description: "Name of the graph (default: 'EventGraph')",
            },
            x: {
              type: "integer",
              description: "X position for the node (optional)",
            },
            y: {
              type: "integer",
              description: "Y position for the node (optional)",
            },
            fields: {
              type: "array",
              items: { type: "string" },
              description: "Array of struct field names to expose as input pins on the node (e.g., ['WantsToSprint_1_840C190D4B23289C5C46E0B5A4C5C936']). Use read_user_defined_struct to get the full field names with GUIDs.",
            },
          },
          required: ["blueprint_path", "struct_type"],
        },
      },
      // Sprint 7: Struct Migration
      {
        name: "migrate_struct_references",
        description: "Migrate all blueprint references from a UserDefinedStruct (BP struct) to a C++ USTRUCT across all blueprints. Handles variable types, graph node pins (Break/Make/Set struct nodes), and GUID-to-clean field name remapping. Preserves pin connections by saving and rewiring after node reconstruction. Use dry_run=true to preview changes without modifying anything.",
        inputSchema: {
          type: "object",
          properties: {
            source_struct_path: {
              type: "string",
              description: "Full asset path to the BP UserDefinedStruct to migrate FROM (e.g., '/Game/Blueprints/Data/S_PlayerInputState')",
            },
            target_struct_path: {
              type: "string",
              description: "Path to the C++ UScriptStruct to migrate TO (e.g., '/Script/UETest1.S_PlayerInputState'). UE5 strips the F prefix from C++ struct names.",
            },
            dry_run: {
              type: "boolean",
              description: "If true, report what would change without modifying anything. Default: false",
            },
          },
          required: ["source_struct_path", "target_struct_path"],
        },
      },
      // Sprint 7b: Enum Migration
      {
        name: "migrate_enum_references",
        description: "Migrate all blueprint references from a UserDefinedEnum (BP enum) to a C++ UENUM across all blueprints. Handles variable types, function parameters, graph node pins, and enum literal nodes. Use dry_run=true to preview changes.",
        inputSchema: {
          type: "object",
          properties: {
            source_enum_path: {
              type: "string",
              description: "Full asset path to the BP UserDefinedEnum to migrate FROM (e.g., '/Game/Blueprints/Data/E_Gait')",
            },
            target_enum_path: {
              type: "string",
              description: "Path to the C++ UEnum to migrate TO (e.g., '/Script/UETest1.E_Gait')",
            },
            dry_run: {
              type: "boolean",
              description: "If true, report what would change without modifying anything. Default: false",
            },
          },
          required: ["source_enum_path", "target_enum_path"],
        },
      },
      {
        name: "fix_optional_struct_pin_defaults",
        description: "Fix invalid enum default values stored in optional struct pins (hidden SetFieldsInStruct/MakeStruct pins). Replaces NewEnumeratorN with the enum value names.",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
          },
          required: ["blueprint_path"],
        },
      },
      {
        name: "fix_enum_defaults",
        description: "Fix invalid enum default values in blueprint pins by remapping NewEnumeratorN to actual enum value names. Scans all graphs and updates pins that reference the specified enum.",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
            enum_path: {
              type: "string",
              description: "Full path to the target enum (e.g., /Script/UETest1.E_Gait)",
            },
            old_enum_path: {
              type: "string",
              description: "Optional: Full path to old BP UserDefinedEnum for legacy name mapping (e.g., /Game/Blueprints/Data/E_Gait)",
            },
            dry_run: {
              type: "boolean",
              description: "If true, reports what would change without modifying the blueprint. Default: false",
            },
          },
          required: ["blueprint_path", "enum_path"],
        },
      },
      {
        name: "force_fix_enum_pin_defaults",
        description: "Force-fix enum pin defaults that still use NewEnumeratorN by remapping to the enum's value names, even when the enum subcategory object is missing.",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
            enum_path: {
              type: "string",
              description: "Full path to the enum (e.g., /Script/UETest1.E_TraversalActionType)",
            },
            pin_name_contains: {
              type: "string",
              description: "Optional: only fix pins whose names contain this substring (e.g., ActionType)",
            },
          },
          required: ["blueprint_path", "enum_path"],
        },
      },
      {
        name: "set_struct_field_default",
        description: "Set the default value for a field in a UserDefinedStruct (by field name or friendly name).",
        inputSchema: {
          type: "object",
          properties: {
            struct_path: {
              type: "string",
              description: "Full path to the UserDefinedStruct asset",
            },
            field_name: {
              type: "string",
              description: "Field VarName (with GUID) or friendly name",
            },
            new_default: {
              type: "string",
              description: "New default value string",
            },
          },
          required: ["struct_path", "field_name", "new_default"],
        },
      },
      {
        name: "clean_property_access_paths",
        description: "Remove invalid path segments (default: 'None') from K2Node_PropertyAccess Path arrays in a blueprint.",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
            remove_segment: {
              type: "string",
              description: "Segment value to remove from property access paths (default: 'None')",
            },
          },
          required: ["blueprint_path"],
        },
      },
      {
        name: "fix_property_access_paths",
        description: "Fix PropertyAccess paths after struct migration or invalid field references (e.g., Control Rig warnings).",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
          },
          required: ["blueprint_path"],
        },
      },
      // Sprint 6: Input System Reading
      {
        name: "read_input_mapping_context",
        description: "Read the contents of an Input Mapping Context asset, including all input action mappings with their keys, modifiers, and triggers. Use this to inspect what keys are bound to which input actions.",
        inputSchema: {
          type: "object",
          properties: {
            path: {
              type: "string",
              description: "Full path to the Input Mapping Context asset (e.g., '/Game/Input/IMC_Sandbox')",
            },
          },
          required: ["path"],
        },
      },
      {
        name: "migrate_chooser_table",
        description: "Migrate a Chooser Table from BP UserDefinedStruct to C++ USTRUCT. Updates ContextData struct pointers and all column PropertyBindingChain entries from GUID-suffixed BP field names to clean C++ field names. Optionally updates output enum references.",
        inputSchema: {
          type: "object",
          properties: {
            chooser_path: {
              type: "string",
              description: "Full path to the Chooser Table asset (e.g., '/Game/Animations/Traversal/CHT_TraversalMontages_CMC')",
            },
            field_name_map: {
              type: "object",
              description: "JSON object mapping old GUID-suffixed BP field names to new clean C++ field names. Example: {\"HasFrontLedge_19_52F8C45A46BED06CD777A5A6EFBB34F4\": \"HasFrontLedge\"}",
              additionalProperties: {
                type: "string",
              },
            },
            struct_map: {
              type: "object",
              description: "Optional. Maps old BP struct names to new C++ struct paths for ContextData replacement. Example: {\"S_TraversalChooserInputs\": \"/Script/UETest1.S_TraversalChooserInputs\", \"S_TraversalChooserOutputs\": \"/Script/UETest1.S_TraversalChooserOutputs\"}",
              additionalProperties: {
                type: "string",
              },
            },
            new_struct_path: {
              type: "string",
              description: "Optional legacy. Single struct path to replace ALL ContextData entries. Prefer struct_map for tables with multiple struct types.",
            },
          },
          required: ["chooser_path", "field_name_map"],
        },
      },
      {
        name: "fix_struct_sub_pins",
        description: "Fix GUID-suffixed sub-pin names on Break/Make/SetFieldsInStruct nodes after struct migration. Renames pins in-place, preserving connections.",
        inputSchema: {
          type: "object",
          properties: {
            source_struct_path: {
              type: "string",
              description: "Full path to the old BP UserDefinedStruct (e.g., /Game/Blueprints/Data/S_PlayerInputState)",
            },
            target_struct_path: {
              type: "string",
              description: "Full path to the new C++ USTRUCT (e.g., /Script/UETest1.S_PlayerInputState)",
            },
            blueprint_path: {
              type: "string",
              description: "Optional: Filter to a specific blueprint path",
            },
            dry_run: {
              type: "boolean",
              description: "If true, reports what would change without modifying anything. Default: false",
            },
            reconstruct_events: {
              type: "boolean",
              description: "If true, reconstruct event nodes after fixing sub-pins. Default: false",
            },
          },
          required: ["source_struct_path", "target_struct_path"],
        },
      },
      {
        name: "migrate_interface_references",
        description: "Update K2Node_Message nodes to use a different interface class (e.g., from BP interface to C++ UINTERFACE). Changes FunctionReference.MemberParent on all matching message nodes.",
        inputSchema: {
          type: "object",
          properties: {
            old_interface_path: {
              type: "string",
              description: "Full path to the old interface (e.g., /Game/Blueprints/BPI_SandboxCharacter_Pawn)",
            },
            new_interface_path: {
              type: "string",
              description: "Full path to the new interface (e.g., /Script/UETest1.BPI_SandboxCharacter_Pawn)",
            },
            blueprint_paths: {
              type: "array",
              items: { type: "string" },
              description: "Optional: Array of blueprint paths to process. If omitted, scans all blueprints.",
            },
          },
          required: ["old_interface_path", "new_interface_path"],
        },
      },
      {
        name: "reconstruct_node",
        description: "Reconstruct a blueprint node (rebuilds pins from scratch). Useful after struct/enum type changes. Provide either node_guid or variable_name.",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
            node_guid: {
              type: "string",
              description: "GUID of the specific node to reconstruct",
            },
            variable_name: {
              type: "string",
              description: "Filter: reconstruct nodes referencing this variable name",
            },
            graph_name: {
              type: "string",
              description: "Optional: Name of the graph to search in",
            },
          },
          required: ["blueprint_path"],
        },
      },
      {
        name: "set_pin_default",
        description: "Set the default value of a specific pin on a blueprint node.",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
            graph_name: {
              type: "string",
              description: "Name of the graph containing the node",
            },
            node_guid: {
              type: "string",
              description: "GUID of the node",
            },
            pin_name: {
              type: "string",
              description: "Name of the pin to set",
            },
            new_default: {
              type: "string",
              description: "New default value string for the pin",
            },
          },
          required: ["blueprint_path", "graph_name", "node_guid", "pin_name", "new_default"],
        },
      },
      {
        name: "fix_asset_struct_reference",
        description: "Fix a struct reference in a non-blueprint asset (e.g., Chooser Table ContextData). Replaces old struct pointer with new one.",
        inputSchema: {
          type: "object",
          properties: {
            asset_path: {
              type: "string",
              description: "Full path to the asset (e.g., /Game/Blueprints/Cameras/CHT_CameraRig)",
            },
            old_struct_path: {
              type: "string",
              description: "Full path to the old struct",
            },
            new_struct_path: {
              type: "string",
              description: "Full path to the new struct",
            },
          },
          required: ["asset_path", "old_struct_path", "new_struct_path"],
        },
      },
      {
        name: "delete_node",
        description: "Delete a specific node from a blueprint graph by its GUID.",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
            node_id: {
              type: "string",
              description: "GUID of the node to delete",
            },
            graph_name: {
              type: "string",
              description: "Name of the graph containing the node. Default: EventGraph",
            },
          },
          required: ["blueprint_path", "node_id"],
        },
      },
      {
        name: "add_implemented_interface",
        description: "Add an interface to a blueprint's implemented interfaces list.",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
            interface_path: {
              type: "string",
              description: "Full path to the interface (e.g., /Script/UETest1.BPI_SandboxCharacter_Pawn)",
            },
            skip_graphs: {
              type: "boolean",
              description: "If true, prevents creating override graphs for interface functions. Default: false",
            },
          },
          required: ["blueprint_path", "interface_path"],
        },
      },
      {
        name: "delete_component",
        description: "Delete a component from a blueprint's Simple Construction Script (SCS).",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
            component_name: {
              type: "string",
              description: "Name of the component to delete",
            },
          },
          required: ["blueprint_path", "component_name"],
        },
      },
      {
        name: "rename_local_variable",
        description: "Rename a local variable within a blueprint function graph.",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
            function_name: {
              type: "string",
              description: "Name of the function containing the variable",
            },
            old_name: {
              type: "string",
              description: "Current name of the local variable",
            },
            new_name: {
              type: "string",
              description: "New name for the local variable",
            },
          },
          required: ["blueprint_path", "function_name", "old_name", "new_name"],
        },
      },
      {
        name: "fix_pin_enum_type",
        description: "Fix a pin's enum type reference (PinSubCategoryObject) from one enum to another.",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
            wrong_enum_path: {
              type: "string",
              description: "Full path to the wrong/old enum to replace",
            },
            correct_enum_path: {
              type: "string",
              description: "Full path to the correct/new enum",
            },
            default_value: {
              type: "string",
              description: "Optional: set this default value on fixed pins",
            },
            node_guid: {
              type: "string",
              description: "Optional: only fix pins on this specific node (by GUID)",
            },
          },
          required: ["blueprint_path", "wrong_enum_path", "correct_enum_path"],
        },
      },
      {
        name: "restore_struct_node_pins",
        description: "Restore hidden pins on a K2Node_SetFieldsInStruct node by re-enabling ShowPinForProperties entries.",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
            node_guid: {
              type: "string",
              description: "GUID of the K2Node_SetFieldsInStruct node",
            },
          },
          required: ["blueprint_path", "node_guid"],
        },
      },
      {
        name: "fix_struct_enum_field_defaults",
        description: "Fix enum default values stored in struct field pins across all nodes in a blueprint.",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
          },
          required: ["blueprint_path"],
        },
      },
      {
        name: "run_python",
        description: "Generic editor operations (C++ backed, no Python plugin needed). Use op param to choose operation.",
        inputSchema: {
          type: "object",
          properties: {
            op: {
              type: "string",
              description: "Operation: duplicate_asset, does_asset_exist, save_asset, inspect_asset, set_property, open_asset, export_fbx, inspect_graph_node, set_graph_node_property, duplicate_node",
            },
            source_path: { type: "string", description: "For duplicate_asset" },
            dest_path: { type: "string", description: "For duplicate_asset" },
            path: { type: "string", description: "Asset path for does_asset_exist / save_asset / inspect_asset / set_property / open_asset / export_fbx" },
            out_file: { type: "string", description: "Absolute destination filename for export_fbx" },
            max_depth: { type: "number", description: "For inspect_asset / inspect_graph_node (default 4 / 2)" },
            property: { type: "string", description: "For set_property / set_graph_node_property: property name on the object" },
            value: { type: "string", description: "For set_property / set_graph_node_property: value as text (FProperty::ImportText_Direct format)" },
            blueprint_path: { type: "string", description: "For inspect_graph_node / set_graph_node_property / duplicate_node: Blueprint asset path" },
            graph_name: { type: "string", description: "For inspect_graph_node / set_graph_node_property / duplicate_node: name of the graph containing the node (e.g. 'AnimGraph', 'HandIK', a function graph name)" },
            node_guid: { type: "string", description: "For inspect_graph_node / set_graph_node_property / duplicate_node: node GUID from read_function_graphs/read_event_graph (the node to act on, or to clone for duplicate_node)" },
            dx: { type: "number", description: "For duplicate_node: X position offset for the new node relative to the source (default 0)" },
            dy: { type: "number", description: "For duplicate_node: Y position offset for the new node relative to the source (default 200)" },
          },
          required: ["op"],
        },
      },
      // Sprint 28 — PCG
      {
        name: "pcg_ops",
        description: "Procedural Content Generation operations. list_components: find all PCG components in level. execute: run PCG generation on actor.",
        inputSchema: {
          type: "object",
          properties: {
            operation: { type: "string", description: "list_components or execute" },
            actor_name: { type: "string", description: "For execute: actor name with PCG component" },
          },
          required: ["operation"],
        },
      },
      // Sprint 29 — GAS
      {
        name: "create_gameplay_ability",
        description: "Create a GameplayAbility Blueprint. Requires GameplayAbilities plugin.",
        inputSchema: {
          type: "object",
          properties: {
            asset_path: { type: "string" }, asset_name: { type: "string" },
            parent_class: { type: "string", description: "Parent class (default: GameplayAbility)" },
          },
          required: ["asset_path", "asset_name"],
        },
      },
      {
        name: "create_gameplay_effect",
        description: "Create a GameplayEffect Blueprint. Requires GameplayAbilities plugin.",
        inputSchema: {
          type: "object",
          properties: {
            asset_path: { type: "string" }, asset_name: { type: "string" },
          },
          required: ["asset_path", "asset_name"],
        },
      },
      // Sprint 30 — Networking
      {
        name: "set_replication",
        description: "Configure actor replication settings (replicate, replicate_movement).",
        inputSchema: {
          type: "object",
          properties: {
            actor_name: { type: "string", description: "Actor name or label" },
            replicate: { type: "boolean", description: "Enable replication" },
            replicate_movement: { type: "boolean", description: "Replicate movement" },
          },
          required: ["actor_name"],
        },
      },
      // Sprint 31 — Volumes + Procedural Mesh
      {
        name: "create_volume",
        description: "Create a volume actor in the level. Types: Trigger, Blocking, Audio, NavMesh.",
        inputSchema: {
          type: "object",
          properties: {
            volume_type: { type: "string", description: "Trigger, Blocking, Audio, or NavMesh" },
            x: { type: "number" }, y: { type: "number" }, z: { type: "number" },
            label: { type: "string" },
          },
          required: ["volume_type"],
        },
      },
      {
        name: "create_procedural_mesh",
        description: "Add a ProceduralMeshComponent to an actor and optionally create geometry from vertices/triangles. Vertices format: 'x1,y1,z1;x2,y2,z2;...' Triangles format: '0,1,2;3,4,5;...'",
        inputSchema: {
          type: "object",
          properties: {
            actor_name: { type: "string", description: "Actor to add mesh to" },
            vertices: { type: "string", description: "Vertices as 'x,y,z;x,y,z;...'" },
            triangles: { type: "string", description: "Triangle indices as '0,1,2;3,4,5;...'" },
            section: { type: "number", description: "Mesh section index (default 0)" },
          },
          required: ["actor_name"],
        },
      },
      // Sprint 27 — Animation Authoring
      {
        name: "create_montage",
        description: "Create a new AnimMontage asset from a skeleton.",
        inputSchema: {
          type: "object",
          properties: {
            asset_path: { type: "string", description: "Folder path" },
            asset_name: { type: "string", description: "Montage name" },
            skeleton_path: { type: "string", description: "Path to USkeleton asset" },
          },
          required: ["asset_path", "asset_name", "skeleton_path"],
        },
      },
      {
        name: "add_montage_section",
        description: "Add a composite section to an AnimMontage at a specific time.",
        inputSchema: {
          type: "object",
          properties: {
            montage_path: { type: "string", description: "Montage asset path" },
            section_name: { type: "string", description: "Section name" },
            start_time: { type: "number", description: "Section start time in seconds (default 0)" },
          },
          required: ["montage_path", "section_name"],
        },
      },
      {
        name: "read_montage",
        description: "Read an AnimMontage structure: sections, slot tracks, notifies, length.",
        inputSchema: {
          type: "object",
          properties: {
            path: { type: "string", description: "Montage asset path" },
          },
          required: ["path"],
        },
      },
      {
        name: "create_blend_space",
        description: "Create a new BlendSpace (2D) or BlendSpace1D asset.",
        inputSchema: {
          type: "object",
          properties: {
            asset_path: { type: "string", description: "Folder path" },
            asset_name: { type: "string", description: "BlendSpace name" },
            skeleton_path: { type: "string", description: "Path to USkeleton asset" },
            is_1d: { type: "boolean", description: "Create 1D blend space (default: false = 2D)" },
          },
          required: ["asset_path", "asset_name", "skeleton_path"],
        },
      },
      {
        name: "add_blend_space_sample",
        description: "Add an animation sample point to a BlendSpace at specified coordinates.",
        inputSchema: {
          type: "object",
          properties: {
            blend_space_path: { type: "string", description: "BlendSpace asset path" },
            animation_path: { type: "string", description: "AnimSequence asset path to add as sample" },
            value_x: { type: "number", description: "X axis value" },
            value_y: { type: "number", description: "Y axis value (2D only)" },
          },
          required: ["blend_space_path", "animation_path"],
        },
      },
      {
        name: "read_anim_sequence",
        description: "Read AnimSequence/AnimMontage info: length, skeleton, notifies, frame count, rate scale.",
        inputSchema: {
          type: "object",
          properties: {
            path: { type: "string", description: "Animation asset path" },
          },
          required: ["path"],
        },
      },
      // Sprint 24 — State Trees
      {
        name: "create_state_tree",
        description: "Create a new StateTree asset.",
        inputSchema: {
          type: "object",
          properties: {
            asset_path: { type: "string", description: "Folder path" },
            asset_name: { type: "string", description: "StateTree name" },
          },
          required: ["asset_path", "asset_name"],
        },
      },
      // Sprint 25 — Audio
      {
        name: "create_sound_cue",
        description: "Create a new SoundCue asset.",
        inputSchema: {
          type: "object",
          properties: {
            asset_path: { type: "string", description: "Folder path" },
            asset_name: { type: "string", description: "SoundCue name" },
          },
          required: ["asset_path", "asset_name"],
        },
      },
      {
        name: "create_sound_attenuation",
        description: "Create a SoundAttenuation settings asset with optional inner radius and falloff distance.",
        inputSchema: {
          type: "object",
          properties: {
            asset_path: { type: "string", description: "Folder path" },
            asset_name: { type: "string", description: "Attenuation name" },
            inner_radius: { type: "number", description: "Inner radius (default engine value)" },
            falloff_distance: { type: "number", description: "Falloff distance (default engine value)" },
          },
          required: ["asset_path", "asset_name"],
        },
      },
      // Sprint 26 — Landscape
      {
        name: "get_landscape_info",
        description: "Get information about all landscapes in the current level: name, bounds, component count, material.",
        inputSchema: {
          type: "object",
          properties: {},
        },
      },
      // Sprint 21 — GameplayTags
      {
        name: "manage_gameplay_tags",
        description: "Manage gameplay tags. Operations: list (with optional parent filter), add (create new tag), request (check if valid and get parents).",
        inputSchema: {
          type: "object",
          properties: {
            operation: { type: "string", description: "list, add, or request" },
            tag: { type: "string", description: "Tag name for add/request (e.g. 'Ability.Skill.Fireball')" },
            parent: { type: "string", description: "For list: filter children of this parent tag" },
            comment: { type: "string", description: "For add: dev comment" },
          },
          required: ["operation"],
        },
      },
      // Sprint 22 — Spline Tools
      {
        name: "spline_ops",
        description: "Operate on a SplineComponent attached to an actor. Operations: read (get all points), add_point (add point at xyz), clear (remove all points), set_closed (open/close loop).",
        inputSchema: {
          type: "object",
          properties: {
            actor_name: { type: "string", description: "Actor name or label with SplineComponent" },
            operation: { type: "string", description: "read, add_point, clear, set_closed" },
            x: { type: "number" }, y: { type: "number" }, z: { type: "number" },
            closed: { type: "boolean", description: "For set_closed" },
          },
          required: ["actor_name", "operation"],
        },
      },
      // Sprint 23 — Physics & Collision
      {
        name: "set_physics",
        description: "Set physics properties on an actor's primitive component: simulate_physics, gravity, mass, damping.",
        inputSchema: {
          type: "object",
          properties: {
            actor_name: { type: "string", description: "Actor name or label" },
            simulate_physics: { type: "boolean" },
            enable_gravity: { type: "boolean" },
            mass: { type: "number", description: "Mass override in kg" },
            linear_damping: { type: "number" },
            angular_damping: { type: "number" },
          },
          required: ["actor_name"],
        },
      },
      {
        name: "set_collision",
        description: "Set collision properties on an actor: collision profile, collision enabled mode, overlap events.",
        inputSchema: {
          type: "object",
          properties: {
            actor_name: { type: "string", description: "Actor name or label" },
            collision_profile: { type: "string", description: "Collision preset name (e.g. 'BlockAll', 'OverlapAll', 'Pawn')" },
            collision_enabled: { type: "string", description: "NoCollision, QueryOnly, PhysicsOnly, QueryAndPhysics" },
            generate_overlap_events: { type: "boolean" },
          },
          required: ["actor_name"],
        },
      },
      // Sprint 19 — Niagara VFX
      {
        name: "spawn_niagara_system",
        description: "Spawn a Niagara particle system at a location in the level.",
        inputSchema: {
          type: "object",
          properties: {
            system_path: { type: "string", description: "Niagara system asset path" },
            x: { type: "number", description: "X location" },
            y: { type: "number", description: "Y location" },
            z: { type: "number", description: "Z location" },
            pitch: { type: "number" }, yaw: { type: "number" },
            auto_destroy: { type: "boolean", description: "Auto-destroy when finished (default false)" },
          },
          required: ["system_path"],
        },
      },
      {
        name: "set_niagara_parameter",
        description: "Set a parameter on a Niagara component by actor name. Types: float, int, vector, color.",
        inputSchema: {
          type: "object",
          properties: {
            actor_name: { type: "string", description: "Actor name or label with NiagaraComponent" },
            param_name: { type: "string", description: "Parameter name" },
            param_type: { type: "string", description: "float, int, vector, or color" },
            value: { type: "number", description: "For float/int" },
            x: { type: "number" }, y: { type: "number" }, z: { type: "number" },
            r: { type: "number" }, g: { type: "number" }, b: { type: "number" }, a: { type: "number" },
          },
          required: ["actor_name", "param_name", "param_type"],
        },
      },
      {
        name: "niagara_control",
        description: "Control a Niagara system on an actor: activate, deactivate, or reset.",
        inputSchema: {
          type: "object",
          properties: {
            actor_name: { type: "string", description: "Actor name or label" },
            operation: { type: "string", description: "activate, deactivate, or reset" },
          },
          required: ["actor_name", "operation"],
        },
      },
      // Sprint 20 — Data Tables
      {
        name: "create_data_table",
        description: "Create a new Data Table asset with a specified row struct.",
        inputSchema: {
          type: "object",
          properties: {
            asset_path: { type: "string", description: "Folder path" },
            asset_name: { type: "string", description: "Data table name" },
            row_struct: { type: "string", description: "Row struct path or name (e.g. struct name or full path)" },
          },
          required: ["asset_path", "asset_name", "row_struct"],
        },
      },
      {
        name: "read_data_table",
        description: "Read a Data Table: row names, column definitions, and row data.",
        inputSchema: {
          type: "object",
          properties: {
            path: { type: "string", description: "Data table asset path" },
          },
          required: ["path"],
        },
      },
      {
        name: "add_data_table_row",
        description: "Add or update a row in a Data Table. Row data is imported as struct text format.",
        inputSchema: {
          type: "object",
          properties: {
            path: { type: "string", description: "Data table asset path" },
            row_name: { type: "string", description: "Row name/key" },
            row_data: { type: "string", description: "Row data as text matching struct format" },
          },
          required: ["path", "row_name", "row_data"],
        },
      },
      // Sprint 18 — Sequencer / Cinematics
      {
        name: "create_level_sequence",
        description: "Create a new Level Sequence (cinematic) asset with optional playback range.",
        inputSchema: {
          type: "object",
          properties: {
            asset_path: { type: "string", description: "Folder path (e.g. '/Game/Cinematics')" },
            asset_name: { type: "string", description: "Sequence name" },
            start_time: { type: "number", description: "Start time in seconds (default 0)" },
            end_time: { type: "number", description: "End time in seconds (default 5)" },
          },
          required: ["asset_path", "asset_name"],
        },
      },
      {
        name: "read_level_sequence",
        description: "Read a Level Sequence structure: playback range, master tracks, object bindings with their tracks.",
        inputSchema: {
          type: "object",
          properties: {
            path: { type: "string", description: "Level Sequence asset path" },
          },
          required: ["path"],
        },
      },
      {
        name: "add_sequence_track",
        description: "Add a track to a Level Sequence. Types: Transform, SkeletalAnimation, Audio, Event, Fade, CameraCut, Float, Bool. Can be master track or bound to an object via binding_guid.",
        inputSchema: {
          type: "object",
          properties: {
            sequence_path: { type: "string", description: "Level Sequence asset path" },
            track_type: { type: "string", description: "Track type (Transform, SkeletalAnimation, Audio, Event, Fade, CameraCut, Float, Bool)" },
            binding_guid: { type: "string", description: "Optional: bind track to object (GUID from read_level_sequence)" },
          },
          required: ["sequence_path", "track_type"],
        },
      },
      {
        name: "set_sequence_playback",
        description: "Set the playback range of a Level Sequence in seconds.",
        inputSchema: {
          type: "object",
          properties: {
            sequence_path: { type: "string", description: "Level Sequence asset path" },
            start_time: { type: "number", description: "Start time in seconds" },
            end_time: { type: "number", description: "End time in seconds" },
          },
          required: ["sequence_path", "start_time", "end_time"],
        },
      },
      // Sprint 17 — Material Graph Authoring
      {
        name: "add_material_expression",
        description: "Add a material expression node to a material graph. Common types: Multiply, Add, Lerp, Constant, VectorParameter, ScalarParameter, TextureSample, TextureCoordinate, Clamp, Power, Fresnel, ConstantBiasScale.",
        inputSchema: {
          type: "object",
          properties: {
            material_path: { type: "string", description: "Material asset path" },
            expression_class: { type: "string", description: "Expression type (e.g. 'Multiply', 'TextureSample')" },
            x: { type: "number", description: "Node X position (default 0)" },
            y: { type: "number", description: "Node Y position (default 0)" },
            description: { type: "string", description: "Optional description label for the node" },
          },
          required: ["material_path", "expression_class"],
        },
      },
      {
        name: "connect_material_expressions",
        description: "Wire two material expression nodes together. Find expressions by name, desc, or index from list_material_expressions.",
        inputSchema: {
          type: "object",
          properties: {
            material_path: { type: "string", description: "Material asset path" },
            from_expression: { type: "string", description: "Source expression (name, desc, or index)" },
            from_output: { type: "string", description: "Output pin name (empty string for default)" },
            to_expression: { type: "string", description: "Target expression (name, desc, or index)" },
            to_input: { type: "string", description: "Input pin name (e.g. 'A', 'B', 'BaseColor')" },
          },
          required: ["material_path", "from_expression", "from_output", "to_expression", "to_input"],
        },
      },
      {
        name: "delete_material_expression",
        description: "Remove a material expression node from a material graph.",
        inputSchema: {
          type: "object",
          properties: {
            material_path: { type: "string", description: "Material asset path" },
            expression: { type: "string", description: "Expression to delete (name, desc, or index)" },
          },
          required: ["material_path", "expression"],
        },
      },
      {
        name: "recompile_material",
        description: "Recompile a material after graph changes and save it.",
        inputSchema: {
          type: "object",
          properties: {
            material_path: { type: "string", description: "Material asset path" },
          },
          required: ["material_path"],
        },
      },
      {
        name: "list_material_expressions",
        description: "List all expression nodes in a material graph with their index, class, position, desc, and output pin names.",
        inputSchema: {
          type: "object",
          properties: {
            material_path: { type: "string", description: "Material asset path" },
          },
          required: ["material_path"],
        },
      },
      // Sprint 16 — PIE + Editor Workflow
      {
        name: "play_in_editor",
        description: "Control Play-In-Editor sessions. Operations: start (begin PIE), stop (end PIE), status (check if playing).",
        inputSchema: {
          type: "object",
          properties: {
            operation: { type: "string", description: "start, stop, or status (default: status)" },
          },
        },
      },
      {
        name: "execute_console_command",
        description: "Execute a UE console command in the editor. Common: stat fps, stat unit, ShowFlag.Collision 1, obj list, r.SetRes 1920x1080. Output not captured — use read_log with keyword filter to see results.",
        inputSchema: {
          type: "object",
          properties: {
            command: { type: "string", description: "Console command to execute" },
          },
          required: ["command"],
        },
      },
      {
        name: "read_log",
        description: "Read recent lines from the editor output log file. Supports keyword filtering.",
        inputSchema: {
          type: "object",
          properties: {
            line_count: { type: "number", description: "Number of recent lines to return (default 50, max 5000)" },
            keyword: { type: "string", description: "Filter lines containing this keyword (case insensitive)" },
          },
        },
      },
      {
        name: "get_engine_version",
        description: "Get the Unreal Engine version, branch, and build configuration.",
        inputSchema: {
          type: "object",
          properties: {},
        },
      },
      {
        name: "open_asset",
        description: "Open an asset in the Unreal Editor (Blueprint, Animation, Texture, etc.). The asset will open in its default editor window.",
        inputSchema: {
          type: "object",
          properties: {
            path: { type: "string", description: "Asset path (e.g. '/Game/Blueprints/SandboxCharacter_CMC')" },
          },
          required: ["path"],
        },
      },
      {
        name: "save_all_and_close",
        description: "Save all modified assets in the project, then cleanly close the Unreal Editor. The connection will drop once the editor exits - relaunch the editor process externally to reconnect.",
        inputSchema: {
          type: "object",
          properties: {},
        },
      },
      {
        name: "create_anim_blueprint",
        description: "Create an Animation Blueprint for a skeleton and save it to the project.",
        inputSchema: {
          type: "object",
          properties: {
            asset_path: { type: "string", description: "Destination content folder, for example '/Game/Blueprints/Animations'" },
            asset_name: { type: "string", description: "Name for the new Animation Blueprint" },
            skeleton_path: { type: "string", description: "Asset path of the target USkeleton" },
          },
          required: ["asset_path", "asset_name", "skeleton_path"],
        },
      },
      {
        name: "setup_fp_spine_pitch_abp",
        description: "Replace or create a post-process Animation Blueprint containing linked-input, spine, and head Modify Bone nodes. Runtime code can update the generated FAnimNode_ModifyBone rotations directly.",
        inputSchema: {
          type: "object",
          properties: {
            asset_path: { type: "string", description: "Destination content folder" },
            asset_name: { type: "string", description: "Animation Blueprint name; an existing asset with this path is replaced" },
            skeleton_path: { type: "string", description: "Asset path of the target USkeleton" },
            bone_name: { type: "string", description: "Spine bone to modify (default 'spine_01')" },
          },
          required: ["asset_path", "asset_name", "skeleton_path"],
        },
      },
      {
        name: "add_socket",
        description: "Add a new socket to a Skeleton asset, parented to a bone. Sockets are needed for anything that must query a runtime-accurate attach point (muzzle location, grip points, etc.) via GetSocketLocation/GetSocketRotation - USkeleton::Sockets is not readable/writable from Python, so this is the only way to author one outside the Skeleton Editor UI.",
        inputSchema: {
          type: "object",
          properties: {
            skeleton_path: { type: "string", description: "Asset path of the target USkeleton" },
            bone_name: { type: "string", description: "Bone to parent the socket to" },
            socket_name: { type: "string", description: "Name for the new socket; must not already exist" },
            x: { type: "number", description: "Relative location X (default 0)" },
            y: { type: "number", description: "Relative location Y (default 0)" },
            z: { type: "number", description: "Relative location Z (default 0)" },
            pitch: { type: "number", description: "Relative rotation pitch (default 0)" },
            yaw: { type: "number", description: "Relative rotation yaw (default 0)" },
            roll: { type: "number", description: "Relative rotation roll (default 0)" },
          },
          required: ["skeleton_path", "bone_name", "socket_name"],
        },
      },
      {
        name: "set_socket_transform",
        description: "Update the relative location and/or rotation of an existing Skeleton socket. Only supplied fields are changed.",
        inputSchema: {
          type: "object",
          properties: {
            skeleton_path: { type: "string", description: "Asset path of the target USkeleton" },
            socket_name: { type: "string", description: "Name of the existing socket to edit" },
            x: { type: "number" },
            y: { type: "number" },
            z: { type: "number" },
            pitch: { type: "number" },
            yaw: { type: "number" },
            roll: { type: "number" },
          },
          required: ["skeleton_path", "socket_name"],
        },
      },
      {
        name: "list_sockets",
        description: "List all sockets defined on a Skeleton asset, with their bone, relative location, and rotation.",
        inputSchema: {
          type: "object",
          properties: {
            skeleton_path: { type: "string", description: "Asset path of the target USkeleton" },
          },
          required: ["skeleton_path"],
        },
      },
      {
        name: "retarget_anim_asset",
        description: "Retarget an animation asset (AnimSequence, AnimMontage, etc.) from its current Skeleton onto a different one by bone name, using the same EditorAnimUtils::RetargetAnimations() the classic 'Retarget Skeleton' Content Browser right-click action calls internally - not exposed to Blueprint/Python at all otherwise. Creates a new duplicate asset targeting the new skeleton; does not modify the source. Only works well when the two skeletons share compatible bone names/hierarchy (this remaps which Skeleton asset the data is interpreted against, it does not run IK Rig retargeting math) - for skeletons with meaningfully different proportions or bone naming, a proper IK Rig/IK Retargeter setup is needed instead.",
        inputSchema: {
          type: "object",
          properties: {
            source_asset_path: { type: "string", description: "Asset path of the animation to retarget (AnimSequence/AnimMontage/etc.)" },
            target_skeleton_path: { type: "string", description: "Asset path of the destination USkeleton" },
            dest_folder: { type: "string", description: "Content folder for the new retargeted duplicate, e.g. '/Game/ALSHost/Animations'" },
            name_prefix: { type: "string", description: "Optional prefix for the duplicate's name" },
            name_suffix: { type: "string", description: "Optional suffix for the duplicate's name" },
            convert_space: { type: "boolean", description: "Convert animation data in component space to match the new skeleton's proportions (default true)" },
          },
          required: ["source_asset_path", "target_skeleton_path", "dest_folder"],
        },
      },
      {
        name: "compare_anim_bone_pose",
        description: "Compare a bone's component-space (world-relative-to-actor-root) transform between two animations at two given times, using UAnimPoseExtensions::GetAnimPoseAtTime/GetBonePose - not otherwise exposed to Blueprint/Python. Returns both poses plus the delta (B expressed relative to A). Useful for diagnosing a weapon/hand mismatch: if a known-good animation (e.g. an aim-pose sweep) and a problem animation (e.g. a retargeted reload) differ by a roughly constant delta at the grip bone (commonly 'hand_r'), that delta is a good starting point for a corrective offset; if the delta varies a lot between frames, the mismatch isn't a simple constant offset.",
        inputSchema: {
          type: "object",
          properties: {
            anim_path_a: { type: "string", description: "Asset path of the first (e.g. known-good reference) animation" },
            time_a: { type: "number", description: "Time in seconds into anim_path_a to sample" },
            anim_path_b: { type: "string", description: "Asset path of the second (e.g. problem) animation" },
            time_b: { type: "number", description: "Time in seconds into anim_path_b to sample" },
            bone_name: { type: "string", description: "Bone to compare, e.g. 'hand_r' (default: hand_r)" },
          },
          required: ["anim_path_a", "time_a", "anim_path_b", "time_b"],
        },
      },
      {
        name: "set_anim_curve_keys",
        description: "Author a float curve (constant or keyed) directly onto an AnimSequence/AnimMontage, via UAnimationBlueprintLibrary::AddCurve/AddFloatCurveKeys - not otherwise exposed to Blueprint node-authoring through this MCP. Useful for baking ALS-specific gating curves (e.g. Enable_HandIK_L, Layering_Arm_L - see ALS_AnimBP's HandIK graph) onto an animation retargeted from a project that didn't use ALS's curve conventions. The curve name must already exist in the target Skeleton's curve metadata (check via run_python inspect_asset on the Skeleton) for the AnimGraph's GetCurveValue lookups to see it. For a constant value across the whole clip, pass two keys: times=[0, clip_length], values=[v, v].",
        inputSchema: {
          type: "object",
          properties: {
            anim_path: { type: "string", description: "Asset path of the AnimSequence/AnimMontage to add the curve to" },
            curve_name: { type: "string", description: "Curve name, must already exist in the target Skeleton's curve metadata (e.g. 'Enable_HandIK_L')" },
            times: { type: "array", items: { type: "number" }, description: "Key times in seconds, same length as values" },
            values: { type: "array", items: { type: "number" }, description: "Key values, same length as times" },
          },
          required: ["anim_path", "curve_name", "times", "values"],
        },
      },
      {
        name: "set_anim_notify_property",
        description: "Sets a property on every AnimNotify/AnimNotifyState instance of a given class found within AnimSequence/AnimMontage assets under path_filter, then saves each modified asset. The raw Notifies array on an AnimSequence is blocked from Python's generic reflection ('protected and cannot be read'), so this is the only way to bulk-edit per-anim notify instance properties (e.g. wiring UALSAnimNotifyFootstep::HitDataTable across every locomotion animation that already has footstep notifies placed) without hand-editing each animation in the editor.",
        inputSchema: {
          type: "object",
          properties: {
            path_filter: { type: "string", description: "Folder to scan for AnimSequence/AnimMontage assets, e.g. '/ALSV4_CPP'" },
            notify_class_name: { type: "string", description: "Notify class name to match (UAnimNotify or UAnimNotifyState subclass), e.g. 'ALSAnimNotifyFootstep'" },
            property_name: { type: "string", description: "Property name to set on each matching notify instance" },
            property_value: { type: "string", description: "Value to set (as text; for object references, use the full asset path)" },
            recursive: { type: "boolean", description: "Search subfolders too (default true)" },
          },
          required: ["path_filter", "notify_class_name", "property_name", "property_value"],
        },
      },
      {
        name: "read_state_machine",
        description: "Reads an AnimGraph state machine's states and transitions (with each transition's rule graph nodes/pins). No other tool can see past the containing AnimGraphNode_StateMachine node - read_function_graphs/read_event_graph_detailed only walk Blueprint->FunctionGraphs/UbergraphPages directly, and a state machine's states/transitions live in a nested UAnimationStateMachineGraph reachable only via that node's own EditorStateMachineGraph property. Use this before ever editing a state machine (e.g. before duplicate_state_transition).",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: { type: "string", description: "Full path to the Animation Blueprint asset" },
            graph_name: { type: "string", description: "Name of the function graph containing the state machine node, e.g. an Anim Layer function name like 'OverlayLayer'" },
            state_machine_node_name: { type: "string", description: "Filter matching the state machine node's title or its bound graph's name (e.g. 'Overlay States'). Omit to return the first state machine node found in the graph." },
          },
          required: ["blueprint_path", "graph_name"],
        },
      },
      {
        name: "duplicate_state_transition",
        description: "Clones an existing state-machine transition (including its BoundGraph rule graph) so a new target state can be entered from the same origin state an existing transition already uses, without hand-authoring a brand-new transition edge from scratch. Common use: giving a newly-added enum value (e.g. a new EALSOverlayState entry) an entry transition into an existing state that already has working rendering/pose logic (e.g. reusing Torch's pose for a new Sword value) by cloning whatever transition currently enters that state. The cloned rule graph still contains the SOURCE transition's original enum-literal comparison - follow up with set_pin_default on the new rule graph's comparison node (see read_state_machine's transitions[].rule.nodes for the guid/pin to target) to point it at the new enum value. Read the state machine first via read_state_machine to find the source_transition_guid and confirm the target state's exact name.",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: { type: "string", description: "Full path to the Animation Blueprint asset" },
            graph_name: { type: "string", description: "Name of the function graph containing the state machine node" },
            state_machine_node_name: { type: "string", description: "Filter matching the state machine node's title or bound graph name, same as read_state_machine" },
            source_transition_guid: { type: "string", description: "node_guid of the transition to clone, from read_state_machine's transitions[] array. Its origin (from_state) is kept as-is on the clone." },
            new_target_state_name: { type: "string", description: "Name of an existing state (from read_state_machine's states[].name) to point the cloned transition's target at" },
          },
          required: ["blueprint_path", "graph_name", "source_transition_guid", "new_target_state_name"],
        },
      },
      {
        name: "add_enum_or_condition_to_transition_rule",
        description: "Widens an existing state-machine transition rule from 'field X' to 'field X OR field Y', where X and Y are two bool output pins on the same K2Node_BreakStruct node inside the rule's BoundGraph - ALS's own pattern for gating a transition on one FALSOverlayState field (a 'Break ALSOverlay State' node exposes one bool output pin per struct field, including newly-added ones, left unlinked until used). Rewires whatever the existing field's pin was plugged into (a Result node, a NOT node, anything) to instead come from a new BooleanOR node combining both fields. Use read_state_machine first to find the transition_guid and the exact pin names (e.g. 'Torch_' already wired, 'Sword_' unwired) from its rule.nodes.",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: { type: "string", description: "Full path to the Animation Blueprint asset" },
            graph_name: { type: "string", description: "Name of the function graph containing the state machine node" },
            state_machine_node_name: { type: "string", description: "Filter matching the state machine node's title or bound graph name, same as read_state_machine" },
            transition_guid: { type: "string", description: "node_guid of the transition whose rule to widen, from read_state_machine's transitions[] array" },
            existing_bool_pin_name: { type: "string", description: "The BreakStruct output pin currently driving this rule, e.g. 'Torch_'" },
            new_bool_pin_name: { type: "string", description: "The BreakStruct output pin to OR in, e.g. 'Sword_'" },
          },
          required: ["blueprint_path", "graph_name", "transition_guid", "existing_bool_pin_name", "new_bool_pin_name"],
        },
      },
      {
        name: "add_static_mesh_socket",
        description: "Adds a mesh-level socket (UStaticMeshSocket) to a StaticMesh asset, with an explicit local location/rotation. Used for the held-weapon 'Grip' socket convention: AttachToHand auto-aligns this socket to the hand bone when present, so a weapon mesh needs no per-weapon hardcoded offset tuning in Blueprint. Fails if a socket with that name already exists (delete it in the editor first, or pick a new name).",
        inputSchema: {
          type: "object",
          properties: {
            mesh_path: { type: "string", description: "Full path to the StaticMesh asset, e.g. '/Game/ALSHost/Weapons/Melee/SM_Sword'" },
            socket_name: { type: "string", description: "Name for the new socket, e.g. 'Grip'" },
            x: { type: "number", description: "Local location X (default 0)" },
            y: { type: "number", description: "Local location Y (default 0)" },
            z: { type: "number", description: "Local location Z (default 0)" },
            pitch: { type: "number", description: "Local rotation pitch in degrees (default 0)" },
            yaw: { type: "number", description: "Local rotation yaw in degrees (default 0)" },
            roll: { type: "number", description: "Local rotation roll in degrees (default 0)" },
          },
          required: ["mesh_path", "socket_name"],
        },
      },
      {
        name: "list_static_mesh_sockets",
        description: "Lists all mesh-level sockets (UStaticMeshSocket) on a StaticMesh asset, with their local location/rotation/scale.",
        inputSchema: {
          type: "object",
          properties: {
            mesh_path: { type: "string", description: "Full path to the StaticMesh asset" },
          },
          required: ["mesh_path"],
        },
      },
      {
        name: "set_static_mesh_socket_transform",
        description: "Updates the local location/rotation of an existing mesh-level socket (UStaticMeshSocket) on a StaticMesh asset. Any axis omitted keeps its current value. Use this to re-tune a weapon's 'Grip' socket in place without deleting and recreating it.",
        inputSchema: {
          type: "object",
          properties: {
            mesh_path: { type: "string", description: "Full path to the StaticMesh asset" },
            socket_name: { type: "string", description: "Name of the existing socket to update, e.g. 'Grip'" },
            x: { type: "number", description: "Local location X" },
            y: { type: "number", description: "Local location Y" },
            z: { type: "number", description: "Local location Z" },
            pitch: { type: "number", description: "Local rotation pitch in degrees" },
            yaw: { type: "number", description: "Local rotation yaw in degrees" },
            roll: { type: "number", description: "Local rotation roll in degrees" },
          },
          required: ["mesh_path", "socket_name"],
        },
      },
      {
        name: "import_texture",
        description: "Import an image file (jpg/png/tga/etc.) from disk as a Texture2D, via UTextureFactory's legacy synchronous FactoryCreateBinary API - deliberately bypasses AssetTools.import_asset_tasks/Interchange, which crashes the editor when called from this MCP's own command-handling context (a TaskGraph recursion assertion - see AGENTS.md). Use this for any scripted texture import instead of the Python AssetTools path.",
        inputSchema: {
          type: "object",
          properties: {
            image_path: { type: "string", description: "Absolute path to the image file on disk" },
            destination_path: { type: "string", description: "Destination content folder, e.g. '/Game/ALSHost/Props/MedKit'" },
            asset_name: { type: "string", description: "Optional asset name override (default: the file's base name)" },
          },
          required: ["image_path", "destination_path"],
        },
      },
      {
        name: "move_actor",
        description: "Set any supplied transform fields on an existing level actor while preserving omitted fields. Wrapped in an undo transaction.",
        inputSchema: {
          type: "object",
          properties: {
            actor_name: { type: "string", description: "Actor object name or editor label" },
            x: { type: "number" },
            y: { type: "number" },
            z: { type: "number" },
            pitch: { type: "number" },
            yaw: { type: "number" },
            roll: { type: "number" },
            scale_x: { type: "number" },
            scale_y: { type: "number" },
            scale_z: { type: "number" },
          },
          required: ["actor_name"],
        },
      },
      {
        name: "set_viewport_camera",
        description: "Get or set the editor viewport camera position and rotation.",
        inputSchema: {
          type: "object",
          properties: {
            operation: { type: "string", description: "get or set (default: get)" },
            x: { type: "number", description: "X location" },
            y: { type: "number", description: "Y location" },
            z: { type: "number", description: "Z location" },
            pitch: { type: "number", description: "Pitch rotation" },
            yaw: { type: "number", description: "Yaw rotation" },
            roll: { type: "number", description: "Roll rotation" },
          },
        },
      },
      // Sprint 14 — Behavior Tree
      {
        name: "create_behavior_tree",
        description: "Create a new Behavior Tree asset. Optionally link to a Blackboard.",
        inputSchema: {
          type: "object",
          properties: {
            asset_path: { type: "string", description: "Folder path (e.g. '/Game/AI')" },
            asset_name: { type: "string", description: "BT name" },
            blackboard_path: { type: "string", description: "Optional: blackboard asset path to link" },
          },
          required: ["asset_path", "asset_name"],
        },
      },
      {
        name: "create_blackboard",
        description: "Create a new Blackboard data asset for use with Behavior Trees.",
        inputSchema: {
          type: "object",
          properties: {
            asset_path: { type: "string", description: "Folder path" },
            asset_name: { type: "string", description: "Blackboard name" },
          },
          required: ["asset_path", "asset_name"],
        },
      },
      {
        name: "add_blackboard_key",
        description: "Add a key to a Blackboard asset. Types: Bool, Float, Int, String, Name, Object, Class, Enum, Vector, Rotator.",
        inputSchema: {
          type: "object",
          properties: {
            blackboard_path: { type: "string", description: "Blackboard asset path" },
            key_name: { type: "string", description: "Key name" },
            key_type: { type: "string", description: "Key type (Bool, Float, Int, String, Name, Object, Class, Enum, Vector, Rotator)" },
            instance_synced: { type: "boolean", description: "Whether key is instance-synced (default false)" },
          },
          required: ["blackboard_path", "key_name", "key_type"],
        },
      },
      {
        name: "read_behavior_tree",
        description: "Read a Behavior Tree's full node hierarchy including composites, tasks, decorators, services, and linked blackboard keys.",
        inputSchema: {
          type: "object",
          properties: {
            path: { type: "string", description: "Behavior Tree asset path" },
          },
          required: ["path"],
        },
      },
      // Sprint 15 — Quality of Life
      {
        name: "search_assets",
        description: "Search for assets by name pattern. Supports class and path filtering.",
        inputSchema: {
          type: "object",
          properties: {
            query: { type: "string", description: "Name pattern to search (* for all)" },
            class_filter: { type: "string", description: "Optional class filter (e.g. '/Script/Engine.StaticMesh')" },
            path_filter: { type: "string", description: "Path prefix (default '/Game')" },
            max_results: { type: "number", description: "Max results (default 50)" },
          },
          required: ["query"],
        },
      },
      {
        name: "rename_asset",
        description: "Rename or move an asset to a new path.",
        inputSchema: {
          type: "object",
          properties: {
            source_path: { type: "string", description: "Current asset path" },
            dest_path: { type: "string", description: "New asset path (full path including name)" },
          },
          required: ["source_path", "dest_path"],
        },
      },
      {
        name: "delete_asset",
        description: "Delete an asset. By default checks for references first and refuses if referenced.",
        inputSchema: {
          type: "object",
          properties: {
            path: { type: "string", description: "Asset path to delete" },
            check_references: { type: "boolean", description: "Check for references before deleting (default true)" },
          },
          required: ["path"],
        },
      },
      // Sprint 13 — Widget Blueprint / UMG
      {
        name: "create_widget_blueprint",
        description: "Create a new Widget Blueprint (UMG) with a default CanvasPanel root.",
        inputSchema: {
          type: "object",
          properties: {
            asset_path: { type: "string", description: "Folder path (e.g. '/Game/UI')" },
            asset_name: { type: "string", description: "Widget blueprint name" },
          },
          required: ["asset_path", "asset_name"],
        },
      },
      {
        name: "add_widget",
        description: "Add a widget to a Widget Blueprint. Types: TextBlock, Button, Image, VerticalBox, HorizontalBox, Overlay, CanvasPanel, Border, Slider, CheckBox, EditableTextBox, ProgressBar, ScrollBox, SizeBox, Spacer, GridPanel.",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: { type: "string", description: "Widget blueprint path" },
            widget_type: { type: "string", description: "Widget type name" },
            widget_name: { type: "string", description: "Widget name (optional, auto-generated if omitted)" },
            parent_name: { type: "string", description: "Parent panel name (default: root)" },
            x: { type: "number", description: "X position (CanvasPanel only)" },
            y: { type: "number", description: "Y position (CanvasPanel only)" },
            width: { type: "number", description: "Width (CanvasPanel only, default 200)" },
            height: { type: "number", description: "Height (CanvasPanel only, default 50)" },
          },
          required: ["blueprint_path", "widget_type"],
        },
      },
      {
        name: "remove_widget",
        description: "Remove a widget (and its children) from a Widget Blueprint's tree, via UWidgetTree::RemoveWidget - the counterpart to add_widget, which has no built-in way to undo/restructure what it added. Cannot remove the root widget.",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: { type: "string", description: "Widget blueprint path" },
            widget_name: { type: "string", description: "Name of the widget to remove" },
          },
          required: ["blueprint_path", "widget_name"],
        },
      },
      {
        name: "set_widget_property",
        description: "Set a property on a widget in a Widget Blueprint. Common: Text (on TextBlock), Visibility, ColorAndOpacity. Uses FProperty import for generic properties.",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: { type: "string", description: "Widget blueprint path" },
            widget_name: { type: "string", description: "Widget name" },
            property: { type: "string", description: "Property name (e.g. 'Text', 'Visibility')" },
            value: { type: "string", description: "Value as text" },
          },
          required: ["blueprint_path", "widget_name", "property", "value"],
        },
      },
      {
        name: "read_widget_tree",
        description: "Read the widget hierarchy of a Widget Blueprint. Returns tree structure with names, classes, visibility, and children.",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: { type: "string", description: "Widget blueprint path" },
          },
          required: ["blueprint_path"],
        },
      },
      // Sprint 12 — Material System
      {
        name: "create_material",
        description: "Create a new empty material asset.",
        inputSchema: {
          type: "object",
          properties: {
            asset_path: { type: "string", description: "Folder path (e.g. '/Game/Materials')" },
            asset_name: { type: "string", description: "Material name" },
          },
          required: ["asset_path", "asset_name"],
        },
      },
      {
        name: "create_material_instance",
        description: "Create a material instance from a parent material.",
        inputSchema: {
          type: "object",
          properties: {
            parent_path: { type: "string", description: "Parent material path" },
            asset_path: { type: "string", description: "Folder path for new instance" },
            asset_name: { type: "string", description: "Instance name" },
          },
          required: ["parent_path", "asset_path", "asset_name"],
        },
      },
      {
        name: "set_material_parameter",
        description: "Set a parameter on a material instance (scalar, vector, or texture).",
        inputSchema: {
          type: "object",
          properties: {
            material_path: { type: "string", description: "Material instance path" },
            param_name: { type: "string", description: "Parameter name" },
            param_type: { type: "string", description: "scalar, vector, or texture" },
            value: { type: "number", description: "For scalar params" },
            r: { type: "number", description: "Red (0-1) for vector params" },
            g: { type: "number", description: "Green (0-1) for vector params" },
            b: { type: "number", description: "Blue (0-1) for vector params" },
            a: { type: "number", description: "Alpha (0-1) for vector params (default 1)" },
            texture_path: { type: "string", description: "Texture path for texture params" },
          },
          required: ["material_path", "param_name", "param_type"],
        },
      },
      {
        name: "list_material_parameters",
        description: "List all parameters (scalar, vector, texture) on a material or material instance with current values.",
        inputSchema: {
          type: "object",
          properties: {
            material_path: { type: "string", description: "Material or material instance path" },
          },
          required: ["material_path"],
        },
      },
      {
        name: "assign_material_to_actor",
        description: "Assign a material to an actor's mesh component at a given slot index.",
        inputSchema: {
          type: "object",
          properties: {
            actor_name: { type: "string", description: "Actor name or label" },
            material_path: { type: "string", description: "Material or material instance path" },
            slot_index: { type: "number", description: "Material slot index (default 0)" },
          },
          required: ["actor_name", "material_path"],
        },
      },
      // Sprint 11 — Actor & Level Management
      {
        name: "spawn_actor",
        description: "Spawn an actor into the current level. Supports both native classes and Blueprint classes. Wrapped in undo transaction.",
        inputSchema: {
          type: "object",
          properties: {
            class_path: { type: "string", description: "Class path (e.g. '/Script/Engine.StaticMeshActor', '/Game/Blueprints/MyBP')" },
            x: { type: "number", description: "X location (default 0)" },
            y: { type: "number", description: "Y location (default 0)" },
            z: { type: "number", description: "Z location (default 0)" },
            pitch: { type: "number", description: "Pitch rotation (default 0)" },
            yaw: { type: "number", description: "Yaw rotation (default 0)" },
            roll: { type: "number", description: "Roll rotation (default 0)" },
            label: { type: "string", description: "Optional actor label" },
          },
          required: ["class_path"],
        },
      },
      {
        name: "destroy_actor",
        description: "Remove an actor from the current level by name or label. Wrapped in undo transaction.",
        inputSchema: {
          type: "object",
          properties: {
            actor_name: { type: "string", description: "Actor name or label" },
          },
          required: ["actor_name"],
        },
      },
      {
        name: "duplicate_actor",
        description: "Duplicate an existing actor in the level with an optional offset. Wrapped in undo transaction.",
        inputSchema: {
          type: "object",
          properties: {
            actor_name: { type: "string", description: "Actor name or label to duplicate" },
            offset_x: { type: "number", description: "X offset from original (default 0)" },
            offset_y: { type: "number", description: "Y offset from original (default 0)" },
            offset_z: { type: "number", description: "Z offset from original (default 0)" },
          },
          required: ["actor_name"],
        },
      },
      {
        name: "get_current_level",
        description: "Get information about the currently loaded level (name, path, actor count).",
        inputSchema: {
          type: "object",
          properties: {},
        },
      },
      {
        name: "load_level",
        description: "Open a level in the editor. Saves dirty packages first.",
        inputSchema: {
          type: "object",
          properties: {
            level_path: { type: "string", description: "Level asset path (e.g. '/Game/Maps/MyLevel')" },
          },
          required: ["level_path"],
        },
      },
      {
        name: "list_levels",
        description: "List the persistent level and all streaming sublevels in the current world.",
        inputSchema: {
          type: "object",
          properties: {},
        },
      },
];
