# Stage 9B: caption table for the cinematic / dialogue sounds, timed from the actual audio files.
# For each sound: the script's sentences (original romanised wording + the script's English meaning where it has one) are mapped
# onto the speech in the WAV: speech regions are found from the short-time energy, then split at the N-1 longest silences
# (N = number of sentences). Descriptive captions ([Thunder rumbling] ...) have fixed times. Output: captions_9b.csv
# (Name,SoundName,Order,Start,End,Text) for the Unreal DataTable DT_P9_Captions (row struct FP9CaptionRow).
import csv, json, os, sys, wave
import numpy as np

A = "C:/Users/niles/Documents/Blender Projects/P9_Assets/"
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "captions_9b.csv")


def read_wav(p):
    with wave.open(p) as w:
        n, ch, sr = w.getnframes(), w.getnchannels(), w.getframerate()
        x = np.frombuffer(w.readframes(n), dtype="<i2").astype(np.float64) / 32768.0
    if ch > 1:
        x = x.reshape(-1, ch).mean(1)
    return x, sr


def speech_spans(x, sr, n_sent, min_gap=0.18):
    hop = int(0.01 * sr)
    fr = np.sqrt(np.convolve(x ** 2, np.ones(hop * 3) / (hop * 3), "same")[::hop] + 1e-12)
    db = 20 * np.log10(fr)
    thr = max(db.max() - 38.0, np.percentile(db, 20) + 10.0)
    on = db > thr
    # regions of sound
    regs, i = [], 0
    while i < len(on):
        if on[i]:
            j = i
            while j < len(on) and on[j]:
                j += 1
            regs.append([i * 0.01, j * 0.01])
            i = j
        else:
            i += 1
    # merge tiny gaps (inside words)
    m = []
    for r in regs:
        if m and r[0] - m[-1][1] < min_gap:
            m[-1][1] = r[1]
        else:
            m.append(r)
    m = [r for r in m if r[1] - r[0] > 0.06]
    if not m:
        return []
    if len(m) <= n_sent:
        return m + [m[-1]] * (n_sent - len(m))
    gaps = sorted(range(len(m) - 1), key=lambda k: m[k + 1][0] - m[k][1], reverse=True)[:n_sent - 1]
    cuts = sorted(gaps)
    spans, s = [], 0
    for c in cuts:
        spans.append([m[s][0], m[c][1]]); s = c + 1
    spans.append([m[s][0], m[-1][1]])
    return spans


rows = []


def add(sound, order, start, end, text):
    rows.append({"Name": "%s_%02d" % (sound, order), "SoundName": sound, "Order": order, "Start": round(start, 2), "End": round(end, 2), "Text": text})


ann = json.load(open(A + "Announcements/Scripts/announcements_mr.json", encoding="utf-8"))
# cinematic announcements + the chaiwala's epilogue lines (the gameplay PA variants stay uncaptioned: rule 8, no anomaly tell)
for it in ann["items"]:
    if it["id"] not in ("PA_PF1_ThaneArrival", "PA_PF9_Epilogue_NoAiroli", "VO_Chaiwala_Wake", "VO_Chaiwala_Call"):
        continue
    snd = "SW_P9_" + it["id"]
    x, sr = read_wav(A + "Announcements/Final/" + snd + ".wav")
    sents = it["sentences"]
    pa = it.get("pa", False)
    # a PA starts with its chime: caption it, then map the sentences onto the speech after it
    # an ellipsis inside a sentence is a spoken pause too: split there as well, then merge the parts back per sentence
    parts = [1 + s["roman"].strip().rstrip(".").count("...") for s in sents]
    raw = speech_spans(x, sr, sum(parts) + (1 if pa else 0))
    spans = raw[:1] if pa else []
    k0 = 1 if pa else 0
    for p in parts:
        seg = raw[k0:k0 + p]; k0 += p
        spans.append([seg[0][0], seg[-1][1]])
    if pa:
        add(snd, 0, spans[0][0], max(spans[0][1], spans[0][0] + 1.2), "[Station chime]")
        spans = spans[1:]
    for k, (s, (a, b)) in enumerate(zip(sents, spans)):
        who = "ANNOUNCER: " if pa else ("CHAIWALA: " if k == 0 else "")
        en = s.get("en", "")
        add(snd, k + 1, a, b + 0.35, "%s%s\n(%s)" % (who, s["roman"], en) if en else who + s["roman"])
    print(snd, "%.1f s" % (len(x) / sr), [(round(a, 2), round(b, 2)) for a, b in spans])

npc = json.load(open(A + "StationAudio/Scripts/npc_dialogue.json", encoding="utf-8"))
for s in npc["silhouette"]:
    snd = "SW_P9_NPC_" + s["id"]
    p = A + "StationAudio/Final/" + snd + ".wav"
    if not os.path.exists(p):
        print("missing", p); continue
    x, sr = read_wav(p)
    sp = speech_spans(x, sr, 1)
    add(snd, 1, sp[0][0], sp[0][1] + 0.4, "KANCHANA: " + s["roman"])
    print(snd, round(len(x) / sr, 2), sp)

# descriptive captions (non-verbal sounds that matter in the cinematic moments)
for snd, a, b, t in (("SFX_Greeting_Scream", 0.0, 2.0, "[A sudden scream]"),
                     ("SFX_LanternMan_Stinger", 0.0, 3.0, "[A deep, ominous drone]"),
                     ("SFX_Thunder9A_Close_01", 0.0, 3.0, "[Thunder cracks overhead]"),
                     ("SFX_Thunder9A_Close_02", 0.0, 3.0, "[Thunder cracks overhead]"),
                     ("SFX_Thunder9A_Strong_01", 0.3, 3.3, "[Thunder rumbling]"),
                     ("SFX_Thunder9A_Strong_02", 0.3, 3.3, "[Thunder rumbling]"),
                     ("SFX_Thunder9A_Strong_03", 0.3, 3.3, "[Thunder rumbling]"),
                     ("SW_P9_TrainRunLoop", 1.5, 5.0, "[The local train rattles through the night]"),
                     ("SFX_Train9A_Horn_Long", 0.0, 2.2, "[Train horn]"),
                     ("SFX_Train9A_Horn_Double", 0.0, 1.6, "[Train horn]"),
                     ("SFX_Train9A_Horn_Distant", 0.0, 2.4, "[Distant train horn]")):
    add(snd, 1, a, b, t)

with open(OUT, "w", encoding="utf-8", newline="") as f:
    w = csv.DictWriter(f, fieldnames=["Name", "SoundName", "Order", "Start", "End", "Text"])
    w.writeheader()
    for r in rows:
        w.writerow(r)
print("rows", len(rows), "->", OUT)
