/* sim.c — playing a table on the PC with a simple robot, for tuning
 *
 *   sim <table.tbl> [frames] [trace.ppm] [trace frames] [seed]
 *
 * The robot pulls the plunger fully, flips when a ball comes down near a
 * flipper, and nudges now and then. It reports how long balls last, what
 * they hit, and anything that looks wrong: balls stuck, balls escaping.
 * The trace is the table with the first balls' paths drawn on it.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/table.h"
#include "../src/theme.h"
#include "../src/render.h"
#include "../src/physics.h"
#include "../src/geom.h"

static u8 fb[TH][TW];
static unsigned hits[MAX_PARTS], evcount[16];

static const char *evname[] = {"hit", "bumper", "sling", "target", "drop", "roll", "spin", "kick in",
                               "kick out", "drain", "flip", "launch", "click", "gate"};

int main(int argc, char **argv) {
    FILE *f;
    long frames = argc > 2 ? atol(argv[2]) : 50000, n;
    const char *trace = argc > 3 ? argv[3] : NULL;
    long tframes = argc > 4 ? atol(argv[4]) : 3000;
    int x, y, c, i;
    u8 flipkeys = 0, plunge = 0, hold[MAX_FLIP] = {0};
    long ball_start = 0, balls_played = 0, total_life = 0, longest = 0, plunge_t = 0;
    long escapes = 0, stucks = 0, launches = 0, weak = 0;
    u16 seed = argc > 5 ? (u16)atoi(argv[5]) : 1;
    long last_lane_frame = 0, lane_wait = 0, relaunch = 0;
    if (argc < 2) { fprintf(stderr, "usage: sim table.tbl [frames] [trace.ppm] [trace frames] [seed]\n"); return 2; }
    sin_init();
    table_clear();
    f = fopen(argv[1], "rb");
    if (!f) { perror(argv[1]); return 1; }
    if (fread(&tbl, 1, sizeof tbl, f) < sizeof(struct table_hdr)) return 1;
    fclose(f);
    if (!table_check()) return 1;
    if (trace) {
        render_begin();
        theme_set(tbl.h.theme);
        for (c = 0; c < STRIPS; c++) {
            render_strip((u8)c, 0);
            for (y = 0; y < TH; y++)
                for (x = 0; x < 8; x++) fb[y][c * 8 + x] = render_buf[y * 8 + x];
        }
    }
    phys_build();
    phys_seed = seed;
    srand(seed);
    printf("%s: %d colliders, %d flippers, %d sensors\n", tbl.h.name, ncoll, nflips, nsens);
    phys_launch_ball(0);
    plunge_t = 0;
    for (n = 0; n < frames; n++) {
        /* the robot */
        flipkeys = 0;
        for (i = 0; i < nflips; i++) {
            struct flip *fl = &flips[i];
            int b, want = 0;
            for (b = 0; b < MAX_BALLS; b++) {
                struct ball *bl = &balls[b];
                long bx = bl->x >> 10, by = bl->y >> 10;
                long dx = bx - fl->px, dy = by - fl->py;
                if (!bl->live || bl->held) continue;
                if (fl->side == 0 && dx > -6 && dx < fl->len + 6 && dy > -30 && dy < 26 && bl->vy > -200) want = 1;
                if (fl->side == 1 && dx < 6 && dx > -fl->len - 6 && dy > -30 && dy < 26 && bl->vy > -200) want = 1;
            }
            if (want && hold[i] == 0 && (rand() % 3 == 0)) hold[i] = 10 + rand() % 8;
            if (hold[i]) { hold[i]--; flipkeys |= (u8)(fl->side ? 2 : 1); }
        }
        plunge = 0;
        if (plunge_t < 0 && plunger_x >= 0) {
            for (i = 0; i < MAX_BALLS; i++) {
                long bx = balls[i].x >> 10, by = balls[i].y >> 10;
                if (balls[i].live && bx > plunger_x - 10 && bx < plunger_x + 10 && by > plunger_y - 20 &&
                    balls[i].vy > -50 && balls[i].vy < 50) { if (++lane_wait > 40) { plunge_t = 0; lane_wait = 0; relaunch++; } }
            }
        }
        if (plunge_t >= 0 && plunge_t < 50) { plunge = 1; plunge_t++; }
        else if (plunge_t == 50) { plunge_t = -1; }
        if (rand() % 400 == 0) phys_nudge((s16)(rand() % 3 - 1), -1);
        phys_frame(flipkeys, plunge);
        for (i = 0; i < nev; i++) {
            struct event *e = &evq[i];
            evcount[e->kind]++;
            if (e->kind != EV_DRAIN && e->kind != EV_LAUNCH && e->kind != EV_CLICK && e->kind != EV_FLIP) hits[e->part]++;
            if (e->kind == EV_LAUNCH) { launches++; last_lane_frame = n; }
            if (e->kind == EV_DRAIN) {
                long life = n - ball_start;
                balls_played++;
                total_life += life;
                if (life > longest) longest = life;
                if (life < 200) weak++;
                ball_start = n;
                phys_launch_ball(0);
                plunge_t = 0;
            }
            if (e->kind == EV_DROP) {
                /* the bank comes back when it is all down */
                u8 g = F_GROUP(tbl.p[e->part].flags), k, down = 1;
                for (k = 0; k < ncoll; k++)
                    if (coll[k].kind == C_DROP && F_GROUP(tbl.p[coll[k].part].flags) == g && coll[k].on) down = 0;
                if (down) phys_drop_reset(g);
            }
        }
        nev = 0;
        for (i = 0; i < MAX_BALLS; i++) {
            if (balls[i].live && balls[i].still == 200) { stucks++; if (getenv("STUCK")) printf("stuck at %d,%d\n", (int)(balls[i].x >> 10), (int)(balls[i].y >> 10)); }
            if (trace && n < tframes && balls[i].live) {
                long bx = balls[i].x >> 10, by = balls[i].y >> 10;
                if (bx >= 0 && bx < TW && by >= 0 && by < TH) fb[by][bx] = (u8)(SLOT(0) + 1);
            }
        }
        (void)escapes;
        (void)last_lane_frame;
    }
    for (i = 0; i < MAX_BALLS; i++) if (balls[i].live) printf("ball %d at %d,%d v %d,%d held %d plunge_t %ld\n", i, (int)(balls[i].x >> 10), (int)(balls[i].y >> 10), balls[i].vx, balls[i].vy, balls[i].held, plunge_t);
    printf("balls %ld, average %.1f s, longest %.1f s, under 4 s: %ld, launches %ld (again %ld), stuck nudges %ld\n",
           balls_played, balls_played ? total_life / 50.0 / balls_played : 0.0, longest / 50.0, weak, launches, relaunch, stucks);
    for (i = 0; i < 14; i++) if (evcount[i]) printf("  %-9s %u\n", evname[i], evcount[i]);
    printf("parts never hit:");
    for (i = 0; i < tbl.h.nparts; i++) {
        u8 t = tbl.p[i].type;
        if ((t == P_BUMPER || t == P_TARGET || t == P_DROP || t == P_ROLLOVER || t == P_KICKER || t == P_SPINNER || t == P_SLING) && !hits[i])
            printf(" %d(type %d at %d,%d)", i, t, tbl.p[i].x, tbl.p[i].y);
    }
    printf("\n");
    if (trace) {
        slot_colour[0] = 0;
        slot_level[0] = 16;
        slot_put(0);
        f = fopen(trace, "wb");
        fprintf(f, "P6\n%d %d\n255\n", TW, TH);
        for (y = 0; y < TH; y++)
            for (x = 0; x < TW; x++) {
                u8 v = fb[y][x];
                fputc(pal_r[v], f); fputc(pal_g[v], f); fputc(pal_b[v], f);
            }
        fclose(f);
    }
    return 0;
}
