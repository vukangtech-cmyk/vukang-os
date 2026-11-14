#!/usr/bin/env python3
"""VUKANG OS - tools/mkfont.py

Converts a PSF2 console font (Terminus, SIL Open Font License 1.1) into
assets/font/console.vkfont, a 256-glyph bitmap font indexed by the
classic PC code page 437, so the framebuffer console stays compatible
with the old VGA text mode.

Format: "VKFN" | u8 width | u8 height | u8 bytes_per_row | u8 0 |
        256 * height * bytes_per_row bitmap bytes (MSB = left pixel).
"""
import gzip, struct, sys, os

SRC = sys.argv[1] if len(sys.argv) > 1 else "/usr/share/consolefonts/Uni2-TerminusBold20x10.psf.gz"
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "assets", "font", "console.vkfont")

d = gzip.open(SRC).read()
magic, ver, hsz, flags, n, gsz, h, w = struct.unpack("<8I", d[:32])
assert magic == 0x864AB572, "not a PSF2 font"
glyphs = [d[hsz + i * gsz: hsz + (i + 1) * gsz] for i in range(n)]
umap = {}
if flags & 1:                                    # unicode table
    p = hsz + n * gsz
    for i in range(n):
        seq = b""
        while d[p] != 0xFF:
            seq += bytes([d[p]]); p += 1
        p += 1
        for part in seq.split(b"\xfe")[0:1]:
            for ch in part.decode("utf-8", "ignore"):
                umap.setdefault(ord(ch), i)
bpr = (w + 7) // 8
out = bytearray(b"VKFN" + bytes([w, h, bpr, 0]))
missing = []
for code in range(256):
    ch = bytes([code]).decode("cp437") if code >= 32 else " "
    if code == 0x7F: ch = " "
    g = umap.get(ord(ch), umap.get(ord("?")))
    if ord(ch) not in umap: missing.append(code)
    out += glyphs[g][:h * bpr]
open(OUT, "wb").write(out)
print(f"  font {w}x{h} from {os.path.basename(SRC)}, {len(missing)} glyphs replaced by '?'")
