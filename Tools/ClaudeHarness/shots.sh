#!/bin/sh
# usage: shots.sh "<name>|<x y z yaw>|<pitch yaw>|<flash 0/1>" ...   (PIE must be running)
cd "$(dirname "$0")"
IDX="C:/Users/niles/Documents/Unreal Projects/ThanePlatform9/Plugins/ClaudeUnrealMCP/MCPServer/index.js"
for spec in "$@"; do
  N="${spec%%|*}"; R="${spec#*|}"; TP="${R%%|*}"; R2="${R#*|}"; ROT="${R2%%|*}"; FL="${R2#*|}"; OUTP=$(cygpath -w "$PWD/$N.png")
  if [ "$FL" = "1" ]; then F=',["execute_console_command",{"command":"p9.Storm.Flash 0 15"}],["__sleep",{"ms":700}]'; else F=''; fi
  printf '[["__assert_pie",{"before":"%s"}],["execute_console_command",{"command":"p9.Test.Teleport %s"}],["__sleep",{"ms":1100}],["execute_console_command",{"command":"setrot %s"}],["__sleep",{"ms":2200}]%s,["execute_console_command",{"command":"setrot %s"}],["__sleep",{"ms":300}]]' "$N" "$TP" "$ROT" "$F" "$ROT" > "$N.json"
  node mcp_call.mjs "$IDX" "$N" "$N.json" >/dev/null 2>&1
  powershell -NoProfile -File capture_window.ps1 -OutFile "$OUTP" | sed 's/.*-> //'
done
