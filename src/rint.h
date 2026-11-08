/* rint.h — what the renderer's files share (rprim.c, rbg.c, rparts.c) */
#pragma once
#include "pb.h"

#define LX (-7373)                    /* the light, toward it: 1.14 */
#define LY (-9011)
#define LZ 11469
#define SH_DX 3                       /* the shadow's offset */
#define SH_DY 4

#define B_FLAT 1                      /* flat: a lamp (rim, body, glint) */
#define B_DULL 2                      /* matte */
#define B_DECAL 4                     /* flat, two colours: ramp inside, b_edge at its rim */
#define B_SHADOW 0x80

extern s16 sx, cx0, cx1, cy0, cy1;
extern u8 shadow, b_edge;
#define AT(x, y) render_buf[((u16)(y) << 3) + (u8)((x) - sx)]

void darken(s16 x, s16 y);
u8 clip(s16 x0, s16 y0, s16 x1, s16 y1);
u8 level(s32 i);
void blob(s16 ax, s16 ay, s16 bx, s16 by, u8 r, u8 ramp, u8 style);
void poly(const s16 *pt, u8 n, u8 ramp, u8 bev);
void turned(s16 *q, s16 x, s16 y, u8 a, const s8 *v, u8 n);
void text(s16 x, s16 y, const char *s, u8 ramp, u8 size);
void playfield(void);
