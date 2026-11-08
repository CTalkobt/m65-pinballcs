/* shared.h — what the two programs (PINBALL: the menu and the editor;
 * PLAY: the game) hand each other in the mailbox at $CD00 */
#pragma once
#include "pb.h"

#define MB_MAGIC 0x5B

enum { MB_MENU, MB_PLAY, MB_DEMO, MB_TEST };

struct mailbox {
    u8 magic;
    u8 mode;                  /* what PLAY is to do, or what it did */
    u8 slot;                  /* the table's slot (1-24) */
    u8 players;
    u8 drawn;                 /* attic RAM holds tbl drawn (the art, the flippers) */
    u8 music;
    u8 finished;              /* PLAY: the game ended (not quit) */
    u8 spare;
    u32 score[4];             /* PLAY: the scores */
    u8 edit[24];              /* the editor's state while its table is tried */
};
extern struct mailbox mb;
