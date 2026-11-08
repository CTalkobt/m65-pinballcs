/* game.h — the rules: scoring, lamps, balls, players
 *
 * The same rules serve every table, driven by its parts and their groups:
 *  - targets, drop targets and rollovers of a group (1-12) are a bank:
 *    each one made lights; all lit (or down) completes the bank: points,
 *    the bonus multiplier up, and every second completion lights the
 *    extra ball; with a lock on the table, the lock lights
 *  - rollovers of a bank change lanes with the flippers
 *  - a kicker in group G_EXTRA gives the extra ball when lit; a kicker
 *    marked lock holds balls while the lock is lit: two locked, multiball,
 *    and the lock kicker scores the jackpot while it lasts
 *  - lamps show their group: the banks still to finish blink, completions
 *    light them one by one; G_MULT lamps are the multiplier's ladder,
 *    G_EXTRA the extra ball, G_LOCK the lock
 *  - the bonus (1000 a target or lane) times the multiplier at the end of
 *    each ball, a ball saved in its first seconds, a skill shot, tilt
 */
#pragma once
#include "pb.h"

#define MAX_PLAYERS 4

struct game {
    u8 players, cur, ball, balls;        /* ball: 1.. */
    u32 score[MAX_PLAYERS];
    u32 bonus;
    u8 mult;                             /* 1-5 */
    u8 extra_lit, extra_balls;
    u8 lock_lit, locked, multiball;
    u32 jackpot;
    u16 save_frames;                     /* ball save left */
    u16 skill_frames;
    u8 skill_part;
    u8 tilt_warn, tilted;
    s16 shake;
    u8 over;
    u8 in_play;                          /* balls on the table */
    u8 waiting;                          /* a ball on the plunger */
    u16 frame;
};
extern struct game gm;

/* the platform's side (main.c / host) */
void hook_sfx(u8 id, u8 vol);
void hook_chime(u8 which);
void hook_sidfx(u8 which);
void hook_message(const char *a, const char *b, u8 frames);   /* the display's two lines */
void hook_drop_gfx(u8 part, u8 down);
void hook_new_ball(u8 ball);             /* a ball onto the plunger */
void hook_knocker(void);

void game_new(u8 players);
void game_frame(u8 flip_keys, u8 plunger);   /* rules after phys_frame's events */
void game_nudge(s8 dir);                /* -1 left, 0 up, 1 right */
void game_flip_pressed(u8 side);        /* lane change */
void lamps_frame(void);                 /* the lamps' slots toward their states */
u8 game_end_ball(void);                 /* the bonus count after a drain: 1 while counting */
