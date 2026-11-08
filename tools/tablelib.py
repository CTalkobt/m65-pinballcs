"""tablelib.py — building tables in Python: the construction set's file format

A table is written as the MEGA65 program keeps it (src/table.h): a 32-byte
header, eight texts of 16, parts of 10 bytes. Angles here are degrees
(0 along +x, 90 down the screen); the file has 256ths of a turn.
"""
import struct

P = dict(wall=1, arc=2, bumper=3, post=4, sling=5, flipper=6, target=7, drop=8,
         rollover=9, kicker=10, spinner=11, lamp=12, text=13, gate=14, plunger=15, decal=16)
STYLES = dict(rail=0, rubber=1, plastic=2, wood=3)
COLOURS = ["red", "orange", "yellow", "lime", "green", "teal", "cyan", "sky", "blue",
           "violet", "purple", "magenta", "pink", "white", "amber", "gold"]
THEMES = ["starfall", "neon", "jungle", "deep", "inferno", "ice", "gold", "robot",
          "sugar", "haunted", "dragon", "classic"]
SHAPES = dict(round=0, arrow=1, tri=2, oval=3)
G_LOCK, G_EXTRA, G_MULT = 13, 14, 15
TEXT_INK = dict(accent=0, white=1, display=2, metal=3, flipper=4)


def ang(deg):
    return int(round(deg * 256 / 360)) & 255


def col(c):
    return COLOURS.index(c) if isinstance(c, str) else c


class Table:
    def __init__(self, name, theme, balls=3):
        self.name = name.upper()
        self.theme = THEMES.index(theme)
        self.balls = balls
        self.parts = []
        self.texts = []

    def _add(self, t, flags, x, y, a=0, b=0):
        self.parts.append((P[t], flags & 255, int(round(x)), int(round(y)), int(a), int(b)))

    @staticmethod
    def _wf(style, thick):
        return STYLES[style] << 4 | thick << 6

    def wall(self, x1, y1, x2, y2, style="rail", thick=1):
        self._add("wall", self._wf(style, thick), x1, y1, x2, y2)

    def chain(self, pts, style="rail", thick=1, closed=False):
        for i in range(len(pts) - 1 + (1 if closed else 0)):
            a, b = pts[i], pts[(i + 1) % len(pts)]
            self.wall(a[0], a[1], b[0], b[1], style, thick)

    def arc(self, cx, cy, r, a0, a1, style="rail", thick=1):
        """clockwise (on the screen) from a0 to a1 degrees"""
        self._add("arc", self._wf(style, thick), cx, cy, r, ang(a0) | ang(a1) << 8)

    def bumper(self, x, y, r=14, colour="red", group=0):
        self._add("bumper", group, x, y, r, col(colour))

    def post(self, x, y, r=4, style="rubber"):
        self._add("post", STYLES[style] << 4, x, y, r, 0)

    def sling(self, x, y, w, h, group=0):
        self._add("sling", group, x, y, w, h)

    def flipper(self, x, y, side, length=46):
        self._add("flipper", 0, x, y, length, 1 if side == "right" else 0)

    def target(self, x, y, facing, colour="yellow", group=0):
        self._add("target", group, x, y, ang(facing), col(colour))

    def drop(self, x, y, facing, colour="red", group=0):
        self._add("drop", group, x, y, ang(facing), col(colour))

    def drops(self, x, y, n, dx, dy, facing, colour="red", group=1):
        for i in range(n):
            self.drop(x + i * dx, y + i * dy, facing, colour, group)

    def rollover(self, x, y, colour="yellow", group=0):
        self._add("rollover", group, x, y, 0, col(colour))

    def kicker(self, x, y, eject, colour="green", group=0, lock=False):
        self._add("kicker", group | (0x10 if lock else 0), x, y, ang(eject), col(colour))

    def spinner(self, x, y, axle, half=11):
        self._add("spinner", 0, x, y, ang(axle), half)

    def lamp(self, x, y, colour="yellow", shape="round", angle=0, group=0):
        self._add("lamp", group, x, y, SHAPES[shape] | ang(angle) << 8, col(colour))

    def text(self, x, y, s, ink="accent", size=1):
        s = s.upper()[:15]
        if s in self.texts:
            i = self.texts.index(s)
        else:
            assert len(self.texts) < 8, "eight texts at most"
            self.texts.append(s)
            i = len(self.texts) - 1
        self._add("text", 0, x, y, i, TEXT_INK[ink] | size << 8)

    def ctext(self, cx, y, s, ink="accent", size=1):
        """text centred on cx (the width from the font's metrics)"""
        self.text(cx - text_width(s, size) // 2, y, s, ink, size)

    def gate(self, x1, y1, x2, y2):
        self._add("gate", 0, x1, y1, x2, y2)

    def plunger(self, x, y):
        self._add("plunger", 0, x, y)

    def decal(self, x, y, w, h, shape="box", shade=0):
        self._add("decal", (1 if shape == "ellipse" else 0) << 4 | shade << 5, x, y, w, h)

    def bytes(self):
        assert len(self.parts) <= 160, "%s: %d parts" % (self.name, len(self.parts))
        name = self.name[:16].ljust(16).encode("ascii")
        hdr = struct.pack("<2sBB16sBBB9s", b"PB", 1, self.theme, name, self.balls, 1, len(self.parts), bytes(9))
        texts = b"".join(t.encode("ascii")[:15].ljust(16, b"\0") for t in self.texts)
        texts = texts.ljust(128, b"\0")
        parts = b"".join(struct.pack("<BBHHHH", p[0], p[1], *[v & 0xFFFF for v in p[2:]]) for p in self.parts)
        return hdr + texts + parts


_FONT_W = None


def text_width(s, size=1):
    global _FONT_W
    if _FONT_W is None:
        import re, os
        src = open(os.path.join(os.path.dirname(__file__), "..", "build", "gen_tables.c")).read()
        _FONT_W = [int(v) for v in re.search(r"font_w\[64\] = \{(.*?)\};", src, re.S).group(1).replace("\n", "").split(",") if v.strip()]
    w = 0
    for ch in s.upper():
        c = ord(ch)
        if c < 32 or c > 95:
            c = 32
        w += (_FONT_W[c - 32] + 1) * size
    return w


def standard_lower(t, sling_group=0, inlane_colour="yellow", lanes=True):
    """the frame most tables share: the outer walls, the top arch, the
    shooter lane on the right, slingshots, inlanes and outlanes, two
    flippers (pivots 96 and 184 at y 352)"""
    t.arc(152, 150, 148, 180, 0, "rail", 2)               # the top arch
    t.wall(4, 150, 4, 336, "rail", 2)                     # the left side
    t.wall(4, 336, 24, 399, "rail", 2)                    # down into the left outlane
    t.wall(300, 150, 300, 399, "rail", 2)                 # the right side (the shooter lane's)
    t.wall(276, 132, 276, 399, "rail", 2)                 # the shooter lane's inner wall
    t.gate(276, 132, 300, 118)                            # the lane's exit: one way
    t.plunger(288, 372)
    # outlanes (x 10-27), dividers (x 30) with posts, inlanes (x 33-49)
    # under the slingshots, guides down to the flippers
    t.chain([(30, 284), (30, 322), (92, 343)], "rail", 1)
    t.chain([(250, 284), (250, 322), (188, 343)], "rail", 1)
    t.post(30, 278, 4)
    t.post(250, 278, 4)
    t.wall(276, 336, 256, 399, "rail", 2)                 # the right outlane's floor
    t.sling(52, 262, 26, 44, sling_group)
    t.sling(228, 262, -26, 44, sling_group)
    t.flipper(96, 352, "left")
    t.flipper(184, 352, "right")
    if lanes:
        t.rollover(18, 300, inlane_colour, 0)             # the outlanes' and inlanes' rollovers
        t.rollover(262, 300, inlane_colour, 0)
        t.rollover(41, 300, inlane_colour, 0)
        t.rollover(239, 300, inlane_colour, 0)
