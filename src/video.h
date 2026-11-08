/* video.h — the MEGA65's screen: the table, the panel, sprites */
#pragma once
#include "pb.h"

void video_on(void);
void video_layout(void);                 /* the table's characters and an empty panel */
void video_wait(void);                   /* the next frame */
void video_shake(s8 dx, s8 dy);          /* the picture moved (a nudge) */

void pal_commit(void);                   /* the whole palette (theme.c's) */
void pal_slots(void);                    /* only the lamp slots */
void pal_entries(u8 from, u8 n);

/* part of the screen put back from a copy (0: A, 1: B); all of it */
void restore_box(s16 x0, s16 y0, s16 x1, s16 y1, u8 from);
void table_restore(void);
void video_copy(u32 dst, u32 src, u16 len);

/* flippers, on the raster rewrite buffer: boxes from the table (nk
 * columns, nr character rows, at x0 and row row0), each one's characters a
 * buffer; fl_frame[i] the image flipper i shows */
#define FLIP_STORE(i) (ATTIC_FLIP + (u32)(i) * 0x20000UL)
void flippers_init(void);                /* the boxes and the buffers */
void flippers_rrb(void);                 /* the rows' rewrite cells for them */
void flippers_draw(u8 force);            /* those whose image changed */
extern u8 nfl, fl_frame[4], fl_part[4], fl_nk[4], fl_nr[4], fl_row0[4], fl_slot[4];
extern s16 fl_x0[4];
extern u32 fl_buf[4];

/* text in the panel (ROM characters) and on the score display (glyphs) */
void text_at(u8 col, u8 row, const char *s, u8 colour);
void text_fill(u8 col, u8 row, u8 n, u8 ch, u8 colour);
void text_num(u8 col, u8 row, u32 v, u8 digits, u8 colour);
void disp_at(u8 col, u8 row, const char *s, u8 n);   /* n glyphs (padded) */
void panel_clear(u8 row0, u8 row1);
void cell_put(u8 col, u8 row, u16 ch, u8 colour);

/* sprites: 0-2 balls, 3-5 their shadows, 6 the plunger's knob, 7 the cursor */
void spr_on(void);
void spr_set(u8 n, s16 x, s16 y, u16 ptr);  /* table pixels, the sprite's top left */
void spr_hide(u8 n);
void spr_colours(u8 n, u8 r, u8 g, u8 b);   /* tint a sprite's 15 colours */
