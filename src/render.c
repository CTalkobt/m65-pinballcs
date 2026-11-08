/* render.c — the table's art, drawn from its parts a strip at a time
 *
 * A strip is 8 pixels wide and the whole table high; every primitive clips
 * itself to it, so a part costs about its own pixels however many strips
 * it crosses. Nearly everything is a "blob": the pixels within r of a
 * skeleton, which is a segment (a rail, a rubber band; a dome when its
 * ends meet); rings are a dome over a dome. A blob is shaded from the
 * light at the top left as the round thing it is: the height across it
 * (hemi_t) and the slope toward the light. A flat blob is a lamp insert or
 * a decal. Plastics are convex polygons with a bevel. Raised parts cast a
 * shadow down and to the right, darkening what is under it (the
 * playfield's ramps have shadow twins, theme.c).
 */
#include "render.h"
#include "hi.h"
#include "rint.h"
#include "geom.h"
#include "theme.h"

HI u8 render_buf[STRIP_BYTES];
static HI s16 bx0[MAX_PARTS], bx1[MAX_PARTS];

#define DOME(x, y, r, ramp) blob((x), (y), (x), (y), (r), (ramp), 0)
#define TUBE(ax, ay, bx, by, r, ramp) blob((ax), (ay), (bx), (by), (r), (ramp), 0)
void sling(const struct part *p, u8 slot);
void spinner(const struct part *p, u8 slot);
void lamp(const struct part *p, u8 slot);
#define LAMP(x, y, r, slot) blob((x), (y), (x), (y), (r), (slot), B_FLAT)

/* ---------------------------------------------------------------- */
/* the parts */

static u8 style_ramp(u8 flags) {
    u8 s = F_STYLE(flags);
    return s == WS_RUBBER ? PAL_RUBBER : s == WS_PLASTIC ? PAL_ACCENT : PAL_WALL;
}

static void arc(const struct part *p) {
    u8 n = arc_pieces(p), k, r = wall_radius(p->flags), ramp = style_ramp(p->flags);
    s16 x0, y0, x1, y1;
    arc_point(p, 0, &x0, &y0);
    for (k = 1; k <= n; k++) {
        arc_point(p, k, &x1, &y1);
        blob(x0, y0, x1, y1, r, ramp, F_STYLE(p->flags) == WS_WOOD ? B_DULL : 0);
        x0 = x1;
        y0 = y1;
    }
}

static void bumper(const struct part *p, u8 slot) {
    u8 r = (u8)p->a;
    if (shadow) {
        DOME(p->x, p->y, (u8)(r + 1), 0);
        return;
    }
    DOME(p->x, p->y, r, PAL_RUBBER);
    DOME(p->x, p->y, (u8)(r - 3), PAL_ACCENT);
    DOME(p->x, p->y, (u8)(r - 5), SLOT(slot));
    DOME((s16)(p->x - 1), (s16)(p->y - 1), 2, PAL_WALL);
}


/* a target: its face a box between front and -depth along its facing */
static void target_box(const struct part *p, s16 *q, s16 depth, s16 front) {
    s16 e[4], k;
    target_ends(p, e);
    for (k = 0; k < 4; k++) {
        s16 dd = k < 2 ? front : (s16)-depth;
        u8 i = (u8)(k == 0 || k == 3 ? 0 : 2);
        q[k * 2] = (s16)(e[i] + (s16)((mul32(COS(p->a), dd) + 8192) >> 14));
        q[k * 2 + 1] = (s16)(e[i + 1] + (s16)((mul32(SIN(p->a), dd) + 8192) >> 14));
    }
}

static void target(const struct part *p, u8 slot, u8 down) {
    s16 q[8];
    if (down) {
        if (shadow) return;
        target_box(p, q, 2, 1);
        poly(q, 4, PAL_WALL, 1);
        return;
    }
    target_box(p, q, TGT_DEPTH + 3, 1);
    if (shadow) {
        poly(q, 4, 0, 0);
        return;
    }
    poly(q, 4, PAL_WALL, 1);
    target_box(p, q, TGT_DEPTH, 1);
    poly(q, 4, SLOT(slot), 0);
}



static void text_part(const struct part *p) {
    static const u8 __noescape inks[5] = {PAL_ACCENT, PAL_RUBBER, PAL_DISP, PAL_WALL, PAL_FLIP};
    u8 sz = (u8)(p->b >> 8);
    text(p->x, p->y, tbl.text[p->a & 7], inks[(u8)p->b < 5 ? (u8)p->b : 0], sz ? sz : 1);
}

static void decal_part(const struct part *p) {
    u8 sh = (u8)((p->flags >> 5) & 3), band = (u8)(p->y * 15 / (TH - 1)), c;
    s16 hw = (s16)(p->a / 2), hh = (s16)(p->b / 2), r = hw < hh ? hw : hh;
    switch (sh) {
    case 0: c = (u8)(PAL_PAT + band); b_edge = (u8)(PAL_PATS + band); break;
    case 1: c = (u8)(PAL_BGS + band); b_edge = (u8)(PAL_PAT + band); break;
    case 2: c = PAL_ACCENT + 4; b_edge = PAL_ACCENT + 9; break;
    default: c = PAL_WALL + 3; b_edge = PAL_WALL + 8; break;
    }
    /* a stadium: a fat segment along its long side */
    blob((s16)(p->x - hw + r), (s16)(p->y - hh + r), (s16)(p->x + hw - r), (s16)(p->y + hh - r), (u8)r, c, B_DECAL);
}

static void kicker(const struct part *p, u8 slot) {
    DOME(p->x, p->y, 14, PAL_WALL);
    LAMP(p->x, p->y, 11, SLOT(slot));
    DOME(p->x, p->y, 9, PAL_WALL);
    b_edge = PAL_WALL + 2;
    blob(p->x, p->y, p->x, p->y, 6, PAL_WALL, B_DECAL);
    DOME(p->x, p->y, 3, PAL_WALL + 0);
}

/* ---------------------------------------------------------------- */

void render_begin(void) {
    u8 i;
    s16 y0, y1;
    slots_assign();
    for (i = 0; i < tbl.h.nparts; i++) part_box(&tbl.p[i], &bx0[i], &y0, &bx1[i], &y1);
}

void render_strip(u8 c, u8 drops) {
    u8 i, pass;
    sx = (s16)(c * 8);
    shadow = 0;
    playfield();
    for (pass = 0; pass < 3; pass++) {
        shadow = (u8)(pass == 1);
        for (i = 0; i < tbl.h.nparts; i++) {
            const struct part *p = &tbl.p[i];
            u8 sl = part_slot[i], t = p->type;
            if (bx1[i] < sx || bx0[i] > sx + 7) continue;
            if (pass == 0) {
                switch (t) {
                case P_DECAL: decal_part(p); break;
                case P_TEXT: text_part(p); break;
                case P_LAMP: lamp(p, sl); break;
                case P_ROLLOVER:
                    DOME(p->x, p->y, 7, PAL_WALL);
                    LAMP(p->x, p->y, 5, SLOT(sl));
                    TUBE(p->x, (s16)(p->y - 4), p->x, (s16)(p->y + 4), 1, PAL_WALL);
                    break;
                case P_KICKER: kicker(p, sl); break;
                }
                continue;
            }
            switch (t) {
            case P_WALL:
                blob(p->x, p->y, p->a, p->b, wall_radius(p->flags), style_ramp(p->flags),
                     F_STYLE(p->flags) == WS_WOOD ? B_DULL : 0);
                break;
            case P_ARC: arc(p); break;
            case P_BUMPER: bumper(p, sl); break;
            case P_POST:
                if (F_STYLE(p->flags) == WS_RAIL || shadow) DOME(p->x, p->y, (u8)p->a, PAL_WALL);
                else {
                    DOME(p->x, p->y, (u8)p->a, PAL_RUBBER);
                    DOME(p->x, p->y, (u8)(p->a > 3 ? p->a - 3 : 1), PAL_WALL);
                }
                break;
            case P_SLING: sling(p, sl); break;
            case P_TARGET: target(p, sl, 0); break;
            case P_DROP: target(p, sl, drops); break;
            case P_SPINNER: spinner(p, sl); break;
            case P_GATE:
                TUBE(p->x, p->y, p->a, p->b, 1, PAL_WALL);
                DOME(p->x, p->y, 3, PAL_WALL);
                DOME(p->a, p->b, 3, PAL_WALL);
                break;
            case P_PLUNGER: {
                s16 y;
                TUBE(p->x, (s16)(p->y + 8), p->x, TH + 4, 2, PAL_WALL);
                if (!shadow)
                    for (y = (s16)(p->y + 14); y < TH - 4; y += 4) TUBE((s16)(p->x - 5), y, (s16)(p->x + 5), (s16)(y + 2), 1, PAL_WALL);
                break;
            }
            }
        }
    }
    shadow = 0;
}

/* ---------------------------------------------------------------- */
/* flippers: a tapered tube, rubber round its edge */

