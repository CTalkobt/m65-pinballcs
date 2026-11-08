/* pb.h — types and constants shared by every part of the construction set
 *
 * The core (table, geom, theme, render, physics, rules) is portable C: it
 * builds with cc45 for the MEGA65 and with the host's cc for the tools in
 * host/ (previews, the simulator). cc45's int is 16 bits and its plain
 * integer types are unsigned, so the core only uses the sized types below,
 * and every product that can pass 16 bits goes through MUL().
 */
#pragma once
#include <stdint.h>

typedef uint8_t  u8;
typedef int8_t   s8;
typedef uint16_t u16;
typedef int16_t  s16;
typedef uint32_t u32;
typedef int32_t  s32;

#define MUL(a, b) ((s32)(a) * (s32)(b))

/* fixed point on the hardware multiplier (fmath.s45): (a*b) >> 14,
 * (a*b + c*d) >> 14, a*a + b*b, a*b */
#ifdef __CC45_ZPCALL__
s16 fmul(s16 a, s16 b);
s16 fdot(s16 a, s16 b, s16 c, s16 d);
s32 sq2(s16 a, s16 b);
s32 mul32(s16 a, s16 b);
#else
static inline s16 fmul(s16 a, s16 b) { return (s16)(MUL(a, b) >> 14); }
static inline s16 fdot(s16 a, s16 b, s16 c, s16 d) { return (s16)((MUL(a, b) + MUL(c, d)) >> 14); }
static inline s32 sq2(s16 a, s16 b) { return MUL(a, a) + MUL(b, b); }
static inline s32 mul32(s16 a, s16 b) { return MUL(a, b); }
#endif

#ifndef __CC45_ZPCALL__
#define __noescape                   /* (cc45: constants kept with an overlay's code) */
#define __section(s)                 /* (cc45: a fixed address) */
#endif

/* the playfield: 304 x 400 pixels, 38 strips of 8 pixels (a strip is a
 * column of 50 full-colour characters, 3200 contiguous bytes) */
#define TW       304
#define TH       400
#define STRIPS   38
#define STRIP_BYTES 3200

/* angles: 256 to a turn, 0 along +x, 64 along +y (down the screen) */
#define ANG_UNIT 16384               /* sin/cos in 1.14 */

/* palette layout (FCM pixels are palette entries; 0-15 stay the system
 * colours for text) */
#define PAL_BG      16               /* the playfield's gradient, 16 bands */
#define PAL_BGS     32               /* the same in shadow */
#define PAL_PAT     48               /* the pattern colour's gradient */
#define PAL_PATS    64               /* in shadow */
#define PAL_WALL    80               /* ramps of 16 levels, dark to light */
#define PAL_RUBBER  96
#define PAL_FLIP    112
#define PAL_ACCENT  128
#define PAL_DISP    144              /* the score display (the panel) */
#define PAL_LAMP    160              /* 32 lamp slots of 3 entries: rim, body, glint */
#define NSLOTS      32
#define SLOT(s)     (PAL_LAMP + (s) * 3)
