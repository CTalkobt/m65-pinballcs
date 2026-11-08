/* preview.c — a table drawn on the PC by the MEGA65's own renderer
 *
 *   preview <table.tbl> <out.ppm> [drops_down]
 *
 * Every strip is drawn as on the MEGA65, then the flippers at rest and a
 * ball, and the result written through the palette as a 304 x 400 PPM.
 */
#include <stdio.h>
#include <stdlib.h>
#include "../src/table.h"
#include "../src/theme.h"
#include "../src/render.h"
#include "../src/geom.h"

static u8 fb[TH][TW];

int main(int argc, char **argv) {
    FILE *f;
    int c, y, x, i;
    u8 drops = argc > 3 ? (u8)atoi(argv[3]) : 0;
    if (argc < 3) {
        fprintf(stderr, "usage: preview table.tbl out.ppm [drops]\n");
        return 2;
    }
    sin_init();
    table_clear();
    f = fopen(argv[1], "rb");
    if (!f) { perror(argv[1]); return 1; }
    if (fread(&tbl, 1, sizeof tbl, f) < sizeof(struct table_hdr)) { fprintf(stderr, "short\n"); return 1; }
    fclose(f);
    if (!table_check()) { fprintf(stderr, "not a table\n"); return 1; }
    render_begin();
    for (i = 0; i < NSLOTS; i++) slot_level[i] = (u8)((i * 7) % 3 == 0 ? 16 : 0);
    theme_set(tbl.h.theme);
    for (c = 0; c < STRIPS; c++) {
        render_strip((u8)c, drops);
        for (y = 0; y < TH; y++)
            for (x = 0; x < 8; x++) fb[y][c * 8 + x] = render_buf[y * 8 + x];
    }
    for (i = 0; i < tbl.h.nparts; i++) {
        struct part *p = &tbl.p[i];
        s16 x0, y0, x1, y1;
        if (p->type != P_FLIPPER) continue;
        flipper_box(p, &x0, &y0, &x1, &y1);
        for (c = 0; c < (x1 - x0) / 8; c++) {
            s16 left = (s16)(x0 + c * 8);
            render_flipper(p, drops ? FLIP_FRAMES - 1 : 0, left, y0, y1);
            for (y = y0; y <= y1; y++)
                for (x = 0; x < 8; x++)
                    if (render_buf[y * 8 + x] && left + x < TW) fb[y][left + x] = render_buf[y * 8 + x];
        }
    }
    f = fopen("build/fb.raw", "wb");
    fwrite(fb, 1, sizeof fb, f);
    fclose(f);
    f = fopen(argv[2], "wb");
    fprintf(f, "P6\n%d %d\n255\n", TW, TH);
    for (y = 0; y < TH; y++)
        for (x = 0; x < TW; x++) {
            u8 v = fb[y][x];
            fputc(pal_r[v], f);
            fputc(pal_g[v], f);
            fputc(pal_b[v], f);
        }
    fclose(f);
    return 0;
}
