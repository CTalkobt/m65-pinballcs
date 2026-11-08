/* prep.c — drawing a table for play (PINBALL): every strip of its art to
 * the screen and to attic RAM (copy A as drawn, copy B with the drop
 * targets down), the flippers' images, the palette */
#include "hw.h"
#include "table.h"
#include "theme.h"
#include "render.h"
#include "geom.h"
#include "video.h"
#include "shared.h"
#include "ui.h"
#include "physics.h"
#include "world.h"


#define NEAR(p) ((u32)(u16)(p))

static void strip_out(u8 c, u8 where) {
    u32 off = (u32)c * STRIP_BYTES;
    if (where & 1) video_copy(TABLE_LIN + off, NEAR(render_buf), STRIP_BYTES);
    if (where & 2) video_copy(ATTIC_A + off, NEAR(render_buf), STRIP_BYTES);
    if (where & 4) video_copy(ATTIC_B + off, NEAR(render_buf), STRIP_BYTES);
}

static u8 strip_has_drop(u8 c) {
    u8 i;
    s16 x0, y0, x1, y1;
    for (i = 0; i < tbl.h.nparts; i++)
        if (tbl.p[i].type == P_DROP) {
            part_box(&tbl.p[i], &x0, &y0, &x1, &y1);
            if (x1 >= c * 8 && x0 <= c * 8 + 7) return 1;
        }
    return 0;
}

void palette(void) {
    u8 s;
    render_begin();
    for (s = 0; s < NSLOTS; s++) slot_level[s] = 0;
    theme_set(tbl.h.theme);
    pal_commit();
}

/* the flippers' images: FLIP_FRAMES of each one's box, its columns one
 * after the other (a buffer's characters, numbered down the columns) */
static void flipper_images(void) {
    u8 i, f, k;
    flippers_init();
    for (i = 0; i < nfl; i++) {
        const struct part *p = &tbl.p[fl_part[i]];
        u32 at = FLIP_STORE(i);
        s16 y0 = (s16)(fl_row0[i] * 8), y1 = (s16)(y0 + fl_nr[i] * 8 - 1);
        u16 bytes = (u16)(fl_nr[i] * 64);
        for (f = 0; f < FLIP_FRAMES; f++)
            for (k = 0; k < fl_nk[i]; k++) {
                render_flipper(p, f, (s16)(fl_x0[i] + k * 8), y0, y1);
                video_copy(at, NEAR(render_buf) + (u16)y0 * 8, bytes);
                at += bytes;
            }
    }
    flippers_rrb();
}

void prepare(void) {
    u8 c;
    palette();
    for (c = 0; c < STRIPS; c++) {
        render_strip(c, 0);
        strip_out(c, 3);
        if (strip_has_drop(c)) render_strip(c, 1);
        strip_out(c, 4);
    }
    flipper_images();
    flippers_draw(1);
    phys_build();
    world_save();
    mb.drawn = 1;
}

/* strips c0..c1 again (the editor): screen and copy A */
void redraw_strips(u8 c0, u8 c1) {
    u8 c;
    for (c = c0; c <= c1 && c < STRIPS; c++) {
        render_strip(c, 0);
        strip_out(c, 3);
    }
    mb.drawn = 0;
}

/* the table as last drawn, from attic RAM */
void show_drawn(void) {
    palette();
    table_restore();
    flippers_init();
    flippers_rrb();
    flippers_draw(1);
}
