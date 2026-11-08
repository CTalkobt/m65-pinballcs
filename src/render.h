/* render.h — drawing the table, a strip (8 x 400 pixels) at a time
 *
 * render_buf is laid out as the screen's strip is (y * 8 + x % 8), so the
 * platform moves it to the screen with one DMA job. Everything is drawn
 * from the parts: the playfield's pattern, the inserts and their lamps,
 * the shadows, then what stands on the playfield. Flippers are not part of
 * the background: render_flipper draws one of their positions on its own
 * (on 0, which the screen's DMA leaves transparent).
 */
#pragma once
#include "pb.h"
#include "table.h"

extern u8 render_buf[STRIP_BYTES];

/* before drawing strips: the parts' boxes and lamp slots */
void render_begin(void);
/* strip c (0-37) into render_buf; drops: the drop targets down */
void render_strip(u8 c, u8 drops);
/* flipper p at position f (0 at rest .. FLIP_FRAMES-1 up): the 8 pixels
 * from x, rows y0..y1, into render_buf (as a strip), on 0: transparent
 * where the raster rewrite buffer lays it over the table (video.c) */
void render_flipper(const struct part *p, u8 f, s16 x, s16 y0, s16 y1);
