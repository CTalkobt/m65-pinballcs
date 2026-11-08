#!/usr/bin/env python3
"""gen_tables.py — the core's constant tables: build/gen_tables.c

sin_t      256 x 1.14 sine
hemi_t     255 * sqrt(1 - (i/32)^2), i = 0..32  (a tube's height across it)
hemi2_t    255 * sqrt(1 - i/64), i = 0..64      (a dome's height from r^2)
sqrt8_t    round(8 * sqrt(i)), i < 1024, at most 255
font_w, font_g  the playfield's font: ASCII 32-95, 12 rows, 2 bits a pixel
                (anti-aliased), at most 8 wide; rendered from DejaVu Sans Bold
"""
import math
import os
import sys

from PIL import Image, ImageDraw, ImageFont

FONTS = ["/usr/share/fonts/truetype/dejavu/DejaVuSansCondensed-Bold.ttf",
         "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"]
ROWS = 12


def c_array(ctype, name, vals, per=16):
    out = [("%s %s[%d] = {" if ctype.startswith("static") else "const %s %s[%d] = {") % (ctype, name, len(vals))]
    for i in range(0, len(vals), per):
        out.append("    " + ", ".join(str(v) for v in vals[i:i + per]) + ",")
    out.append("};")
    return "\n".join(out)


def font():
    path = next(p for p in FONTS if os.path.exists(p))
    f = ImageFont.truetype(path, 11)
    widths, glyphs = [], []
    for code in range(32, 96):
        ch = chr(code)
        im = Image.new("L", (16, ROWS + 4), 0)
        d = ImageDraw.Draw(im)
        d.text((1, -1), ch, font=f, fill=255)
        # the glyph's ink columns
        px = im.load()
        cols = [x for x in range(16) if any(px[x, y] > 40 for y in range(ROWS))]
        if ch == " ":
            x0, w = 0, 3
        elif cols:
            x0, w = cols[0], min(8, cols[-1] - cols[0] + 1)
        else:
            x0, w = 0, 3
        rows = []
        for y in range(ROWS):
            v = 0
            for x in range(8):
                a = px[x0 + x, y] if x < w and x0 + x < 16 else 0
                lv = 0 if a < 48 else 1 if a < 120 else 2 if a < 200 else 3
                v = v << 2 | lv
            rows.append(v)
        widths.append(w)
        glyphs += rows
    return widths, glyphs


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "build"
    os.makedirs(out, exist_ok=True)
    sin_t = [int(round(math.sin(i * 2 * math.pi / 256) * 16384)) for i in range(256)]
    sin_t = [max(-16384, min(16384, v)) for v in sin_t]
    hemi = [int(round(255 * math.sqrt(max(0.0, 1 - (i / 32) ** 2)))) for i in range(33)]
    hemi2 = [int(round(255 * math.sqrt(max(0.0, 1 - i / 64)))) for i in range(65)]
    sq = [min(255, int(round(8 * math.sqrt(i)))) for i in range(1024)]
    fw, fg = font()
    src = ["/* gen_tables.c — written by tools/gen_tables.py: what both programs share */",
           '#include "pb.h"',
           c_array("s16", "sin_q", sin_t[:65]),
           c_array("u8", "font_w", fw),
           ""]
    open(os.path.join(out, "gen_tables.c"), "w").write("\n".join(src))
    # the renderer's own: kept with its code (cc45 __noescape)
    hdr = ["/* gen_render.h — written by tools/gen_tables.py: the renderer's tables */",
           "#ifdef GEN_PRIM",
           c_array("static const u8 __noescape", "sqrt8_t", sq, 32),
           c_array("static const u16 __noescape", "hz_t", [h * 11469 >> 8 for h in hemi]),
           "#endif",
           "#ifdef GEN_TEXT",
           c_array("static const u16 __noescape", "font_g", fg, 12),
           "#endif",
           ""]
    open(os.path.join(out, "gen_render.h"), "w").write("\n".join(hdr))


if __name__ == "__main__":
    main()
