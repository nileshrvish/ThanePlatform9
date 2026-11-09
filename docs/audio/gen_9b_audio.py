# Stage 9B - main menu audio. 100 % procedural (numpy), reusing the 8A / 9A helpers read-only. New files only.
#   python gen_9b_audio.py <out_root>
# Music9B/BGM_Menu_9B.wav  : 96 s seamless stereo loop - low minor drone, a slow sparse piano figure, distant railway hum and
#                             rumble, far metallic pings, a soft slow breeze, one very distant EMU horn.
# UI9B/UI_Click_9B.wav, UI_Confirm_9B.wav, UI_Back_9B.wav : quiet relay-click / low-chime interface sounds.
import os, sys, json
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = sys.argv[1]
sys.path.insert(0, os.path.join(HERE, "..", "OriginalAudioPack8A"))
sys.path.insert(0, os.path.join(HERE, "..", "OriginalAudioPack9A"))
_argv = sys.argv
sys.argv = [_argv[0], OUT]
import gen_8a_audio as G   # noqa: E402
import gen_9a_audio as N   # noqa: E402  (only its functions are used)
sys.argv = _argv
SR, TAU = G.SR, G.TAU


def menu_music():
    L, XF = 96 * SR, 6 * SR
    n = L + XF
    out = np.zeros((n, 2))
    t = np.arange(n) / SR
    # low drone: D minor colour with a slow beating minor second above, very soft
    pad = G.pad_chord([G.note_hz("D2"), G.note_hz("A2"), G.note_hz("F3")], n / SR, 9701, attack=6.0, release=6.0, bright=900, detune=0.003)
    pad *= (0.8 + 0.2 * G.periodic_lfo(n, 2))[:, None]
    out += pad * 0.55
    rub = G.osc(G.note_hz("Eb3"), n) * 0.04 * (0.5 + 0.5 * G.periodic_lfo(n, 3, 1.0))
    out += G.pan(rub, 0.3)
    # distant railway: overhead-line hum + far rumble swells
    hum = sum(a * np.sin(TAU * f * t) for f, a in ((50, 0.05), (100, 0.04), (150, 0.015)))
    out += G.pan(hum, -0.2)
    for c in range(2):
        out[:, c] += G.bp(G.coloured(n, 9710 + c, "brown"), 25, 160, 2) * (0.25 + 0.15 * G.periodic_lfo(n, 3, c)) * 0.5
    # a slow, sparse piano figure (every ~12 s, notes far apart) - unresolved
    figure = [("D4", 0.0), ("F4", 1.8), ("E4", 3.9), ("A3", 7.2)]
    for k, start in enumerate(np.arange(4.0, 92.0, 12.0)):
        for i, (nm, off) in enumerate(figure):
            if (k + i) % 5 == 4:
                continue                                                    # leave gaps, never the same twice in a row
            note = G.piano_note(G.note_hz(nm) * (0.5 if (k % 3 == 2 and i == 3) else 1.0), 5.0, vel=0.32, seed=9720 + k * 7 + i, bright=0.6)
            G.mix_wrap(out, G.pan(note, -0.35 + 0.2 * i), start + off, 0.55)
    # distant metallic pings (rails / steel cooling in the night)
    r = G.rng(9730)
    for k in range(9):
        hit = G.metal_hit(4.0, 9731 + k, base=r.uniform(260, 700), t60=2.5)
        G.mix_wrap(out, G.pan(G.bp(hit, 200, 4000), r.uniform(-0.9, 0.9)), r.uniform(0, 96), 0.05)
    # one very distant horn
    horn = N.emu_horn(2.4, 9740, distant=True)
    G.mix_wrap(out, G.pan(G.bp(horn, 200, 1500), 0.6), 52.0, 0.10)
    # gentle slow breeze (the 9A wind, quieter)
    w = N.wind_slow(n - 4 * SR, 4 * SR, 9750)
    out[:len(w)] += w * 0.9
    out = G.reverb(out, 4.5, 0.35, seed=9760, stereo=True, bright=3500)
    return G.loopify(out, L, XF)


def ui_click(seed, f=2200.0):
    n = int(0.12 * SR)
    r = G.rng(seed)
    c = G.bp(r.standard_normal(n), 1200, 7000) * G.decay_env(n, 0.006)
    c += G.osc(f, n) * G.decay_env(n, 0.02) * 0.25 + G.osc(f * 0.5, n) * G.decay_env(n, 0.03) * 0.15
    return G.reverb(c, 0.4, 0.15, seed=seed + 1)


def ui_chime(seed, notes):
    n = int(1.2 * SR)
    out = np.zeros(n)
    for i, nm in enumerate(notes):
        b = G.bell(G.note_hz(nm), 1.2, seed + i, t60=1.2)
        out[int(i * 0.07 * SR):] += b[:n - int(i * 0.07 * SR)]
    return G.reverb(G.bp(out, 200, 6000), 1.2, 0.25, seed=seed + 5)


if __name__ == "__main__":
    G.save("Music9B/BGM_Menu_9B.wav", menu_music(), loop=True, target_rms_db=-26.0, notes="main menu music: drone, sparse piano, distant railway, wind (96 s loop)")
    G.save("UI9B/UI_Click_9B.wav", ui_click(9770), notes="menu button press: soft relay click")
    G.save("UI9B/UI_Confirm_9B.wav", ui_chime(9780, ("A4", "D5")), notes="menu confirm: low two-note chime")
    G.save("UI9B/UI_Back_9B.wav", ui_click(9790, 1500.0), notes="menu back: lower click")
    with open(os.path.join(OUT, "manifest_9b.json"), "w") as f:
        json.dump(G.MANIFEST, f, indent=1)
    print("P9AUD manifest", len(G.MANIFEST))
