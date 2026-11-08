/* physbuild.c — the colliders, flippers and sensors made from the table's
 * parts, and the grid that finds them (PINBALL; world.c hands them to PLAY)
 */
#include "physics.h"
#include "geom.h"
#include "world.h"

#define GW 19
#define GH 26
#define MAX_ITEMS 1920
extern u16 cell_start[GW * GH + 1];
#ifdef __CC45_ZPCALL__
#pragma section("gridsec", 0x1500)
#endif
extern __section("gridsec") u8 cell_items[MAX_ITEMS];

/* ---------------------------------------------------------------- */
/* building */

static void seg(s16 ax, s16 ay, s16 ex, s16 ey, u8 r, u8 kind, u8 part) {
    struct coll *c;
    s16 dx = (s16)(ex - ax), dy = (s16)(ey - ay);
    if (ncoll >= MAX_COLL) return;
    c = &coll[ncoll++];
    c->ax = ax;
    c->ay = ay;
    c->len = (s16)isqrt32((u32)(sq2(dx, dy)));
    c->ux = c->len ? (s16)(mul32(dx, ANG_UNIT) / c->len) : 0;
    c->uy = c->len ? (s16)(mul32(dy, ANG_UNIT) / c->len) : 0;
    c->r = r;
    c->kind = kind;
    c->part = part;
    c->on = 1;
}

static void grid(void) {
    u8 pass, i;
    u16 n, k;
    for (k = 0; k <= GW * GH; k++) cell_start[k] = 0;
    for (pass = 0; pass < 2; pass++) {
        if (pass) {
            n = 0;
            for (k = 0; k < GW * GH; k++) {
                n = (u16)(n + cell_start[k]);
                cell_start[k] = n;
            }
            cell_start[GW * GH] = n;
        }
        for (i = 0; i < ncoll; i++) {
            struct coll *c = &coll[i];
            s16 ex = (s16)(c->ax + fmul(c->ux, c->len)), ey = (s16)(c->ay + fmul(c->uy, c->len));
            s16 m = (s16)(c->r + BALL_R + 5), cx, cy;
            s16 x0 = (s16)(((c->ax < ex ? c->ax : ex) - m) >> 4), x1 = (s16)(((c->ax > ex ? c->ax : ex) + m) >> 4);
            s16 y0 = (s16)(((c->ay < ey ? c->ay : ey) - m) >> 4), y1 = (s16)(((c->ay > ey ? c->ay : ey) + m) >> 4);
            if (x0 < 0) x0 = 0;
            if (y0 < 0) y0 = 0;
            if (x1 > GW - 1) x1 = GW - 1;
            if (y1 > GH - 1) y1 = GH - 1;
            for (cy = y0; cy <= y1; cy++)
                for (cx = x0; cx <= x1; cx++) {
                    u16 cell = (u16)(cy * GW + cx);
                    if (!pass) cell_start[cell]++;
                    else if ((k = --cell_start[cell]) < MAX_ITEMS) cell_items[k] = i;
                }
        }
    }
}

void phys_build(void) {
    u8 i, k, n, kind;
    s16 pt[6], x0, y0, x1, y1;
    ncoll = 0;
    nflips = 0;
    nsens = 0;
    plunger_x = -1;
    plunger_pull = 0;
    phys_tilted = 0;
    nev = 0;
    for (i = 0; i < MAX_BALLS; i++) balls[i].live = 0;
    for (i = 0; i < tbl.h.nparts; i++) {
        struct part *p = &tbl.p[i];
        u8 st = F_STYLE(p->flags);
        kind = st == WS_RUBBER ? C_RUBBER : st == WS_PLASTIC ? C_PLASTIC : st == WS_WOOD ? C_WOOD : C_RAIL;
        switch (p->type) {
        case P_WALL:
            seg(p->x, p->y, p->a, p->b, wall_radius(p->flags), kind, i);
            break;
        case P_ARC:
            n = arc_pieces(p);
            arc_point(p, 0, &x0, &y0);
            for (k = 1; k <= n; k++) {
                arc_point(p, k, &x1, &y1);
                seg(x0, y0, x1, y1, wall_radius(p->flags), kind, i);
                x0 = x1;
                y0 = y1;
            }
            break;
        case P_BUMPER:
            seg(p->x, p->y, p->x, p->y, (u8)p->a, C_BUMPER, i);
            break;
        case P_POST:
            seg(p->x, p->y, p->x, p->y, (u8)p->a, st == WS_RAIL ? C_RAIL : C_POST, i);
            break;
        case P_SLING:
            sling_points(p, pt);
            seg(pt[0], pt[1], pt[4], pt[5], 3, C_KICK, i);
            seg(pt[0], pt[1], pt[2], pt[3], 3, C_RUBBER, i);
            seg(pt[2], pt[3], pt[4], pt[5], 3, C_RUBBER, i);
            break;
        case P_TARGET:
        case P_DROP:
            target_ends(p, pt);
            seg(pt[0], pt[1], pt[2], pt[3], 3, p->type == P_TARGET ? C_TARGET : C_DROP, i);
            break;
        case P_GATE:
            seg(p->x, p->y, p->a, p->b, 1, C_GATE, i);
            break;
        case P_FLIPPER:
            if (nflips < MAX_FLIP) {
                struct flip *f = &flips[nflips++];
                f->part = i;
                f->px = p->x;
                f->py = p->y;
                f->len = p->a;
                f->dir = flip_dir(p);
                f->side = (u8)(p->b ? 1 : 0);
                f->rest = (s16)((s16)flip_rest(p) * 64);
                f->up = (s16)(f->rest + f->dir * FLIP_SWING * 64);
                f->ang = f->rest;
                f->w = 0;
                f->frame = 0;
            }
            break;
        case P_ROLLOVER:
        case P_KICKER:
        case P_SPINNER:
            if (nsens < MAX_SENS) {
                struct sens *s = &sens[nsens++];
                s->kind = p->type == P_ROLLOVER ? S_ROLL : p->type == P_KICKER ? S_KICKER : S_SPIN;
                s->part = i;
                s->x = p->x;
                s->y = p->y;
                s->ux = COS(p->a);
                s->uy = SIN(p->a);
                s->half = p->b;
                s->spin = 0;
                s->angle = 0;
            }
            break;
        case P_PLUNGER:
            plunger_x = p->x;
            plunger_y = p->y;
            break;
        }
    }
    grid();
    /* the drop targets' boxes, for PLAY to put them up and down */
    ndrops = 0;
    for (i = 0; i < tbl.h.nparts && ndrops < MAX_DROPS; i++)
        if (tbl.p[i].type == P_DROP) {
            struct dropbox *d = &drops[ndrops++];
            d->part = i;
            part_box(&tbl.p[i], &d->x0, &d->y0, &d->x1, &d->y1);
        }
}

