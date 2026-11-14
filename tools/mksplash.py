#!/usr/bin/env python3
"""VUKANG OS - tools/mksplash.py

Converts the boot logo picture (assets/splash/logo.jpg) into the raw
layers used by the boot animation (kernel/splash.c):

  assets/splash/emblem.vkimg    the "V" emblem with its glow
  assets/splash/wordmark.vkimg  the "VUKANG" wordmark

Format (.vkimg): "VKIM" | u16 width | u16 height | width*height*3 RGB bytes.
The background is forced to pure black, because the kernel blends the
layers additively (black = transparent).

To change the boot picture in a future version: replace logo.jpg (same
layout: emblem on top, wordmark below, black background), adjust the
crop boxes below if needed, run `python3 tools/mksplash.py`, commit
the regenerated .vkimg files. Requires Pillow (pip install pillow).
"""
import struct, os
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
SRC  = os.path.join(HERE, "..", "assets", "splash", "logo.jpg")
OUT  = os.path.join(HERE, "..", "assets", "splash")

SCALE = 1.4                      # upscale for a 1024x768 screen
LAYERS = {                       # crop boxes in logo.jpg (left, top, right, bottom)
    "emblem":   (196, 40, 508, 238),
    "wordmark": (214, 238, 490, 286),
}
BLACK_LEVEL = 10                 # JPEG noise below this becomes pure black

def build(name, box):
    im = Image.open(SRC).convert("RGB").crop(box)
    im = im.resize((round(im.width * SCALE), round(im.height * SCALE)), Image.LANCZOS)
    px = bytearray()
    raw = im.tobytes()
    for i in range(0, len(raw), 3):
        r, g, b = raw[i], raw[i + 1], raw[i + 2]
        m = max(r, g, b)
        if m <= BLACK_LEVEL:
            r = g = b = 0
        else:                       # remove the black level smoothly
            k = (m - BLACK_LEVEL) / (m * (1 - BLACK_LEVEL / 255))
            r, g, b = (min(255, int(c * k)) for c in (r, g, b))
        px += bytes((r, g, b))
    path = os.path.join(OUT, name + ".vkimg")
    with open(path, "wb") as f:
        f.write(b"VKIM" + struct.pack("<HH", im.width, im.height) + px)
    print(f"  {name:9s} {im.width}x{im.height}  -> {os.path.relpath(path)}")

for n, b in LAYERS.items():
    build(n, b)
