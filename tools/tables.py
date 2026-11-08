#!/usr/bin/env python3
"""tables.py — the twelve tables that come with the construction set

    tables.py <out dir>     writes PBT01 .. PBT12 (.tbl) and their names

Every table is made of the same parts a player can place in the editor.
"""
import os
import sys

from tablelib import Table, standard_lower, G_LOCK, G_EXTRA, G_MULT


def starfall():
    t = Table("Starfall", "starfall")
    t.decal(140, 214, 150, 118, "ellipse", 1)
    standard_lower(t)
    t.ctext(140, 186, "STARFALL", "display", 2)
    # three pop bumpers under the top lanes
    t.bumper(104, 112, 15, "sky", 1)
    t.bumper(168, 112, 15, "sky", 1)
    t.bumper(136, 160, 15, "violet", 1)
    # the top lanes: three rollovers between posts
    top_lanes(t, (92, 136, 180), "yellow", 2)
    # a bank of drop targets on the left, standups on the right
    t.drops(30, 160, 4, 0, 22, 0, "red", 3)
    t.target(246, 170, 180, "green", 4)
    t.target(246, 196, 180, "green", 4)
    t.target(246, 222, 180, "green", 4)
    t.kicker(216, 76, 200, "cyan", G_EXTRA)
    t.spinner(56, 96, 0, 10)
    for i in range(5):
        t.lamp(140, 240 + i * 0, "orange", "arrow", 270, G_MULT) if False else None
    t.lamp(110, 250, "orange", "round", 0, G_MULT)
    t.lamp(130, 258, "orange", "round", 0, G_MULT)
    t.lamp(150, 258, "orange", "round", 0, G_MULT)
    t.lamp(170, 250, "orange", "round", 0, G_MULT)
    t.lamp(140, 290, "red", "arrow", 270, G_EXTRA)
    t.lamp(60, 234, "red", "arrow", 250, 3)
    t.lamp(222, 234, "green", "arrow", 290, 4)
    t.ctext(140, 316, "SHOOT AGAIN", "white", 1)
    return t


def orbit_left(t, style="rail"):
    """an inner wall along the arch's left side: a lane round the top"""
    t.arc(152, 150, 124, 196, 250, style, 1)


def orbit_right(t, style="rail"):
    t.arc(152, 150, 124, 290, 334, style, 1)


def top_lanes(t, xs, colour="yellow", group=2, y=62):
    """rollover lanes between rails"""
    for x in xs:
        t.rollover(x, y, colour, group)
    gaps = [xs[0] - 22] + [(a + b) // 2 for a, b in zip(xs, xs[1:])] + [xs[-1] + 22]
    for x in gaps:
        t.wall(x, y - 12, x, y + 14, "rail", 1)


def neon():
    t = Table("Neon Nights", "neon")
    t.decal(140, 236, 150, 36, "box", 1)
    standard_lower(t)
    orbit_left(t)
    orbit_right(t)
    t.ctext(140, 226, "NEON NIGHTS", "display", 1)
    for x, y in ((110, 112), (170, 112), (140, 82), (140, 142)):
        t.bumper(x, y, 13, "magenta", 1)
    t.spinner(44, 150, 0, 10)
    t.spinner(238, 150, 0, 10)
    t.drops(98, 186, 5, 21, 0, 270, "cyan", 3)
    t.kicker(52, 196, 300, "lime", 0, lock=True)
    t.kicker(228, 196, 240, "yellow", 14)
    t.lamp(52, 168, "lime", "arrow", 270, 13)
    t.lamp(228, 168, "yellow", "arrow", 270, 14)
    for i, x in enumerate((112, 126, 140, 154, 168)):
        t.lamp(x, 278, "cyan", "round", 0, 15) if i < 4 else None
    t.lamp(140, 300, "magenta", "arrow", 270, 3)
    t.ctext(140, 322, "SHOOT AGAIN", "white", 1)
    t.lamp(140, 340, "red", "oval", 0, 14)
    return t


def jungle():
    t = Table("Jungle Quest", "jungle")
    t.decal(100, 150, 120, 160, "ellipse", 0)
    standard_lower(t)
    t.ctext(110, 140, "JUNGLE", "accent", 2)
    t.ctext(110, 166, "QUEST", "accent", 2)
    top_lanes(t, (100, 136, 172), "orange", 2)
    t.bumper(96, 96, 14, "green", 1)
    t.bumper(150, 88, 14, "lime", 1)
    t.bumper(124, 132, 14, "green", 1)
    # an upper flipper on the right, under a rubber guide
    t.flipper(244, 196, "right", 40)
    t.wall(276, 152, 244, 196, "rail", 1)
    t.targets = None
    for y in (210, 232):
        t.target(30, y, 0, "orange", 3)
    t.target(30, 188, 0, "orange", 3)
    t.drops(170, 160, 3, 18, 10, 225, "yellow", 4)
    t.kicker(210, 90, 160, "red", 14)
    t.spinner(212, 238, 0, 12)
    t.lamp(118, 230, "orange", "arrow", 200, 3)
    t.lamp(160, 230, "yellow", "arrow", 330, 4)
    for x in (116, 132, 148, 164):
        t.lamp(x, 268, "lime", "round", 0, 15)
    return t


def deep():
    t = Table("Deep Blue", "deep")
    t.decal(140, 120, 200, 120, "ellipse", 1)
    standard_lower(t)
    orbit_right(t)
    t.ctext(140, 62, "DEEP BLUE", "white", 2)
    t.bumper(104, 116, 15, "cyan", 1)
    t.bumper(176, 116, 15, "cyan", 1)
    t.bumper(140, 160, 15, "sky", 1)
    t.drops(36, 140, 4, 6, 22, 345, "blue", 3)
    t.kicker(220, 196, 225, "teal", 0, lock=True)
    t.spinner(140, 222, 0, 13)
    t.target(246, 236, 180, "yellow", 4)
    t.target(246, 214, 180, "yellow", 4)
    t.rollover(90, 214, "sky", 2)
    t.rollover(190, 214, "sky", 2)
    t.lamp(140, 252, "teal", "arrow", 270, 13)
    for x in (112, 126, 154, 168):
        t.lamp(x, 272, "cyan", "round", 0, 15)
    t.lamp(140, 290, "yellow", "tri", 270, 14)
    return t


def inferno():
    t = Table("Inferno", "inferno")
    standard_lower(t)
    orbit_left(t)
    t.ctext(140, 226, "INFERNO", "display", 2)
    for x, y in ((92, 96), (140, 80), (188, 96), (116, 136), (164, 136)):
        t.bumper(x, y, 13, "orange", 1)
    t.flipper(36, 196, "left", 38)
    t.wall(4, 152, 36, 196, "rail", 1)
    t.target(246, 150, 180, "yellow", 3)
    t.target(246, 172, 180, "yellow", 3)
    t.target(246, 194, 180, "yellow", 3)
    t.drops(110, 186, 4, 20, 0, 270, "red", 4)
    t.kicker(220, 64, 170, "amber", 14)
    t.lamp(140, 296, "orange", "arrow", 270, 4)
    t.lamp(220, 222, "yellow", "arrow", 290, 3)
    for x in (110, 125, 155, 170):
        t.lamp(x, 270, "amber", "round", 0, 15)
    return t


def ice():
    t = Table("Ice Palace", "ice")
    t.decal(140, 160, 230, 110, "box", 0)
    standard_lower(t)
    t.ctext(140, 150, "ICE PALACE", "flipper", 2)
    top_lanes(t, (96, 128, 160, 192), "sky", 2)
    t.bumper(112, 100, 13, "white", 1)
    t.bumper(168, 100, 13, "white", 1)
    t.kicker(40, 90, 30, "cyan", 14)
    t.kicker(238, 110, 150, "blue", 0, lock=True)
    t.drops(50, 196, 3, 16, 0, 315, "sky", 3)
    t.drops(198, 196, 3, 16, 0, 225, "sky", 4)
    t.spinner(140, 216, 0, 12)
    t.lamp(140, 244, "white", "arrow", 270, 13)
    for x in (110, 125, 155, 170):
        t.lamp(x, 268, "cyan", "round", 0, 15)
    t.lamp(140, 290, "blue", "oval", 0, 14)
    return t


def gold():
    t = Table("Gold Rush", "gold")
    standard_lower(t)
    orbit_left(t, "wood")
    orbit_right(t, "wood")
    t.ctext(140, 172, "GOLD RUSH", "flipper", 2)
    t.spinner(44, 150, 0, 10)
    t.spinner(236, 150, 0, 10)
    t.spinner(140, 140, 0, 12)
    t.bumper(100, 96, 14, "gold", 1)
    t.bumper(180, 96, 14, "gold", 1)
    t.drops(80, 214, 3, 16, 8, 300, "amber", 3)
    t.drops(168, 230, 3, 16, -8, 240, "amber", 4)
    t.kicker(140, 74, 60, "red", 14)
    t.target(60, 120, 0, "red", 5)
    t.target(220, 120, 180, "red", 5)
    for x in (110, 125, 155, 170):
        t.lamp(x, 264, "gold", "round", 0, 15)
    t.lamp(140, 290, "red", "arrow", 270, 14)
    return t


def robot():
    t = Table("Robot Factory", "robot")
    t.decal(140, 236, 190, 40, "box", 3)
    standard_lower(t)
    t.ctext(140, 226, "ROBOT FACTORY", "display", 1)
    top_lanes(t, (110, 146, 182), "red", 2)
    t.drops(40, 120, 4, 0, 20, 0, "orange", 3)
    t.drops(240, 120, 4, 0, 20, 180, "orange", 4)
    t.bumper(110, 110, 13, "yellow", 1)
    t.bumper(170, 110, 13, "yellow", 1)
    t.kicker(140, 160, 270, "red", 0, lock=True)
    t.post(140, 200, 5)
    t.target(96, 196, 300, "green", 5)
    t.target(184, 196, 240, "green", 5)
    t.lamp(140, 262, "red", "arrow", 270, 13)
    for x in (110, 125, 155, 170):
        t.lamp(x, 284, "yellow", "round", 0, 15)
    t.lamp(140, 306, "green", "oval", 0, 14)
    return t


def sugar():
    t = Table("Sugar Rush", "sugar")
    standard_lower(t)
    t.ctext(140, 250, "SUGAR RUSH", "accent", 2)
    for i, (x, y) in enumerate(((96, 92), (140, 76), (184, 92))):
        t.bumper(x, y, 13, ["pink", "magenta", "pink"][i], 1)
    for row in range(3):
        for k in range(5 - (row & 1)):
            x = 72 + k * 34 + (17 if row & 1 else 0)
            t.post(x, 140 + row * 26, 4)
    t.kicker(140, 228, 70, "violet", 14)
    t.target(30, 180, 0, "purple", 3)
    t.target(30, 204, 0, "purple", 3)
    t.target(250, 180, 180, "purple", 3)
    t.target(250, 204, 180, "purple", 3)
    for x in (110, 125, 155, 170):
        t.lamp(x, 280, "pink", "round", 0, 15)
    t.lamp(140, 300, "violet", "tri", 270, 14)
    return t


def haunted():
    t = Table("Haunted Manor", "haunted")
    t.decal(140, 110, 150, 90, "ellipse", 1)
    standard_lower(t)
    orbit_left(t)
    t.ctext(140, 96, "HAUNTED", "flipper", 2)
    t.ctext(140, 122, "MANOR", "flipper", 2)
    t.bumper(100, 166, 13, "purple", 1)
    t.bumper(156, 160, 13, "violet", 1)
    t.bumper(128, 196, 13, "purple", 1)
    t.kicker(220, 92, 200, "lime", 0, lock=True)
    t.spinner(44, 150, 0, 10)
    t.target(246, 160, 180, "lime", 3)
    t.target(246, 182, 180, "lime", 3)
    t.target(246, 204, 180, "lime", 3)
    t.drops(186, 228, 3, 16, -6, 250, "violet", 4)
    t.lamp(220, 124, "lime", "arrow", 270, 13)
    for x in (110, 125, 155, 170):
        t.lamp(x, 262, "lime", "round", 0, 15)
    t.lamp(140, 284, "purple", "oval", 0, 14)
    return t


def dragon():
    t = Table("Dragon Peak", "dragon")
    standard_lower(t)
    t.ctext(140, 50, "DRAGON PEAK", "accent", 2)
    t.flipper(32, 206, "left", 36)
    t.flipper(248, 206, "right", 36)
    t.wall(4, 168, 32, 206, "rail", 1)
    t.wall(276, 168, 248, 206, "rail", 1)
    t.bumper(104, 92, 14, "red", 1)
    t.bumper(176, 92, 14, "red", 1)
    t.bumper(140, 128, 14, "orange", 1)
    t.kicker(140, 196, 270, "green", 0, lock=True)
    t.drops(62, 112, 3, 0, 20, 330, "gold", 3)
    t.drops(218, 112, 3, 0, 20, 210, "gold", 4)
    t.rollover(100, 156, "green", 2)
    t.rollover(180, 156, "green", 2)
    t.lamp(140, 228, "green", "arrow", 270, 13)
    for x in (110, 125, 155, 170):
        t.lamp(x, 262, "orange", "round", 0, 15)
    t.lamp(140, 284, "gold", "tri", 270, 14)
    return t


def classic():
    t = Table("Classic 77", "classic", balls=5)
    t.decal(140, 230, 160, 60, "box", 0)
    standard_lower(t)
    t.ctext(140, 214, "CLASSIC", "flipper", 2)
    top_lanes(t, (92, 128, 164, 200), "orange", 2)
    for x, y in ((100, 104), (180, 104), (140, 136)):
        t.bumper(x, y, 15, "red", 1)
    t.kicker(36, 120, 330, "yellow", 14)
    t.kicker(244, 120, 210, "yellow", 0)
    t.target(60, 180, 315, "red", 3)
    t.target(220, 180, 225, "red", 3)
    t.post(140, 186, 5)
    for x in (110, 125, 155, 170):
        t.lamp(x, 266, "amber", "round", 0, 15)
    t.lamp(140, 290, "red", "arrow", 270, 14)
    t.ctext(140, 304, "SAME PLAYER SHOOTS AGAIN", "accent", 1) if False else t.ctext(140, 304, "SHOOT AGAIN", "accent", 1)
    return t


TABLES = [starfall, neon, jungle, deep, inferno, ice, gold, robot, sugar, haunted, dragon, classic]


def template():
    t = Table("New table", "starfall")
    standard_lower(t)
    return t


def main():
    import struct
    out = sys.argv[1] if len(sys.argv) > 1 else "build"
    os.makedirs(out, exist_ok=True)
    names = []
    for i, make in enumerate([template] + TABLES):
        t = make()
        # as a PRG: two bytes of load address, which the program's LOAD skips
        open(os.path.join(out, "pbt%02d.prg" % i), "wb").write(b"\0\0" + t.bytes())
        open(os.path.join(out, "pbt%02d.tbl" % i), "wb").write(t.bytes())
        if i:
            names.append(t.name)
            print("PBT%02d %-16s %3d parts, %d bytes" % (i, t.name, len(t.parts), len(t.bytes())))
    idx = bytearray([0xB1])
    for i in range(24):
        if i < len(names):
            idx += names[i][:16].ljust(16).encode("ascii") + struct.pack("<I", 0) + b"---" + b"\1"
        else:
            idx += b" " * 16 + struct.pack("<I", 0) + b"---" + b"\0"
    open(os.path.join(out, "pbindex.prg"), "wb").write(b"\0\0" + bytes(idx))
    open(os.path.join(out, "tables.list"), "w").write("\n".join("pbt%02d" % i for i in range(len(names) + 1)) + "\n")


if __name__ == "__main__":
    main()
