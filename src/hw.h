/* hw.h — the MEGA65's registers and this program's memory map
 *
 * Chip RAM:
 *   $40000-$5DAFF  the table: 38 strips of 50 full-colour characters
 *   $5DB00-$5FFFF  flipper images (the raster rewrite buffer shows them)
 *   $12000-$1B0FF  PBGFX from disk: display glyphs, sprites, samples (assets.h)
 *   $1B100-$1D7AB  the screen: 50 rows of ROWC cells of 2 bytes
 *   $1D800-$1F7FF  flipper images ($1F800-$1FFFF: the C65's colour RAM)
 * Attic RAM:
 *   $8000000       the table as drawn (drop targets up): drop targets put back from it
 *   $8020000       the same with the drop targets down
 *   $8040000       flipper images: $20000 for each flipper (FLIP_STORE)
 */
#pragma once
#include "pb.h"

#define REG(a) (*(volatile u8 *)(a))
#define FRAMECOUNT REG(0xD7FA)

#define TABLE_LIN  0x0040000UL
#define TABLE_CHAR 0x1000                    /* TABLE_LIN / 64 */
#define SCREEN_LIN 0x001B100UL
#define COLOR_LIN  0xFF80000UL
#define GFX_LIN    0x0012000UL
#define ATTIC_A    0x8000000UL
#define ATTIC_B    0x8020000UL
#define ATTIC_FLIP 0x8040000UL

#define COLS 80                              /* cells shown on a row */
#define ROWS 50
/* a row: 80 cells, then the raster rewrite buffer's: two flippers (a GOTOX
 * cell and up to FL_NK characters each), a GOTOX to the right edge */
#define FL_NK 8
#define ROWC (COLS + 2 * (1 + FL_NK) + 1)
#define FLBUF_A 0x001D800UL                  /* flipper images: two areas */
#define FLBUF_A_END 0x001F800UL
#define FLBUF_B 0x005DB00UL
#define FLBUF_B_END 0x0060000UL
#define PANEL_COL 39                         /* the panel: columns 39-79 */

/* system colours for text */
#define C_BLACK  0
#define C_WHITE  1
#define C_RED    2
#define C_CYAN   3
#define C_PURPLE 4
#define C_GREEN  5
#define C_BLUE   6
#define C_YELLOW 7
#define C_ORANGE 8
#define C_BROWN  9
#define C_LRED   10
#define C_DGREY  11
#define C_GREY   12
#define C_LGREEN 13
#define C_LBLUE  14
#define C_LGREY  15
