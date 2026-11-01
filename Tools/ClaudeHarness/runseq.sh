#!/bin/sh
# usage: runseq.sh tag "cmd|ms" ...  -> prints P9 log lines produced
cd "$(dirname "$0")"; T="$1"; shift
python -I mkseq.py "$T.json" "$@"
LOG="/c/Users/niles/Documents/Unreal Projects/ThanePlatform9/Saved/Logs/ThanePlatform9.log"; n=$(wc -l < "$LOG")
node mcp_call.mjs "C:/Users/niles/Documents/Unreal Projects/ThanePlatform9/Plugins/ClaudeUnrealMCP/MCPServer/index.js" "$T" "$T.json" > "$T.out" 2>&1
tail -n +$((n+1)) "$LOG" | grep -E "LogP9|Error|Ensure" | grep -v "capsule r=" | sed 's/^\[[^]]*\]\[[^]]*\]//; s/LogP9: //; s/ vel=V([^)]*)//; s/ jumps=0 mantling=no//; s/ ledgeComp=present//'
