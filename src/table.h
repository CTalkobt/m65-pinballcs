/* table.h — a pinball table as the construction set keeps it
 *
 * A table is a header, eight short texts and up to MAX_PARTS parts of ten
 * bytes. The file on disk is the same bytes, up to the last part used.
 * Coordinates are playfield pixels (0-303, 0-399).
 */
#pragma once
#include "pb.h"

#define TBL_MAGIC0  0x50             /* "PB" */
#define TBL_MAGIC1  0x42
#define TBL_VERSION 1
#define MAX_PARTS   160
#define NAME_LEN    16
#define NTEXTS      8
#define TEXT_LEN    16

enum {
    P_NONE,
    P_WALL,      /* x,y - a,b; flags: style (bits 4-5), thickness (bits 6-7) */
    P_ARC,       /* centre x,y; a radius; b start | end << 8 (clockwise); flags as a wall */
    P_BUMPER,    /* x,y; a radius; b colour; flags: group */
    P_POST,      /* x,y; a radius; flags: style */
    P_SLING,     /* x,y the top; a width (+ left sling, - right); b height */
    P_FLIPPER,   /* x,y the pivot; a length; b 0 left, 1 right */
    P_TARGET,    /* x,y; a the angle it faces; b colour; flags: group */
    P_DROP,      /* x,y; a the angle it faces; b colour; flags: group */
    P_ROLLOVER,  /* x,y; b colour; flags: group */
    P_KICKER,    /* x,y; a the angle it ejects to; b colour; flags: group, bit 4: lock */
    P_SPINNER,   /* x,y; a the angle of its axle; b half its width */
    P_LAMP,      /* x,y; a shape | angle << 8; b colour; flags: group */
    P_TEXT,      /* x,y the top left; a the text (0-7); b colour | size << 8 */
    P_GATE,      /* x,y - a,b: the ball passes it only from its right */
    P_PLUNGER,   /* x,y the top of the plunger at rest */
    P_DECAL,     /* x,y the centre; a width, b height; flags: shape (bit 4), shade (bits 5-6) */
    P_TYPES
};

/* flags: the group, and per type */
#define F_GROUP(f)   ((f) & 15)
#define F_STYLE(f)   (((f) >> 4) & 3)
#define F_THICK(f)   (((f) >> 6) & 3)
#define F_LOCK       0x10

/* wall styles */
#define WS_RAIL    0                 /* a metal rail */
#define WS_RUBBER  1                 /* white rubber: lively */
#define WS_PLASTIC 2                 /* a coloured plastic guide */
#define WS_WOOD    3                 /* the cabinet's wood */

/* groups: 1-12 are the table's own banks; these have meanings */
#define G_LOCK     13                /* lit: the lock is lit */
#define G_EXTRA    14                /* lit: the extra ball is lit */
#define G_MULT     15                /* the bonus multiplier's ladder */

/* lamp shapes */
#define LS_ROUND   0
#define LS_ARROW   1
#define LS_TRI     2
#define LS_OVAL    3

struct part {
    u8 type, flags;
    s16 x, y, a, b;
};

struct table_hdr {
    u8 magic[2];
    u8 version;
    u8 theme;
    char name[NAME_LEN];             /* screen codes, padded with spaces */
    u8 balls;                        /* 3 or 5 */
    u8 options;                      /* bit 0: ball save */
    u8 nparts;
    u8 spare[9];
};

struct table {
    struct table_hdr h;
    char text[NTEXTS][TEXT_LEN];     /* screen codes, ending at 0 or the end */
    struct part p[MAX_PARTS];
};

#define TABLE_SIZE(t) (sizeof(struct table_hdr) + NTEXTS * TEXT_LEN + (u16)(t)->h.nparts * sizeof(struct part))

extern struct table tbl;

/* the lamp colours parts name (b): 16 of them */
#define NCOLOURS 16
extern const u8 colour_rgb[NCOLOURS][3];

/* a part's box (inclusive), shadow included (geom.c) */
void part_box(const struct part *p, s16 *x0, s16 *y0, s16 *x1, s16 *y1);
/* checks a table loaded from disk: 1 if it can be used */
u8 table_check(void);
void table_clear(void);
