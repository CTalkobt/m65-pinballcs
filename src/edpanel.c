/* edpanel.c — the editor's panel: what is chosen, the keys; saving,
 * naming, trying the table */
#include <input.h>
#include <mega65.h>
#include "hw.h"
#include "assets.h"
#include "table.h"
#include "theme.h"
#include "geom.h"
#include "video.h"
#include "sound.h"
#include "files.h"
#include "shared.h"
#include "ui.h"
#include "edint.h"

static const char *const names[P_TYPES] = {
    "", "wall", "arc", "bumper", "post", "slingshot", "flipper", "target", "drop target",
    "rollover", "kicker", "spinner", "lamp", "text", "gate", "plunger", "decal"
};
static const char *const colours[NCOLOURS] = {
    "red", "orange", "yellow", "lime", "green", "teal", "cyan", "sky",
    "blue", "violet", "purple", "magenta", "pink", "white", "amber", "gold"
};

/* ---------------------------------------------------------------- */
/* the panel */

void info(void) {
    struct part *p;
    char nm[NAME_LEN + 1];
    u8 i;
    panel_clear(9, 22);
    for (i = 0; i < NAME_LEN; i++) nm[i] = tbl.h.name[i];
    nm[NAME_LEN] = 0;
    text_at(PANEL_COL + 2, 9, nm, C_WHITE);
    text_at(PANEL_COL + 20, 9, theme->name, C_LBLUE);
    text_at(PANEL_COL + 30, 9, "balls", C_GREY);
    cell_put(PANEL_COL + 36, 9, (u16)('0' + tbl.h.balls), C_WHITE);
    text_at(PANEL_COL + 2, 11, "add:", C_GREY);
    text_at(PANEL_COL + 7, 11, names[add_type], C_YELLOW);
    text_at(PANEL_COL + 22, 11, "parts", C_GREY);
    text_num(PANEL_COL + 28, 11, tbl.h.nparts, 3, C_WHITE);
    text_at(PANEL_COL + 32, 11, fine ? "fine" : "    ", C_LGREEN);
    if (wall_x >= 0) text_at(PANEL_COL + 2, 12, "return: its other end", C_LGREEN);
    if (sel == 0xFF) {
        text_at(PANEL_COL + 2, 14, "no part chosen (space)", C_DGREY);
        return;
    }
    p = &tbl.p[sel];
    text_at(PANEL_COL + 2, 14, names[p->type], C_YELLOW);
    if (grab) text_at(PANEL_COL + 16, 14, grab == 2 ? "(its end moves)" : "(moving)", C_LGREEN);
    text_at(PANEL_COL + 2, 15, "x", C_GREY);
    text_num(PANEL_COL + 4, 15, (u32)p->x, 3, C_WHITE);
    text_at(PANEL_COL + 9, 15, "y", C_GREY);
    text_num(PANEL_COL + 11, 15, (u32)p->y, 3, C_WHITE);
    text_at(PANEL_COL + 16, 15, "group", C_GREY);
    text_num(PANEL_COL + 22, 15, F_GROUP(p->flags), 2, C_WHITE);
    switch (p->type) {
    case P_BUMPER: case P_TARGET: case P_DROP: case P_ROLLOVER: case P_KICKER: case P_LAMP:
        text_at(PANEL_COL + 2, 16, "colour", C_GREY);
        text_at(PANEL_COL + 9, 16, colours[p->b & 15], C_WHITE);
        break;
    case P_WALL: case P_ARC: case P_POST: {
        static const char *const st[4] = {"rail", "rubber", "plastic", "wood"};
        text_at(PANEL_COL + 2, 16, "style", C_GREY);
        text_at(PANEL_COL + 8, 16, st[F_STYLE(p->flags)], C_WHITE);
        if (p->type != P_POST) {
            text_at(PANEL_COL + 17, 16, "thick", C_GREY);
            text_num(PANEL_COL + 23, 16, F_THICK(p->flags) + 1, 1, C_WHITE);
        }
        break;
    }
    case P_TEXT:
        text_at(PANEL_COL + 2, 16, tbl.text[p->a & 7], C_WHITE);
        break;
    }
}

void help(void) {
    static const char *const lines[] = {
        "crsr  cursor     f fine steps",
        "f1/f3 part type  return add it",
        "space choose     g grab, move",
        "e     its end    del delete",
        "+ -   size       , .  turn",
        "c colour  u group  y style",
        "h thickness      x its text",
        "t theme  n name  b balls",
        "q undo   p try it   s save",
        "run/stop   back to the menu",
    };
    u8 i;
    panel_clear(25, 49);
    for (i = 0; i < 10; i++) text_at(PANEL_COL + 2, (u8)(38 + i), lines[i], C_GREY);
}

void handles(void) {
    spr_set(7, (s16)(cx - 7), (s16)(cy - 7), SPR_CURSOR);
    if (sel != 0xFF) {
        struct part *p = &tbl.p[sel];
        spr_set(0, (s16)(p->x - 7), (s16)(p->y - 7), SPR_HANDLE);
        if (p->type == P_WALL || p->type == P_GATE) spr_set(1, (s16)(p->a - 7), (s16)(p->b - 7), SPR_HANDLE);
        else spr_hide(1);
    } else {
        spr_hide(0);
        spr_hide(1);
    }
}

/* ---------------------------------------------------------------- */
/* saving and trying */

static u8 choose_slot(void) {
    u8 s = slot ? slot : 13, c;
    panel_clear(25, 37);
    text_at(PANEL_COL + 2, 26, "save to slot (crsr up/down,", C_WHITE);
    text_at(PANEL_COL + 2, 27, "return; run/stop: no)", C_WHITE);
    for (;;) {
        char nm[NAME_LEN + 1];
        u8 i;
        text_num(PANEL_COL + 2, 29, s, 2, C_YELLOW);
        for (i = 0; i < NAME_LEN; i++) nm[i] = idx.e[s - 1].used ? idx.e[s - 1].name[i] : ' ';
        nm[NAME_LEN] = 0;
        text_at(PANEL_COL + 6, 29, idx.e[s - 1].used ? nm : "- empty -       ", idx.e[s - 1].used ? C_LRED : C_LGREEN);
        if (idx.e[s - 1].used && s != slot) text_at(PANEL_COL + 2, 30, "(it will be replaced)", C_LRED);
        else text_fill(PANEL_COL + 2, 30, 24, ' ', C_GREY);
        c = ui_key();
        if (c == CH_CURSOR_UP && s > 1) s--;
        else if (c == CH_CURSOR_DOWN && s < SLOTS) s++;
        else if (c == CH_RETURN) break;
        else if (c == CH_STOP || c == CH_ESC) { s = 0; break; }
    }
    panel_clear(25, 37);
    return s;
}

void save(void) {
#ifdef AUTOEDIT
    u8 s = 13, i;                      /* (tests: no questions) */
#else
    u8 s = choose_slot(), i;
#endif
    if (!s) return;
    text_at(PANEL_COL + 2, 26, "saving...", C_GREY);
    if (!table_save(s)) {
        text_at(PANEL_COL + 2, 26, "the disk said no", C_LRED);
        sid_fx(FX_ERROR);
        return;
    }
    if (!idx.e[s - 1].used || s != slot) idx.e[s - 1].best = 0;
    for (i = 0; i < NAME_LEN; i++) idx.e[s - 1].name[i] = tbl.h.name[i];
    idx.e[s - 1].used = 1;
    index_save();
    slot = s;
    mb.slot = s;
    dirty = 0;
    text_at(PANEL_COL + 2, 26, "saved.   ", C_LGREEN);
    sid_fx(FX_EXTRA);
}

void edit_name(void) {
    char buf[NAME_LEN + 1];
    u8 i;
    for (i = 0; i < NAME_LEN; i++) buf[i] = tbl.h.name[i];
    buf[NAME_LEN] = 0;
    text_at(PANEL_COL + 2, 26, "the table's name:", C_WHITE);
    if (ui_input(PANEL_COL + 2, 27, buf, NAME_LEN)) {
        undo_keep();
        for (i = 0; i < NAME_LEN; i++) tbl.h.name[i] = buf[i] ? buf[i] : ' ';
        for (i = 0; i < NAME_LEN && buf[i]; i++) {}
        for (; i < NAME_LEN; i++) tbl.h.name[i] = ' ';
    }
    panel_clear(25, 37);
}

void edit_text(void) {
    char buf[TEXT_LEN];
    struct part *p;
    u8 i, k;
    if (sel == 0xFF || tbl.p[sel].type != P_TEXT) return;
    p = &tbl.p[sel];
    k = (u8)(p->a & 7);
    for (i = 0; i < TEXT_LEN - 1; i++) buf[i] = tbl.text[k][i];
    buf[TEXT_LEN - 1] = 0;
    text_at(PANEL_COL + 2, 26, "its text:", C_WHITE);
    if (ui_input(PANEL_COL + 2, 27, buf, TEXT_LEN - 1)) {
        undo_keep();
        mark(sel);
        for (i = 0; i < TEXT_LEN; i++) tbl.text[k][i] = i < TEXT_LEN - 1 ? buf[i] : 0;
        mark(sel);
        redraw();
    }
    panel_clear(25, 37);
}

void try_it(void) {
    mb.edit[0] = (u8)cx;
    mb.edit[1] = (u8)(cx >> 8);
    mb.edit[2] = (u8)cy;
    mb.edit[3] = (u8)(cy >> 8);
    mb.edit[4] = add_type;
    mb.edit[5] = slot;
    mb.edit[6] = dirty;
    mb.edit[7] = fine;
    text_at(PANEL_COL + 2, 26, "drawing it for play...", C_GREY);
    for (cx = 0; cx < 8; cx++) spr_hide((u8)cx);
    prepare();
    mb.mode = MB_TEST;
    mb.players = 1;
    mb.slot = slot;
    run("play");
}


/* the chosen part changed by k (size, turn, colour, ...) */
void change(u8 k, s8 d) {
    struct part *p;
    s16 *v;
    if (sel == 0xFF) return;
    undo_keep();
    p = &tbl.p[sel];
    mark(sel);
    switch (k) {
    case 's':                                  /* size */
        switch (p->type) {
        case P_BUMPER: p->a = (s16)(p->a + d); if (p->a < 9) p->a = 9; if (p->a > 22) p->a = 22; break;
        case P_POST: p->a = (s16)(p->a + d); if (p->a < 3) p->a = 3; if (p->a > 8) p->a = 8; break;
        case P_FLIPPER: p->a = (s16)(p->a + d * 2); if (p->a < 30) p->a = 30; if (p->a > 60) p->a = 60; break;
        case P_SLING: p->b = (s16)(p->b + d * 2); if (p->b < 24) p->b = 24; if (p->b > 70) p->b = 70; break;
        case P_SPINNER: p->b = (s16)(p->b + d); if (p->b < 6) p->b = 6; if (p->b > 20) p->b = 20; break;
        case P_ARC: p->a = (s16)(p->a + d * 2); if (p->a < 12) p->a = 12; if (p->a > 160) p->a = 160; break;
        case P_TEXT: v = &p->b; *v = (s16)(((*v >> 8) == 1 ? 2 : 1) << 8 | (*v & 0xFF)); break;
        case P_DECAL: p->a = (s16)(p->a + d * 4); p->b = (s16)(p->b + d * 4); if (p->a < 8) p->a = 8; if (p->b < 8) p->b = 8; break;
        case P_LAMP: p->a = (s16)((p->a & 0xFF00) | ((p->a + d) & 3)); break;
        }
        break;
    case 'r':                                  /* turn */
        switch (p->type) {
        case P_TARGET: case P_DROP: case P_KICKER: case P_SPINNER: p->a = (s16)((u8)(p->a + d * 8)); break;
        case P_LAMP: p->a = (s16)((p->a & 3) | ((u16)(u8)((p->a >> 8) + d * 8) << 8)); break;
        case P_FLIPPER: p->b = (s16)!p->b; break;
        case P_SLING: p->a = (s16)-p->a; break;
        case P_ARC: p->b = (s16)((u16)(u8)(p->b + d * 8) | ((u16)(u8)((p->b >> 8) + d * 8) << 8)); break;
        case P_DECAL: v = &p->a; { s16 t = *v; *v = p->b; p->b = t; } break;
        }
        break;
    case 'R':                                  /* an arc's length (F7) */
        if (p->type == P_ARC) p->b = (s16)((p->b & 0xFF) | ((u16)(u8)((p->b >> 8) + d * 8) << 8));
        break;
    case 'c':
        p->b = (s16)((p->b & ~15) | ((p->b + 1) & 15));
        break;
    case 'u':
        p->flags = (u8)((p->flags & 0xF0) | ((p->flags + 1) & 15));
        break;
    case 'y':
        if (p->type == P_DECAL) p->flags = (u8)((p->flags & ~0x70) | ((p->flags + 0x10) & 0x70));
        else p->flags = (u8)((p->flags & ~0x30) | ((p->flags + 0x10) & 0x30));
        break;
    case 'h':
        p->flags = (u8)((p->flags & ~0xC0) | ((p->flags + 0x40) & 0xC0));
        break;
    }
    mark(sel);
    redraw();
    sid_fx(FX_MENU);
}
