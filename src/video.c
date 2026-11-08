/* video.c — the screen: 80 x 50 cells at 640 x 400 (VIC-IV, H640 V400),
 * and the flippers on the raster rewrite buffer
 *
 * Cells use 16-bit character numbers (CHR16); numbers above $FF are full
 * colour (FCLRHI: 64 bytes, a byte a pixel, the byte a palette entry), the
 * rest are the ROM's font in a colour. The table is columns 0-37: strip c
 * is characters TABLE_CHAR + c * 50 .. + 49, top to bottom, so each strip
 * of 8 x 400 pixels is 3200 contiguous bytes, which one DMA job moves.
 * The panel (columns 39-79) is text, and the score display's glyphs, which
 * are full-colour characters too (assets.h).
 *
 * The flippers are not in the table's characters. Each row has ROWC cells:
 * the 80 shown, then the raster rewrite buffer's. A GOTOX cell (colour RAM
 * bit 4) moves the drawing back to a flipper's x, with its pixels of value
 * 0 transparent (bit 7), and the flipper's characters for that row follow;
 * a last GOTOX to x 640 lets the rest of the row show (the VIC-IV shows
 * only what is left of the last character drawn). A flipper's characters
 * are a buffer in chip RAM, numbered down its columns as the table's are;
 * moving it copies the image of its new position from attic RAM there.
 *
 * The DMA jobs are the F018B's, with its options: the source and
 * destination megabytes (attic RAM is megabyte $80).
 */
#include <dma.h>
#include "hw.h"
#include "hi.h"
#include "video.h"
#include "theme.h"
#include "geom.h"
#include "table.h"
#include "assets.h"

static u8 pal_screen, pal_sprite;
static u16 textx, texty;
u16 spr_y0 = 104;

/* ---------------------------------------------------------------- */
/* DMA */

static u8 job[21] = {
    0x0B, 0x80, 0, 0x81, 0, 0x06, 0x86, 0x00, 0x00,   /* options */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0                 /* +9 the job */
};

static void dma(u8 cmd, u32 dst, u32 src, u16 len, u8 transparent) {
    if (!len) return;
    job[2] = (u8)(src >> 20);
    job[4] = (u8)(dst >> 20);
    job[5] = (u8)(transparent ? 0x07 : 0x06);
    job[9] = cmd;
    job[10] = (u8)len;
    job[11] = (u8)(len >> 8);
    job[12] = (u8)src;
    job[13] = (u8)(src >> 8);
    job[14] = (u8)((src >> 16) & 15);
    job[15] = (u8)dst;
    job[16] = (u8)(dst >> 8);
    job[17] = (u8)((dst >> 16) & 15);
    REG(0xD702) = 0;
    REG(0xD704) = 0;
    REG(0xD701) = (u8)((u16)job >> 8);
    REG(0xD705) = (u8)(u16)job;
}

#define COPY(d, s, n) dma(0, (d), (s), (n), 0)
#define NEAR(p) ((u32)(u16)(p))

/* ---------------------------------------------------------------- */

void video_wait(void) {
    u8 f = FRAMECOUNT;
    while (FRAMECOUNT == f) {}
}

static u8 nib(u8 v) {
    return (u8)(v << 4 | v >> 4);
}

void pal_entries(u8 from, u8 n) {
    REG(0xD070) = pal_screen;
    while (n--) {
        REG(0xD100 + from) = nib(pal_r[from]);
        REG(0xD200 + from) = nib(pal_g[from]);
        REG(0xD300 + from) = nib(pal_b[from]);
        from++;
    }
}

void pal_commit(void) {
    pal_entries(16, 240);
}

void pal_slots(void) {
    pal_entries(PAL_LAMP, NSLOTS * 3);
}

void spr_colours(u8 n, u8 r, u8 g, u8 b) {
    u8 i, e = (u8)(n * 16);
    REG(0xD070) = pal_sprite;
    for (i = 1; i < 16; i++) {
        /* a ramp: dark to the colour to white */
        u8 k = (u8)(i * 17);
        u8 rr, gg, bb;
        if (i < 11) {
            rr = (u8)(((u16)r * k) / 187);
            gg = (u8)(((u16)g * k) / 187);
            bb = (u8)(((u16)b * k) / 187);
        } else {
            rr = (u8)(r + ((255 - r) * (u16)(i - 10)) / 6);
            gg = (u8)(g + ((255 - g) * (u16)(i - 10)) / 6);
            bb = (u8)(b + ((255 - b) * (u16)(i - 10)) / 6);
        }
        REG(0xD100 + e + i) = nib(rr);
        REG(0xD200 + e + i) = nib(gg);
        REG(0xD300 + e + i) = nib(bb);
    }
    REG(0xD070) = pal_screen;
}

/* the mode: 80 x 50 cells, 640 x 400, 16-bit characters (the screen's
 * contents are left as they are: video_layout sets them) */
void video_on(void) {
    u8 bank;
    REG(0xD02F) = 0x47;                      /* the VIC-IV's registers */
    REG(0xD02F) = 0x53;
    REG(0xD020) = 0;
    REG(0xD021) = 0;
    REG(0xD05D) = REG(0xD05D) & 0x7F;        /* hot registers off */
    REG(0xD031) = REG(0xD031) | 0xC8;        /* H640, V400, FAST */
    REG(0xD054) = REG(0xD054) | 0x55;        /* CHR16, FCLRHI, SPR640, VFAST */
    REG(0xD058) = (u8)(ROWC * 2);           /* bytes a row */
    REG(0xD059) = 0;
    REG(0xD05E) = ROWC;                      /* characters a row: the rewrite buffer's too */
    REG(0xD07B) = ROWS;
    REG(0xD060) = (u8)SCREEN_LIN;
    REG(0xD061) = (u8)(SCREEN_LIN >> 8);
    REG(0xD062) = (u8)(SCREEN_LIN >> 16);
    REG(0xD063) = REG(0xD063) & 0xF0;
    REG(0xD064) = 0;
    REG(0xD065) = 0;
    if (!textx) {
        textx = REG(0xD04C) | (u16)(REG(0xD04D) & 0x0F) << 8;
        texty = REG(0xD04E) | (u16)(REG(0xD04F) & 0x0F) << 8;
    }
    spr_y0 = texty;
    bank = (u8)((REG(0xD070) >> 4) & 3);
    pal_sprite = (u8)(((bank + 1) & 3) << 6 | bank << 4 | ((bank + 1) & 3) << 2 | bank);
    pal_screen = (u8)(bank << 6 | bank << 4 | ((bank + 1) & 3) << 2 | bank);
    REG(0xD070) = pal_screen;
}

void video_shake(s8 dx, s8 dy) {
    u16 x = (u16)(textx + dx), y = (u16)(texty + dy);
    REG(0xD04C) = (u8)x;
    REG(0xD04D) = (u8)((REG(0xD04D) & 0xF0) | ((x >> 8) & 0x0F));
    REG(0xD04E) = (u8)y;
    REG(0xD04F) = (u8)((REG(0xD04F) & 0xF0) | ((y >> 8) & 0x0F));
    spr_y0 = y;
}

static HI u16 vrow[3][ROWC];                  /* a row of cells (three for the glyphs) */

#define GOTOX_EDGE 640                     /* a GOTOX to the right edge */
#define CR_GOTOX 0x0010                      /* colour RAM byte 0: GOTOX */
#define CR_GOTOX_T 0x0090                    /*   and transparent pixels */

void video_layout(void) {
    u16 *row = vrow[0], *col = vrow[1];
    u8 r, c;
    for (c = 0; c < ROWC; c++) col[c] = c < COLS ? 0 : CR_GOTOX;
    for (r = 0; r < ROWS; r++) {
        for (c = 0; c < ROWC; c++) row[c] = c < STRIPS ? (u16)(TABLE_CHAR + c * 50 + r) : c < COLS ? 32 : GOTOX_EDGE;
        COPY(SCREEN_LIN + (u16)r * ROWC * 2, NEAR(row), ROWC * 2);
        COPY(COLOR_LIN + (u16)r * ROWC * 2, NEAR(col), ROWC * 2);
    }
}

/* ---------------------------------------------------------------- */
/* the table */

void restore_box(s16 x0, s16 y0, s16 x1, s16 y1, u8 from) {
    s16 c;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > TW - 1) x1 = TW - 1;
    if (y1 > TH - 1) y1 = TH - 1;
    if (x0 > x1 || y0 > y1) return;
    for (c = (s16)(x0 >> 3); c <= (x1 >> 3); c++) {
        u32 off = (u32)c * STRIP_BYTES + (u16)y0 * 8;
        COPY(TABLE_LIN + off, (from ? ATTIC_B : ATTIC_A) + off, (u16)((y1 - y0 + 1) * 8));
    }
}

/* the whole table put back from attic RAM (as drawn: no flippers) */
void table_restore(void) {
    COPY(TABLE_LIN, ATTIC_A, 0xFFFF);
    COPY(TABLE_LIN + 0xFFFFUL, ATTIC_A + 0xFFFFUL, (u16)(STRIPS * STRIP_BYTES - 0xFFFFUL));
}

/* flippers: their boxes (from the table), their images in attic RAM
 * (prep.c draws them), the buffer each one is shown from, which image */
u8 nfl, fl_frame[4];
u8 fl_part[4], fl_nk[4], fl_nr[4], fl_row0[4], fl_slot[4];
s16 fl_x0[4];
u32 fl_buf[4];
static u8 fl_drawn[4];

void flippers_init(void) {
    u8 i;
    s16 x0, y0, x1, y1;
    u32 a = FLBUF_A, b = FLBUF_B;
    nfl = 0;
    for (i = 0; i < tbl.h.nparts && nfl < 4; i++)
        if (tbl.p[i].type == P_FLIPPER) {
            u16 size;
            flipper_box(&tbl.p[i], &x0, &y0, &x1, &y1);
            fl_part[nfl] = i;
            fl_x0[nfl] = x0;
            fl_nk[nfl] = (u8)((x1 - x0) >> 3);
            if (fl_nk[nfl] > FL_NK) fl_nk[nfl] = FL_NK;
            fl_row0[nfl] = (u8)(y0 >> 3);
            fl_nr[nfl] = (u8)((y1 - y0 + 1) >> 3);
            size = (u16)(fl_nk[nfl] * fl_nr[nfl] * 64);
            /* its buffer: in the first area with room (none: not shown) */
            if (a + size <= FLBUF_A_END) { fl_buf[nfl] = a; a += size; }
            else if (b + size <= FLBUF_B_END) { fl_buf[nfl] = b; b += size; }
            else continue;
            fl_frame[nfl] = 0;
            fl_drawn[nfl] = 0xFF;
            nfl++;
        }
}

/* the rows' rewrite cells: each flipper on the rows it covers, in the first
 * slot free on all of them (two a row); every other cell a GOTOX to the edge */
void flippers_rrb(void) {
    u16 *scr = vrow[0], *col = vrow[1];
    u8 r, i, k, used[ROWS];
    for (r = 0; r < ROWS; r++) used[r] = 0;
    for (i = 0; i < nfl; i++) {
        u8 s;
        for (s = 0; s < 2; s++) {
            for (r = 0; r < fl_nr[i]; r++)
                if (used[fl_row0[i] + r] & (1 << s)) break;
            if (r == fl_nr[i]) break;
        }
        fl_slot[i] = s;
        if (s < 2)
            for (r = 0; r < fl_nr[i]; r++) used[fl_row0[i] + r] |= (u8)(1 << s);
    }
    for (r = 0; r < ROWS; r++) {
        for (k = 0; k < ROWC - COLS; k++) {
            scr[k] = GOTOX_EDGE;
            col[k] = CR_GOTOX;
        }
        for (i = 0; i < nfl; i++) {
            u16 *q = scr + fl_slot[i] * (1 + FL_NK);
            if (fl_slot[i] > 1 || r < fl_row0[i] || r >= fl_row0[i] + fl_nr[i]) continue;
            q[0] = (u16)fl_x0[i];
            col[fl_slot[i] * (1 + FL_NK)] = CR_GOTOX_T;
            for (k = 0; k < fl_nk[i]; k++) {
                q[1 + k] = (u16)(fl_buf[i] / 64 + k * fl_nr[i] + (r - fl_row0[i]));
                col[fl_slot[i] * (1 + FL_NK) + 1 + k] = 0;
            }
        }
        COPY(SCREEN_LIN + ((u16)r * ROWC + COLS) * 2, NEAR(scr), (ROWC - COLS) * 2);
        COPY(COLOR_LIN + ((u16)r * ROWC + COLS) * 2, NEAR(col), (ROWC - COLS) * 2);
    }
}

/* the flippers whose position changed: its image into its buffer */
void flippers_draw(u8 force) {
    u8 i;
    for (i = 0; i < nfl; i++)
        if (force || fl_frame[i] != fl_drawn[i]) {
            u16 size = (u16)(fl_nk[i] * fl_nr[i] * 64);
            COPY(fl_buf[i], FLIP_STORE(i) + (u32)fl_frame[i] * size, size);
            fl_drawn[i] = fl_frame[i];
        }
}

/* a DMA copy for prep.c */
void video_copy(u32 dst, u32 src, u16 len) {
    COPY(dst, src, len);
}

/* ---------------------------------------------------------------- */
/* the panel */

static u8 scr(u8 c) {                        /* PETSCII / ASCII to a screen code */
    if (c >= 0x41 && c <= 0x5A) return (u8)(c - 0x40);
    if (c >= 0x61 && c <= 0x7A) return (u8)(c - 0x60);
    if (c >= 0xC1 && c <= 0xDA) return (u8)(c - 0xC0);
    return c < 0x40 ? c : 32;
}

void cell_put(u8 col, u8 row, u16 ch, u8 colour) {
    u16 at = (u16)(row * ROWC + col) * 2;
    COPY(SCREEN_LIN + at, NEAR(&ch), 2);
    dma(3, COLOR_LIN + at + 1, colour, 1, 0);
}

void text_at(u8 col, u8 row, const char *s, u8 colour) {
    u16 *cells = vrow[0];
    u8 n = 0, i;
    while (s[n] && col + n < COLS) {
        cells[n] = scr((u8)s[n]);
        n++;
    }
    if (!n) return;
    COPY(SCREEN_LIN + (u16)(row * ROWC + col) * 2, NEAR(cells), (u16)(n * 2));
    /* the colours: every other byte of colour RAM */
    for (i = 0; i < n; i++) cells[i] = (u16)colour << 8;
    COPY(COLOR_LIN + (u16)(row * ROWC + col) * 2, NEAR(cells), (u16)(n * 2));
}

void text_fill(u8 col, u8 row, u8 n, u8 ch, u8 colour) {
    char *buf = (char *)vrow[1];
    u8 i;
    for (i = 0; i < n; i++) buf[i] = (char)ch;
    buf[n] = 0;
    text_at(col, row, buf, colour);
}

void text_num(u8 col, u8 row, u32 v, u8 digits, u8 colour) {
    char buf[11];
    u8 i = digits;
    buf[digits] = 0;
    while (i--) {
        buf[i] = (char)('0' + (u8)(v % 10));
        v /= 10;
        if (!v && i && digits > 9) break;
    }
    while (i && i < digits) buf[--i] = ' ';
    text_at(col, row, buf, colour);
}

void disp_at(u8 col, u8 row, const char *s, u8 n) {
    u16 (*cells)[ROWC] = vrow;
    u8 i, k, c, g;
    for (i = 0; i < n; i++) {
        c = (u8)*s;
        if (c) s++;
        if (c >= 0x61 && c <= 0x7A) c = (u8)(c - 0x20);
        if (c >= 0xC1 && c <= 0xDA) c = (u8)(c - 0x80);
        g = (c >= 32 && c < 96) ? glyph_of[c - 32] : 0;
        for (k = 0; k < 3; k++) {
            cells[k][i * 2] = (u16)(GLYPH_CHAR + glyph_cell[g * 6 + k]);
            cells[k][i * 2 + 1] = (u16)(GLYPH_CHAR + glyph_cell[g * 6 + 3 + k]);
        }
    }
    for (k = 0; k < 3; k++) {
        u16 at = (u16)((row + k) * ROWC + col) * 2;
        COPY(SCREEN_LIN + at, NEAR(cells[k]), (u16)(n * 4));
        dma(3, COLOR_LIN + at, 0, (u16)(n * 4), 0);
    }
}

void panel_clear(u8 row0, u8 row1) {
    u16 *cells = vrow[0];
    u8 r, i;
    for (i = 0; i < COLS - PANEL_COL; i++) cells[i] = 32;
    for (r = row0; r <= row1; r++)
        COPY(SCREEN_LIN + (u16)(r * ROWC + PANEL_COL) * 2, NEAR(cells), (COLS - PANEL_COL) * 2);
}

/* ---------------------------------------------------------------- */
/* sprites: 16 colours, 16 x 16, at the native resolution */

_Alignas(16) u16 spr_ptr[8];

void spr_on(void) {
    u8 n;
    REG(0xD015) = 0;
    for (n = 0; n < 8; n++) REG(0xD027 + n) = 0;
    REG(0xD06C) = (u8)(u16)spr_ptr;
    REG(0xD06D) = (u8)((u16)spr_ptr >> 8);
    REG(0xD06E) = 0x80;
    REG(0xD06B) = 0xFF;
    REG(0xD057) = 0xFF;
    REG(0xD055) = 0xFF;
    REG(0xD056) = 16;
    REG(0xD076) = 0xFF;
    REG(0xD01B) = 0;
    REG(0xD017) = 0;
    REG(0xD01D) = 0;

}

void spr_set(u8 n, s16 x, s16 y, u16 ptr) {
    u16 sx = (u16)(80 + x), sy = (u16)(spr_y0 + y);
    u8 bit = (u8)(1 << n);
    spr_ptr[n] = ptr;
    REG(0xD000 + n * 2) = (u8)sx;
    REG(0xD001 + n * 2) = (u8)sy;
    REG(0xD010) = (u8)((sx & 0x100) ? REG(0xD010) | bit : REG(0xD010) & ~bit);
    REG(0xD05F) = (u8)((sx & 0x200) ? REG(0xD05F) | bit : REG(0xD05F) & ~bit);
    REG(0xD077) = (u8)((sy & 0x100) ? REG(0xD077) | bit : REG(0xD077) & ~bit);
    REG(0xD078) = (u8)((sy & 0x200) ? REG(0xD078) | bit : REG(0xD078) & ~bit);
    REG(0xD015) = (u8)(REG(0xD015) | bit);
}

void spr_hide(u8 n) {
    REG(0xD015) = (u8)(REG(0xD015) & ~(1 << n));
}
