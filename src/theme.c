/* theme.c — the palettes */
#include "theme.h"
#include "hi.h"
#include "table.h"

HI u8 pal_r[256], pal_g[256], pal_b[256];
u8 slot_colour[NSLOTS];
u8 slot_level[NSLOTS];
u8 gi = 16;
static struct theme cur;
const struct theme *theme = &cur;

const u8 colour_rgb[NCOLOURS][3] = {
    {255, 40, 30},   /* red */
    {255, 130, 0},   /* orange */
    {255, 230, 30},  /* yellow */
    {170, 255, 30},  /* lime */
    {30, 220, 60},   /* green */
    {0, 200, 160},   /* teal */
    {40, 230, 255},  /* cyan */
    {80, 160, 255},  /* sky */
    {40, 70, 255},   /* blue */
    {140, 70, 255},  /* violet */
    {200, 40, 230},  /* purple */
    {255, 40, 180},  /* magenta */
    {255, 140, 190}, /* pink */
    {255, 255, 255}, /* white */
    {255, 180, 40},  /* amber */
    {230, 190, 70},  /* gold */
};

/*  name        pattern        bg top          bg bottom       pattern        wall            rubber          flipper         accent          display */
static const struct theme __noescape themes[NTHEMES] = {
    {"starfall", PAT_STARS,    {6, 6, 34},     {44, 18, 96},   {255, 250, 220}, {200, 212, 236}, {244, 244, 236}, {255, 214, 50},  {40, 140, 255},  {255, 120, 20}},
    {"neon",     PAT_GRID,     {16, 0, 34},    {70, 0, 90},    {255, 0, 200},   {176, 180, 210}, {240, 240, 250}, {0, 250, 230},   {255, 40, 200},  {0, 230, 255}},
    {"jungle",   PAT_DIAMONDS, {8, 44, 14},    {44, 96, 22},   {30, 130, 40},   {160, 108, 56},  {246, 244, 230}, {255, 240, 120}, {255, 120, 0},   {130, 255, 60}},
    {"deep",     PAT_WAVES,    {0, 52, 110},   {0, 14, 50},    {0, 130, 190},   {190, 222, 244}, {240, 250, 255}, {255, 255, 255}, {255, 200, 0},   {0, 210, 255}},
    {"inferno",  PAT_FLAMES,   {50, 0, 0},     {170, 36, 0},   {255, 150, 0},   {120, 100, 96},  {250, 240, 230}, {255, 255, 255}, {255, 200, 0},   {255, 60, 0}},
    {"ice",      PAT_DIAMONDS, {120, 176, 236}, {214, 236, 255}, {250, 254, 255}, {110, 130, 200}, {255, 255, 255}, {40, 90, 210},   {0, 180, 255},   {120, 200, 255}},
    {"gold",     PAT_WOOD,     {70, 38, 10},   {136, 84, 24},  {104, 60, 18},   {236, 196, 96},  {250, 246, 230}, {255, 255, 240}, {210, 30, 30},   {255, 200, 40}},
    {"robot",    PAT_CHECK,    {56, 60, 68},   {104, 110, 118}, {136, 142, 150}, {210, 210, 220}, {240, 240, 240}, {255, 90, 0},    {255, 220, 0},   {255, 40, 40}},
    {"sugar",    PAT_DOTS,     {255, 150, 196}, {255, 214, 232}, {255, 255, 255}, {250, 250, 255}, {255, 255, 255}, {130, 60, 210},  {0, 200, 200},   {255, 80, 160}},
    {"haunted",  PAT_BRICKS,   {20, 10, 34},   {34, 54, 30},   {62, 50, 76},    {150, 130, 170}, {230, 236, 220}, {180, 255, 80},  {150, 50, 210},  {140, 255, 60}},
    {"dragon",   PAT_RAYS,     {96, 10, 20},   {30, 0, 10},    {170, 40, 20},   {214, 178, 92},  {250, 240, 220}, {255, 230, 120}, {40, 200, 90},   {255, 170, 0}},
    {"classic",  PAT_STRIPES,  {236, 226, 196}, {206, 186, 146}, {226, 92, 60},   {140, 86, 42},   {255, 255, 250}, {255, 255, 255}, {220, 40, 40},   {255, 120, 30}},
};

static u8 scale(u8 c, u8 k) {                  /* c * k / 256 */
    return (u8)(((u16)c * k) >> 8);
}

static u8 lerp(u8 a, u8 b, u8 t, u8 n) {       /* a + (b - a) * t / n */
    return (u8)(a + (s16)(((s16)b - (s16)a) * (s16)t / (s16)n));
}

static void put(u8 e, u8 r, u8 g, u8 b) {
    if (gi != 16) {
        r = (u8)(((u16)r * gi) >> 4);
        g = (u8)(((u16)g * gi) >> 4);
        b = (u8)(((u16)b * gi) >> 4);
    }
    pal_r[e] = r;
    pal_g[e] = g;
    pal_b[e] = b;
}

/* a ramp of 16: dark (level 0) to the colour (10) to near white (15) */
static void ramp(u8 e, const u8 *c) {
    u8 i, k;
    for (i = 0; i < 16; i++) {
        if (i <= 10) {
            k = (u8)(38 + i * 21);                 /* 38 .. 248 */
            put((u8)(e + i), scale(c[0], k), scale(c[1], k), scale(c[2], k));
        } else {
            k = (u8)((i - 10) * 36);               /* toward white, up to 180/255 */
            put((u8)(e + i), (u8)(c[0] + scale((u8)(255 - c[0]), k)),
                (u8)(c[1] + scale((u8)(255 - c[1]), k)), (u8)(c[2] + scale((u8)(255 - c[2]), k)));
        }
    }
}

static void background(void) {
    u8 i, c[3], d[3], k;
    const struct theme *t = theme;
    for (i = 0; i < 16; i++) {
        for (k = 0; k < 3; k++) {
            c[k] = lerp(t->bg_top[k], t->bg_bot[k], i, 15);
            d[k] = lerp(t->pat[k], (u8)((t->pat[k] >> 1) + (t->bg_bot[k] >> 1)), i, 15);
        }
        put((u8)(PAL_BG + i), c[0], c[1], c[2]);
        put((u8)(PAL_BGS + i), scale(c[0], 110), scale(c[1], 110), scale(c[2], 120));
        put((u8)(PAL_PAT + i), d[0], d[1], d[2]);
        put((u8)(PAL_PATS + i), scale(d[0], 110), scale(d[1], 110), scale(d[2], 120));
    }
}

/* a lamp: off it is the colour's ember, on its full glow; level 0-16
 * between */
void slot_put(u8 s) {
    const u8 *c = colour_rgb[slot_colour[s] & 15];
    u8 lv = slot_level[s], i, e = (u8)SLOT(s), k;
    /* off: rim 14%, body 24%, glint 38% of the colour; on: 62%, 100%, toward white */
    for (k = 0; k < 3; k++) {
        static const u8 base[3] = {36, 62, 98}, step[3] = {7, 12, 9};
        u8 v[3];
        for (i = 0; i < 3; i++) {
            u16 x = (u16)c[i] * (u8)(base[k] + lv * step[k] - (k == 1 && lv == 16)) >> 8;
            if (k == 2 && lv > 8) x += (u16)(255 - x) * ((lv - 8) * 22) >> 8;
            v[i] = (u8)x;
        }
        put((u8)(e + k), v[0], v[1], v[2]);
    }
}

void theme_set(u8 t) {
    u8 s;
    cur = themes[t % NTHEMES];
    background();
    ramp(PAL_WALL, theme->wall);
    ramp(PAL_RUBBER, theme->rubber);
    ramp(PAL_FLIP, theme->flip);
    ramp(PAL_ACCENT, theme->accent);
    ramp(PAL_DISP, theme->disp);
    for (s = 0; s < NSLOTS; s++) slot_put(s);
}

void gi_level(u8 k) {
    gi = k;
    theme_set(tbl.h.theme);
}

/* the lamp slots: each lit part its own, while there are slots; then they
 * share by colour. Both programs give them out the same way. */
HI u8 part_slot[MAX_PARTS];

void slots_assign(void) {
    u8 i, s = 0, k;
    for (i = 0; i < tbl.h.nparts; i++) {
        u8 t = tbl.p[i].type;
        part_slot[i] = 0xFF;
        if (t == P_BUMPER || t == P_SLING || t == P_TARGET || t == P_DROP || t == P_ROLLOVER ||
            t == P_KICKER || t == P_SPINNER || t == P_LAMP) {
            u8 col = (u8)(t == P_SLING || t == P_SPINNER ? 13 : (u8)tbl.p[i].b & 15);
            if (s < NSLOTS) {
                slot_colour[s] = col;
                part_slot[i] = s++;
            } else {
                for (k = 0; k < NSLOTS - 1 && slot_colour[k] != col; k++) {}
                part_slot[i] = k;
            }
        }
    }
}

/* the lights' show while nobody plays */
void lamps_attract(u16 t) {
    u8 s, lv;
    for (s = 0; s < NSLOTS; s++) {
        u8 ph = (u8)((t >> 2) + s * 5);
        lv = (u8)((ph & 15) < 6 ? 16 : ((ph & 15) < 9 ? 6 : 0));
        if (slot_level[s] != lv) {
            slot_level[s] = lv;
            slot_put(s);
        }
    }
}
