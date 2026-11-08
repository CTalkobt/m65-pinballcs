/* rprim.c — the renderer's primitives: clipping, shading, blobs, polygons
 * (render.c's introduction says what they draw) */
#include "render.h"
#include "hi.h"
#include "rint.h"
#include "geom.h"
#include "theme.h"

/* the tables: sqrt8_t, hz_t (gen_render.h: also font_g, for rbg.c) */
#define GEN_PRIM
#include "gen_render.h"

#define LX (-7373)
#define LY (-9011)
#define LZ 11469
#define SH_DX 3
#define SH_DY 4

s16 sx;                               /* the strip's first x */
u8 shadow;                            /* drawing shadows */
s16 cx0, cx1, cy0, cy1;


void darken(s16 x, s16 y) {
    u8 *q = &AT(x, y), v = *q;
    if ((v >= PAL_BG && v < PAL_BGS) || (v >= PAL_PAT && v < PAL_PATS)) *q = (u8)(v + 16);
    else if (v >= PAL_WALL && v < PAL_LAMP) *q = (u8)((v & 15) >= 5 ? v - 5 : v & 0xF0);
    else if (v >= PAL_LAMP) *q = (u8)(PAL_LAMP + (v - PAL_LAMP) / 3 * 3);
}

u8 clip(s16 x0, s16 y0, s16 x1, s16 y1) {
    if (shadow) { x0 += SH_DX; x1 += SH_DX; y0 += SH_DY; y1 += SH_DY; }
    cx0 = x0 < sx ? sx : x0;
    cx1 = x1 > sx + 7 ? (s16)(sx + 7) : x1;
    cy0 = y0 < 0 ? 0 : y0;
    cy1 = y1 > TH - 1 ? (s16)(TH - 1) : y1;
    return (u8)(cx0 <= cx1 && cy0 <= cy1);
}

/* an intensity (1.14) to a level of a ramp */
u8 level(s32 i) {
    if (i > 15400) return 15;
    if (i <= 0) return (u8)(i < -8192 ? 1 : 2);
    return (u8)(2 + (u8)((i * 12) >> 14));
}

/* ---------------------------------------------------------------- */
/* blobs: set up here, the pixels done by blob_core (blob.s45; on the PC
 * the C below, which does the same) */


u8 b_edge;
#ifdef __CC45_ZPCALL__
#define BLEXT extern
#else
#define BLEXT
#endif
BLEXT s16 bl_ax, bl_ay, bl_ux, bl_uy, bl_len16, bl_ex16, bl_ey16, bl_lxr, bl_lyr;
BLEXT u16 bl_rr, bl_rin, bl_kmul;
BLEXT u8 bl_ramp, bl_style, bl_edge, bl_glint;
BLEXT s16 bl_cx0, bl_cx1, bl_cy0, bl_cy1, bl_sx;
BLEXT u8 *bl_buf;
const u8 *sqrt8_ext;
const u16 *hz_ext;

#ifdef __CC45_ZPCALL__
void blob_core(void);
#else
static u8 lvl(s16 in) {
    if (in > 15400) return 15;
    if (in <= 0) return (u8)(in < -8192 ? 1 : 2);
    return (u8)(2 + fmul(in, 12));
}

static void blob_core(void) {
    s16 x, y;
    for (y = bl_cy0; y <= bl_cy1; y++)
        for (x = bl_cx0; x <= bl_cx1; x++) {
            u8 *q = &bl_buf[((u16)y << 3) + (u8)(x - bl_sx)];
            s16 px = (s16)((x - bl_ax) << 4), py = (s16)((y - bl_ay) << 4), t, qx, qy;
            u32 d2;
            if (bl_len16) {
                t = fdot(px, bl_ux, py, bl_uy);
                qx = (s16)(px - fmul(bl_ux, t));
                qy = (s16)(py - fmul(bl_uy, t));
                if (t < 0) d2 = (u32)sq2(px, py);
                else if (t > bl_len16) d2 = (u32)sq2((s16)(px - bl_ex16), (s16)(py - bl_ey16));
                else d2 = (u32)sq2(qx, qy);
                d2 = (d2 + 128) >> 8;
                if (d2 > bl_rr) continue;
                if (bl_style & B_DECAL) { *q = d2 > bl_rin ? bl_edge : bl_ramp; continue; }
                px = qx;
                py = qy;
                d2 = ((u32)sq2(px, py) + 128) >> 8;
            } else {
                d2 = ((u32)sq2(px, py) + 128) >> 8;
                if (d2 > bl_rr) continue;
                if (bl_style & B_DECAL) { *q = d2 > bl_rin ? bl_edge : bl_ramp; continue; }
                if (bl_style & B_FLAT) {
                    s16 gx = (s16)((px >> 4) + bl_glint), gy = (s16)((py >> 4) + bl_glint);
                    *q = (u8)(bl_ramp + (d2 > bl_rin ? 0 : (u16)sq2(gx, gy) <= (u16)(bl_glint * bl_glint) ? 2 : 1));
                    continue;
                }
            }
            if (bl_style & B_SHADOW) {
                u8 v = *q;
                if ((v >= PAL_BG && v < PAL_BGS) || (v >= PAL_PAT && v < PAL_PATS)) *q = (u8)(v + 16);
                else if (v >= PAL_WALL && v < PAL_LAMP) *q = (u8)((v & 15) >= 5 ? v - 5 : v & 0xF0);
                else if (v >= PAL_LAMP) *q = (u8)(PAL_LAMP + (v - PAL_LAMP) / 3 * 3);
                continue;
            }
            {
                s16 in;
                u8 k;
                px = (s16)((px + 8) >> 4);
                py = (s16)((py + 8) >> 4);
                k = d2 < 1024 ? (u8)((sqrt8_ext[d2] * (u32)bl_kmul) >> 8) : 32;
                if (k > 32) k = 32;
                in = (s16)(px * bl_lxr + py * bl_lyr + (s16)hz_ext[k]);
                if (bl_ramp >= PAL_LAMP)
                    *q = (u8)(bl_ramp + (in > 14000 ? 2 : (k > 26 || in < 4000) ? 0 : 1));
                else
                    *q = (u8)(bl_ramp + lvl(in));
            }
        }
}
#endif

/* pixels within r of the segment a-b (a dome when they are one point) */
void blob(s16 ax, s16 ay, s16 bx, s16 by, u8 r, u8 ramp, u8 style) {
    s16 dx, dy, len;
    if (shadow) {
        if (style & (B_FLAT | B_DECAL)) return;
        ax += SH_DX; ay += SH_DY; bx += SH_DX; by += SH_DY;
    }
    if (!clip((s16)((ax < bx ? ax : bx) - r - (shadow ? SH_DX : 0)), (s16)((ay < by ? ay : by) - r - (shadow ? SH_DY : 0)),
              (s16)((ax > bx ? ax : bx) + r - (shadow ? SH_DX : 0)), (s16)((ay > by ? ay : by) + r - (shadow ? SH_DY : 0))))
        return;
    dx = (s16)(bx - ax);
    dy = (s16)(by - ay);
    bl_ax = ax;
    bl_ay = ay;
    bl_len16 = 0;
    if (dx || dy) {
        len = (s16)isqrt32((u32)sq2(dx, dy));
        bl_ux = (s16)(mul32(dx, ANG_UNIT) / len);
        bl_uy = (s16)(mul32(dy, ANG_UNIT) / len);
        bl_len16 = (s16)(len * 16);
        bl_ex16 = fmul(bl_len16, bl_ux);
        bl_ey16 = fmul(bl_len16, bl_uy);
    }
    bl_rr = (u16)(r * r + r);
    bl_rin = (u16)((r - 1) * (r - 1));
    bl_kmul = (u16)(1024 / r);
    bl_lxr = (s16)(LX / r);
    bl_lyr = (s16)(LY / r);
    if (style & B_DULL) { bl_lxr = (s16)(bl_lxr * 5 / 8); bl_lyr = (s16)(bl_lyr * 5 / 8); }
    bl_ramp = ramp;
    bl_style = (u8)(style | (shadow ? B_SHADOW : 0));
    bl_edge = b_edge;
    bl_glint = (u8)(r / 3);
    bl_cx0 = cx0; bl_cx1 = cx1; bl_cy0 = cy0; bl_cy1 = cy1;
    bl_sx = sx;
    bl_buf = render_buf;
    sqrt8_ext = sqrt8_t;
    hz_ext = hz_t;
    blob_core();
}



