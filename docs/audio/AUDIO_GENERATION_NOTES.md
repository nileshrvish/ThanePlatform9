# Platform No. 9 - Original Audio Pack: generation notes (Stage 8A, 2026-11-08)

## How to regenerate

```
cd "C:\Users\niles\Documents\Blender Projects\P9_Assets\StationAudio\OriginalAudioPack8A"
python -I gen_8a_audio.py <out_dir> music|sfx|creatures|ambience     (refuses to overwrite existing WAVs)
python -I check_8a_audio.py <out_dir>                                  (independent check -> check_8a.json)
```

Everything is deterministic (fixed seeds), 48 kHz. Requirements: Python 3 + NumPy only (no SciPy: filters are FFT-domain,
time-varying filters are STFT overlap-add, reverb is block FFT convolution with generated impulses).

## Building blocks

| Block | Method |
|---|---|
| Filters | zero-phase spectral shaping (`bp`: Butterworth-shaped magnitude), STFT time-varying filters (`stft_filter`, Hann 2048-4096, 75 % overlap) |
| Noise colours | white -> pink / brown / blue by 1/f^p spectral shaping |
| Reverb | generated impulse (decaying filtered noise + a few early reflections), FFT overlap-add convolution, separate seeds per channel |
| Seamless loops | render loop length + crossfade time, then crossfade the tail into the head (equal power); LFOs use a whole number of cycles over the loop; events near the end wrap to the start (`mix_wrap`) |
| Piano | 2 detuned strings per note, stiff-string inharmonicity, partial-dependent two-stage decays, hammer noise |
| Bell / metal | inharmonic partial sets (bell ratios / random modal sets) with per-partial decays, noise excitation |
| Train | brown-noise body rumble + motor whine following a speed curve + bogie-pair wheel clacks timed by speed |
| Scream / call / breath | band-limited pulse train with jittered, vibrato-ed pitch contour -> STFT vowel formants -> soft clipping; layered (main, low growl, high shriek); breath = noise through moving formants with inhale/exhale envelope |
| Output | DC removed, peak or RMS normalised (music/ambience by RMS, SFX to -1 dBFS peak), 4x true-peak check over the whole file (capped at -0.5 dBTP), TPDF dither to 16-bit |

## Track notes

- **BGM_Main_Station_Ambience** (180 s loop, RMS -24 dBFS): distant railway rumble swelling 3 times per loop, 50 Hz mains hum with harmonics, gusting wind band, two distant trains passing (wheel joints), rare far metallic pings (steel cooling), 3.2 s reverb.
- **BGM_Platform9_Horror** (120 s loop, RMS -22): D/A drones with a minor-second Eb rising with an arc that peaks late and recedes into the loop point, a thin glassy G#/A cluster at the peak, sparse dark piano (D minor with the flat second), a low pulse only at the peak.
- **BGM_Anomaly_Ambience** (90 s loop, RMS -24): 31 / 33.25 Hz beating with a sub, reversed bells / metal / piano swells, a drifting metallic ring, noise through slowly moving formants.
- **BGM_Final_Epilogue** (122 s, not a loop): D-minor pad progression (Dm-Bb-F-C/E-Gm-Bb-Asus-Dm, 7.5 s each) played twice with brightening pads and a piano melody, a low pulse joining the second pass, then the last chord bending into a cluster over a falling drone (dark ending). The final screen plays it from 58 s.

## Integration (AP9AudioDirector, Lantern Man, Greeting)

- Normal station: main ambience 0.30, footbridge bed 0.30 on the bridge or platform wind 0.26 on platforms; both lower inside a train.
- Approaching Platform 9: the Platform 9 drone rises 0 -> 0.40 from y 5600 to 7000 (and is full on the island); the main bed ducks to 0.17.
- Anomaly ambience (0.32): **only after the player has seen / met the day's anomaly** (user decision 2026-11-08 under rule 8: never a tell before it is found; normal days never play it). "Seen" per handler: target in view 0.5 s, the wrong PA heard, the endless bridge / escalator met, the Lantern Man seen, the train boarded or looked at.
- Lantern Man: flame loop (falloff 7 m) and low rumble (15 m) on him, the lamp's buzz on the flickering lamp during bursts, one stinger the first time he is seen.
- Kanchana: breathing loop attached to her (9 m), presence layer growing as she comes within 3-18 m, her footsteps (close within 7 m, else distant) from her own movement.
- Epilogue: station layers fade out, the epilogue music plays from the wake-up; on the last ride everything fades from ride time 22 s so the Greeting comes out of silence; her scream + distorted layer at the lunge, the impact at contact; the final screen plays the ending.
- Player: footsteps by surface (footbridge above z 540, stairs when the stride climbs > 9 cm, platform otherwise), stride = max(72 cm, 0.26 x speed) (about 4 steps/s at full pace), jump and landing.
- Not wired (library only): railway approach / pass / moving loop / hum / screech / distant horn (the trains keep their existing sounds), gate, interaction clicks, horror one-shots (impact, metallic stinger, reverse bell, electrical, whisper, low hit, breathing, transition), Greeting distant call, Kanchana movement, Lantern creak.

## Honest limits

- The sounds were checked by measurement (peaks, true peaks, RMS, seams, clipping) and by spectrogram pictures, **not by listening** - a person must listen before release, especially to the scream level against the rest of the mix.
- Procedural synthesis sounds synthetic up close (the scream and breathing especially); it is designed to sit under reverb in a dark scene, not to pass as a recording.
- No Android device test of the mix or of memory use; the Unreal import uses compression quality 45 for music/ambience.


## Stage 9A (2026-11-08) - `gen_9a_audio.py`

Imports the 8A helpers read-only and writes only new files (save() refuses existing ones). `python gen_9a_audio.py <out_root>`.

- **Footsteps** (`footstep9`): rubber sole; heel strike = band-passed noise burst + a short mid-low body thump (105-200 Hz, never sub-bass:
  everything is high-passed at 70 Hz and low-passed at 9 kHz, no long resonance), then a toe/scuff burst 55-85 ms later (32-48 ms when
  running), per-step weight variation. Platform: concrete with grit crunch, small canopy room. Footbridge: firmer deck thump, a very short
  faint girder ring, covered-bridge reverb. Stairs: sharper and brighter strike, soft nosing tick, short roll, stairwell reverb. Run variants are
  heavier. Left / right sets differ slightly in spectrum and sit +-0.12 off-centre (stereo).
- **Thunder** (`thunder`): a tortuous channel of 420 segments; each segment's N-wave arrives at distance / 343 m/s and is low-passed by its
  distance (air absorption), plus a low rumble following the arrivals, long outdoor reverb. Distant rolls (7-11 km), strong (2-4 km), close
  cracks (0.7-1 km, tearing crack then roll).
- **Wind** (`wind_slow`): brown + pink noise in the 45-900 Hz band, level drifting with 25-60 s periods, a slowly moving soft resonance; no
  hiss layer; normalised to -30 dBFS RMS (8A wind: -24, faster gusts, a 1.5-5 kHz hiss). The director plays it at 0.20 (was 0.26) and fades it
  in / out at 0.08 per second (was 0.2).
- **Train**: two-tone pneumatic EMU horn (F4 + A4 reed-like pulse source, pressure rise at the start, sag and drop at the end, bell formants
  at 1.15 / 2.5 / 3.6 kHz, breath noise) - long, short, double, distant (filtered + open-air echo). Rolling loop (wheel-rail roar + body
  rumble), traction loop (EMU motor / gear whine at 210 Hz; the code moves its pitch 0.35-1.2 with speed), idle loop (compressor + blower),
  five rail-joint clatters, air-brake application, brake-block friction loop (grind + soft, intermittent squeal at 8 %% level), standstill vent.
- Runtime (C++): footstep cadence 1.6 + 0.45 x speed (m/s) steps/s (max 4.3), stride = speed / cadence, left / right alternate, run set above
  3.3 m/s, volume grows with speed, one soft settling step when the player stops, nothing while standing. Thunder: flash detected from the
  moon light (storm controller untouched); a visible bolt -> delay = its distance / 343 m/s (close under 1.5 km); no bolt -> 55 %% chance of a
  distant thunder 2.5-9 km away (7-22 s). Train: AP9TrainAudio reads speed / state / route distance: clatter per axle per 13 m rail joint at
  the axle, roll / traction / brake / idle levels from speed and state, brake application at Braking, vent at the stop, approach horn when a
  train starts braking within 90 m of a player who is not aboard.

**Not listened to by Claude.** All checks are measurements (peaks, true peaks, RMS, loop seams) and PIE logs (which sound played when, step
counts, delays, layer levels). A person must listen in PIE and on the phone before release - especially the horn character, the thunder
level against the station bed and the stair steps.


## Stage 9B (2026-11-08) - `gen_9b_audio.py`

Menu loop: D-minor drone pad with a soft beating Eb, a slow sparse piano figure every 12 s (gaps, unresolved), overhead-line hum and
far rumble, 9 distant metallic pings, one very distant EMU horn (the 9A synth, filtered), the 9A slow breeze underneath, 4.5 s reverb;
96 s seamless loop at -26 dBFS RMS. UI: a soft relay click (2.2 kHz / 1.5 kHz for Back) and a low two-note bell chime.
Captions: `build_captions_9b.py` maps each script sentence onto the speech found in the WAV (energy threshold, split at the N-1 longest
silences, ellipsis pauses counted). NOT listened to by Claude.

## Stage 9C - footsteps (2026-11-09)

`gen_9c_audio.py` (copy here; source in `P9_Assets/StationAudio/OriginalAudioPack9C/`), NumPy only, fixed seeds, reuses the 8A helpers.
One step = heel strike (raised-cosine contact-force pulse, 4-8 ms walk, shorter for a run) driving the surface's damped modes + a
contact slap + a short shoe-body "tock"; heel-to-toe roll friction (stick-slip flutter); a weaker ball/toe plant 55-110 ms later
(walk) or 22-45 ms (run); faint toe-off scuff; surface texture (grit, girder ring, plate ring, panel rattle); a faint trouser swish;
a short room response per space. Left/right feet differ slightly (weight, spectrum tilt). Each file is normalised by its loudest
60 ms window (walk -19, run -16.5, scuff -22 dBFS, per-surface trim), true peak capped at -1.5 dBFS.

Iterations (all kept, nothing deleted): `Out/` v1 and `Out_v2/` (identical copy of v1: a patch did not apply) were dominated by
low-frequency slab ring (88-100 % of the energy below 200 Hz, toe plant masked); `Out_v3/` rebalanced (slab ring x0.27, stronger
slap, shoe tock added: heel and toe onsets both detectable); `Out_v4/` (imported) shortened the footbridge deck ring.
Objective checks (`analyse_9c.py`): window loudness, peak, spectral centroid (3-5 kHz, train ~3 kHz), share below 200 Hz
(0.3-0.8), onset detection (heel + toe found), no energy in the last 40 ms (no end clicks).
**Nobody has listened to these sounds yet** - Claude has no audio output; the numbers above are not a listening test.
