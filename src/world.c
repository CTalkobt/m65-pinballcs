/* world.c — the table made ready for play, handed from PINBALL to PLAY
 *
 * PINBALL builds the colliders, the grid, the flippers' boxes, the lamp
 * slots and the drop targets' boxes, and saves them in attic RAM; PLAY
 * loads them into its own variables (the two programs keep them at
 * different addresses: the same list of them is copied, in order). The
 * grid's items are at $1500 in both (low RAM survives the change of
 * program), the art and the flippers' images are in attic RAM already, the
 * palettes in the VIC.
 */
#include "hw.h"
#include "hi.h"
#include "physics.h"
#include "theme.h"
#include "video.h"
#include "world.h"

#define WORLD_LIN 0x8078000UL

HI struct dropbox drops[MAX_DROPS];
u8 ndrops;

extern u16 cell_start[];
#define CELLS_BYTES ((19 * 26 + 1) * 2)

static u32 at;
static u8 saving;

static void move(void *p, u16 n) {
    if (saving) video_copy(at, (u32)(u16)p, n);
    else video_copy((u32)(u16)p, at, n);
    at += n;
}

static void *const what[] = {
    coll, &ncoll, flips, &nflips, sens, &nsens, &plunger_x, &plunger_y, cell_start,
    part_slot, slot_colour, &nfl, fl_part, fl_nk, fl_nr, fl_row0, fl_slot, fl_x0, fl_buf, drops, &ndrops,
};
static const u16 sizes[] = {
    sizeof coll, 1, sizeof flips, 1, sizeof sens, 1, 2, 2, CELLS_BYTES,
    sizeof part_slot, sizeof slot_colour, 1, 4, 4, 4, 4, 4, 8, 16, sizeof drops, 1,
};

static void xfer(void) {
    u8 i;
    at = WORLD_LIN;
    for (i = 0; i < sizeof sizes / sizeof sizes[0]; i++) move(what[i], sizes[i]);
}

void world_save(void) {
    saving = 1;
    xfer();
}

void world_load(void) {
    saving = 0;
    xfer();
}
