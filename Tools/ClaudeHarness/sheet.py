import sys, glob
from PIL import Image
pre, out, n, step = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4])
fs = sorted(glob.glob(pre + "*.png"))[::step][:n]
W, H = 400, 230; cols = 5
o = Image.new("RGB", (W * cols, H * ((len(fs) + cols - 1) // cols)))
for i, f in enumerate(fs):
    im = Image.open(f).convert("RGB"); w, h = im.size
    im = im.crop((int(w * 0.25), int(h * 0.25), int(w * 0.85), int(h * 0.9))).resize((W, H)); o.paste(im, ((i % cols) * W, (i // cols) * H))
o.save(out); print(len(fs))
