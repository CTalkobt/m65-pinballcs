#!/usr/bin/env python3
"""gen_assets.py — the art and sounds loaded from disk: PBGFX (bank 1, $12000)

    gen_assets.py <build dir>    writes pbgfx.bin, assets.h, and wav/*.wav

PBGFX holds
  * the score display's glyphs: 16-segment characters, 16 x 24 pixels, in
    full-colour characters (2 columns of 3); lit segments glow in the
    display ramp (PAL_DISP), unlit ones show faintly, as on a real display
  * the sprites: 16-colour, 16 x 16 (a chrome ball, its shadow, the
    plunger's knob, the editor's cursor and handles)
  * the sound samples, 8-bit unsigned at 11025 Hz, made here
"""
import math
import os
import struct
import sys
import wave

import numpy as np
from PIL import Image, ImageDraw

BASE = 0x12000
PAL_DISP = 144
RATE = 11025

# ---------------------------------------------------------------------------
# 16-segment glyphs

# segments: a1 a2 (top halves), b c (right), d1 d2 (bottom halves), e f (left),
# g1 g2 (middle halves), h i (upper diagonals l, r), j (upper centre),
# k l (lower diagonals l, r), m (lower centre); plus dp
SEGS = {
    " ": "", "0": "a1 a2 b c d1 d2 e f i k", "1": "b c i", "2": "a1 a2 b g1 g2 e d1 d2",
    "3": "a1 a2 b c d1 d2 g2", "4": "f g1 g2 b c", "5": "a1 a2 f g1 g2 c d1 d2",
    "6": "a1 a2 f e d1 d2 c g1 g2", "7": "a1 a2 b c", "8": "a1 a2 b c d1 d2 e f g1 g2",
    "9": "a1 a2 b c d1 d2 f g1 g2", "A": "a1 a2 b c e f g1 g2", "B": "a1 a2 b c d1 d2 j m g2",
    "C": "a1 a2 f e d1 d2", "D": "a1 a2 b c d1 d2 j m", "E": "a1 a2 f e d1 d2 g1 g2",
    "F": "a1 a2 f e g1", "G": "a1 a2 f e d1 d2 c g2", "H": "f e b c g1 g2", "I": "a1 a2 j m d1 d2",
    "J": "b c d1 d2 e", "K": "f e g1 i l", "L": "f e d1 d2", "M": "f e b c h i", "N": "f e b c h l",
    "O": "a1 a2 b c d1 d2 e f", "P": "a1 a2 b f e g1 g2", "Q": "a1 a2 b c d1 d2 e f l",
    "R": "a1 a2 b f e g1 g2 l", "S": "a1 a2 f g1 g2 c d1 d2", "T": "a1 a2 j m", "U": "f e b c d1 d2",
    "V": "f e k i", "W": "f e b c k l", "X": "h i k l", "Y": "h i m", "Z": "a1 a2 i k d1 d2",
    "-": "g1 g2", "+": "g1 g2 j m", "*": "g1 g2 j m h i k l", "/": "i k", "=": "g1 g2 d1 d2",
    ".": "dp", ",": "dp k", "!": "j dp", "?": "a1 a2 b g2 m", "'": "j", ":": "j m",
    "<": "i l", ">": "h k", "(": "i l", ")": "h k", "%": "a1 f g1 j m g2 c d2 i k", "$": "a1 a2 f g1 g2 c d1 d2 j m",
    "#": "b c g1 g2 d1 d2 j m", "&": "a1 h j g1 e d1 d2 l",
}
GLYPHS = list(SEGS.keys())
W, H = 16, 24
SS = 4                  # supersampling


def seg_shapes():
    """each segment as a polygon in glyph pixels (before the slant)"""
    x0, x1, xm = 2.5, 13.0, 7.75
    y0, y1, ym = 1.5, 21.0, 11.25
    t = 1.6             # half thickness
    g = 0.45            # the gap at a segment's ends
    def hbar(xa, xb, y):
        return [(xa + g, y), (xa + g + t, y - t), (xb - g - t, y - t), (xb - g, y), (xb - g - t, y + t), (xa + g + t, y + t)]
    def vbar(x, ya, yb):
        return [(x, ya + g), (x + t, ya + g + t), (x + t, yb - g - t), (x, yb - g), (x - t, yb - g - t), (x - t, ya + g + t)]
    def diag(xa, ya, xb, yb):
        dx, dy = xb - xa, yb - ya
        l = math.hypot(dx, dy)
        nx, ny = -dy / l * t * 0.9, dx / l * t * 0.9
        sx, sy = dx / l * 2.2, dy / l * 2.2
        return [(xa + sx + nx, ya + sy + ny), (xb - sx + nx, yb - sy + ny), (xb - sx - nx, yb - sy - ny), (xa + sx - nx, ya + sy - ny)]
    return {
        "a1": hbar(x0, xm, y0), "a2": hbar(xm, x1, y0),
        "d1": hbar(x0, xm, y1), "d2": hbar(xm, x1, y1),
        "g1": hbar(x0, xm, ym), "g2": hbar(xm, x1, ym),
        "f": vbar(x0, y0, ym), "e": vbar(x0, ym, y1),
        "b": vbar(x1, y0, ym), "c": vbar(x1, ym, y1),
        "j": vbar(xm, y0, ym), "m": vbar(xm, ym, y1),
        "h": diag(x0, y0, xm, ym), "i": diag(x1, y0, xm, ym),
        "k": diag(xm, ym, x0, y1), "l": diag(xm, ym, x1, y1),
        "dp": [(14.2, 20), (15.6, 20), (15.6, 22.2), (14.2, 22.2)],
    }


def glyph_image(ch):
    shapes = seg_shapes()
    on = set(SEGS[ch].split())
    lit = Image.new("L", (W * SS, H * SS), 0)
    dim = Image.new("L", (W * SS, H * SS), 0)
    dl, dd = ImageDraw.Draw(lit), ImageDraw.Draw(dim)
    for name, poly in shapes.items():
        pts = [((x + (H / 2 - y) * 0.12) * SS, y * SS) for x, y in poly]     # slanted a little
        (dl if name in on else dd).polygon(pts, fill=255)
    lit = np.asarray(lit.resize((W, H), Image.BOX), dtype=float) / 255
    dim = np.asarray(dim.resize((W, H), Image.BOX), dtype=float) / 255
    # a glow round the lit segments
    glow = np.asarray(Image.fromarray((lit * 255).astype(np.uint8)).resize((W * 2, H * 2)).resize((W, H)), dtype=float) / 255
    k = np.zeros((H, W), dtype=int)
    for y in range(H):
        for x in range(W):
            if lit[y, x] > 0.55:
                k[y, x] = PAL_DISP + int(round(9 + (lit[y, x] - 0.55) * 13))
            elif lit[y, x] > 0.2:
                k[y, x] = PAL_DISP + 6

    return k


def glyph_chars(k):
    """6 characters: column 0 rows 0-2, then column 1"""
    out = bytearray()
    for cx in range(2):
        for cy in range(3):
            for y in range(8):
                for x in range(8):
                    out.append(int(k[cy * 8 + y, cx * 8 + x]))
    return out


# ---------------------------------------------------------------------------
# sprites (16 colours: 8 bytes a row, the left pixel in the high nibble)

def sprite_bytes(px):
    out = bytearray()
    for y in range(16):
        for x in range(0, 16, 2):
            out.append((px[y][x] & 15) << 4 | (px[y][x + 1] & 15))
    return out


def ball():
    """a chrome ball, radius 7.5 centred at (7.5, 7.5): colours 1-15 dark to
    light (the palette is a grey ramp with a little blue)"""
    px = [[0] * 16 for _ in range(16)]
    L = np.array([-0.45, -0.6, 0.66])
    L /= np.linalg.norm(L)
    for y in range(16):
        for x in range(16):
            dx, dy = (x - 7.0) / 7.3, (y - 7.0) / 7.3
            d2 = dx * dx + dy * dy
            if d2 > 1:
                continue
            z = math.sqrt(1 - d2)
            n = np.array([dx, dy, z])
            # reflection of the view (0,0,1): the environment is light above, dark below
            r = 2 * n[2] * n - np.array([0, 0, 1])
            env = 0.55 - 0.4 * r[1]
            spec = max(0.0, float(np.dot(r, L))) ** 24
            diff = max(0.0, float(np.dot(n, L)))
            v = 0.15 + 0.45 * env + 0.25 * diff + 0.9 * spec
            v *= 1.0 - 0.35 * d2 ** 3          # darker at the rim
            px[y][x] = max(1, min(15, int(round(v * 14)) + 1))
    return px


def shadow():
    px = [[0] * 16 for _ in range(16)]
    for y in range(16):
        for x in range(16):
            dx, dy = x - 7.0, y - 7.0
            if dx * dx + dy * dy <= 7.2 ** 2 and (x + y) % 2 == 0:
                px[y][x] = 1
    return px


def knob():
    px = [[0] * 16 for _ in range(16)]
    for y in range(3, 13):
        for x in range(1, 15):
            dx = (x - 7.5) / 7
            v = 0.5 + 0.45 * (1 - abs(dx) ** 2) - 0.2 * (y - 3) / 10 - (0.25 if dx > 0.4 else 0)
            if y in (3, 12):
                v -= 0.25
            px[y][x] = max(1, min(15, int(v * 14) + 1))
    return px


def cursor():
    px = [[0] * 16 for _ in range(16)]
    for i in range(16):
        if i in (6, 7, 8):
            continue
        px[7][i] = 15
        px[i][7] = 15
    for i in (6, 8):
        px[7][i - 0] = 0
    px[7][7] = 12
    return px


def handle():
    px = [[0] * 16 for _ in range(16)]
    for y in range(4, 11):
        for x in range(4, 11):
            edge = x in (4, 10) or y in (4, 10)
            px[y][x] = 15 if edge else (0 if 5 < x < 9 and 5 < y < 9 else 8)
    return px


# ---------------------------------------------------------------------------
# sounds

def t_(sec):
    return np.arange(int(RATE * sec)) / RATE


def env(n, attack=0.001, decay=0.05):
    t = np.arange(n) / RATE
    a = np.clip(t / attack, 0, 1) if attack > 0 else np.ones(n)
    return a * np.exp(-t / decay)


def noise(n, seed=1):
    return np.random.default_rng(seed).uniform(-1, 1, n)


def lowpass(x, fc):
    a = math.exp(-2 * math.pi * fc / RATE)
    y = np.zeros_like(x)
    s = 0.0
    for i, v in enumerate(x):
        s = (1 - a) * v + a * s
        y[i] = s
    return y


def bandpass(x, fc, q=4.0):
    w = 2 * math.pi * fc / RATE
    alpha = math.sin(w) / (2 * q)
    b0, b2 = alpha, -alpha
    a0, a1, a2 = 1 + alpha, -2 * math.cos(w), 1 - alpha
    y = np.zeros_like(x)
    x1 = x2 = y1 = y2 = 0.0
    for i, v in enumerate(x):
        o = (b0 * v + b2 * x2 - a1 * y1 - a2 * y2) / a0
        x2, x1, y2, y1 = x1, v, y1, o
        y[i] = o
    return y


def tone(sec, f0, f1=None, decay=0.05, attack=0.0005):
    t = t_(sec)
    f1 = f0 if f1 is None else f1
    f = f0 + (f1 - f0) * (t / sec)
    ph = 2 * math.pi * np.cumsum(f) / RATE
    return np.sin(ph) * env(len(t), attack, decay)


def ring(sec, partials, decay):
    t = t_(sec)
    out = np.zeros(len(t))
    for f, a in partials:
        out += a * np.sin(2 * math.pi * f * t) * np.exp(-t / (decay * (1200 / f) ** 0.3))
    return out


def click(sec, fc, decay, seed=3, q=2.0):
    n = int(RATE * sec)
    return bandpass(noise(n, seed), fc, q) * env(n, 0.0002, decay)


def mix(*parts):
    n = max(len(p) for p in parts)
    out = np.zeros(n)
    for p in parts:
        out[:len(p)] += p
    return out


def delay(x, sec):
    return np.concatenate([np.zeros(int(RATE * sec)), x])


def finish(x, peak=0.97):
    x = x / (np.max(np.abs(x)) + 1e-9) * peak
    # fade the last 5 ms
    n = min(len(x), int(RATE * 0.005))
    x[-n:] *= np.linspace(1, 0, n)
    return x


def sounds():
    s = {}
    s["flipup"] = finish(mix(click(0.02, 3500, 0.002, 1) * 1.2, tone(0.09, 160, 90, 0.025) * 0.9,
                             ring(0.09, [(1250, 0.25), (2900, 0.15), (4700, 0.08)], 0.02)))
    s["flipdn"] = finish(mix(click(0.015, 2200, 0.002, 2) * 0.8, tone(0.06, 110, 80, 0.018) * 0.7), 0.6)
    s["bumper"] = finish(mix(click(0.02, 3000, 0.003, 4) * 1.4, tone(0.2, 120, 55, 0.05) * 1.2,
                             tone(0.08, 420, 300, 0.02) * 0.4, ring(0.15, [(1700, 0.2), (3300, 0.12)], 0.03)))
    s["sling"] = finish(mix(click(0.02, 4200, 0.0025, 5) * 1.3, tone(0.14, 140, 70, 0.035),
                            tone(0.1, 260, 230, 0.03) * 0.5))
    s["rubber"] = finish(mix(tone(0.07, 180, 120, 0.02) * 0.9, click(0.01, 1200, 0.002, 6) * 0.3), 0.7)
    s["metal"] = finish(mix(ring(0.12, [(2100, 0.5), (3420, 0.35), (5150, 0.2), (1330, 0.2)], 0.035),
                            click(0.006, 5000, 0.001, 7) * 0.5), 0.6)
    s["target"] = finish(mix(click(0.03, 1500, 0.006, 8) * 1.0, tone(0.06, 330, 240, 0.02) * 0.6), 0.85)
    s["drop"] = finish(mix(click(0.02, 2600, 0.003, 9) * 1.1, tone(0.14, 220, 90, 0.04) * 0.8,
                           delay(click(0.03, 1800, 0.004, 10) * 0.4, 0.05)))
    s["kickin"] = finish(mix(tone(0.16, 90, 50, 0.05) * 1.2, click(0.02, 900, 0.004, 11) * 0.6,
                             delay(click(0.04, 2400, 0.005, 12) * 0.3, 0.04)))
    s["kickout"] = finish(mix(click(0.02, 2800, 0.003, 13) * 1.4, tone(0.18, 110, 45, 0.05) * 1.3,
                              ring(0.12, [(900, 0.2), (2300, 0.12)], 0.03)))
    t = t_(0.32)
    spring = np.sin(2 * math.pi * (190 + 25 * np.sin(2 * math.pi * 30 * t)) * t) * np.exp(-t / 0.08)
    s["launch"] = finish(mix(click(0.03, 2000, 0.004, 14), spring * 0.8,
                             ring(0.3, [(1100, 0.15), (2650, 0.1)], 0.06)))
    s["drain"] = finish(mix(tone(0.5, 140, 40, 0.18) * 1.0, click(0.04, 700, 0.01, 15) * 0.5,
                            delay(click(0.05, 1600, 0.01, 16) * 0.3, 0.12), delay(click(0.05, 1300, 0.01, 17) * 0.2, 0.22)))
    s["roll"] = roll_loop()
    s["rollover"] = finish(mix(click(0.012, 3000, 0.0015, 18), delay(click(0.012, 2500, 0.0015, 19) * 0.7, 0.012)), 0.6)
    s["spin"] = finish(mix(click(0.01, 4000, 0.0012, 20), ring(0.03, [(4300, 0.3)], 0.008)), 0.5)
    s["knocker"] = finish(mix(click(0.04, 1500, 0.006, 21) * 1.5, tone(0.3, 70, 40, 0.07) * 1.4,
                              ring(0.25, [(410, 0.4), (860, 0.2)], 0.05)))
    return s


def roll_loop():
    """a ball rolling: low rumble, a loop that joins up"""
    n = int(RATE * 0.36)
    x = noise(n + 2000, 30)
    x = lowpass(lowpass(x, 260), 380)
    x += 0.25 * bandpass(noise(n + 2000, 31), 900, 3)
    x = x[1000:1000 + n]
    # crossfade the ends for a seamless loop
    m = 300
    x[:m] = x[:m] * np.linspace(0, 1, m) + x[-m:] * np.linspace(1, 0, m)
    x = x[:-m]
    return x / (np.max(np.abs(x)) + 1e-9) * 0.9


def to_u8(x):
    return bytes(int(max(0, min(255, round(128 + v * 127)))) for v in x)


# ---------------------------------------------------------------------------

def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "build"
    os.makedirs(out, exist_ok=True)
    blob = bytearray()
    defs = []

    def here():
        return BASE + len(blob)

    def align(n):
        while len(blob) % n:
            blob.append(0)

    # glyphs: their characters, each different one once (many glyphs share
    # quarters: the halves of 0 O D U, blank corners); glyph_cell says which
    align(64)
    defs.append("#define GLYPH_LIN 0x%05XUL" % here())
    defs.append("#define GLYPH_CHAR 0x%X" % (here() // 64))
    uniq, cellmap = [], []
    for ch in GLYPHS:
        data = glyph_chars(glyph_image(ch))
        for k in range(6):
            c = bytes(data[k * 64:(k + 1) * 64])
            if c not in uniq:
                uniq.append(c)
            cellmap.append(uniq.index(c))
    assert len(uniq) < 256
    for c in uniq:
        blob += c
    defs.append("#define NGLYPHS %d" % len(GLYPHS))
    # sprites
    align(64)
    for name, px in (("BALL", ball()), ("SHADOW", shadow()), ("KNOB", knob()), ("CURSOR", cursor()), ("HANDLE", handle())):
        defs.append("#define SPR_%s 0x%X" % (name, here() // 64))
        blob += sprite_bytes(px)
    # sounds
    os.makedirs(os.path.join(out, "wav"), exist_ok=True)
    for name, x in sounds().items():
        data = to_u8(x)
        w = wave.open(os.path.join(out, "wav", name + ".wav"), "wb")
        w.setnchannels(1); w.setsampwidth(1); w.setframerate(RATE); w.writeframes(data); w.close()
        defs.append("#define SND_%s 0x%05XUL" % (name.upper(), here()))
        defs.append("#define SND_%s_LEN %d" % (name.upper(), len(data)))
        blob += data
    defs.append("#define SND_RATE %dUL" % int(RATE * (1 << 24) / 40500000))
    end = here()
    assert end <= 0x20000, "PBGFX ends at $%X" % end
    defs.append("#define GFX_END 0x%05XUL" % end)
    # the glyph of each character 32-95 (unknown: space)
    gmap = [GLYPHS.index(chr(c)) if chr(c) in SEGS else 0 for c in range(32, 96)]
    h = ["/* assets.h — written by tools/gen_assets.py */", "#pragma once"] + defs
    h.append("#ifndef __CC45_ZPCALL__\n#define __noescape\n#endif")
    h.append("static const unsigned char __noescape glyph_of[64] = {%s};" % ", ".join(map(str, gmap)))
    h.append("/* a glyph's characters (from GLYPH_CHAR): column 0 rows 0-2, then column 1 */")
    h.append("static const unsigned char __noescape glyph_cell[%d] = {%s};" % (len(cellmap), ", ".join(map(str, cellmap))))
    open(os.path.join(out, "assets.h"), "w").write("\n".join(h) + "\n")
    open(os.path.join(out, "pbgfx.bin"), "wb").write(struct.pack("<H", BASE & 0xFFFF) + blob)
    # a preview of the glyphs
    img = Image.new("RGB", (W * 20, H * 3))
    for i, ch in enumerate(GLYPHS[:60]):
        k = glyph_image(ch)
        for y in range(H):
            for x in range(W):
                v = k[y, x]
                lv = 0 if v == 0 else v - PAL_DISP
                img.putpixel(((i % 20) * W + x, (i // 20) * H + y), (min(255, lv * 18), min(255, lv * 8), 0))
    img.resize((W * 40, H * 6), Image.NEAREST).save(os.path.join(out, "glyphs.png"))
    print("PBGFX $%05X-$%05X (%d bytes)" % (BASE, end, len(blob)))


if __name__ == "__main__":
    main()
