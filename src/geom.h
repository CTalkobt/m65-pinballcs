/* geom.h — trigonometry and the shapes of the parts
 *
 * Shared by the renderer and the physics, so that what is drawn is what
 * the ball meets.
 */
#pragma once
#include "pb.h"
#include "table.h"

extern s16 sin_t[256];                /* 1.14: sin_init builds it */
void sin_init(void);
#define SIN(a) sin_t[(u8)(a)]
#define COS(a) sin_t[(u8)((a) + 64)]

u16 isqrt32(u32 v);

/* walls and arcs: a radius from the thickness flag */
u8 wall_radius(u8 flags);
/* an arc's pieces: a chord at most 8 angle steps long; n pieces; piece k's ends */
u8 arc_pieces(const struct part *p);
void arc_point(const struct part *p, u8 k, s16 *x, s16 *y);

/* a slingshot's corners: top, bottom, foot (the foot is toward the flippers) */
void sling_points(const struct part *p, s16 *pt);   /* 6 values */

/* flippers */
#define FLIP_RB 7                     /* the radius at the pivot */
#define FLIP_RT 4                     /* and at the tip */
#define FLIP_SWING 40                 /* the swing: 56 degrees */
#define FLIP_FRAMES 12                /* drawn positions */
u8 flip_rest(const struct part *p);   /* the angle at rest */
s8 flip_dir(const struct part *p);    /* +1: up is clockwise ... -1 */

/* targets: half the face's width, and their thickness */
#define TGT_HALF 8
#define TGT_DEPTH 3
void target_ends(const struct part *p, s16 *pt);    /* 4 values: the face */

void flip_tip(const struct part *p, u8 f, s16 *tx, s16 *ty);
/* a flipper's box over all its positions: x0 a strip's start, x1 the next's */
void flipper_box(const struct part *p, s16 *x0, s16 *y0, s16 *x1, s16 *y1);

/* the playfield's font */
u8 text_code(u8 c);                   /* a character as the font has it (32-95) */
s16 text_width(const char *s, u8 size);
extern const u8 font_w[64];
