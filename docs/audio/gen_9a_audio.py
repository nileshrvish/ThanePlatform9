# Stage 9A - "Platform No. 9" audio additions. 100 % procedural (numpy only, fixed seeds): no samples, no recordings, no AI
# services. Reuses the 8A helpers (gen_8a_audio.py, imported read-only). Run:  python gen_9a_audio.py <out_root>
# Writes 48 kHz 16-bit PCM WAVs into <out_root>/{SFX/Player9A, SFX/Train9A, Ambience9A, Weather9A, Music9A} + manifest_9a.json.
# New file names only - nothing from 8A is overwritten (save() refuses existing files).
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
rng, bp, osc, coloured, decay_env, smooth_noise, reverb, loopify, pan, softclip, fade = (G.rng, G.bp, G.osc, G.coloured, G.decay_env,
    G.smooth_noise, G.reverb, G.loopify, G.pan, G.softclip, G.fade)
spectral_shape, peak_eq, env_adsr, periodic_lfo, mix_at, mix_wrap, metal_hit, stft_filter = (G.spectral_shape, G.peak_eq, G.env_adsr,
    G.periodic_lfo, G.mix_at, G.mix_wrap, G.metal_hit, G.stft_filter)


def hp(x, f, order=2):
    return bp(x, f, None, order)


def burst(n, seed, lo, hi, t60):
    return bp(rng(seed).standard_normal(n), lo, hi, 2) * decay_env(n, t60)


# =============================================================================================================== FOOTSTEPS
def footstep9(seed, surface, run=False, side=0):
    """Rubber-soled shoe. Heel strike (body thump + broadband contact), roll, toe push (scuff), surface texture.
    No sub-bass (high-passed 70 Hz), no hollow resonance, no click transients above 9 kHz."""
    r = rng(seed)
    n = int(0.42 * SR)
    out = np.zeros(n)
    w = r.uniform(0.85, 1.15)                                         # per-step weight variation
    gap = int((r.uniform(0.032, 0.048) if run else r.uniform(0.055, 0.085)) * SR)   # heel -> toe
    tilt = 1.0 + 0.06 * side                                          # left / right foot: slightly different spectrum
    if surface == "platform":        # concrete with grit, open air under the canopy
        hk = int(0.07 * SR)
        heel = burst(hk, seed + 1, 140 * tilt, 3200 * tilt, 0.022) * 0.9
        heel += osc(r.uniform(105, 135) * tilt, hk) * decay_env(hk, 0.028) * 0.55          # body thump (mid-low, not boomy)
        toe = burst(int(0.06 * SR), seed + 2, 700, 6500, 0.03) * 0.42
        grit = np.zeros(n)
        for _ in range(r.integers(5, 12)):                            # sand / grit crunch under the sole
            j = r.integers(0, int(0.16 * SR)); k = int(0.004 * SR)
            grit[j:j + k] += burst(k, int(r.integers(1, 1 << 30)), 2500, 8500, 0.0015) * r.uniform(0.05, 0.16)
        room = (0.45, 0.10, 6500)
    elif surface == "footbridge":    # concrete deck on steel girders, covered: a firmer, slightly resonant step
        hk = int(0.08 * SR)
        heel = burst(hk, seed + 1, 120 * tilt, 2800 * tilt, 0.026) * 0.85
        heel += osc(r.uniform(150, 175) * tilt, hk) * decay_env(hk, 0.045) * 0.5            # deck body
        toe = burst(int(0.06 * SR), seed + 2, 600, 5500, 0.03) * 0.38
        grit = metal_hit(0.42, seed + 3, base=r.uniform(620, 900), t60=0.12, nmodes=8) * 0.08  # faint girder ring, very short
        room = (0.9, 0.17, 5000)
    else:                            # stairs: hard stone treads with steel nosing - sharper, brighter, shorter roll
        hk = int(0.05 * SR)
        heel = burst(hk, seed + 1, 260 * tilt, 6500 * tilt, 0.014) * 1.0
        heel += osc(r.uniform(160, 200) * tilt, hk) * decay_env(hk, 0.018) * 0.4
        heel[:int(0.0025 * SR)] += burst(int(0.0025 * SR), seed + 4, 2000, 7000, 0.001) * 0.35   # nosing tick (soft)
        toe = burst(int(0.04 * SR), seed + 2, 900, 7000, 0.02) * 0.30
        gap = int(gap * 0.6)
        grit = np.zeros(n)
        room = (0.8, 0.2, 6000)
    if run:
        heel = heel * 1.35 + burst(len(heel), seed + 7, 90, 900, 0.03) * 0.35               # heavier landing
        toe = toe * 1.4
    out[:len(heel)] += heel * w
    out[gap:gap + len(toe)] += toe * r.uniform(0.8, 1.2)
    out += grit
    out = hp(out, 70, 2)
    out = bp(out, None, 9000, 2)
    y = reverb(out, room[0], room[1], seed=seed + 5, bright=room[2])
    p = 0.12 * (-1 if side < 0 else 1)
    return pan(y, p)                                                  # stereo: left / right foot slightly off-centre


# =============================================================================================================== THUNDER
def thunder(seed, dist_m, length_m, dur, crack=False):
    """Physically motivated rolling thunder: a tortuous lightning channel is split into many segments; each emits an N-wave
    whose arrival time is its distance / 343 m/s and whose brightness falls with distance (air absorption). Summed, then a
    long outdoor reverb (ground + cloud reflections)."""
    r = rng(seed)
    n = int(dur * SR)
    out = np.zeros(n)
    segs = 420
    # channel: a random walk from the cloud base down; distances of its segments from the listener
    d = dist_m + np.cumsum(r.normal(0.0, length_m / segs * 1.8, segs))
    d = np.clip(d, max(80.0, dist_m * 0.6), None)
    t0 = (d - d.min()) / 343.0
    t0 = t0 / max(t0.max(), 1e-3) * min(t0.max(), dur * 0.62)
    for i in range(segs):
        L = int(r.uniform(0.002, 0.009) * SR * (1 + d[i] / 3000))
        nw = np.linspace(1, -1, L) * r.uniform(0.3, 1.0)               # N-wave
        cut = 9000.0 / (1 + d[i] / 450.0)                               # air absorption: far = dull
        j = int((t0[i] + 0.02) * SR)
        if j + L >= n:
            continue
        seg = np.zeros(int(0.08 * SR)); seg[:L] = nw
        seg = bp(seg, 25, max(120.0, cut), 1)
        out[j:j + len(seg)] += seg[:n - j] * (1.0 / (1 + d[i] / 1500.0))
    # low rumble body following the arrivals
    w = int(0.25 * SR)
    cs = np.concatenate([[0.0], np.cumsum(np.abs(out))])
    idx = np.arange(n)
    env = (cs[np.minimum(idx + w // 2, n)] - cs[np.maximum(idx - w // 2, 0)]) / w      # moving average (O(n))
    env /= env.max() + 1e-9
    rum = bp(coloured(n, seed + 1, "brown"), 25, 160 if not crack else 260, 2) * env ** 0.7
    out = out / (np.max(np.abs(out)) + 1e-9) + rum * (0.9 if not crack else 0.6)
    if crack:                                                        # close strike: tearing crack at the start
        k = int(0.35 * SR)
        cr = bp(r.standard_normal(k), 300, 7000, 2) * decay_env(k, 0.12)
        cr *= 1 + 0.8 * np.sign(np.sin(TAU * r.uniform(25, 45) * np.arange(k) / SR))   # ripping texture
        mix_at(out, cr / (np.max(np.abs(cr)) + 1e-9), 0.0, 1.1)
    out = hp(out, 22, 2)
    out = out * env_adsr(n, 0.01 if crack else 0.25, 0.4, 0.85, dur * 0.45)
    st = reverb(out, 4.5 if not crack else 3.2, 0.45, seed=seed + 2, predelay=0.05, bright=2500 if not crack else 5000, stereo=True)
    st[:, 0] *= 1 + 0.15 * smooth_noise(n, 0.5, seed + 3)            # slow width / movement
    st[:, 1] *= 1 - 0.15 * smooth_noise(n, 0.5, seed + 3)
    return st


# =============================================================================================================== WIND
def wind_slow(L, XF, seed):
    """Deep, soft, slow breeze: low band noise whose level and colour drift with periods of 25-60 s; no hiss bed."""
    n = L + XF
    w = np.zeros((n, 2))
    for c in range(2):
        g = np.clip(0.55 + 0.45 * smooth_noise(n, 0.022, seed + c) + 0.12 * smooth_noise(n, 0.07, seed + 10 + c), 0.15, 1.0)
        base = bp(coloured(n, seed + 2 + c, "brown"), 45, 520, 2)
        body = bp(coloured(n, seed + 4 + c, "pink"), 160, 900, 2)
        def g2(f, tt, c=c):                                           # a gentle moving resonance (the air round the canopy)
            fc = 230 + 110 * np.sin(TAU * tt / 47.0 + c)
            return 0.35 + 0.65 / (1 + ((f - fc) / 90.0) ** 2)
        body = stft_filter(body, g2, nfft=4096, hop=1024)
        w[:, c] = base * g * 0.9 + body * g ** 2 * 0.45
    return loopify(w, L, XF)


# =============================================================================================================== TRAIN
def emu_horn(dur, seed, notes=(349.2, 440.0), distant=False):
    """Pneumatic two-tone EMU horn: reed-like pulse source, air-pressure pitch rise at the start, slight sag and drop at the
    end, bell formants, breath noise. Higher and more nasal than a truck air horn, not buzzy like a car horn."""
    r = rng(seed)
    n = int(dur * SR)
    t = np.arange(n) / SR
    out = np.zeros(n)
    rise = 0.86 + 0.14 * (1 - np.exp(-t / 0.06))
    rel = np.clip((dur - 0.22 - t) / 0.22, 0, 1)
    sag = 1 - 0.035 * (1 - rel)
    for k, f in enumerate(notes):
        fr = f * rise * sag * (1 + 0.003 * np.sin(TAU * r.uniform(4.5, 6.0) * t + k) + 0.002 * smooth_noise(n, 3, seed + k))
        ph = np.cumsum(TAU * fr / SR)
        src = np.zeros(n)
        for h in range(1, int(7000 / f)):
            src += np.sin(h * ph + r.uniform(0, TAU)) * (1.0 / h ** 0.85) * (1 if h % 2 else 0.75)
        out += src * (1.0 if k == 0 else 0.85)
    shape = lambda f: (0.25 + peak_eq(f, 1150, 2.5, 9) * peak_eq(f, 2500, 3.0, 6) * peak_eq(f, 3600, 4, 3)) / (1 + (f / 5200) ** 4)
    out = spectral_shape(out, shape)
    breath = bp(coloured(n, seed + 9, "pink"), 800, 6000) * 0.06
    out = out / (np.max(np.abs(out)) + 1e-9) + breath
    chuff = int(0.07 * SR)
    out[:chuff] += bp(r.standard_normal(chuff), 300, 4000) * decay_env(chuff, 0.04) * 0.4
    out *= env_adsr(n, 0.05, 0.08, 0.92, 0.2)
    out = softclip(out * 1.2, 1.3)
    if distant:
        out = bp(out, 180, 2600, 2)
        out = reverb(out, 3.2, 0.62, seed=seed + 3, predelay=0.09, bright=2200)
    else:
        out = reverb(out, 1.6, 0.2, seed=seed + 3, predelay=0.02, bright=6000)
    return out


def train_roll_loop(L, XF, seed):
    """Wheel-rail rolling roar + body rumble at cruising speed (pitch / level follow the speed in code). No joint clatter here
    (clatter is triggered per axle per rail joint by the code)."""
    n = L + XF
    roar = bp(coloured(n, seed, "pink"), 180, 2600, 2)
    roar = spectral_shape(roar, lambda f: peak_eq(f, 550, 1.5, 4) * peak_eq(f, 1300, 2, 3))
    roar *= 0.8 + 0.2 * smooth_noise(n, 1.3, seed + 1)
    rum = bp(coloured(n, seed + 2, "brown"), 30, 180, 2) * (0.85 + 0.15 * periodic_lfo(n, 6))
    flat = np.zeros(n)                                                # faint periodic "flat wheel" thump, 1 per wheel turn
    per = int(0.21 * SR)
    for j in range(0, n - 2000, per):
        k = int(0.03 * SR)
        flat[j:j + k] += osc(85, k) * decay_env(k, 0.02) * 0.12
    return loopify(roar * 0.55 + rum * 0.75 + flat, L, XF)


def traction_loop(L, XF, seed, f0=210.0):
    """EMU traction motors + gears + inverter: harmonic whine at f0 (the code shifts the pitch with speed), blower hum."""
    n = L + XF
    r = rng(seed)
    t = np.arange(n) / SR
    wob = 1 + 0.002 * periodic_lfo(n, 4)
    out = np.zeros(n)
    for h, a in ((1, 0.5), (2, 0.35), (3, 0.18), (4.5, 0.08), (6, 0.12), (9, 0.05)):
        out += a * np.sin(TAU * f0 * h * wob * t + r.uniform(0, TAU))
    out += 0.06 * np.sin(TAU * 3 * f0 * 6.02 * t)                     # inverter sideband (thin, high)
    blower = bp(coloured(n, seed + 1, "pink"), 120, 900) * 0.35
    hum = 0.15 * np.sin(TAU * 100 * t) + 0.08 * np.sin(TAU * 300 * t)
    out = out * (0.85 + 0.15 * smooth_noise(n, 0.8, seed + 2)) + blower + hum
    return loopify(bp(out, 60, 7000), L, XF)


def idle_loop(L, XF, seed):
    """A train standing at the platform: compressor thump-chug (slow), blower hum, faint transformer hum."""
    n = L + XF
    t = np.arange(n) / SR
    out = bp(coloured(n, seed, "pink"), 90, 700) * 0.4
    out += sum(a * np.sin(TAU * f * t) for f, a in ((100, 0.25), (200, 0.08), (300, 0.05)))
    per = int(0.42 * SR)                                              # compressor pistons
    for j in range(0, n - per, per):
        k = int(0.12 * SR)
        out[j:j + k] += bp(rng(seed + j).standard_normal(k), 60, 500) * decay_env(k, 0.07) * 0.5
    return loopify(out, L, XF)


def clatter(seed):
    """One wheelset over a rail joint: low thud + short metallic ring + contact click."""
    r = rng(seed)
    n = int(0.35 * SR)
    out = np.zeros(n)
    k = int(0.06 * SR)
    out[:k] += osc(r.uniform(70, 95), k) * decay_env(k, 0.035) * 1.0
    out[:k] += burst(k, seed + 1, 200, 3000, 0.012) * 0.7
    out += metal_hit(0.35, seed + 2, base=r.uniform(700, 1100), t60=0.09, nmodes=10) * 0.5
    return reverb(hp(out, 45), 0.5, 0.12, seed=seed + 3, bright=6000)


def brake_apply(seed):
    """Air brake application: brake-pipe exhaust hiss (falling), block contact clunk."""
    n = int(2.4 * SR)
    r = rng(seed)
    hiss = bp(coloured(n, seed, "white"), 1800, 9000) * env_adsr(n, 0.03, 0.4, 0.55, 1.6)
    def g(f, tt):                                                      # the hiss narrows and falls as the pressure drops
        fc = 5200 - 2400 * min(1.0, tt / 2.4)
        return 0.3 + 1.0 / (1 + ((f - fc) / 1800) ** 2)
    hiss = stft_filter(hiss, g, 2048, 512)
    clunk = np.zeros(n); k = int(0.15 * SR)
    clunk[int(0.25 * SR):int(0.25 * SR) + k] += (osc(110, k) * decay_env(k, 0.05) + burst(k, seed + 1, 150, 2500, 0.03)) * 0.8
    return reverb(hiss * 0.55 + clunk, 1.0, 0.18, seed=seed + 2)


def brake_friction_loop(L, XF, seed):
    """Cast-iron blocks on the wheel treads: rough grinding (level follows the braking in code) and a soft, unsteady squeal
    (narrow partials with jitter; kept low - no cinematic screech)."""
    n = L + XF
    grind = bp(coloured(n, seed, "pink"), 350, 3200, 2) * (0.7 + 0.3 * smooth_noise(n, 9, seed + 1))
    t = np.arange(n) / SR
    sq = np.zeros(n)
    for f, a in ((2180, 1.0), (3330, 0.45), (4410, 0.2)):
        fr = f * (1 + 0.006 * smooth_noise(n, 2.5, int(f)))
        sq += a * np.sin(np.cumsum(TAU * fr / SR))
    sq *= np.clip(smooth_noise(n, 0.6, seed + 2) * 1.4 - 0.1, 0, 1) ** 2   # comes and goes
    return loopify(grind * 0.8 + sq * 0.08, L, XF)


def stop_release(seed):
    """At standstill: brake cylinders vent (psssh) + a settling clunk of the couplers."""
    n = int(2.6 * SR)
    hiss = bp(coloured(n, seed, "white"), 1500, 8000) * env_adsr(n, 0.02, 0.2, 0.8, 1.9)
    k = int(0.2 * SR)
    clunk = np.zeros(n)
    clunk[:k] += osc(80, k) * decay_env(k, 0.06) * 0.7 + burst(k, seed + 1, 120, 1800, 0.04) * 0.5
    return reverb(hiss * 0.5 + clunk, 1.2, 0.2, seed=seed + 2)


# =============================================================================================================== MAIN
class _Jobs(dict):
    """Saves each sound as soon as it is made (progress visible, memory small)."""
    def __setitem__(self, rel, val):
        x, loop, notes = val
        if ONLY and ONLY not in rel:
            return
        rms = -30.0 if rel.startswith("Ambience9A") else None
        G.save(rel, x, loop=loop, target_peak_db=-3.0 if "Step9A" in rel else -1.0, target_rms_db=rms, notes=notes)


ONLY = sys.argv[2] if len(sys.argv) > 2 else ""


def all_jobs():
    out = _Jobs()
    # footsteps: 3 surfaces x walk (6 = 3 left + 3 right) + run (4 = 2 + 2)
    for surf, name in (("platform", "Platform"), ("footbridge", "Footbridge"), ("stairs", "Stairs")):
        base = {"platform": 9100, "footbridge": 9200, "stairs": 9300}[surf]
        for side, sname in ((-1, "L"), (1, "R")):
            for v in range(3):
                out[f"SFX/Player9A/SFX_Step9A_{name}_Walk_{sname}{v + 1}.wav"] = (footstep9(base + v * 11 + (side > 0) * 5, surf, False, side), False,
                                                                               f"player footstep 9A, {surf}, walk, {sname} foot, variation {v + 1}")
            for v in range(2):
                out[f"SFX/Player9A/SFX_Step9A_{name}_Run_{sname}{v + 1}.wav"] = (footstep9(base + 50 + v * 13 + (side > 0) * 7, surf, True, side), False,
                                                                              f"player footstep 9A, {surf}, run, {sname} foot, variation {v + 1}")
    # thunder: distant rolls, strong, close cracks
    for v, (d, ln, du) in enumerate(((9000, 5000, 13.0), (7000, 4000, 11.0), (11000, 6000, 14.0), (8000, 3500, 10.0))):
        out[f"Weather9A/SFX_Thunder9A_DistantRoll_{v + 1:02d}.wav"] = (thunder(9400 + v, d, ln, du), False, f"distant rolling thunder {d / 1000:.0f} km")
    for v, (d, ln, du) in enumerate(((3000, 3000, 11.0), (2200, 2500, 10.0), (4000, 3000, 12.0))):
        out[f"Weather9A/SFX_Thunder9A_Strong_{v + 1:02d}.wav"] = (thunder(9450 + v, d, ln, du), False, f"strong thunder {d / 1000:.1f} km")
    for v, (d, ln, du) in enumerate(((700, 2000, 9.0), (1000, 2200, 9.5))):
        out[f"Weather9A/SFX_Thunder9A_Close_{v + 1:02d}.wav"] = (thunder(9480 + v, d, ln, du, crack=True), False, f"close strike crack + roll {d} m")
    # wind (slow, deep, soft) - stereo loop
    out["Ambience9A/AMB_Platform_Wind9A_Loop.wav"] = (wind_slow(90 * SR, 6 * SR, 9500), True, "platform breeze 9A: deep, soft, slow gusts (90 s loop)")
    # train
    out["SFX/Train9A/SFX_Train9A_Horn_Long.wav"] = (emu_horn(2.2, 9600), False, "EMU two-tone pneumatic horn, long, near")
    out["SFX/Train9A/SFX_Train9A_Horn_Short.wav"] = (emu_horn(0.85, 9601), False, "EMU horn, short toot, near")
    dbl = np.concatenate([emu_horn(0.55, 9602), np.zeros(int(0.16 * SR)), emu_horn(0.9, 9603)])
    out["SFX/Train9A/SFX_Train9A_Horn_Double.wav"] = (dbl, False, "EMU horn, double toot, near")
    out["SFX/Train9A/SFX_Train9A_Horn_Distant.wav"] = (emu_horn(2.4, 9604, distant=True), False, "EMU horn, long, distant (filtered + open-air echo)")
    out["SFX/Train9A/SFX_Train9A_Roll_Loop.wav"] = (train_roll_loop(12 * SR, SR, 9610), True, "running train: wheel-rail roll + body rumble (12 s loop)")
    out["SFX/Train9A/SFX_Train9A_Traction_Loop.wav"] = (traction_loop(6 * SR, SR // 2, 9620), True, "EMU traction motor / gear whine at 210 Hz (6 s loop)")
    out["SFX/Train9A/SFX_Train9A_Idle_Loop.wav"] = (idle_loop(8 * SR, SR // 2, 9630), True, "standing EMU: compressor + blower hum (8 s loop)")
    for v in range(5):
        out[f"SFX/Train9A/SFX_Train9A_Clatter_{v + 1:02d}.wav"] = (clatter(9640 + v * 3), False, "wheelset over a rail joint")
    out["SFX/Train9A/SFX_Train9A_BrakeApply.wav"] = (brake_apply(9660), False, "air brake application hiss + block clunk")
    out["SFX/Train9A/SFX_Train9A_BrakeFriction_Loop.wav"] = (brake_friction_loop(6 * SR, SR // 2, 9670), True, "brake blocks on wheels: grind + soft squeal (6 s loop)")
    out["SFX/Train9A/SFX_Train9A_StopRelease.wav"] = (stop_release(9680), False, "brake cylinder vent at standstill + coupler clunk")
    return out


if __name__ == "__main__":
    only = ONLY
    all_jobs()                                                           # each sound is saved as it is made (wind loop RMS -30 dBFS)
    with open(os.path.join(OUT, "manifest_9a%s.json" % ("_" + only.replace("/", "_") if only else "")), "w") as f:
        json.dump(G.MANIFEST, f, indent=1)
    print("P9AUD manifest", len(G.MANIFEST), "files")
