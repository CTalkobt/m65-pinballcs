/* physics.c — the ball on the table
 *
 * A step: gravity, the move, then each collider in the ball's grid cell,
 * the flippers, the plunger, the sensors and the other balls. Every
 * contact is the same: the ball's distance from the nearest point of a
 * segment (a circle is a segment of length 0), less the radii. The ball is
 * pushed out along the normal and, if it was moving in, takes back the
 * normal speed (relative to the surface: a flipper moves) times the bounce,
 * plus a kick for bumpers and slingshots.
 *
 * The ball being stepped is in globals (bx, by, bvx, bvy): absolute
 * addresses make less code than a pointer's fields.
 */
#include "physics.h"
#include "hi.h"
#include "geom.h"
#include "pint.h"

HI struct coll coll[MAX_COLL];
u8 ncoll;
HI struct ball balls[MAX_BALLS];
HI struct flip flips[MAX_FLIP];
u8 nflips;
HI struct sens sens[MAX_SENS];
u8 nsens;
s16 plunger_x = -1, plunger_y;
u8 plunger_pull;
HI struct event evq[MAX_EV];
u8 nev;
u8 phys_tilted;
u16 phys_seed = 12345;



#define GW 19
#define GH 26
#define MAX_ITEMS 1920
HI u16 cell_start[GW * GH + 1];
#ifdef __CC45_ZPCALL__
#pragma section("gridsec", 0x1500)            /* low RAM, $1500-$1C7F */
#endif
__section("gridsec") u8 cell_items[MAX_ITEMS];

/* the ball being stepped (contact.c works on it) */
s32 bx, by;
s16 bvx, bvy;
u8 bi;

static u16 rnd(void) {
    phys_seed = (u16)(phys_seed * 25173u + 13849u);
    return phys_seed;
}

void event(u8 kind, u8 part, u8 strength) {
    if (nev < MAX_EV) {
        evq[nev].kind = kind;
        evq[nev].part = part;
        evq[nev].strength = strength;
        nev++;
    }
}

/* a new game on the built table: everything back to the start */
void phys_reset(void) {
    u8 i;
    for (i = 0; i < MAX_BALLS; i++) balls[i].live = 0;
    for (i = 0; i < ncoll; i++) coll[i].on = 1;
    for (i = 0; i < nflips; i++) {
        flips[i].ang = flips[i].rest;
        flips[i].w = 0;
        flips[i].frame = 0;
    }
    for (i = 0; i < nsens; i++) sens[i].spin = 0;
    plunger_pull = 0;
    phys_tilted = 0;
    nev = 0;
}

void phys_ball(u8 b, s16 x, s16 y) {
    struct ball *bl = &balls[b];
    bl->x = (s32)x * FP;
    bl->y = (s32)y * FP;
    bl->vx = 0;
    bl->vy = 0;
    bl->live = 1;
    bl->held = 0;
    bl->in_sens = 0xFF;
    bl->still = 0;
}

void phys_launch_ball(u8 b) {
    if (plunger_x >= 0) phys_ball(b, plunger_x, (s16)(plunger_y - BALL_R - 1));
    else phys_ball(b, TW / 2, 40);
}

u8 phys_balls_live(void) {
    u8 i, n = 0;
    for (i = 0; i < MAX_BALLS; i++) n = (u8)(n + balls[i].live);
    return n;
}

void phys_nudge(s16 dx, s16 dy) {
    u8 i;
    for (i = 0; i < MAX_BALLS; i++)
        if (balls[i].live && !balls[i].held) {
            balls[i].vx = (s16)(balls[i].vx + dx * (FP / SUBSTEPS));
            balls[i].vy = (s16)(balls[i].vy + dy * (FP / SUBSTEPS));
        }
}

void phys_drop_reset(u8 group) {
    u8 i;
    for (i = 0; i < ncoll; i++)
        if (coll[i].kind == C_DROP && F_GROUP(tbl.p[coll[i].part].flags) == group) coll[i].on = 1;
}

static s16 pix(s32 v) {
    return (s16)(v >> 10);
}

static void step_ball(struct ball *b) {
    s16 cx, cy;
    u16 k, e;
    s32 v2;
    u8 i;
    bx = b->x;
    by = b->y;
    bvx = b->vx;
    bvy = (s16)(b->vy + GRAVITY);
    v2 = sq2(bvx, bvy);
    if (v2 > mul32(VMAX, VMAX)) {
        s16 v = (s16)isqrt32((u32)v2);
        bvx = (s16)(mul32(bvx, VMAX) / v);
        bvy = (s16)(mul32(bvy, VMAX) / v);
    }
    bx += bvx;
    by += bvy;
    cx = (s16)(bx >> 14);
    cy = (s16)(by >> 14);
    if (cx >= 0 && cx < GW && cy >= 0 && cy < GH) {
        u16 cell = (u16)(cy * GW + cx);
        e = cell_start[cell + 1];
        for (k = cell_start[cell]; k < e; k++) {
            struct coll *c = &coll[cell_items[k]];
            if (c->on) collide(c);
        }
    }
    for (i = 0; i < nflips; i++) flipper_contact(&flips[i]);
    /* the plunger's top: a floor in its lane */
    if (plunger_x >= 0 && pix(bx) > plunger_x - 9 && pix(bx) < plunger_x + 9) {
        s32 floor = (s32)(plunger_y + plunger_pull - BALL_R) * FP;
        if (by > floor && by < floor + 8 * FP) {
            by = floor;
            if (bvy > 0) bvy = (s16)-(bvy >> 2);
        }
    }
    sensors(b);
    b->x = bx;
    b->y = by;
    b->vx = bvx;
    b->vy = bvy;
}

static void flippers_step(u8 keys) {
    u8 i;
    for (i = 0; i < nflips; i++) {
        struct flip *f = &flips[i];
        s16 lo = f->dir > 0 ? f->rest : f->up, hi = f->dir > 0 ? f->up : f->rest;
        f->w = (!phys_tilted && (keys & (f->side ? 2 : 1))) ? (s16)(f->dir * FLIP_W_UP) : (s16)(-f->dir * FLIP_W_DOWN);
        f->ang = (s16)(f->ang + f->w);
        if (f->ang <= lo) { f->ang = lo; f->w = 0; }
        if (f->ang >= hi) { f->ang = hi; f->w = 0; }
    }
}

/* a ball at (px, py) resting in the plunger's lane */
static u8 on_plunger(s16 px, s16 py) {
    return (u8)(plunger_x >= 0 && px > plunger_x - 12 && px < plunger_x + 12 && py > plunger_y - 30);
}

void phys_frame(u8 keys, u8 plunger) {
    u8 st, i, j;
    static u8 pull_tick, was_held;
    struct ball *b;
    if (plunger) {
        if (plunger_pull < 20 && (++pull_tick & 1)) plunger_pull++;
        was_held = 1;
    } else if (was_held) {
        was_held = 0;
        for (i = 0, b = balls; i < MAX_BALLS; i++, b++)
            if (b->live && on_plunger(pix(b->x), pix(b->y))) {
                b->vy = (s16)(-(s16)plunger_pull * LAUNCH_K - (s16)(rnd() & 63));
                event(EV_LAUNCH, 0, (u8)(plunger_pull * 12));
            }
        plunger_pull = 0;
    }
    for (st = 0; st < SUBSTEPS; st++) {
        flippers_step(keys);
        for (i = 0; i < MAX_BALLS; i++)
            if (balls[i].live && !balls[i].held) {
                bi = i;
                step_ball(&balls[i]);
            }
        for (i = 0; i < MAX_BALLS - 1; i++)
            for (j = (u8)(i + 1); j < MAX_BALLS; j++)
                if (balls[i].live && balls[j].live) ball_pair(&balls[i], &balls[j]);
    }
    for (i = 0; i < nflips; i++) {
        struct flip *f = &flips[i];
        u16 off = (u16)((f->ang - f->rest) * f->dir);
        f->frame = (u8)((off * (FLIP_FRAMES - 1) + FLIP_SWING * 32) / (FLIP_SWING * 64));
    }
    for (i = 0; i < nsens; i++) {
        struct sens *s = &sens[i];
        if (s->spin) {
            u8 before = s->angle;
            s->angle = (u8)(s->angle + (u8)(s->spin >> 1));
            if ((before ^ s->angle) & 0x80) event(EV_SPIN, s->part, 0);
            s->spin = (s16)(s->spin - 1 - (s->spin >> 5));
            if (s->spin < 0) s->spin = 0;
        }
    }
    for (i = 0, b = balls; i < MAX_BALLS; i++, b++) {
        s16 px, py;
        if (!b->live) continue;
        if (b->held) {
            if (b->held != 0xFF && --b->held == 0) {
                struct sens *s = &sens[b->kick];
                u8 a = (u8)tbl.p[s->part].a;
                b->vx = fmul(COS(a), KICK_OUT);
                b->vy = fmul(SIN(a), KICK_OUT);
                b->x += (s32)b->vx * 3;
                b->y += (s32)b->vy * 3;
                event(EV_KICK_OUT, s->part, i);
            }
            continue;
        }
        b->vx = (s16)(b->vx - (b->vx >> 9));
        b->vy = (s16)(b->vy - (b->vy >> 10));
        px = pix(b->x);
        py = pix(b->y);
        if (py > TH + 12) {
            b->live = 0;
            event(EV_DRAIN, 0, i);
        } else if (px < -12 || px > TW + 12 || py < -20)
            phys_launch_ball(i);
        else if ((u16)(b->vx + 60) < 120 && (u16)(b->vy + 60) < 120 && !on_plunger(px, py)) {
            /* hardly moving for 4 seconds: shaken loose */
            if (++b->still > 200) {
                b->still = 0;
                b->vx = (s16)((rnd() & 1023) - 512);
                b->vy = -900;
            }
        } else
            b->still = 0;
    }
}
