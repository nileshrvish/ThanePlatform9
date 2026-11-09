# Stage 9C: objective checks of the footsteps (no listening): loudness window RMS, peaks, heel->toe structure, spectral centroid,
# low-end share; a sheet of waveforms + spectrograms (PNG). Usage: python analyse_9c.py <9C dir> <9A dir> <out.png>
import sys, os, wave, glob
import numpy as np
from PIL import Image, ImageDraw
SR = 48000
def rd(p):
    with wave.open(p) as w:
        x = np.frombuffer(w.readframes(w.getnframes()), "<i2").astype(float) / 32768
        if w.getnchannels() == 2: x = x.reshape(-1, 2).mean(1)
    return x
def win_rms(x):
    w = int(0.06 * SR); h = int(0.005 * SR)
    return 20 * np.log10(max(np.sqrt(np.mean(x[s:s + w] ** 2)) for s in range(0, len(x) - w, h)) + 1e-12)
def centroid(x):
    X = np.abs(np.fft.rfft(x)); f = np.fft.rfftfreq(len(x), 1 / SR); return float((X * f).sum() / X.sum())
def low_share(x):
    X = np.abs(np.fft.rfft(x)) ** 2; f = np.fft.rfftfreq(len(x), 1 / SR); return float(X[f < 200].sum() / X.sum())
def onsets(x):
    e = np.convolve(x ** 2, np.ones(96) / 96, "same"); e = 10 * np.log10(e + 1e-12)
    d = np.diff(e); pk = [i for i in range(1, len(d) - 1) if d[i] > 0.35 and e[i] > e.max() - 22]
    out = []
    for i in pk:
        if not out or i - out[-1] > int(0.015 * SR): out.append(i)
    return [round(i / SR * 1000) for i in out[:4]]
d9c, d9a, png = sys.argv[1], sys.argv[2], sys.argv[3]
rows = []
for p in sorted(glob.glob(os.path.join(d9c, "*.wav"))):
    x = rd(p); n = os.path.basename(p)[11:-4]
    rows.append((n, win_rms(x), 20 * np.log10(np.abs(x).max()), centroid(x), low_share(x), onsets(x)))
for p in sorted(glob.glob(os.path.join(d9a, "SFX_Step9A_*_Walk_L1.wav")) + glob.glob(os.path.join(d9a, "SFX_Step9A_*_Run_L1.wav"))):
    x = rd(p); n = "9A " + os.path.basename(p)[11:-4]
    rows.append((n, win_rms(x), 20 * np.log10(np.abs(x).max()), centroid(x), low_share(x), onsets(x)))
for r in rows:
    print("%-26s win %6.1f dB peak %6.1f centroid %5.0f Hz low<200 %4.2f onsets(ms) %s" % r)
sel = ["Platform_Walk_L1", "Footbridge_Walk_L1", "Concourse_Walk_L1", "Stairs_Walk_L1", "Metal_Walk_L1", "Train_Walk_L1",
       "Platform_Run_R1", "Concourse_Run_R1", "Stairs_Run_R1", "Metal_Run_R1", "Concourse_Scuff1", "Platform_Scuff1"]
W, H = 700, 110
img = Image.new("RGB", (W * 2, H * len(sel)), (12, 12, 16)); dr = ImageDraw.Draw(img)
for i, s_ in enumerate(sel):
    x = rd(os.path.join(d9c, "SFX_Step9C_%s.wav" % s_))[:int(0.5 * SR)]
    y0 = i * H + H // 2
    for px in range(W):
        seg = x[int(px / W * len(x)):int((px + 1) / W * len(x)) + 1]
        if len(seg): dr.line([(px, y0 - seg.max() * H * 0.45), (px, y0 - seg.min() * H * 0.45)], fill=(120, 200, 255))
    dr.text((4, i * H + 2), s_, fill=(255, 255, 255))
    nf, hop = 512, 64
    frames = [np.abs(np.fft.rfft(x[k:k + nf] * np.hanning(nf))) for k in range(0, len(x) - nf, hop)]
    S = 20 * np.log10(np.array(frames).T[:128] + 1e-9)   # 0..12 kHz
    S = np.clip((S + 90) / 80, 0, 1)
    sp = Image.fromarray((np.flipud(S) * 255).astype("uint8")).resize((W, H - 4))
    img.paste(Image.merge("RGB", (sp, sp.point(lambda v: v * 0.6), sp.point(lambda v: 255 - v if v < 40 else v * 0.3))), (W, i * H + 2))
img.save(png)
