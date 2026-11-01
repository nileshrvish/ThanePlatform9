#!/bin/sh
# usage: runpy.sh <script.py> <tag>  — runs editor Python via MCP console and prints the new P9PY log lines
cd "$(dirname "$0")"
IDX="C:/Users/niles/Documents/Unreal Projects/ThanePlatform9/Plugins/ClaudeUnrealMCP/MCPServer/index.js"
LOG="/c/Users/niles/Documents/Unreal Projects/ThanePlatform9/Saved/Logs/ThanePlatform9.log"
n=$(wc -l < "$LOG")
PYDIR="$(cygpath -m "$PWD/ue_py")"   # scripts live in ./ue_py (Windows path for the editor)
printf '[["execute_console_command",{"command":"py \\"%s\\""}],["__sleep",{"ms":%s}]]' "$PYDIR/$1" "${3:-3000}" > "$2.json"
node mcp_call.mjs "$IDX" "$2" "$2.json" >/dev/null 2>&1
tail -n +$((n+1)) "$LOG" | grep -E "P9PY|Error|Traceback|line [0-9]" | sed 's/^\[[^]]*\]\[[^]]*\]//'
