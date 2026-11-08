/* theme.h — the looks a table can have: colours and the playfield's pattern
 *
 * The palette is computed here into pal_r/g/b; the platform copies it to
 * the hardware (pal_commit). Lamps are palette slots: lighting one changes
 * three entries, so the art never has to be redrawn.
 */
#pragma once
#include "pb.h"
#include "table.h"

enum {
    PAT_PLAIN, PAT_STARS, PAT_CHECK, PAT_STRIPES, PAT_RAYS, PAT_DOTS,
    PAT_WOOD, PAT_GRID, PAT_WAVES, PAT_BRICKS, PAT_FLAMES, PAT_DIAMONDS,
    PAT_COUNT
};

struct theme {
    char name[10];
    u8 pattern;
    u8 bg_top[3], bg_bot[3], pat[3];
    u8 wall[3], rubber[3], flip[3], accent[3], disp[3];
};

#define NTHEMES 12
extern const struct theme *theme;    /* the table's (a copy of the chosen one) */

extern u8 pal_r[256], pal_g[256], pal_b[256];

/* the lamp slots: their colour (colour_rgb) and brightness 0-16 */
extern u8 slot_colour[NSLOTS];
extern u8 slot_level[NSLOTS];

void theme_set(u8 t);                /* the whole palette */
void slot_put(u8 s);                 /* one slot's three entries */
void gi_level(u8 k);                 /* the playfield's light: 16 full, lower dimmer */
extern u8 gi;

/* the lamp slot each part shows (0xFF: none) */
extern u8 part_slot[MAX_PARTS];
void slots_assign(void);
void lamps_attract(u16 t);              /* the lights' show while nobody plays */
