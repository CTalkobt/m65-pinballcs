#!/usr/bin/env python3
"""sheet.py — the tables' previews (PPM) as PNGs and one contact sheet"""
import glob
import os
import sys

from PIL import Image

d = sys.argv[1] if len(sys.argv) > 1 else "build/previews"
files = sorted(glob.glob(os.path.join(d, "pbt*.ppm")))
files = [f for f in files if not f.endswith("pbt00.ppm")] or files
ims = [Image.open(f) for f in files]
for f, im in zip(files, ims):
    im.save(f[:-4] + ".png")
cols = 6
w, h = ims[0].size
sheet = Image.new("RGB", (cols * w, ((len(ims) + cols - 1) // cols) * h))
for i, im in enumerate(ims):
    sheet.paste(im, ((i % cols) * w, (i // cols) * h))
sheet.save(os.path.join(d, "sheet.png"))
print(os.path.join(d, "sheet.png"))
