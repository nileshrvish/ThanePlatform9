import json, sys
# usage: mkseq.py out.json "cmd|ms" ...   ("@start" starts PIE, "@assert" asserts PIE)
seq = []
for a in sys.argv[2:]:
    c, _, ms = a.partition("|")
    if c == "@start": seq += [["execute_console_command", {"command": "p9.Test.EditorThrottle 0"}], ["play_in_editor", {"operation": "start"}], ["__sleep", {"ms": 9000}]]
    elif c == "@assert": seq.append(["__assert_pie", {"before": "x"}])
    elif c: seq.append(["execute_console_command", {"command": c}])
    if ms: seq.append(["__sleep", {"ms": int(ms)}])
json.dump(seq, open(sys.argv[1], "w"))
