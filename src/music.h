/* music.h — the tunes (each program's own: menu_tunes.h, game_tunes.h) */
#pragma once
#include "pb.h"
#define MUSIC_NONE 0xFF
void music(u8 id, u8 volume);            /* a tune (TUNE_...), volume 0-15 */
void music_update(void);                 /* once a frame */
u8 music_playing(void);
