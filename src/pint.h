/* pint.h — what the physics' two files share (physics.c, contact.c) */
#pragma once
#include "pb.h"
#include "physics.h"

#define GRAVITY      11
#define VMAX         3700
#define KICK_BUMPER  1700
#define KICK_SLING   1500
#define SLING_MIN    260
#define FLIP_W_UP    170
#define FLIP_W_DOWN  110
#define FLIP_E       90
#define LAUNCH_K     150
#define KICK_OUT     2400
#define HOLD_FRAMES  45

extern s32 bx, by;                     /* the ball being stepped */
extern s16 bvx, bvy;
extern u8 bi;
void event(u8 kind, u8 part, u8 strength);
/* contact.c */
void collide(struct coll *c);
void flipper_contact(struct flip *f);
void sensors(struct ball *b);
void ball_pair(struct ball *a, struct ball *b);
