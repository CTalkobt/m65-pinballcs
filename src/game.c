/* game.c — the rules (see game.h) */
#include "game.h"
#include "physics.h"
#include "table.h"
#include "theme.h"
#include "sound.h"

struct game gm;

#define NGROUPS 16
static u8 members[NGROUPS], nlit[NGROUPS], completions[NGROUPS];
static u8 lit[MAX_PARTS];
static u8 flash[NSLOTS];
static u8 lock_part = 0xFF;            /* the lock kicker */
static u8 skill_group;
static u8 counting, count_tick;
static u8 drop_reset_t[NGROUPS];       /* frames until a bank comes back up */

static u8 bank_part(u8 t) {
    return (u8)(t == P_TARGET || t == P_DROP || t == P_ROLLOVER);
}

static void add(u32 pts) {
    if (gm.tilted || gm.over) return;
    gm.score[gm.cur] += pts;
}

static void ring(u32 pts) {
    if (pts >= 1000) hook_chime(2);
    else if (pts >= 100) hook_chime(1);
    else hook_chime(0);
}

static u32 n_to_s(char *buf, u32 v) {            /* "1,234,560", right-aligned in 11 */
    u8 i = 11, k = 0;
    buf[11] = 0;
    do {
        if (k == 3) { buf[--i] = ','; k = 0; }
        buf[--i] = (char)('0' + (u8)(v % 10));
        v /= 10;
        k++;
    } while (v && i > 0);
    while (i) buf[--i] = ' ';
    return 0;
}

static void say(const char *a, const char *b, u8 frames) {
    hook_message(a, b, frames);
}

static void say_points(const char *what, u32 pts) {
    char num[12];
    n_to_s(num, pts);
    hook_message(what, num, 90);
}

/* ---------------------------------------------------------------- */

static void count_groups(void) {
    u8 i, g;
    for (g = 0; g < NGROUPS; g++) { members[g] = 0; nlit[g] = 0; }
    for (i = 0; i < tbl.h.nparts; i++) {
        const struct part *p = &tbl.p[i];
        if (bank_part(p->type)) {
            g = F_GROUP(p->flags);
            if (g) members[g]++;
            if (lit[i] && g) nlit[g]++;
        }
    }
}

static void bank_reset(u8 g) {
    u8 i;
    for (i = 0; i < tbl.h.nparts; i++) {
        const struct part *p = &tbl.p[i];
        if (bank_part(p->type) && F_GROUP(p->flags) == g) {
            lit[i] = 0;
            if (p->type == P_DROP) hook_drop_gfx(i, 0);
        }
    }
    phys_drop_reset(g);
    nlit[g] = 0;
}

static void bank_done(u8 g) {
    u32 pts = 10000UL * (u32)(completions[g] + 1);
    completions[g]++;
    add(pts);
    hook_sidfx(FX_JACKPOT);
    if (gm.mult < 5) {
        gm.mult++;
        say_points(gm.mult == 2 ? "bonus 2x" : gm.mult == 3 ? "bonus 3x" : gm.mult == 4 ? "bonus 4x" : "bonus 5x", pts);
    } else
        say_points("bank complete", pts);
    if (lock_part != 0xFF && !gm.multiball && !gm.lock_lit) {
        gm.lock_lit = 1;
        say("lock is lit", "", 90);
    } else if ((completions[g] & 1) == 0 || lock_part == 0xFF) {
        if (!gm.extra_lit) {
            gm.extra_lit = 1;
            say("extra ball", "is lit", 90);
            hook_sidfx(FX_EXTRA);
        }
    }
    /* drop targets come back after a moment; lanes and targets at once */
    drop_reset_t[g] = 40;
}

static void made(u8 part) {
    u8 g = F_GROUP(tbl.p[part].flags);
    if (lit[part]) return;
    lit[part] = 1;
    gm.bonus += 1000;
    if (g && g < 13) {
        nlit[g]++;
        if (nlit[g] >= members[g]) bank_done(g);
    }
}

/* the next ball: on the plunger */
static void start_ball(void) {
    u8 i, g;
    gm.mult = 1;
    gm.bonus = 0;
    gm.tilt_warn = 0;
    gm.tilted = 0;
    phys_tilted = 0;
    gm.shake = 0;
    gm.save_frames = (tbl.h.options & 1) ? 600 : 0;
    gm.waiting = 1;
    gm.in_play = 1;
    gm.lock_lit = 0;
    gm.multiball = 0;
    for (i = 0; i < tbl.h.nparts; i++) lit[i] = 0;
    for (g = 1; g < NGROUPS; g++) {
        completions[g] = 0;
        bank_reset(g);
    }
    count_groups();
    /* the skill shot: one of a bank of lanes, if there is one */
    gm.skill_part = 0xFF;
    if (skill_group) {
        u8 k = (u8)(phys_seed % members[skill_group]), n = 0;
        for (i = 0; i < tbl.h.nparts; i++)
            if (tbl.p[i].type == P_ROLLOVER && F_GROUP(tbl.p[i].flags) == skill_group && n++ == k) gm.skill_part = i;
    }
    gm.skill_frames = 0;
    hook_new_ball(gm.ball);
}

void game_new(u8 players) {
    u8 i, g;
    gm.players = players;
    gm.cur = 0;
    gm.ball = 1;
    gm.balls = tbl.h.balls;
    gm.over = 0;
    gm.extra_balls = 0;
    gm.extra_lit = 0;
    gm.locked = 0;
    gm.jackpot = 50000;
    counting = 0;
    for (i = 0; i < MAX_PLAYERS; i++) gm.score[i] = 0;
    lock_part = 0xFF;
    skill_group = 0;
    for (i = 0; i < tbl.h.nparts; i++) {
        const struct part *p = &tbl.p[i];
        if (p->type == P_KICKER && (p->flags & F_LOCK)) lock_part = i;
    }
    count_groups();
    for (g = 1; g < 13 && !skill_group; g++) {
        u8 n = 0;
        for (i = 0; i < tbl.h.nparts; i++)
            if (tbl.p[i].type == P_ROLLOVER && F_GROUP(tbl.p[i].flags) == g) n++;
        if (n >= 2 && n == members[g]) skill_group = g;
    }
    phys_reset();
    start_ball();
}

static void next_ball(void) {
    if (gm.extra_balls) {
        gm.extra_balls--;
        say("shoot again", "", 100);
        start_ball();
        return;
    }
    if (++gm.cur >= gm.players) {
        gm.cur = 0;
        gm.ball++;
    }
    if (gm.ball > gm.balls) {
        gm.over = 1;
        gm.in_play = 0;
        say("game over", "", 250);
        return;
    }
    {
        static char line[12] = "player 1";
        line[7] = (char)('1' + gm.cur);
        say(gm.players > 1 ? line : "", gm.ball == gm.balls ? "last ball" : "", 80);
    }
    start_ball();
}

u8 game_end_ball(void) {
    return counting;
}

static void drained(void) {
    u8 live = phys_balls_live();
    if (gm.multiball) {
        if (live <= 1) {
            gm.multiball = 0;
            say("multiball", "over", 80);
        }
        if (live) return;
    }
    if (live) return;
    if (gm.save_frames && !gm.tilted) {
        say("ball saved", "", 80);
        gm.save_frames = 0;
        hook_new_ball(gm.ball);
        gm.waiting = 1;
        return;
    }
    gm.in_play = 0;
    counting = 1;
    count_tick = 30;
    if (gm.tilted) gm.bonus = 0;
    {
        static char line[12] = "bonus  x1";
        line[8] = (char)('0' + gm.mult);
        say(line, "", 40);
    }
}

static void count_bonus(void) {
    if (count_tick) { count_tick--; return; }
    if (gm.bonus) {
        add(1000UL * gm.mult);
        gm.bonus -= 1000;
        hook_chime(2);
        count_tick = 3;
        {
            char num[12];
            n_to_s(num, gm.bonus);
            hook_message("bonus", num, 10);
        }
        return;
    }
    counting = 0;
    next_ball();
}

static void start_multiball(void) {
    u8 i;
    gm.multiball = 1;
    gm.lock_lit = 0;
    gm.locked = 0;
    say("multiball", "", 120);
    hook_knocker();
    for (i = 0; i < MAX_BALLS; i++)
        if (balls[i].live && balls[i].held == 0xFF) balls[i].held = (u8)(20 + i * 25);
}

/* a free ball onto the plunger, for locks */
static void ball_to_plunger(void) {
    u8 i;
    for (i = 0; i < MAX_BALLS; i++)
        if (!balls[i].live) {
            phys_launch_ball(i);
            gm.waiting = 1;
            return;
        }
}

static void kicker_in(u8 part, u8 bi) {
    const struct part *p = &tbl.p[part];
    if (part == lock_part) {
        if (gm.multiball) {
            add(gm.jackpot);
            say_points("jackpot", gm.jackpot);
            gm.jackpot += 25000;
            hook_sidfx(FX_JACKPOT);
            hook_knocker();
            return;
        }
        if (gm.lock_lit) {
            gm.locked++;
            balls[bi].held = 0xFF;
            gm.lock_lit = 0;
            add(5000);
            if (gm.locked >= 2) start_multiball();
            else {
                say("ball locked", "", 90);
                hook_sidfx(FX_LOCK);
                ball_to_plunger();
            }
            return;
        }
        add(2500);
        return;
    }
    if (F_GROUP(p->flags) == G_EXTRA && gm.extra_lit) {
        gm.extra_lit = 0;
        gm.extra_balls++;
        say("extra ball", "", 120);
        hook_knocker();
        return;
    }
    add(5000);
    say_points("", 5000);
}

void game_flip_pressed(u8 side) {
    u8 g, i, first, prev, n, k;
    u8 idx[12];
    if (gm.tilted) return;
    for (g = 1; g < 13; g++) {
        n = 0;
        for (i = 0; i < tbl.h.nparts && n < 12; i++)
            if (tbl.p[i].type == P_ROLLOVER && F_GROUP(tbl.p[i].flags) == g) idx[n++] = i;
        if (n < 2 || n != members[g]) continue;
        if (side) {                      /* right: each lane takes its left neighbour's light */
            prev = lit[idx[n - 1]];
            for (k = 0; k < n; k++) { first = lit[idx[k]]; lit[idx[k]] = prev; prev = first; }
        } else {
            prev = lit[idx[0]];
            for (k = n; k > 0; k--) { first = lit[idx[k - 1]]; lit[idx[k - 1]] = prev; prev = first; }
        }
    }
}

void game_nudge(s8 dir) {
    if (gm.tilted || gm.over) return;
    phys_nudge(dir, -1);
    gm.shake = (s16)(gm.shake + 50);
    if (gm.shake > 110) {
        gm.tilt_warn++;
        gm.shake = 40;
        if (gm.tilt_warn >= 3) {
            gm.tilted = 1;
            phys_tilted = 1;
            say("tilt", "", 200);
            hook_sidfx(FX_TILT);
        } else {
            say("danger", "", 60);
            hook_sidfx(FX_WARN);
        }
    }
}

void game_frame(u8 flip_keys, u8 plunger) {
    u8 i, g;
    (void)flip_keys;
    (void)plunger;
    gm.frame++;
    if (gm.shake) gm.shake--;
    if (gm.save_frames && !gm.waiting) gm.save_frames--;
    if (gm.skill_frames) gm.skill_frames--;
    for (g = 1; g < NGROUPS; g++)
        if (drop_reset_t[g] && --drop_reset_t[g] == 0) bank_reset(g);
    for (i = 0; i < nev; i++) {
        struct event *e = &evq[i];
        const struct part *p = &tbl.p[e->part];
        u8 slot = part_slot[e->part];
        switch (e->kind) {
        case EV_HIT:
            if (p->type == P_POST || (p->type == P_WALL && F_STYLE(p->flags) == WS_RUBBER) || p->type == P_SLING) {
                hook_sfx(SFX_RUBBER, (u8)(e->strength > 120 ? 255 : 80 + e->strength));
                add(10);
            } else
                hook_sfx(SFX_METAL, (u8)(e->strength > 150 ? 200 : 40 + e->strength));
            break;
        case EV_BUMPER:
            hook_sfx(SFX_BUMPER, 255);
            {
                u32 pts = 100 + 100UL * completions[F_GROUP(p->flags)];
                if (pts > 1000) pts = 1000;
                add(pts);
                ring(pts);
            }
            if (slot < NSLOTS) flash[slot] = 8;
            break;
        case EV_SLING:
            hook_sfx(SFX_SLING, 255);
            add(10);
            ring(10);
            if (slot < NSLOTS) flash[slot] = 6;
            break;
        case EV_TARGET:
            hook_sfx(SFX_TARGET, 230);
            add(500);
            ring(500);
            made(e->part);
            if (slot < NSLOTS) flash[slot] = 6;
            break;
        case EV_DROP:
            hook_sfx(SFX_DROP, 255);
            hook_drop_gfx(e->part, 1);
            add(1000);
            ring(1000);
            made(e->part);
            break;
        case EV_ROLL:
            hook_sfx(SFX_ROLLOVER, 200);
            add(500);
            ring(500);
            if (e->part == gm.skill_part && gm.skill_frames) {
                add(25000);
                say_points("skill shot", 25000);
                hook_sidfx(FX_SKILL);
                gm.skill_frames = 0;
            }
            if (F_GROUP(p->flags)) made(e->part);
            else if (slot < NSLOTS) flash[slot] = 20;
            break;
        case EV_SPIN:
            hook_sfx(SFX_SPIN, 150);
            add(100);
            ring(100);
            break;
        case EV_KICK_IN:
            hook_sfx(SFX_KICKIN, 255);
            kicker_in(e->part, e->strength);
            if (slot < NSLOTS) flash[slot] = 20;
            break;
        case EV_KICK_OUT:
            hook_sfx(SFX_KICKOUT, 255);
            break;
        case EV_LAUNCH:
            hook_sfx(SFX_LAUNCH, (u8)(e->strength < 60 ? 60 : e->strength));
            if (gm.waiting) {
                gm.waiting = 0;
                gm.skill_frames = 160;
            }
            break;
        case EV_DRAIN:
            hook_sfx(SFX_DRAIN, 255);
            drained();
            break;
        case EV_CLICK:
            hook_sfx(SFX_METAL, (u8)(e->strength > 150 ? 220 : 60 + e->strength));
            break;
        case EV_FLIP:
            hook_sfx(SFX_RUBBER, (u8)(e->strength > 150 ? 200 : 50 + e->strength));
            break;
        }
    }
    nev = 0;
    if (counting) count_bonus();
}

/* ---------------------------------------------------------------- */
/* lamps */

static u8 want[NSLOTS];

void lamps_frame(void) {
    u8 i, s, w, blink = (u8)((gm.frame & 32) ? 16 : 0), fast = (u8)((gm.frame & 8) ? 16 : 0);
    u8 kidx[NGROUPS];
    for (s = 0; s < NSLOTS; s++) want[s] = 0;
    for (i = 0; i < NGROUPS; i++) kidx[i] = 0;
    for (i = 0; i < tbl.h.nparts; i++) {
        const struct part *p = &tbl.p[i];
        u8 g = F_GROUP(p->flags);
        s = part_slot[i];
        if (s >= NSLOTS) continue;
        switch (p->type) {
        case P_BUMPER: w = 10; break;
        case P_SLING: w = 0; break;
        case P_TARGET: case P_DROP: case P_ROLLOVER:
            w = lit[i] ? 16 : 0;
            if (i == gm.skill_part && (gm.waiting || gm.skill_frames)) w = fast;
            break;
        case P_KICKER:
            if (i == lock_part) w = gm.multiball ? fast : gm.lock_lit ? blink : 4;
            else if (g == G_EXTRA) w = gm.extra_lit ? blink : 4;
            else w = 10;
            break;
        case P_SPINNER: {
            u8 k;
            w = 3;
            for (k = 0; k < nsens; k++)
                if (sens[k].part == i && sens[k].spin) w = (sens[k].angle & 0x40) ? 16 : 2;
            break;
        }
        case P_LAMP:
            if (g == 0) w = 12;
            else if (g == G_MULT) w = gm.mult >= kidx[g] + 2 ? 16 : 0;
            else if (g == G_EXTRA) w = gm.extra_balls ? 16 : gm.extra_lit ? blink : 0;
            else if (g == G_LOCK) w = gm.locked > kidx[g] ? 16 : gm.lock_lit ? blink : 0;
            else w = completions[g] > kidx[g] ? 16 : (members[g] ? blink : 0);
            kidx[g]++;
            break;
        default: w = 0;
        }
        if (gm.tilted) w = 0;
        if (w > want[s]) want[s] = w;
    }
    for (s = 0; s < NSLOTS; s++) {
        u8 lv = slot_level[s], t = want[s];
        if (flash[s]) { flash[s]--; t = 16; lv = 16; }
        if (lv < t) lv = (u8)(lv + 5 > t ? t : lv + 5);
        else if (lv > t) lv = (u8)(lv < t + 3 ? t : lv - 3);
        if (lv != slot_level[s]) {
            slot_level[s] = lv;
            slot_put(s);
        }
    }
}

