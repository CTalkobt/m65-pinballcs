/* rpoly.c — the renderer's polygons: plastics, targets, lamp inserts */
#include "render.h"
#include "rint.h"
#include "geom.h"
#include "theme.h"

/* a convex polygon (n <= 4), flat at the ramp's body level with a bevel of
 * width bev lit by its edges' facing; a lamp slot has a rim and a glint */
void poly(const s16 *pt, u8 n, u8 ramp, u8 bev) {
    s16 ea[4], eb[4], ex[4], ey[4], x, y, x0 = 999, y0 = 999, x1 = -999, y1 = -999, ox = 0, oy = 0;
    u8 elev[4], i, j, best, body = level(LZ + 2000);
    s32 area = 0, d[4], dmin;
    if (shadow) { ox = SH_DX; oy = SH_DY; }
    for (i = 0; i < n; i++) {
        j = (u8)(i + 1 == n ? 0 : i + 1);
        area += mul32(pt[i * 2], pt[j * 2 + 1]) - mul32(pt[j * 2], pt[i * 2 + 1]);
        if (pt[i * 2] < x0) x0 = pt[i * 2];
        if (pt[i * 2] > x1) x1 = pt[i * 2];
        if (pt[i * 2 + 1] < y0) y0 = pt[i * 2 + 1];
        if (pt[i * 2 + 1] > y1) y1 = pt[i * 2 + 1];
    }
    if (!clip(x0, y0, x1, y1)) return;
    for (i = 0; i < n; i++) {
        s16 dx, dy, len;
        j = (u8)(i + 1 == n ? 0 : i + 1);
        dx = (s16)(pt[j * 2] - pt[i * 2]);
        dy = (s16)(pt[j * 2 + 1] - pt[i * 2 + 1]);
        len = (s16)isqrt32((u32)(sq2(dx, dy)));
        if (!len) len = 1;
        if (area < 0) { dx = (s16)-dx; dy = (s16)-dy; }
        ea[i] = (s16)(mul32(-dy, ANG_UNIT) / len);     /* the inward normal */
        eb[i] = (s16)(mul32(dx, ANG_UNIT) / len);
        ex[i] = (s16)(pt[i * 2] + ox);
        ey[i] = (s16)(pt[i * 2 + 1] + oy);
        elev[i] = level(((-fdot(ea[i], LX, eb[i], LY)) * 11 >> 4) + (mul32(LZ, 11) >> 4));
    }
    for (y = cy0; y <= cy1; y++) {
        for (i = 0; i < n; i++) d[i] = mul32(cx0 - ex[i], ea[i]) + mul32(y - ey[i], eb[i]);
        for (x = cx0; x <= cx1; x++) {
            dmin = 0x7FFFFFFFL;
            best = 0;
            for (i = 0; i < n; i++) {
                if (d[i] < -8192) break;
                if (d[i] < dmin) { dmin = d[i]; best = i; }
            }
            if (i == n) {
                if (shadow) darken(x, y);
                else if (ramp >= PAL_LAMP)
                    AT(x, y) = (u8)(ramp + (dmin < 16384 ? 0 : (dmin < 49152L && elev[best] > 9) ? 2 : 1));
                else
                    AT(x, y) = (u8)(ramp + (dmin < ((s32)bev << 14) ? elev[best] : body));
            }
            for (i = 0; i < n; i++) d[i] += ea[i];
        }
    }
}

/* points (x, y) + the turn of (lx, ly) by angle a: n of them */
void turned(s16 *q, s16 x, s16 y, u8 a, const s8 *v, u8 n) {
    s16 c = COS(a), s = SIN(a);
    u8 i;
    for (i = 0; i < n; i++) {
        q[i * 2] = (s16)(x + (s16)((mul32(c, v[i * 2]) - mul32(s, v[i * 2 + 1])) >> 14));
        q[i * 2 + 1] = (s16)(y + fdot(s, v[i * 2], c, v[i * 2 + 1]));
    }
}

#define DOME(x, y, r, ramp) blob((x), (y), (x), (y), (r), (ramp), 0)
#define TUBE(ax, ay, bx, by, r, ramp) blob((ax), (ay), (bx), (by), (r), (ramp), 0)

void sling(const struct part *p, u8 slot) {
    s16 pt[6], in[6], cxm, cym;
    u8 i;
    sling_points(p, pt);
    if (shadow) {
        poly(pt, 3, 0, 0);
        return;
    }
    cxm = (s16)((pt[0] + pt[2] + pt[4]) / 3);
    cym = (s16)((pt[1] + pt[3] + pt[5]) / 3);
    for (i = 0; i < 6; i += 2) {
        in[i] = (s16)(pt[i] + (cxm - pt[i]) / 5);
        in[i + 1] = (s16)(pt[i + 1] + (cym - pt[i + 1]) / 5);
    }
    poly(pt, 3, PAL_ACCENT, 3);
    poly(in, 3, SLOT(slot), 0);
    for (i = 0; i < 6; i += 2) {
        u8 j = (u8)(i == 4 ? 0 : i + 2);
        TUBE(pt[i], pt[i + 1], pt[j], pt[j + 1], 2, PAL_RUBBER);
    }
    for (i = 0; i < 6; i += 2) {
        DOME(pt[i], pt[i + 1], 4, PAL_RUBBER);
        DOME(pt[i], pt[i + 1], 2, PAL_WALL);
    }
}

void spinner(const struct part *p, u8 slot) {
    s16 hx = (s16)((mul32(COS(p->a), p->b) + 8192) >> 14), hy = (s16)((mul32(SIN(p->a), p->b) + 8192) >> 14);
    s16 ax = (s16)(p->x - hx), ay = (s16)(p->y - hy), bx = (s16)(p->x + hx), by = (s16)(p->y + hy);
    if (!shadow) blob((s16)(ax + hx / 5), (s16)(ay + hy / 5), (s16)(bx - hx / 5), (s16)(by - hy / 5), 3, SLOT(slot), 0);
    TUBE(ax, ay, bx, by, 1, PAL_WALL);
    DOME(ax, ay, 3, PAL_WALL);
    DOME(bx, by, 3, PAL_WALL);
}

#define LAMP(x, y, r, slot) blob((x), (y), (x), (y), (r), (slot), B_FLAT)

static const s8 __noescape lamp_shapes[3][8] = {
    {11, 0, -2, 9, -2, -9, 0, 0},              /* the arrow's head (its shaft below) */
    {8, 0, -6, 7, -6, -7, 0, 0},               /* a triangle */
    {9, -4, 9, 4, -9, 4, -9, -4},              /* an oval */
};
static const s8 __noescape shaft[8] = {-1, -3, -1, 3, -10, 3, -10, -3};

void lamp(const struct part *p, u8 slot) {
    u8 shape = (u8)(p->a & 3), ang = (u8)(p->a >> 8);
    s16 q[8];
    if (shape == LS_ROUND) {
        DOME(p->x, p->y, 8, PAL_WALL);
        LAMP(p->x, p->y, 6, SLOT(slot));
        return;
    }
    if (shape == LS_ARROW) {
        turned(q, p->x, p->y, ang, shaft, 4);
        poly(q, 4, SLOT(slot), 0);
    }
    turned(q, p->x, p->y, ang, lamp_shapes[shape - 1], (u8)(shape == LS_OVAL ? 4 : 3));
    poly(q, (u8)(shape == LS_OVAL ? 4 : 3), SLOT(slot), 0);
}
