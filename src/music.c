/* music.c — the tunes (tune45) on SID 1: each program links its own
 * (menu_tunes: PINBALL's, game_tunes: PLAY's), both as tunes[] */
#include <audio_tune.h>
#include "music.h"

extern const unsigned char *const tunes[];
static u8 current = MUSIC_NONE;

void music(u8 id, u8 volume) {
    if (id == MUSIC_NONE) {
        tune_stop();
        current = MUSIC_NONE;
        return;
    }
    tune_voices(0);
    tune_play(tunes[id]);
    tune_volume(volume);
    current = id;
}

u8 music_playing(void) {
    return (u8)(current != MUSIC_NONE && tune_playing());
}

void music_update(void) {
    if (current != MUSIC_NONE) tune_update();
}
