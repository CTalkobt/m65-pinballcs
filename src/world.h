/* world.h — the table made ready for play, PINBALL to PLAY (world.c) */
#pragma once
#include "pb.h"

#define MAX_DROPS 24
struct dropbox { u8 part; s16 x0, y0, x1, y1; };
extern struct dropbox drops[MAX_DROPS];
extern u8 ndrops;

void world_save(void);                   /* PINBALL, after phys_build */
void world_load(void);                   /* PLAY, at the start */
