/* ui.c — the menu: the tables on the disk, the best scores, play or edit
 *
 * The table area shows the chosen table (drawn when the choice has rested
 * a moment), its lamps running a light show. Left alone for a while, the
 * robot plays it.
 */
#include <input.h>
#include "music.h"
#include "menu_tunes.h"
#include <mega65.h>
#include "hw.h"
#include "table.h"
#include "theme.h"
#include "render.h"
#include "game.h"
#include "shared.h"
#include "sound.h"
#include "video.h"
#include "files.h"
#include "ui.h"

#define LIST_ROW 12

static u8 sel, players, shown;

u8 ui_key(void) {
    u8 c;
    while (!(c = input_key())) {
        video_wait();
        snd_update();
    }
    return c;
}

u8 ui_input(u8 col, u8 row, char *buf, u8 max) {
    u8 n = 0, c, i;
    while (buf[n] && buf[n] != ' ' && n < max) n++;
    /* the text so far, without trailing spaces */
    for (i = n; i < max; i++) buf[i] = 0;
    for (;;) {
        text_fill(col, row, max + 1, '.', C_DGREY);
        buf[n] = 0;
        text_at(col, row, buf, C_WHITE);
        cell_put((u8)(col + n), row, 0xA0, C_YELLOW);          /* the cursor: a reversed space */
        c = ui_key();
        if (c == CH_RETURN) break;
        if (c == CH_STOP || c == CH_ESC) return 0;
        if (c == CH_DELETE && n) { buf[--n] = 0; continue; }
        /* letters: unshifted they come as $41-$5A (ASCII capitals), shifted $C1-$DA */
        if (c >= 0xC1 && c <= 0xDA) c = (u8)(c - 0x80);
        if (n < max && ((c >= 0x41 && c <= 0x5A) || (c >= '0' && c <= '9') || c == ' ' || c == '-' || c == '!' || c == '.' || c == '\'')) {
            buf[n++] = (char)c;
        }
    }
    buf[n] = 0;
    text_fill(col, row, max + 1, ' ', C_WHITE);
    return 1;
}

static void slot_line(u8 s) {
    char name[NAME_LEN + 1];
    u8 i, row = (u8)(LIST_ROW + s - 1), on = (u8)(s == sel);
    u8 col = on ? C_YELLOW : (s <= 12 ? C_WHITE : C_LGREEN);
    struct index_entry *e = &idx.e[s - 1];
    cell_put(PANEL_COL + 1, row, on ? 0x3E : 32, C_YELLOW);           /* > */
    text_num(PANEL_COL + 2, row, s, 2, col);
    if (e->used) {
        for (i = 0; i < NAME_LEN; i++) name[i] = e->name[i];
        name[NAME_LEN] = 0;
        text_at(PANEL_COL + 5, row, name, col);
        if (e->best) {
            text_num(PANEL_COL + 22, row, e->best, 10, on ? C_YELLOW : C_GREY);
            name[0] = e->who[0]; name[1] = e->who[1]; name[2] = e->who[2]; name[3] = 0;
            text_at(PANEL_COL + 34, row, name, on ? C_YELLOW : C_GREY);
        } else
            text_fill(PANEL_COL + 22, row, 15, ' ', col);
    } else {
        text_at(PANEL_COL + 5, row, "- empty -       ", C_DGREY);
        text_fill(PANEL_COL + 22, row, 15, ' ', col);
    }
}

static void menu_draw(void) {
    u8 s;
    panel_clear(0, 49);
    disp_at(PANEL_COL, 1, "      pinball       ", 20);
    disp_at(PANEL_COL, 5, " construction  set  ", 20);
    text_at(PANEL_COL + 2, LIST_ROW - 2, "table                 best score", C_LBLUE);
    for (s = 1; s <= SLOTS; s++) slot_line(s);
    text_at(PANEL_COL + 2, 38, "crsr up/down  choose a table", C_GREY);
    text_at(PANEL_COL + 2, 39, "return        play", C_GREY);
    text_at(PANEL_COL + 2, 40, "1-4           players:", C_GREY);
    cell_put(PANEL_COL + 25, 40, (u16)('0' + players), C_YELLOW);
    text_at(PANEL_COL + 2, 41, "e             edit (empty: new)", C_GREY);
    text_at(PANEL_COL + 2, 42, "d             demo", C_GREY);
    text_at(PANEL_COL + 2, 43, "m             music on/off", C_GREY);
    text_at(PANEL_COL + 2, 47, "a table is yours to change: e", C_DGREY);
}

static void blank_table(void) {
    table_clear();
    tbl.h.theme = 0;
    tbl.h.nparts = 0;
}

/* the chosen table onto the table area */
static u8 show(void) {
    u8 ok = 0;
    music(MUSIC_NONE, 0);
    text_at(PANEL_COL + 2, 36, "loading...", C_GREY);
    if (idx.e[sel - 1].used) ok = table_load(sel);
    if (!ok) blank_table();
    text_at(PANEL_COL + 2, 36, "drawing...", C_GREY);
    prepare();
    mb.slot = sel;
    text_fill(PANEL_COL + 2, 36, 12, ' ', C_GREY);
    shown = sel;
    if (snd_music_on) music(TUNE_TITLE, 12);
    return ok;
}

/* a game of the table on show: PLAY takes over */
static void go(u8 mode) {
    if (shown != sel || !mb.drawn) show();
    music(MUSIC_NONE, 0);
    snd_quiet();
    mb.mode = mode;
    mb.players = players;
    mb.slot = sel;
    mb.music = snd_music_on;
    text_at(PANEL_COL + 2, 36, "loading...", C_GREY);
    run("play");
}

/* a new best score: initials */
static void best_score(u32 score) {
    struct index_entry *e = &idx.e[sel - 1];
    char who[4] = {0, 0, 0, 0};
    if (score <= e->best) return;
    panel_clear(30, 37);
    disp_at(PANEL_COL, 1, "    high score      ", 20);
    text_at(PANEL_COL + 2, 32, "a new best for this table!", C_YELLOW);
    text_at(PANEL_COL + 2, 34, "your initials:", C_WHITE);
    sid_fx(FX_EXTRA);
    if (!ui_input(PANEL_COL + 17, 34, who, 3)) return;
    e->best = score;
    e->who[0] = who[0] ? who[0] : ' ';
    e->who[1] = who[1] ? who[1] : ' ';
    e->who[2] = who[2] ? who[2] : ' ';
    text_at(PANEL_COL + 2, 36, "saving...", C_GREY);
    index_save();
}

void ui_main(void) {
    u8 c, idle_s = 0, i;
    u16 t = 0, rest = 0;
    sel = mb.slot ? mb.slot : 1;
    players = mb.players ? mb.players : 1;
    menu_draw();
    if ((mb.mode == MB_PLAY || mb.mode == MB_DEMO) && mb.drawn) {
        /* back from a game: the table is still drawn in attic RAM */
        show_drawn();
        shown = sel;
        if (mb.mode == MB_PLAY && mb.finished) {
            u32 best = 0;
            for (i = 0; i < players; i++) if (mb.score[i] > best) best = mb.score[i];
            best_score(best);
            menu_draw();
        }
        if (snd_music_on) music(TUNE_TITLE, 12);
    } else
        show();
    mb.mode = MB_MENU;
#ifdef AUTODEMO
    if (!mb.finished) go(MB_DEMO);     /* (tests: straight into the demo) */
#endif
#ifdef AUTOEDIT
    editor(sel);                       /* (tests: straight into the editor) */
#endif
    for (;;) {
        video_wait();
        snd_update();
        music_update();
        lamps_attract(t++);
        pal_slots();
        if (shown != sel && ++rest > 20) {
            show();
            rest = 0;
        }
        if ((t & 63) == 0 && ++idle_s > 40) {       /* about 50 seconds alone: the demo */
            idle_s = 0;
            if (shown == sel && idx.e[sel - 1].used) go(MB_DEMO);
        }
        c = input_key();
        if (!c) continue;
        idle_s = 0;
        switch (c) {
        case CH_CURSOR_UP:
            if (sel > 1) { sel--; slot_line((u8)(sel + 1)); slot_line(sel); rest = 0; sid_fx(FX_MENU); }
            break;
        case CH_CURSOR_DOWN:
            if (sel < SLOTS) { sel++; slot_line((u8)(sel - 1)); slot_line(sel); rest = 0; sid_fx(FX_MENU); }
            break;
        case '1': case '2': case '3': case '4':
            players = (u8)(c - '0');
            cell_put(PANEL_COL + 25, 40, (u16)c, C_YELLOW);
            sid_fx(FX_MENU);
            break;
        case CH_RETURN:
            if (!idx.e[sel - 1].used) { sid_fx(FX_ERROR); break; }
            sid_fx(FX_SELECT);
            go(MB_PLAY);
            break;
        case 'd':
            if (!idx.e[sel - 1].used) { sid_fx(FX_ERROR); break; }
            go(MB_DEMO);
            break;
        case 'e':
            sid_fx(FX_SELECT);
            if (shown != sel) show();
            editor(sel);
            menu_draw();
            show();
            break;
        case 'm':
            snd_music_on = (u8)!snd_music_on;
            music(snd_music_on ? TUNE_TITLE : MUSIC_NONE, 12);
            break;
        }
    }
}
