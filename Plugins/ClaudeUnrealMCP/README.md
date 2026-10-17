# ClaudeUnrealMCP

> **WARNING: This project is in active development and is not production-ready.** APIs, commands, and behavior may change without notice. Use at your own risk.

An Unreal Engine 5 editor plugin that provides [Model Context Protocol (MCP)](https://modelcontextprotocol.io/) integration for AI coding assistants like [Claude Code](https://claude.ai/code). Gives Claude Code full read/write access to the UE5 editor via a TCP server.

## Features

**70+ commands** organized by category:

| Category | Commands | Description |
|----------|----------|-------------|
| **Blueprints** | `list_blueprints`, `read_blueprint`, `read_variables`, `read_class_defaults`, `read_event_graph`, `read_function_graphs`, `compile_blueprint` | Read/write blueprint structure, variables, graphs |
| **Components** | `read_components`, `read_component_properties`, `add_component`, `set_component_property`, `delete_component` | Manage blueprint components |
| **Nodes** | `connect_nodes`, `disconnect_pin`, `delete_node`, `reconstruct_node`, `set_pin_default` | Manipulate blueprint graph nodes |
| **Structs/Enums** | `read_user_defined_struct`, `read_user_defined_enum`, `modify_struct_field`, `migrate_struct_references`, `migrate_enum_references` | Struct and enum manipulation + migration |
| **Interfaces** | `read_interface`, `add_implemented_interface`, `remove_implemented_interface`, `migrate_interface_references` | Blueprint interface operations |
| **Actors** | `list_actors`, `find_actors_by_name`, `read_actor_properties`, `set_actor_properties`, `read_actor_components` | Level actor inspection and modification |
| **Assets** | `duplicate_asset`, `inspect_asset`, `set_property`, `save_asset`, `capture_screenshot` | Generic asset operations |
| **Input** | `read_input_mapping_context`, `add_input_mapping` | Enhanced Input system |
| **Migration** | `migrate_chooser_table`, `fix_property_access_paths`, `fix_struct_sub_pins`, `fix_enum_defaults` | BP-to-C++ migration tools |
| **Functions** | `create_blueprint_function`, `add_function_input`, `add_function_output`, `rename_blueprint_function` | Blueprint function creation |

## Installation

### 1. Clone into your project's Plugins folder

```bash
cd YourProject/Plugins
git clone https://github.com/AgustinJimenez/ClaudeUnrealMCP.git
```

### 2. Install Node.js dependencies

```bash
cd ClaudeUnrealMCP/MCPServer
npm install
```

### 3. Add to Claude Code MCP config

Add to `~/.claude.json` under `mcpServers`:

```json
"unreal-engine": {
  "type": "stdio",
  "command": "node",
  "args": ["<path-to-your-project>/Plugins/ClaudeUnrealMCP/MCPServer/index.js"],
  "env": {}
}
```

### 4. Open your project in Unreal Editor

The plugin starts a TCP server on port 9877 automatically when the editor loads.

## Architecture

```
Claude Code  <-->  Node.js MCP Bridge  <-->  TCP Server (port 9877)  <-->  UE5 Editor
              (MCPServer/index.js)         (C++ plugin, runs in-editor)
```

- **C++ TCP Server**: Runs inside UE5 editor, handles JSON commands, has full access to UEditor APIs
- **Node.js Bridge**: Translates MCP protocol to TCP commands, connects Claude Code to the editor
- **Tool Definitions**: `MCPServer/toolDefinitions.js` defines all available MCP tools

## Extending

To add a new command:

1. Add handler declaration in `Source/ClaudeUnrealMCP/Public/MCPServer.h`
2. Register in `CommandHandlers` map in `MCPServerCore.cpp`
3. Implement handler in a `MCPServer*.cpp` file
4. Add tool definition in `MCPServer/toolDefinitions.js`
5. Rebuild the plugin

## Requirements

- Unreal Engine 5.4+ (tested on 5.7)
- Node.js 18+
- Claude Code CLI

## License

MIT
