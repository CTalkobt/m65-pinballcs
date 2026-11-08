/* main_play.c — PLAY's start (resident): the game is in play.c */
#include "hw.h"
#include "geom.h"
#include "table.h"
#include "theme.h"
#include "physics.h"
#include "game.h"
#include "video.h"
#include "sound.h"
#include "music.h"
#include "files.h"
#include "shared.h"
#include "world.h"

u32 play(u8 players, u8 demo);

int main(void) {
    u8 s;
    sin_init();
    video_on();
    spr_on();
    snd_init();
    for (s = 0; s < NSLOTS; s++) slot_level[s] = 0;
    world_load();
    for (s = 0; s < NSLOTS; s++) slot_put(s);
    pal_slots();
    table_restore();
    mb.finished = 0;
    play(mb.players ? mb.players : 1, (u8)(mb.mode == MB_DEMO));
    for (s = 0; s < 4; s++) mb.score[s] = gm.score[s];
    music(MUSIC_NONE, 0);
    run("pinball");
    return 0;
}
