/* physics.h — the ball, the table's colliders, the flippers
 *
 * Built from the table's parts (phys_build). The ball moves in SUBSTEPS
 * steps a frame; positions are 1/1024 pixel (s32), velocities 1/1024 pixel
 * a step (s16). Colliders are segments with a radius (circles have length
 * 0), found through a grid of 16-pixel cells. What happens (hits, lanes,
 * spinners, drains) goes into an event queue for the rules.
 */
#pragma once
#include "pb.h"
#include "table.h"

#define SUBSTEPS   4
#define FP         1024             /* position and velocity units a pixel */
#define BALL_R     7
#define MAX_BALLS  3
#define MAX_COLL   150
#define MAX_FLIP   4
#define MAX_SENS   16

/* collider kinds */
enum { C_RAIL, C_RUBBER, C_PLASTIC, C_WOOD, C_BUMPER, C_KICK, C_TARGET, C_DROP, C_GATE, C_POST };

struct coll {
    s16 ax, ay;                       /* pixels */
    s16 ux, uy;                       /* direction, 1.14 (circles: 0) */
    s16 len;                          /* pixels */
    u8 r, kind, part, on;
};

struct ball {
    s32 x, y;
    s16 vx, vy;
    u8 live;
    u8 held;                          /* frames left in a kicker (0: free) */
    u8 kick;                          /* the kicker holding it */
    u8 in_sens;                       /* the sensor it is inside (0xFF none) */
    u16 still;                        /* frames it has hardly moved */
};

struct flip {
    u8 part;
    s16 px, py;                       /* the pivot */
    s16 len;
    s16 rest, up;                     /* angles in 1/64 of a step (256 steps a turn) */
    s16 ang, w;                       /* now, and its speed a substep */
    s8 dir;                           /* toward up: +1 or -1 */
    u8 side;                          /* 0 left, 1 right */
    u8 frame;                         /* the drawn position */
};

/* sensors: rollovers, kickers, spinners */
enum { S_ROLL, S_KICKER, S_SPIN };
struct sens {
    u8 kind, part;
    s16 x, y;
    s16 ux, uy, half;                 /* spinners: the axle */
    s16 spin;                         /* spinners: speed, 1/256 of a turn a frame */
    u8 angle;                         /* spinners: the plate's angle */
};

extern struct coll coll[MAX_COLL];
extern u8 ncoll;
extern struct ball balls[MAX_BALLS];
extern struct flip flips[MAX_FLIP];
extern u8 nflips;
extern struct sens sens[MAX_SENS];
extern u8 nsens;
extern s16 plunger_x, plunger_y;      /* -1: no plunger */
extern u8 plunger_pull;               /* pixels drawn back (0-20) */

/* events */
enum {
    EV_HIT,          /* part, strength: a wall, rubber or post */
    EV_BUMPER,       /* part */
    EV_SLING,        /* part */
    EV_TARGET,       /* part */
    EV_DROP,         /* part: it fell */
    EV_ROLL,         /* part */
    EV_SPIN,         /* part: a half turn */
    EV_KICK_IN,      /* part: a ball caught */
    EV_KICK_OUT,     /* part */
    EV_DRAIN,        /* ball */
    EV_FLIP,         /* a flipper hit the ball (strength) */
    EV_LAUNCH,       /* the plunger let go: strength */
    EV_CLICK,        /* balls knocked together */
    EV_GATE
};
struct event { u8 kind, part, strength; };
#define MAX_EV 12
extern struct event evq[MAX_EV];
extern u8 nev;

void phys_build(void);                /* from tbl (physbuild.c, PINBALL) */
void phys_reset(void);                /* a new game on it */
void phys_ball(u8 b, s16 x, s16 y);   /* a ball placed at rest (pixels) */
void phys_launch_ball(u8 b);          /* a new ball on the plunger */
/* one frame: flippers held (bit 0 left, bit 1 right), the plunger held */
void phys_frame(u8 flip_keys, u8 plunger);
void phys_nudge(s16 dx, s16 dy);      /* the table bumped: pixels a frame */
void phys_drop_reset(u8 group);       /* a bank of drop targets up again */
u8 phys_balls_live(void);
extern u8 phys_tilted;                /* the flippers dead */
extern u16 phys_seed;
