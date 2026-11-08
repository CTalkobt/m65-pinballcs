/* geom.c — trigonometry and the shapes of the parts */
#include "geom.h"
#include "hi.h"

/* the sine table: built from the quarter wave (gen_tables.c) by sin_init */
extern const s16 sin_q[65];
HI s16 sin_t[256];

void sin_init(void) {
    u8 i;
    for (i = 0; i <= 64; i++) {
        sin_t[i] = sin_q[i];
        sin_t[128 - i] = sin_q[i];
        sin_t[(u8)(128 + i)] = (s16)-sin_q[i];
        sin_t[(u8)(256 - i)] = (s16)-sin_q[i];
    }
}


u8 wall_radius(u8 flags) {
    static const u8 r[4] = {2, 3, 4, 6};
    return r[F_THICK(flags)];
}

u8 arc_pieces(const struct part *p) {
    u8 a0 = (u8)p->b, a1 = (u8)(p->b >> 8);
    u8 span = (u8)(a1 - a0);
    u8 step = p->a > 60 ? 4 : 8;              /* big arcs in finer pieces */
    if (span == 0) span = 255;
    return (u8)((span + step - 1) / step);
}

void arc_point(const struct part *p, u8 k, s16 *x, s16 *y) {
    u8 a0 = (u8)p->b, a1 = (u8)(p->b >> 8);
    u8 n = arc_pieces(p);
    u8 span = (u8)(a1 - a0);
    u8 a;
    if (span == 0) span = 255;
    a = (u8)(a0 + (u8)(((u16)span * k + n / 2) / n));
    if (k == n) a = (u8)(a0 + span);
    *x = (s16)(p->x + (s16)((mul32(COS(a), p->a) + 8192) >> 14));
    *y = (s16)(p->y + (s16)((mul32(SIN(a), p->a) + 8192) >> 14));
}

void sling_points(const struct part *p, s16 *pt) {
    s16 w = p->a, aw = w < 0 ? -w : w;
    pt[0] = p->x;
    pt[1] = p->y;
    pt[2] = p->x;
    pt[3] = (s16)(p->y + p->b);
    pt[4] = (s16)(p->x + w);
    pt[5] = (s16)(p->y + p->b + (aw * 3) / 8);
}

u8 flip_rest(const struct part *p) {
    return p->b ? (u8)(128 - 20) : (u8)20;
}

s8 flip_dir(const struct part *p) {
    return p->b ? 1 : -1;
}

void target_ends(const struct part *p, s16 *pt) {
    u8 a = (u8)(p->a + 64);                   /* along the face */
    s16 dx = (s16)((mul32(COS(a), TGT_HALF) + 8192) >> 14);
    s16 dy = (s16)((mul32(SIN(a), TGT_HALF) + 8192) >> 14);
    pt[0] = (s16)(p->x - dx);
    pt[1] = (s16)(p->y - dy);
    pt[2] = (s16)(p->x + dx);
    pt[3] = (s16)(p->y + dy);
}

/* flippers: the tip at drawn position f */
void flip_tip(const struct part *p, u8 f, s16 *tx, s16 *ty) {
    u8 ang = (u8)(flip_rest(p) + flip_dir(p) * (s16)((FLIP_SWING * f + (FLIP_FRAMES - 1) / 2) / (FLIP_FRAMES - 1)));
    *tx = (s16)(p->x + (s16)((mul32(COS(ang), p->a) + 8192) >> 14));
    *ty = (s16)(p->y + (s16)((mul32(SIN(ang), p->a) + 8192) >> 14));
}

/* a flipper's box over all its drawn positions (the tip passes the
 * horizontal between rest and up): x0 any pixel, x1 = x0 + 8 * columns,
 * y0 on a character row, y1 the last pixel of a whole row */
void flipper_box(const struct part *p, s16 *x0, s16 *y0, s16 *x1, s16 *y1) {
    s16 tx, ty, a = p->x, b = p->x, c = p->y, d = p->y;
    u8 f;
    for (f = 0; f < FLIP_FRAMES; f++) {
        flip_tip(p, f, &tx, &ty);
        if (tx < a) a = tx;
        if (tx > b) b = tx;
        if (ty < c) c = ty;
        if (ty > d) d = ty;
    }
    a = (s16)(a - FLIP_RB - 1);
    b = (s16)(b + FLIP_RB + 1);
    c = (s16)((c - FLIP_RB - 1) & ~7);
    d = (s16)(d + FLIP_RB + 1);
    if (a < 0) a = 0;
    if (c < 0) c = 0;
    if (d > TH - 1) d = TH - 1;
    *x0 = a;
    *x1 = (s16)(a + ((b - a + 8) & ~7));
    *y0 = c;
    *y1 = (s16)(c + ((d - c + 8) & ~7) - 1);
    if (*y1 > TH - 1) *y1 = TH - 1;
}


/* the playfield's font: a character's code 32-95, its width */


u8 text_code(u8 c) {
    if (c >= 0x61 && c <= 0x7A) c = (u8)(c - 32);      /* (ASCII: cc45's 'a' is PETSCII $41) */
    return (u8)(c < 32 || c > 95 ? 32 : c);
}

s16 text_width(const char *s, u8 size) {
    s16 w = 0;
    while (*s) w = (s16)(w + (font_w[text_code((u8)*s++) - 32] + 1) * size);
    return w;
}

void part_box(const struct part *p, s16 *x0, s16 *y0, s16 *x1, s16 *y1) {
    s16 r = 0, a = p->x, b = p->y, c = p->x, d = p->y, pt[6];
    u8 sz;
    switch (p->type) {
    case P_WALL:
    case P_GATE:
        if (p->a < a) a = p->a; else c = p->a;
        if (p->b < b) b = p->b; else d = p->b;
        r = (s16)(wall_radius(p->flags) + 3);
        break;
    case P_ARC: r = (s16)(p->a + wall_radius(p->flags) + 1); break;
    case P_BUMPER: case P_POST: r = (s16)(p->a + 1); break;
    case P_SLING:
        sling_points(p, pt);
        if (pt[4] < a) a = pt[4]; else c = pt[4];
        d = pt[5];
        r = 5;
        break;
    case P_FLIPPER: r = (s16)(p->a + FLIP_RB + 1); break;
    case P_TARGET: case P_DROP: r = TGT_HALF + TGT_DEPTH + 4; break;
    case P_ROLLOVER: r = 8; break;
    case P_KICKER: r = 15; break;
    case P_SPINNER: r = (s16)(p->b + 4); break;
    case P_LAMP: r = 12; break;
    case P_TEXT:
        sz = (u8)(p->b >> 8);
        if (!sz) sz = 1;
        c = (s16)(p->x + text_width(tbl.text[p->a & 7], sz) + 2);
        d = (s16)(p->y + 12 * sz + 2);
        r = 1;
        break;
    case P_PLUNGER: r = 6; d = TH; break;
    case P_DECAL:
        a = (s16)(p->x - p->a / 2); c = (s16)(p->x + p->a / 2);
        b = (s16)(p->y - p->b / 2); d = (s16)(p->y + p->b / 2);
        break;
    }
    *x0 = (s16)(a - r);
    *y0 = (s16)(b - r);
    *x1 = (s16)(c + r + 3);
    *y1 = (s16)(d + r + 4);
}
