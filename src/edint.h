/* edint.h — what the editor's two files share (editor.c, edpanel.c) */
#pragma once
#include "pb.h"

#define UNDO_LIN 0x8070000UL                  /* the table before the last change */

extern s16 cx, cy, wall_x, wall_y, rx0, rx1;
extern u8 add_type, sel, grab, fine, slot, dirty;

void undo_keep(void);
void mark(u8 i);
void redraw(void);
/* edpanel.c */
void info(void);
void help(void);
void handles(void);
void save(void);
void edit_name(void);
void edit_text(void);
void try_it(void);
void change(u8 k, s8 d);             /* the chosen part: size, turn, colour, ... */
