/* sound.h — effects (samples), chimes and noises (SID), the ball's roll */
#pragma once
#include "pb.h"

enum {
    SFX_FLIPUP, SFX_FLIPDN, SFX_BUMPER, SFX_SLING, SFX_RUBBER, SFX_METAL, SFX_TARGET,
    SFX_DROP, SFX_KICKIN, SFX_KICKOUT, SFX_LAUNCH, SFX_DRAIN, SFX_ROLLOVER, SFX_SPIN,
    SFX_KNOCKER, SFX_COUNT
};

void snd_init(void);
void snd_quiet(void);
void sfx(u8 id, u8 vol);                 /* vol 0-255 */
void roll_sound(u16 speed);              /* the ball's speed (1/1024 px a step), 0 silent */
void chime(u8 which);                    /* 0: 10s, 1: 100s, 2: 1000s */
void sid_fx(u8 which);                   /* SID effects below */
void snd_update(void);                   /* once a frame */
extern u8 snd_music_on;

enum { FX_TILT, FX_WARN, FX_EXTRA, FX_JACKPOT, FX_MENU, FX_SELECT, FX_LOCK, FX_COUNTDOWN, FX_SKILL, FX_ERROR };
