/* rbg.c — the renderer's playfield (its gradient and pattern) and text */
#include "render.h"
#include "rint.h"
#include "geom.h"
#include "theme.h"

#define GEN_TEXT
#include "gen_render.h"

/* ---------------------------------------------------------------- */
/* the playfield */

static const u8 __noescape bayer[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
static const s8 __noescape wave_t[32] = {0, 1, 2, 3, 4, 4, 5, 5, 5, 5, 5, 4, 4, 3, 2, 1,
                                         0, -1, -2, -3, -4, -4, -5, -5, -5, -5, -5, -4, -4, -3, -2, -1};

static u8 hash(s16 x, s16 y) {
    u16 h = (u16)((u16)x * 31421u + (u16)y * 6927u + 1);
    h ^= h >> 7;
    h *= 2053u;
    return (u8)(h >> 8);
}

static u8 pattern(s16 x, s16 y) {
    s16 dx, dy, u, v;
    u8 h;
    switch (theme->pattern) {
    case PAT_STARS:
        h = hash(x >> 4, y >> 4);
        if ((h & 7) == 0) {
            dx = (s16)((x & 15) - (h >> 4));
            dy = (s16)((y & 15) - ((h >> 1) & 7) - 4);
            if ((dx == 0 && dy >= -2 && dy <= 2) || (dy == 0 && dx >= -2 && dx <= 2)) return 1;
        }
        return (u8)(hash(x, y) > 252);
    case PAT_CHECK:
        u = (s16)(x & 15);
        v = (s16)(y & 15);
        if (((x >> 4) ^ (y >> 4)) & 1) { dx = u; u = v; v = dx; }
        return (u8)(u >= 6 && u <= 9 && v >= 2 && v <= 13);
    case PAT_STRIPES:
        return (u8)(((x + y) % 26) < 6);
    case PAT_RAYS:
        dx = (s16)(x - TW / 2);
        dy = (s16)(y - 150);
        u = dx < 0 ? -dx : dx;
        v = dy < 0 ? -dy : dy;
        if (u + v < 6) return 1;
        h = (u8)(mul32(dy < 0 ? -dx : dx, 128) / (u + v) + (dy < 0 ? 0 : 8));
        return (u8)((h & 16) != 0);
    case PAT_DOTS:
        dx = (s16)(((x + ((y / 20) & 1) * 10) % 20) - 10);
        dy = (s16)((y % 20) - 10);
        return (u8)(dx * dx + dy * dy < 16);
    case PAT_WOOD:
        if (x % 38 == 0) return 1;
        u = (s16)((x * 3 + wave_t[(y >> 3) & 31] + wave_t[(x + (y >> 5)) & 31] / 2) % 13);
        return (u8)(u == 0 || (u == 7 && hash(x, y >> 2) > 140));
    case PAT_GRID:
        return (u8)((x % 24) == 0 || (y % 24) == 0);
    case PAT_WAVES:
        return (u8)(((y + wave_t[((x >> 2) + (y >> 4) * 5) & 31]) % 22) < 3);
    case PAT_BRICKS:
        if (y % 14 == 0) return 1;
        return (u8)((x + ((y / 14) & 1) * 14) % 28 == 0);
    case PAT_FLAMES:
        u = (s16)(260 + wave_t[(x >> 1) & 31] * 6 + wave_t[(x * 3 + 7) & 31] * 3);
        return (u8)(y > u && ((y - u) < 30 || hash(x >> 1, y >> 1) > 90));
    case PAT_DIAMONDS:
        dx = (s16)((x % 32) - 16);
        dy = (s16)((y % 48) - 24);
        if (dx < 0) dx = -dx;
        if (dy < 0) dy = -dy;
        return (u8)((dx * 3 + dy * 2) < 40 && ((x / 32 + y / 48) & 1));
    }
    return 0;
}

void playfield(void) {
    s16 x, y;
    u8 band, *q = render_buf;
    u16 f;
    for (y = 0; y < TH; y++) {
        f = (u16)((u16)y * 240u / (TH - 1));
        for (x = sx; x < sx + 8; x++) {
            band = (u8)((f + bayer[((y & 3) << 2) | (x & 3)]) >> 4);
            if (band > 15) band = 15;
            *q++ = (u8)((pattern(x, y) ? PAL_PAT : PAL_BG) + band);
        }
    }
}

/* text: 2-bit glyphs, a gradient down them, a dark shadow */
void text(s16 x, s16 y, const char *s, u8 ramp, u8 size) {
    s16 w = text_width(s, size), gx, gy, px, py;
    u8 c, sh, i;
    if (!clip((s16)(x - 1), (s16)(y - 1), (s16)(x + w + 2), (s16)(y + 12 * size + 2))) return;
    for (sh = 0; sh < 2; sh++) {
        const char *p = s;
        s16 at = x;
        while ((c = (u8)*p++) != 0) {
            c = (u8)(text_code(c) - 32);
            for (gy = 0; gy < 12; gy++) {
                u16 row = font_g[c * 12 + gy];
                for (gx = 0; gx < font_w[c]; gx++) {
                    u8 a = (u8)((row >> (14 - gx * 2)) & 3);
                    if (!a || (!sh && a < 2)) continue;
                    for (i = 0; i < size * size; i++) {
                        px = (s16)(at + gx * size + (i % size) + (sh ? 0 : size));
                        py = (s16)(y + gy * size + (i / size) + (sh ? 0 : size));
                        if (px < cx0 || px > cx1 || py < cy0 || py > cy1) continue;
                        if (!sh) darken(px, py);
                        else AT(px, py) = (u8)(ramp + 14 - (gy * size + i / size) * 6 / (12 * size) - (3 - a) * 3);
                    }
                }
            }
            at = (s16)(at + (font_w[c] + 1) * size);
        }
    }
}


#define DOME(x, y, r, ramp) blob((x), (y), (x), (y), (r), (ramp), 0)
#define TUBE(ax, ay, bx, by, r, ramp) blob((ax), (ay), (bx), (by), (r), (ramp), 0)

/* a flipper: rubber round a tapered body (blobs of falling radius), the
 * pivot's screw; on 0 (left transparent by the screen's DMA) */
void render_flipper(const struct part *p, u8 f, s16 left, s16 y0, s16 y1) {
    s16 tx, ty, mx, my, y;
    u8 x;
    flip_tip(p, f, &tx, &ty);
    mx = (s16)((p->x + tx) / 2);
    my = (s16)((p->y + ty) / 2);
    sx = left;
    shadow = 0;
    for (y = y0; y <= y1; y++)
        for (x = 0; x < 8; x++) render_buf[((u16)y << 3) + x] = 0;
    if (!clip(sx, y0, (s16)(sx + 7), y1)) return;
    DOME(p->x, p->y, FLIP_RB, PAL_ACCENT);
    TUBE(p->x, p->y, mx, my, FLIP_RB - 1, PAL_ACCENT);
    TUBE(p->x, p->y, tx, ty, FLIP_RT, PAL_ACCENT);
    DOME(p->x, p->y, FLIP_RB - 2, PAL_FLIP);
    TUBE(p->x, p->y, mx, my, FLIP_RB - 3, PAL_FLIP);
    TUBE(p->x, p->y, tx, ty, FLIP_RT - 2, PAL_FLIP);
    DOME(p->x, p->y, 2, PAL_WALL);
}
