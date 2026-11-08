/* contact.c — the ball against the table: every contact the same (see
 * physics.c): the nearest point of a segment, the push out, the bounce */
#include "physics.h"
#include "geom.h"
#include "pint.h"

/* rail rubber plastic wood bumper kick target drop gate post */
static const u8 bounce[] = {80, 190, 100, 70, 120, 160, 70, 50, 60, 170};

/* a contact's vector from the surface (1/16 px) */
static s16 gdx, gdy;

static u8 clamp8(s16 v) {
    return (u8)(v > 4080 ? 255 : v >> 4);
}

static s16 pix(s32 v) {
    return (s16)(v >> 10);
}

/* ---------------------------------------------------------------- */
/* contacts */

/* the ball's offset from the point t along u (t clamped to 0..len16), the
 * ball at (rx, ry) from the start: into gdx, gdy (1/16 px); t back */
static s16 nearest(s16 rx, s16 ry, s16 ux, s16 uy, s16 len16) {
    s16 t = fdot(rx, ux, ry, uy);
    if (t < 0) t = 0;
    if (t > len16) t = len16;
    gdx = (s16)(rx - fmul(ux, t));
    gdy = (s16)(ry - fmul(uy, t));
    return t;
}

/* the ball against a surface gdx, gdy away (rs, 1/16 px: the radii's sum)
 * moving at vcx, vcy: the normal speed it came in with (positive), 0 if no
 * contact or moving away. A kick with kick_min only above SLING_MIN. */
static s16 touch(s16 rs, u8 e, s16 kick, u8 kick_min, s16 vcx, s16 vcy) {
    s16 dist, nx, ny;
    s32 d2, vn, j;
    if (gdx >= rs || gdx <= -rs || gdy >= rs || gdy <= -rs) return 0;
    d2 = sq2(gdx, gdy);
    if (d2 >= mul32(rs, rs)) return 0;
    dist = (s16)isqrt32((u32)d2);
    if (!dist) { nx = 0; ny = -ANG_UNIT; }
    else {
        nx = (s16)(mul32(gdx, ANG_UNIT) / dist);
        ny = (s16)(mul32(gdy, ANG_UNIT) / dist);
    }
    bx += mul32(nx, rs - dist) >> 8;
    by += mul32(ny, rs - dist) >> 8;
    vn = fdot(bvx - vcx, nx, bvy - vcy, ny);
    if (vn >= 0) return 0;
    if (kick_min && vn > -SLING_MIN) kick = 0;
    j = vn + ((vn * e) >> 8) - kick;
    bvx = (s16)(bvx - (s16)((j * nx) >> 14));
    bvy = (s16)(bvy - (s16)((j * ny) >> 14));
    return (s16)(vn < -32000 ? 32000 : -vn);
}

void collide(struct coll *c) {
    s16 rx = (s16)((bx >> 6) - ((s32)c->ax << 4)), ry = (s16)((by >> 6) - ((s32)c->ay << 4)), sp;
    u8 kind = c->kind;
    nearest(rx, ry, c->ux, c->uy, (s16)(c->len * 16));
    if (kind == C_GATE && (mul32(rx, -c->uy) + mul32(ry, c->ux)) > 0) return;   /* passed from its right */
    sp = touch((s16)((c->r + BALL_R) * 16), bounce[kind], kind == C_BUMPER ? KICK_BUMPER : kind == C_KICK ? KICK_SLING : 0,
               (u8)(kind == C_KICK), 0, 0);
    if (!sp) return;
    switch (kind) {
    case C_BUMPER: event(EV_BUMPER, c->part, 255); break;
    case C_KICK: event(sp > SLING_MIN ? EV_SLING : EV_HIT, c->part, clamp8(sp)); break;
    case C_TARGET: if (sp > 120) event(EV_TARGET, c->part, clamp8(sp)); break;
    case C_DROP:
        if (sp > 120) {
            c->on = 0;
            event(EV_DROP, c->part, 255);
        }
        break;
    default:
        if (sp > 200) event(EV_HIT, c->part, clamp8(sp));
    }
}

static s16 sin64(s16 a) {
    u8 i = (u8)(a >> 6);
    s16 s0 = sin_t[i];
    return (s16)(s0 + (s16)(mul32(sin_t[(u8)(i + 1)] - s0, a & 63) >> 6));
}

void flipper_contact(struct flip *f) {
    s16 c = sin64((s16)(f->ang + 4096)), s = sin64(f->ang);
    s16 rx = (s16)((bx >> 6) - ((s32)f->px << 4)), ry = (s16)((by >> 6) - ((s32)f->py << 4));
    s16 len16 = (s16)(f->len * 16), t, sp;
    if (rx > len16 + 160 || rx < -len16 - 160 || ry > len16 + 160 || ry < -len16 - 160) return;
    t = nearest(rx, ry, c, s, len16);
    /* the radius there, and the surface's velocity: w x r */
    sp = touch((s16)(FLIP_RB * 16 - (s16)(mul32((FLIP_RB - FLIP_RT) * 16, t) / len16) + BALL_R * 16), FLIP_E, 0, 0,
               (s16)(-(mul32(f->w, fmul(s, t)) * 201) >> 13),
               (s16)((mul32(f->w, fmul(c, t)) * 201) >> 13));
    if (sp > 600) event(EV_FLIP, f->part, clamp8(sp));
}

void sensors(struct ball *b) {
    u8 i;
    s16 px = (s16)(bx >> 10), py = (s16)(by >> 10);
    for (i = 0; i < nsens; i++) {
        struct sens *s = &sens[i];
        s16 dx = (s16)(px - s->x), dy = (s16)(py - s->y), d2;
        if (dx > 24 || dx < -24 || dy > 24 || dy < -24) {
            if (b->in_sens == i) b->in_sens = 0xFF;
            continue;
        }
        d2 = (s16)(dx * dx + dy * dy);
        if (s->kind == S_SPIN) {
            /* the ball crossed the line under the axle (1/16 px), within its width */
            s16 rx = (s16)((bx >> 6) - (s->x << 4)), ry = (s16)((by >> 6) - (s->y << 4));
            s16 d = fdot(rx, -s->uy, ry, s->ux) , vd = fdot(bvx, -s->uy, bvy, s->ux);
            if (d2 < s->half * s->half && (d >= 0) != (d - (vd >> 6) >= 0)) {
                s->spin = (s16)((vd < 0 ? -vd : vd) / 4 + 8);
                if (s->spin > 120) s->spin = 120;
            }
        } else if (d2 < (s->kind == S_ROLL ? 49 : 30)) {
            if (b->in_sens == i) continue;
            if (s->kind == S_ROLL) event(EV_ROLL, s->part, 0);
            else if (sq2(bvx, bvy) < 2300L * 2300L) {        /* a kicker catches a slow ball */
                b->held = HOLD_FRAMES;
                b->kick = i;
                bx = (s32)s->x * FP;
                by = (s32)s->y * FP;
                bvx = bvy = 0;
                event(EV_KICK_IN, s->part, bi);
            } else continue;
            b->in_sens = i;
        } else if (d2 > 225 && b->in_sens == i)
            b->in_sens = 0xFF;
    }
}

/* two balls: a against b as a surface moving at b's speed; b gets the
 * opposite of a's change (equal masses) */
void ball_pair(struct ball *a, struct ball *b) {
    s16 sp, ovx, ovy;
    if (a->held) { struct ball *t = a; a = b; b = t; }
    if (a->held) return;
    bx = a->x; by = a->y;
    bvx = ovx = a->vx; bvy = ovy = a->vy;
    gdx = (s16)((a->x - b->x) >> 6);
    gdy = (s16)((a->y - b->y) >> 6);
    sp = touch(BALL_R * 32, 0, 0, 0, b->held ? 0 : b->vx, b->held ? 0 : b->vy);
    a->x = bx; a->y = by;
    a->vx = bvx; a->vy = bvy;
    if (!b->held) { b->vx = (s16)(b->vx - (bvx - ovx)); b->vy = (s16)(b->vy - (bvy - ovy)); }
    if (sp > 300) event(EV_CLICK, 0, clamp8(sp));
}

