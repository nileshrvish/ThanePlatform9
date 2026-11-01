#!/bin/sh
# usage: snap.sh name "pitch yaw"  - set the view rotation (no teleport) and capture the PIE window
cd "$(dirname "$0")"; OUTP=$(cygpath -w "$PWD/$1.png")
printf '[["__assert_pie",{"before":"%s"}],["execute_console_command",{"command":"setrot %s"}],["__sleep",{"ms":700}]]' "$1" "$2" > "$1.json"
node mcp_call.mjs "C:/Users/niles/Documents/Unreal Projects/ThanePlatform9/Plugins/ClaudeUnrealMCP/MCPServer/index.js" "$1" "$1.json" >/dev/null 2>&1
powershell -NoProfile -File capture_window.ps1 -OutFile "$OUTP" | sed 's/.*-> //'
