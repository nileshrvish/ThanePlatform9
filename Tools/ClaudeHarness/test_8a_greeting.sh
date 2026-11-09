#!/bin/sh
# Stage 8A: epilogue to the Greeting jumpscare in PIE (PIE must be running). Writes frames to e8g_*.png and prints the story log.
cd "$(dirname "$0")"
IDX="C:/Users/niles/Documents/Unreal Projects/ThanePlatform9/Plugins/ClaudeUnrealMCP/MCPServer/index.js"
LOG="/c/Users/niles/Documents/Unreal Projects/ThanePlatform9/Saved/Logs/ThanePlatform9.log"
n0=$(wc -l < "$LOG")
./runseq.sh e8g1 "@assert|100" "p9.Friends.ForceUnlock -1|300" "p9.Story.Epilogue|16500" > /dev/null
./runpy.sh pie_5d_stand.py e8gs 2000 > /dev/null
./runseq.sh e8g2 "@assert|100" "p9.Test.Teleport 1500 7000 100 270|500" > /dev/null
for i in $(seq 1 120); do tail -n +$((n0+1)) "$LOG" | grep -q "Vashi local arrived at Platform 9" && break; sleep 1; done
sleep 4
./runseq.sh e8g3 "@assert|100" "p9.Test.Teleport 700 6740 130 0|300" "setrot 0 0|300" > /dev/null
for i in $(seq 1 200); do tail -n +$((n0+1)) "$LOG" | grep -q "epilogue - riding the Vashi local" && break; sleep 0.2; done
python -I time_8a_burst.py "$(cygpath -w "$PWD")"
sleep 4
tail -n +$((n0+1)) "$LOG" | grep -E "Story:|Greeting|Campaign: COMPLETED|Vashi local departed" | sed 's/^\[[^]]*\]\[[^]]*\]//; s/LogP9: //'
