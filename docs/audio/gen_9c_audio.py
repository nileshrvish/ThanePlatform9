# Stage 9C - "Platform No. 9" player footsteps, rebuilt. 100 % procedural (numpy only, fixed seeds): no samples, no recordings,
# no AI services. Reuses the 8A helpers (gen_8a_audio.py, imported read-only). Run:  python -X utf8 gen_9c_audio.py <out_root>
# Writes 48 kHz 16-bit MONO WAVs (they are played in 3D at the stepping foot) into <out_root>/SFX/Player9C + manifest_9c.json.
# New file names only - nothing from 8A / 9A is overwritten (save() refuses existing files).
#
# Model of one step (rubber-soled shoe), per surface:
#   heel strike  : a raised-cosine contact-force pulse (rubber: 4-8 ms, run: shorter / harder) driving
#                  (a) the surface's damped modes (modal synthesis: slab / deck / plate / car-floor) and
#                  (b) the contact slap (noise shaped by the pulse's derivative, low-passed by the surface hardness)
#   roll         : heel -> toe sole friction (band-limited noise, stick-slip flutter), 45-110 ms walk / 20-45 ms run
#   ball / toe   : a second, weaker plant (forefoot), then a faint toe-off scuff
#   texture      : grit crackles (outdoor stone, stairs), girder ring (footbridge), plate ring (metal), panel rattle (train)
#   foley        : a faint trouser swish around the step
#   room         : a short room response per space (open canopy, covered bridge, tiled hall, stairwell, steel, carriage)
# Left / right feet differ slightly (sole wear / weight bias); every variation has its own weight, pulse, gap and texture.
import os, sys, json
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = sys.argv[1]
sys.path.insert(0, os.path.join(HERE, "..", "OriginalAudioPack8A"))
_argv = sys.argv
sys.argv = [_argv[0], OUT]
import gen_8a_audio as G          # noqa: E402  (helpers; OUT is set from argv at import)
sys.argv = _argv
SR, TAU = G.SR, G.TAU
rng, bp, decay_env, ola_convolve, reverb_ir, smooth_noise = G.rng, G.bp, G.decay_env, G.ola_convolve, G.reverb_ir, G.smooth_noise


def hp(x, f, order=2):
    return bp(x, f, None, order)


def lp(x, f, order=2):
    return bp(x, None, f, order)


def pulse(ms):
    """Raised-cosine contact force pulse (unit area)."""
    n = max(4, int(ms * 1e-3 * SR))
    p = 0.5 - 0.5 * np.cos(np.linspace(0, TAU, n))
    return p / p.sum()


def modes(n, freqs, t60s, amps, r):
    """Impulse response of a bank of damped modes (random phases)."""
    t = np.arange(n) / SR
    y = np.zeros(n)
    for f, t60, a in zip(freqs, t60s, amps):
        if f >= SR * 0.45:
            continue
        y += a * np.sin(TAU * f * t + r.uniform(0, TAU)) * np.exp(-6.91 * t / t60)
    return y


def drive(force, ir):
    return ola_convolve(force, ir)[:len(force)]


def place(dst, src, at):
    at = int(at)
    if at >= len(dst):
        return
    m = min(len(src), len(dst) - at)
    dst[at:at + m] += src[:m]


def slap(r, n_pulse_ms, hard_hz, amp):
    """Contact slap: noise burst whose envelope is the force pulse's rise (air squeezed out, sole impact)."""
    p = pulse(n_pulse_ms)
    env = np.abs(np.gradient(p)); env /= env.max() + 1e-12
    tail = decay_env(int(0.012 * SR), 0.006)
    env = np.concatenate([env, env[-1] * tail])
    x = r.standard_normal(len(env)) * env
    return lp(hp(x, 300.0), hard_hz) * amp


def tock(r, pms, f0, amp):
    """Shoe body: the heel block / sole resonates briefly (the tock that makes a step read as a shoe)."""
    n = int(0.05 * SR)
    p = pulse(pms)
    ir = modes(n, [f0, f0 * r.uniform(1.6, 1.9), f0 * r.uniform(2.6, 3.1)], [0.018, 0.012, 0.008], [1.0, 0.5, 0.25], r)
    f = np.zeros(n); f[:min(n, len(p))] = p[:n]
    return drive(f, ir) * amp


def friction(r, dur, lo, hi, amp, flutter=60.0):
    """Sole-on-floor friction: band noise with stick-slip flutter and a soft swell."""
    n = max(8, int(dur * SR))
    x = bp(r.standard_normal(n), lo, hi, 2)
    sw = np.sin(np.linspace(0, np.pi, n)) ** 1.5
    fl = 0.65 + 0.35 * np.sign(np.sin(TAU * np.cumsum(np.full(n, flutter * r.uniform(0.7, 1.4)) / SR) + r.uniform(0, TAU)))
    fl = np.convolve(fl, np.ones(24) / 24, mode="same")
    return x * sw * fl * amp


def grit(r, n, span, count, lo, hi, amp):
    y = np.zeros(n)
    for _ in range(count):
        j = int(r.uniform(0, span) * SR); k = int(r.uniform(0.0015, 0.004) * SR)
        if j + k < n:
            y[j:j + k] += bp(r.standard_normal(k), lo, hi, 2) * decay_env(k, 0.0012) * r.uniform(0.2, 1.0) * amp
    return y


def swish(r, n, at, dur, amp):
    m = int(dur * SR)
    x = bp(G.coloured(m, int(r.integers(1, 1 << 30)), "pink"), 900, 5200, 1) * np.sin(np.linspace(0, np.pi, m)) ** 2 * amp
    y = np.zeros(n); place(y, x, at * SR)
    return y


def room(x, t60, mix, seed, predelay, bright):
    ir = reverb_ir(t60, seed, predelay, bright)
    return x * (1 - mix) + ola_convolve(x, ir)[:len(x)] * mix


# surface definitions --------------------------------------------------------------------------------------------------------
# body: slab / deck modes (freq Hz, t60 s, amp); hard: low-pass of the slap; tex: texture kind; rm: (t60, mix, predelay, bright)
SURF = {
    "Platform":   dict(tock=620, body=[(78, 0.035, 1.0), (131, 0.03, 0.6), (205, 0.022, 0.35), (340, 0.015, 0.2)], hard=5200, pulse=6.0,
                       tex="grit", rm=(0.55, 0.13, 0.012, 5200), roll=(900, 4200), level=0.0),
    "Footbridge": dict(tock=560, body=[(62, 0.07, 0.5), (96, 0.05, 0.6), (148, 0.035, 0.5), (233, 0.025, 0.3)], hard=6200, pulse=5.0,
                       tex="girder", rm=(0.85, 0.2, 0.01, 5200), roll=(1100, 5200), level=0.0),
    "Concourse":  dict(tock=760, body=[(85, 0.05, 0.8), (142, 0.04, 0.55), (260, 0.025, 0.35), (520, 0.012, 0.15)], hard=7800, pulse=4.2,
                       tex="tile", rm=(1.05, 0.26, 0.016, 6200), roll=(1400, 6500), level=0.0),
    "Stairs":     dict(tock=700, body=[(110, 0.03, 0.8), (180, 0.025, 0.55), (300, 0.018, 0.35), (610, 0.01, 0.15)], hard=7200, pulse=3.8,
                       tex="nosing", rm=(0.8, 0.2, 0.008, 6000), roll=(1200, 5600), level=0.5),
    "Metal":      dict(tock=820, body=[(140, 0.05, 0.5), (233, 0.05, 0.45)], hard=8000, pulse=3.4,
                       tex="plate", rm=(0.7, 0.16, 0.01, 6500), roll=(1500, 6800), level=-0.5),
    "Train":      dict(tock=430, body=[(70, 0.06, 0.9), (118, 0.05, 0.7), (190, 0.03, 0.4)], hard=4200, pulse=7.0,
                       tex="rattle", rm=(0.35, 0.12, 0.004, 4200), roll=(700, 3400), level=-0.5),
}


def texture(kind, r, n, run, w):
    y = np.zeros(n)
    if kind == "grit":           # dust / sand under the sole on the outdoor stone platform
        y += grit(r, n, 0.12 if not run else 0.07, int(r.integers(6, 14)), 2400, 9000, 0.05 * w)
    elif kind == "girder":       # tiled deck on concrete over steel girders: a faint, short, low girder ring
        y += G.metal_hit(n / SR, int(r.integers(1, 1 << 30)), base=r.uniform(380, 520), t60=0.18, nmodes=7) * 0.05 * w
        y += grit(r, n, 0.06, int(r.integers(2, 5)), 3000, 9000, 0.025 * w)
    elif kind == "tile":         # polished tiles: clean, a tiny tick of grit now and then
        y += grit(r, n, 0.05, int(r.integers(0, 3)), 3500, 10000, 0.03 * w)
    elif kind == "nosing":       # stone tread with a painted steel nosing strip: a short bright tick of the heel edge
        k = int(0.006 * SR)
        y[:k] += bp(r.standard_normal(k), 2500, 8500, 2) * decay_env(k, 0.002) * 0.12 * w
        y += grit(r, n, 0.08, int(r.integers(3, 8)), 2400, 9000, 0.035 * w)
    elif kind == "plate":        # grooved steel step: plate ring (inharmonic, damped by the frame) + a loose-step rattle
        y += G.metal_hit(n / SR, int(r.integers(1, 1 << 30)), base=r.uniform(560, 760), t60=0.22, nmodes=12) * 0.55 * w
        rt = np.zeros(n)
        for j in range(int(r.integers(2, 4))):
            place(rt, G.metal_hit(0.06, int(r.integers(1, 1 << 30)), base=r.uniform(1500, 2400), t60=0.03, nmodes=5), (0.012 + 0.011 * j) * SR)
        y += rt * 0.25 * w
    elif kind == "rattle":       # EMU car floor: vinyl over plywood on steel - dull, with seat-frame / panel rattle
        rt = np.zeros(n)
        for j in range(int(r.integers(2, 5))):
            place(rt, bp(r.standard_normal(int(0.01 * SR)), 900, 3000, 2) * decay_env(int(0.01 * SR), 0.004), (0.008 + r.uniform(0, 0.05)) * SR)
        y += rt * 0.08 * w
    return y


def footstep(surface, seed, run, side):
    """One footstep, mono. side -1 left / +1 right."""
    S = SURF[surface]
    r = rng(seed)
    n = int((0.45 if run else 0.52) * SR)
    out = np.zeros(n)
    w = r.uniform(0.86, 1.14) * (1.0 + 0.04 * side)             # weight / force per step; the right foot a touch heavier
    tilt = 1.0 + 0.05 * side * r.uniform(0.5, 1.0)              # sole wear: slightly different spectra per foot
    t0 = int(r.uniform(0.003, 0.008) * SR)
    # --- heel strike (walk) / forefoot-midfoot strike (run)
    pms = S["pulse"] * (0.7 if run else 1.0) * r.uniform(0.85, 1.2)
    F = np.zeros(n); place(F, pulse(pms), t0)
    ir_n = int(0.4 * SR)
    freqs = [f * tilt * r.uniform(0.94, 1.06) for f, _, _ in S["body"]]
    t60s = [t * r.uniform(0.8, 1.2) for _, t, _ in S["body"]]
    amps = [a * r.uniform(0.75, 1.25) for _, _, a in S["body"]]
    body = drive(F, modes(ir_n, freqs, t60s, amps, r)) * 2.4
    if run:
        # the body's weight: a heavier, lower thump under the strike
        body += drive(F, modes(ir_n, [r.uniform(52, 68)], [0.035], [1.0], r)) * 1.6
    sl = slap(r, pms, S["hard"] * tilt, 1.7 if not run else 2.2)
    place(out, sl, t0)
    place(out, tock(r, pms, S["tock"] * tilt * r.uniform(0.93, 1.07), 2.2 if not run else 2.8), t0)
    out += body * (1.3 if run else 1.0)
    # --- roll and the second plant
    gap = r.uniform(0.022, 0.045) if run else r.uniform(0.055, 0.11)
    lo, hi = S["roll"]
    place(out, friction(r, gap * r.uniform(0.8, 1.1), lo * tilt, hi * tilt, 0.05 if not run else 0.07), t0 + int(0.006 * SR))
    F2 = np.zeros(n); place(F2, pulse(pms * r.uniform(0.9, 1.3)), t0 + gap * SR)
    ball = drive(F2, modes(ir_n, [f * 1.12 for f in freqs], [t * 0.7 for t in t60s], amps, r)) * 2.4
    out += ball * r.uniform(0.3, 0.45) * (1.1 if run else 1.0)
    place(out, slap(r, pms * 1.1, S["hard"] * 0.9, r.uniform(0.6, 0.85)), t0 + gap * SR)
    place(out, tock(r, pms * 1.1, S["tock"] * 1.25 * tilt, 0.7), t0 + gap * SR)
    # --- toe-off scuff (faint) a little later (walk only: the run's push-off is folded into the plant)
    if not run and r.uniform() < 0.8:
        place(out, friction(r, r.uniform(0.03, 0.06), lo * 1.4, hi, 0.018), t0 + (gap + r.uniform(0.09, 0.16)) * SR)
    # --- texture, foley
    out += texture(S["tex"], r, n, run, w)
    out += swish(r, n, max(0.0, t0 / SR - 0.05), 0.16 if not run else 0.12, 0.012 if not run else 0.02)
    out *= w
    out = hp(out, 45.0, 2)
    out = lp(out, 11000.0, 2)
    t60, mix, pre, bright = S["rm"]
    out = room(out, t60 * (0.9 if run else 1.0), mix, seed + 77, pre, bright)
    return out


def scuff(surface, seed):
    """Sole dragged across the floor (a hard stop or pivot at speed)."""
    S = SURF[surface]
    r = rng(seed)
    n = int(0.5 * SR)
    out = np.zeros(n)
    dur = r.uniform(0.13, 0.24)
    lo, hi = S["roll"]
    out[int(0.005 * SR):int(0.005 * SR) + int(dur * SR)] += friction(r, dur, lo * 0.9, hi * 1.2, 0.35, flutter=r.uniform(55, 110))[:int(dur * SR)]
    if surface in ("Concourse", "Metal", "Footbridge") and r.uniform() < 0.9:
        # rubber on a smooth hard face: a short squeak (fundamental glide + harmonics)
        m = int(r.uniform(0.04, 0.08) * SR)
        f = np.linspace(r.uniform(1150, 1500), r.uniform(1500, 1900), m)
        ph = TAU * np.cumsum(f) / SR
        sq = (np.sin(ph) + 0.4 * np.sin(2 * ph) + 0.15 * np.sin(3 * ph)) * np.sin(np.linspace(0, np.pi, m)) ** 2
        place(out, sq * 0.05, (0.02 + r.uniform(0, dur * 0.5)) * SR)
    F = np.zeros(n); place(F, pulse(S["pulse"] * 1.3), 0.004 * SR)
    out += drive(F, modes(int(0.3 * SR), [f for f, _, _ in S["body"]], [t for _, t, _ in S["body"]], [a for _, _, a in S["body"]], r)) * 1.2
    out += texture(S["tex"], r, n, False, 0.8)
    out = hp(out, 60.0, 2); out = lp(out, 11000.0, 2)
    t60, mix, pre, bright = S["rm"]
    return room(out, t60, mix, seed + 91, pre, bright)


def loudness(x):
    """RMS of the loudest 60 ms window (step 5 ms) - the perceived punch of a short step."""
    w = int(0.06 * SR); h = int(0.005 * SR)
    best = 0.0
    for s0 in range(0, max(1, len(x) - w), h):
        best = max(best, float(np.mean(x[s0:s0 + w] ** 2)))
    return np.sqrt(best)


TARGET = {"Walk": -19.0, "Run": -16.5, "Scuff": -22.0}       # dBFS (60 ms window RMS), peaks capped at -1 dBFS true peak


def save_norm(rel, x, kind, level_db, notes):
    tgt = TARGET[kind] + level_db
    g = 10 ** (tgt / 20.0) / max(loudness(x), 1e-9)
    x = x * g
    pk = float(np.max(np.abs(x)))
    if pk > 10 ** (-1.5 / 20):                                   # cap; save() keeps this scale (target_peak = actual peak)
        x *= 10 ** (-1.5 / 20) / pk
        pk = 10 ** (-1.5 / 20)
    G.save(rel, x, loop=False, target_peak_db=20 * np.log10(pk), notes=notes)


def all_jobs(only=""):
    base = {"Platform": 9700, "Footbridge": 9750, "Concourse": 9800, "Stairs": 9850, "Metal": 9900, "Train": 9950}
    for surf, b in base.items():
        if only and only not in surf:
            continue
        lev = SURF[surf]["level"]
        for side, sn in ((-1, "L"), (1, "R")):
            for v in range(4):
                save_norm(f"SFX/Player9C/SFX_Step9C_{surf}_Walk_{sn}{v + 1}.wav", footstep(surf, b * 10 + v * 7 + (side > 0) * 3, False, side), "Walk", lev,
                          f"player footstep 9C, {surf}, walk, {sn} foot, variation {v + 1}")
            for v in range(3):
                save_norm(f"SFX/Player9C/SFX_Step9C_{surf}_Run_{sn}{v + 1}.wav", footstep(surf, b * 10 + 50 + v * 11 + (side > 0) * 5, True, side), "Run", lev,
                          f"player footstep 9C, {surf}, run, {sn} foot, variation {v + 1}")
        for v in range(2):
            save_norm(f"SFX/Player9C/SFX_Step9C_{surf}_Scuff{v + 1}.wav", scuff(surf, b * 10 + 90 + v * 13), "Scuff", lev,
                      f"player scuff 9C (hard stop / pivot), {surf}, variation {v + 1}")


if __name__ == "__main__":
    ONLY = sys.argv[2] if len(sys.argv) > 2 else ""
    all_jobs(ONLY)
    with open(os.path.join(OUT, "manifest_9c%s.json" % ("_" + ONLY if ONLY else "")), "w") as f:
        json.dump(G.MANIFEST, f, indent=1)
    print("P9AUD manifest", len(G.MANIFEST), "files")
