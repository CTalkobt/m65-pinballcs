/* play.c — PLAY: a game on the table PINBALL drew (it is in attic RAM;
 * the table itself at $0800), then back to PINBALL with the scores
 *
 * The keys: flippers left/right SHIFT (or Z, CTRL, / and the joystick),
 * the plunger RETURN (held, then let go), nudge SPACE, pause P, RUN/STOP
 * back to the menu.
 */
#include <input.h>
#include <mega65.h>
#include "hw.h"
#include "assets.h"
#include "table.h"
#include "theme.h"
#include "geom.h"
#include "physics.h"
#include "game.h"
#include "robot.h"
#include "sound.h"
#include "music.h"
#include "game_tunes.h"
#include "video.h"
#include "files.h"
#include "shared.h"
#include "world.h"

void hook_drop_gfx(u8 part, u8 down);

/* ---------------------------------------------------------------- */
/* the display */

static char line_a[21], line_b[21];
static u8 msg_t;
static u32 shown_score = 0xFFFFFFFFUL;
static u8 shown_ball = 0xFF;

static void centre(char *dst, const char *s) {
    u8 n = 0, i, at;
    while (s[n] && n < 20) n++;
    for (i = 0; i < 20; i++) dst[i] = ' ';
    dst[20] = 0;
    at = (u8)((20 - n) / 2);
    for (i = 0; i < n; i++) dst[at + i] = s[i];
}

void hook_message(const char *a, const char *b, u8 frames) {
    if (a[0]) centre(line_a, a);
    else line_a[0] = 0;
    centre(line_b, b);
    msg_t = frames;
    if (line_a[0]) disp_at(PANEL_COL, 2, line_a, 20);
    disp_at(PANEL_COL, 6, line_b, 20);
    if (!line_a[0]) shown_score = 0xFFFFFFFFUL;
}

static void score_text(char *buf, u32 v) {
    u8 i = 20, k = 0;
    for (i = 0; i < 20; i++) buf[i] = ' ';
    buf[20] = 0;
    i = 19;
    do {
        if (k == 3) { buf[i--] = ','; k = 0; }
        buf[i--] = (char)('0' + (u8)(v % 10));
        v /= 10;
        k++;
    } while (v && i > 2);
    if (gm.players > 1) {
        buf[0] = 'p';
        buf[1] = (char)('1' + gm.cur);
    }
}

static void panel_info(void) {
    u8 i;
    for (i = 0; i < gm.players; i++) {
        text_at(PANEL_COL + 2, (u8)(14 + i), "player", (u8)(i == gm.cur ? C_YELLOW : C_GREY));
        text_at(PANEL_COL + 9, (u8)(14 + i), "1", (u8)(i == gm.cur ? C_YELLOW : C_GREY));
        cell_put(PANEL_COL + 9, (u8)(14 + i), (u16)('1' + i), (u8)(i == gm.cur ? C_YELLOW : C_GREY));
        text_num(PANEL_COL + 14, (u8)(14 + i), gm.score[i], 10, (u8)(i == gm.cur ? C_WHITE : C_GREY));
    }
}

static void panel_frame(void) {
    char buf[21];
    if (msg_t) {
        if (--msg_t == 0) {
            shown_score = 0xFFFFFFFFUL;
            shown_ball = 0xFF;
        }
        if (line_a[0]) return;
    }
    if (gm.score[gm.cur] != shown_score) {
        shown_score = gm.score[gm.cur];
        score_text(buf, shown_score);
        disp_at(PANEL_COL, 2, buf, 20);
        panel_info();
    }
    if (!msg_t && shown_ball != gm.ball + gm.cur * 16) {
        static char b[21] = "ball 1";
        shown_ball = (u8)(gm.ball + gm.cur * 16);
        b[5] = (char)('0' + gm.ball);
        centre(buf, gm.over ? "game over" : b);
        disp_at(PANEL_COL, 6, buf, 20);
        text_at(PANEL_COL + 2, 19, "bonus", C_GREY);
    }
    text_num(PANEL_COL + 14, 19, gm.bonus, 10, C_LBLUE);
    text_at(PANEL_COL + 2, 20, "multiplier", C_GREY);
    cell_put(PANEL_COL + 23, 20, (u16)('0' + gm.mult), C_LBLUE);
}

/* ---------------------------------------------------------------- */
/* the hooks the rules call */

void hook_sfx(u8 id, u8 vol) { sfx(id, vol); }
void hook_chime(u8 which) { chime(which); }
void hook_sidfx(u8 which) { sid_fx(which); }
void hook_knocker(void) { sfx(SFX_KNOCKER, 255); }

void hook_drop_gfx(u8 part, u8 down) {
    u8 i;
    for (i = 0; i < ndrops; i++)
        if (drops[i].part == part) restore_box(drops[i].x0, drops[i].y0, drops[i].x1, drops[i].y1, down);
}

void hook_new_ball(u8 ball) {
    (void)ball;
    phys_launch_ball(0);
}

/* ---------------------------------------------------------------- */
/* playing */

static u8 keys_prev;
#ifdef AUTODEMO
u16 pass_frames[8];
#endif

#define K_LEFT  1
#define K_RIGHT 2
#define K_PLUNGE 4
#define K_NUDGE 8
#define K_QUIT  16
#define K_PAUSE 32

static u8 read_keys(void) {
    u8 k = 0, joy = (u8)(input_joy(1) | input_joy(2));
    if (input_pressed(KEY_LSHIFT) || input_pressed(KEY_Z) || input_pressed(KEY_CTRL) || (joy & JOY_LEFT)) k |= K_LEFT;
    if (input_pressed(KEY_RSHIFT) || input_pressed(KEY_SLASH) || (joy & JOY_RIGHT)) k |= K_RIGHT;
    if (input_pressed(KEY_RETURN) || (input_pressed(KEY_DOWN) && !(REG(0xD60F) & 2)) || (joy & (JOY_FIRE | JOY_DOWN))) k |= K_PLUNGE;
    if (input_pressed(KEY_SPACE) || (joy & JOY_UP)) k |= K_NUDGE;
    if (input_pressed(KEY_STOP)) k |= K_QUIT;
    if (input_pressed(KEY_P)) k |= K_PAUSE;
    return k;
}

static void sprites_frame(void) {
    u8 i;
    for (i = 0; i < MAX_BALLS; i++) {
        struct ball *b = &balls[i];
        if (b->live) {
            s16 x = (s16)((b->x + 512) >> 10), y = (s16)((b->y + 512) >> 10);
            spr_set(i, (s16)(x - 7), (s16)(y - 7), SPR_BALL);
            if (b->held) spr_hide((u8)(3 + i));
            else spr_set((u8)(3 + i), (s16)(x - 4), (s16)(y - 3), SPR_SHADOW);
        } else {
            spr_hide(i);
            spr_hide((u8)(3 + i));
        }
    }
    if (plunger_x >= 0) spr_set(6, (s16)(plunger_x - 7), (s16)(plunger_y + plunger_pull - 4), SPR_KNOB);
    else spr_hide(6);
}

static u16 fastest(void) {
    u8 i;
    u16 best = 0;
    for (i = 0; i < MAX_BALLS; i++) {
        struct ball *b = &balls[i];
        if (b->live && !b->held) {
            u16 s = (u16)((b->vx < 0 ? -b->vx : b->vx) + (b->vy < 0 ? -b->vy : b->vy));
            if (s > best) best = s;
        }
    }
    return best;
}

/* a game; demo: the robot plays until a key. The final score of player
 * 1 back. */
u32 play(u8 players, u8 demo) {
    u8 k, flip, plunge, shake_t = 0, nudge_prev = 0;
    panel_clear(0, 49);
    shown_score = 0xFFFFFFFFUL;
    shown_ball = 0xFF;
    msg_t = 0;
    game_new(players);
    flippers_draw(1);
    music(TUNE_BALLSTART, 15);
    {
        char nm[NAME_LEN + 1];
        for (k = 0; k < NAME_LEN; k++) nm[k] = tbl.h.name[k];
        nm[NAME_LEN] = 0;
        text_at(PANEL_COL + 2, 10, nm, C_WHITE);
    }
    if (demo) {
        text_at(PANEL_COL + 2, 40, "demo: any key for the menu", C_GREY);
    } else {
        text_at(PANEL_COL + 2, 40, "flippers  left/right shift", C_GREY);
        text_at(PANEL_COL + 2, 41, "plunger   return (hold)", C_GREY);
        text_at(PANEL_COL + 2, 42, "nudge     space", C_GREY);
        text_at(PANEL_COL + 2, 43, "pause p   quit run/stop", C_GREY);
    }
    keys_prev = read_keys();
    input_flush();
    for (;;) {
#ifdef AUTODEMO
        {   /* (tests: how many frames each pass takes) */
            static u8 last;
            u8 f = FRAMECOUNT, d = (u8)(f - last);
            last = f;
            if (d > 7) d = 7;
            pass_frames[d]++;
        }
#endif
        video_wait();
        if (demo) {
            if (input_key() || (read_keys() & (K_PLUNGE | K_QUIT))) break;
            flip = robot(&plunge);
#ifdef FLIPTEST
            flip |= 1;                 /* (tests: the left flipper held up) */
#endif
            k = (u8)(flip | (plunge ? K_PLUNGE : 0));
        } else {
            k = read_keys();
            if (k & K_QUIT) break;
            if ((k & K_PAUSE) && !(keys_prev & K_PAUSE)) {
                roll_sound(0);
                hook_message("paused", "", 1);
                while (read_keys() & K_PAUSE) video_wait();
                while (!(read_keys() & K_PAUSE)) video_wait();
                while (read_keys() & K_PAUSE) video_wait();
                msg_t = 1;
            }
        }
        /* the flippers' solenoids */
        if (!gm.tilted && !gm.over) {
            if ((k & K_LEFT) && !(keys_prev & K_LEFT)) { sfx(SFX_FLIPUP, 200); game_flip_pressed(0); }
            if ((k & K_RIGHT) && !(keys_prev & K_RIGHT)) { sfx(SFX_FLIPUP, 200); game_flip_pressed(1); }
            if (!(k & K_LEFT) && (keys_prev & K_LEFT)) sfx(SFX_FLIPDN, 120);
            if (!(k & K_RIGHT) && (keys_prev & K_RIGHT)) sfx(SFX_FLIPDN, 120);
        }
        if ((k & K_NUDGE) && !nudge_prev) {
            game_nudge(0);
            shake_t = 6;
        }
        nudge_prev = (u8)(k & K_NUDGE);
        if (shake_t) {
            shake_t--;
            video_shake(0, (s8)(shake_t & 2 ? -2 : 1));
            if (!shake_t) video_shake(0, 0);
        }
        keys_prev = k;
        phys_frame((u8)(gm.over ? 0 : k & 3), (u8)((k & K_PLUNGE) ? 1 : 0));
        game_frame(k & 3, (u8)((k & K_PLUNGE) ? 1 : 0));
        for (k = 0; k < nflips; k++) fl_frame[k] = flips[k].frame;
        flippers_draw(0);
        sprites_frame();
        lamps_frame();
        pal_slots();
        panel_frame();
        roll_sound(fastest());
        snd_update();
        if (gm.over && !msg_t) {
            if (demo) game_new(players);
            else {
                mb.finished = 1;
                break;
            }
        }
    }
    roll_sound(0);
    snd_quiet();
    video_shake(0, 0);
    for (k = 0; k < 7; k++) spr_hide(k);
    return gm.score[0];
}


