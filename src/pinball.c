/* pinball.c — PINBALL: the Pinball Construction Set's menu and editor
 *
 * The first program on the disk. It loads the art and sounds (PBGFX, into
 * bank 1) and the index of tables, then runs the menu. A game is PLAY, a
 * program of its own: this one draws the table (into attic RAM too), puts
 * what PLAY needs in the mailbox and loads PLAY over itself; PLAY loads
 * this one back when the game is over.
 */
#include "hw.h"
#include "geom.h"
#include "table.h"
#include "video.h"
#include "sound.h"
#include "music.h"
#include "files.h"
#include "shared.h"
#include "ui.h"

int main(void) {
    u8 first = (u8)(mb.magic != MB_MAGIC);
    sin_init();
    if (first) {
        if (!load_bank("pbgfx", 1)) return 1;
        mb.magic = MB_MAGIC;
        mb.mode = MB_MENU;
        mb.drawn = 0;
        mb.slot = 1;
        mb.players = 1;
        mb.music = 1;
        mb.finished = 0;
        index_load();
    }
    video_on();
    video_layout();
    spr_on();
    if (first) {
        /* the sprites' colours, once (the palette keeps them for PLAY) */
        u8 n;
        for (n = 0; n < 3; n++) spr_colours(n, 150, 160, 190);    /* chrome balls */
        for (n = 3; n < 6; n++) spr_colours(n, 0, 0, 0);          /* their shadows */
        spr_colours(6, 200, 60, 40);                             /* the plunger's knob */
        spr_colours(7, 255, 240, 80);                            /* the editor's cursor */
    }
    snd_init();
    snd_music_on = mb.music;
    if (mb.mode == MB_TEST) editor(0);
    ui_main();
    return 0;
}
