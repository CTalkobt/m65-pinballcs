/* editor.c — the construction set: placing, moving and shaping parts
 *
 * The table is shown as it will play. A cross (sprite 7) is the cursor;
 * the part chosen has handles (sprites 0 and 1) on its points. Every change
 * redraws the strips the part covered and now covers; the flippers are
 * drawn again over them. Q undoes the last change (a copy of the table in
 * attic RAM). P tries the table: PLAY runs it and comes back here.
 */
#include <input.h>
#include <mega65.h>
#include "hw.h"
#include "assets.h"
#include "table.h"
#include "theme.h"
#include "render.h"
#include "geom.h"
#include "video.h"
#include "sound.h"
#include "music.h"
#include "menu_tunes.h"
#include "files.h"
#include "shared.h"
#include "ui.h"
#include "edint.h"


s16 cx = 140, cy = 200;                       /* the cursor */
u8 add_type = P_BUMPER, sel = 0xFF, grab, fine, slot, dirty;
s16 wall_x = -1, wall_y;                      /* a wall's first point, placed */

void undo_keep(void) {
    video_copy(UNDO_LIN, (u32)(u16)&tbl, sizeof tbl);
    dirty = 1;
}

/* ---------------------------------------------------------------- */
/* redrawing */

s16 rx0, rx1;                                  /* the strips to draw again */

void mark(u8 i) {
    s16 x0, y0, x1, y1;
    if (i >= tbl.h.nparts) return;
    part_box(&tbl.p[i], &x0, &y0, &x1, &y1);
    if (tbl.p[i].type == P_FLIPPER) {
        flipper_box(&tbl.p[i], &x0, &y0, &x1, &y1);
        x1++;
    }
    if (x0 < rx0) rx0 = x0;
    if (x1 > rx1) rx1 = x1;
}

void redraw(void) {
    u8 i;
    if (rx1 < rx0) return;
    if (rx0 < 0) rx0 = 0;
    if (rx1 > TW - 1) rx1 = TW - 1;
    render_begin();
    redraw_strips((u8)(rx0 >> 3), (u8)(rx1 >> 3));
    {
        /* the flippers at rest again, on the rewrite buffer (one may have
         * been moved, changed, added or deleted) */
        u8 k;
        flippers_init();
        for (i = 0; i < nfl; i++) {
            s16 y0 = (s16)(fl_row0[i] * 8);
            u16 bytes = (u16)(fl_nr[i] * 64);
            for (k = 0; k < fl_nk[i]; k++) {
                render_flipper(&tbl.p[fl_part[i]], 0, (s16)(fl_x0[i] + k * 8), y0, (s16)(y0 + fl_nr[i] * 8 - 1));
                video_copy(FLIP_STORE(i) + (u32)k * bytes, (u32)(u16)render_buf + (u16)y0 * 8, bytes);
            }
        }
        flippers_rrb();
    }
    flippers_draw(1);
    rx0 = 999;
    rx1 = -999;
}

static void retheme(void) {
    palette();
    rx0 = 0;
    rx1 = TW - 1;
    redraw();
}

/* ---------------------------------------------------------------- */
/* changes */

static u8 nearest(void) {
    u8 i, best = 0xFF;
    s32 bd = 0x7FFFFFFFL;
    for (i = 0; i < tbl.h.nparts; i++) {
        struct part *p = &tbl.p[i];
        s16 dx = (s16)(p->x - cx), dy = (s16)(p->y - cy);
        s32 d = MUL(dx, dx) + MUL(dy, dy);
        if (p->type == P_WALL || p->type == P_GATE) {
            /* the nearer end, or the middle */
            s16 ex = (s16)(p->a - cx), ey = (s16)(p->b - cy), mx = (s16)((p->x + p->a) / 2 - cx), my = (s16)((p->y + p->b) / 2 - cy);
            s32 e = MUL(ex, ex) + MUL(ey, ey), m = MUL(mx, mx) + MUL(my, my);
            if (e < d) d = e;
            if (m < d) d = m;
        }
        if (i == sel) d += 40;                 /* again: the next one near */
        if (d < bd) { bd = d; best = i; }
    }
    return bd < 400 ? best : 0xFF;
}

static void defaults(struct part *p) {
    p->flags = 0;
    p->a = 0;
    p->b = 0;
    switch (p->type) {
    case P_BUMPER: p->a = 14; p->b = 0; p->flags = 1; break;
    case P_POST: p->a = 4; p->flags = WS_RUBBER << 4; break;
    case P_SLING: p->a = cx < TW / 2 ? 26 : -26; p->b = 44; break;
    case P_FLIPPER: p->a = 46; p->b = cx > TW / 2; break;
    case P_TARGET: p->a = cx < TW / 2 ? 0 : 128; p->b = 2; p->flags = 3; break;
    case P_DROP: p->a = 64 * 3; p->b = 0; p->flags = 4; break;
    case P_ROLLOVER: p->b = 2; p->flags = 2; break;
    case P_KICKER: p->a = 64; p->b = 4; break;
    case P_SPINNER: p->a = 0; p->b = 11; break;
    case P_LAMP: p->a = LS_ROUND; p->b = 1; break;
    case P_TEXT: p->a = 0; p->b = 1 << 8; break;
    case P_DECAL: p->a = 60; p->b = 40; p->flags = 1 << 5; break;
    case P_ARC: p->a = 40; p->b = 192 | (0 << 8); p->flags = 1 << 6; break;
    case P_WALL: case P_GATE: p->flags = 1 << 6; break;
    }
}

static void add(void) {
    struct part *p;
    if (tbl.h.nparts >= MAX_PARTS) { sid_fx(FX_ERROR); return; }
    if ((add_type == P_WALL || add_type == P_GATE) && wall_x < 0) {
        wall_x = cx;                           /* the first end; RETURN again for the other */
        wall_y = cy;
        sid_fx(FX_MENU);
        return;
    }
    undo_keep();
    p = &tbl.p[tbl.h.nparts];
    p->type = add_type;
    p->x = cx;
    p->y = cy;
    defaults(p);
    if (add_type == P_WALL || add_type == P_GATE) {
        p->x = wall_x;
        p->y = wall_y;
        p->a = cx;
        p->b = cy;
        wall_x = -1;
    }
    if (add_type == P_TEXT) {
        u8 k;
        for (k = 0; k < NTEXTS - 1 && tbl.text[k][0]; k++) {}
        if (!tbl.text[k][0]) {
            tbl.text[k][0] = 'T'; tbl.text[k][1] = 'E'; tbl.text[k][2] = 'X'; tbl.text[k][3] = 'T'; tbl.text[k][4] = 0;
        }
        p->a = k;
    }
    sel = tbl.h.nparts++;
    mark(sel);
    redraw();
    sid_fx(FX_SELECT);
}

static void delete(void) {
    u8 i;
    if (sel == 0xFF) return;
    undo_keep();
    mark(sel);
    for (i = sel; i + 1 < tbl.h.nparts; i++) tbl.p[i] = tbl.p[i + 1];
    tbl.h.nparts--;
    sel = 0xFF;
    grab = 0;
    redraw();
    sid_fx(FX_ERROR);
}

/* the chosen part changed by k (size, turn, colour, ...) */

static void move_to(s16 x, s16 y) {
    struct part *p = &tbl.p[sel];
    mark(sel);
    if (grab == 2) {
        if (p->type == P_WALL || p->type == P_GATE) { p->a = x; p->b = y; }
        else if (p->type == P_ARC) {
            s16 dx = (s16)(x - p->x), dy = (s16)(y - p->y);
            p->a = (s16)isqrt32((u32)(MUL(dx, dx) + MUL(dy, dy)));
            if (p->a < 12) p->a = 12;
        }
    } else {
        s16 dx = (s16)(x - p->x), dy = (s16)(y - p->y);
        p->x = x;
        p->y = y;
        if (p->type == P_WALL || p->type == P_GATE) { p->a = (s16)(p->a + dx); p->b = (s16)(p->b + dy); }
    }
    mark(sel);
    redraw();
}

/* ---------------------------------------------------------------- */

void editor(u8 s) {
    u8 c, quit = 0, i;
    if (s) {
        slot = s;
        dirty = 0;
        if (!idx.e[s - 1].used) {
            /* a new table: the empty one */
            if (!table_load(0)) table_clear();
            dirty = 1;
        }
        cx = 140;
        cy = 200;
        sel = 0xFF;
        prepare();
    } else {
        /* back from trying it */
        cx = (s16)(mb.edit[0] | mb.edit[1] << 8);
        cy = (s16)(mb.edit[2] | mb.edit[3] << 8);
        add_type = mb.edit[4];
        slot = mb.edit[5];
        dirty = mb.edit[6];
        fine = mb.edit[7];
        sel = 0xFF;
        show_drawn();
    }
    mb.mode = MB_MENU;
    music(MUSIC_NONE, 0);
    grab = 0;
    wall_x = -1;
    rx0 = 999;
    rx1 = -999;
    panel_clear(0, 49);
    disp_at(PANEL_COL, 2, "  construction set  ", 20);
    info();
    help();
    video_copy(UNDO_LIN, (u32)(u16)&tbl, sizeof tbl);
    input_flush();
#ifdef AUTOEDIT
    /* (tests: a part chosen, moved and grown, as keys would) */
    cx = 168; cy = 112;
    sel = nearest();
    change('s', 1);
    change('s', 1);
    save();
    info();
#endif
    while (!quit) {
        handles();
        c = ui_key();
        if (c == CH_CURSOR_LEFT || c == CH_CURSOR_RIGHT || c == CH_CURSOR_UP || c == CH_CURSOR_DOWN) {
            s16 st = fine ? 1 : 4;
            if (c == CH_CURSOR_LEFT && cx - st >= 0) cx = (s16)(cx - st);
            if (c == CH_CURSOR_RIGHT && cx + st < TW) cx = (s16)(cx + st);
            if (c == CH_CURSOR_UP && cy - st >= 0) cy = (s16)(cy - st);
            if (c == CH_CURSOR_DOWN && cy + st < TH) cy = (s16)(cy + st);
            if (grab && sel != 0xFF) move_to(cx, cy);
            continue;
        }
        switch (c) {
        case CH_F1:
        case CH_F3:
            add_type = (u8)(c == CH_F1 ? (add_type > 1 ? add_type - 1 : P_TYPES - 1) : (add_type < P_TYPES - 1 ? add_type + 1 : 1));
            wall_x = -1;
            break;
        case CH_RETURN: add(); break;
        case ' ':
            sel = nearest();
            grab = 0;
            sid_fx(FX_MENU);
            break;
        case 'g':
        case 'e':
            if (sel != 0xFF) {
                if (grab) grab = 0;
                else {
                    undo_keep();
                    grab = (u8)(c == 'g' ? 1 : 2);
                    if (grab == 1) { cx = tbl.p[sel].x; cy = tbl.p[sel].y; }
                    else if (tbl.p[sel].type == P_WALL || tbl.p[sel].type == P_GATE) { cx = tbl.p[sel].a; cy = tbl.p[sel].b; }
                }
            }
            break;
        case CH_DELETE: delete(); break;
        case '+': change('s', 1); break;
        case '-': change('s', -1); break;
        case ',': change('r', -1); break;
        case '.': change('r', 1); break;
        case CH_F5: change('R', -1); break;
        case CH_F7: change('R', 1); break;
        case 'c': change('c', 1); break;
        case 'u': change('u', 1); break;
        case 'y': change('y', 1); break;
        case 'h': change('h', 1); break;
        case 'f': fine = (u8)!fine; break;
        case 't':
            undo_keep();
            tbl.h.theme = (u8)((tbl.h.theme + 1) % NTHEMES);
            retheme();
            break;
        case 'b':
            undo_keep();
            tbl.h.balls = (u8)(tbl.h.balls == 3 ? 5 : 3);
            break;
        case 'n': edit_name(); break;
        case 'x': edit_text(); break;
        case 'q':
            /* undo: the copy back, everything drawn again */
            video_copy((u32)(u16)&tbl, UNDO_LIN, sizeof tbl);
            sel = 0xFF;
            grab = 0;
            retheme();
            break;
        case 'p': try_it(); break;
        case 's': save(); break;
        case CH_STOP:
        case CH_ESC:
            if (dirty) {
                text_at(PANEL_COL + 2, 26, "not saved: run/stop again to", C_LRED);
                text_at(PANEL_COL + 2, 27, "leave it, s to save", C_LRED);
                c = ui_key();
                panel_clear(25, 37);
                if (c == 's') save();
                else if (c != CH_STOP && c != CH_ESC) break;
            }
            quit = 1;
            break;
        }
        info();
    }
    for (i = 0; i < 8; i++) spr_hide(i);
    /* the menu shows the table from its slot again */
    mb.drawn = 0;
}
