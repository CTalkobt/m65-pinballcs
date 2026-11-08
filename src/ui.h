/* ui.h — the menu, and what main.c and the editor share */
#pragma once
#include "pb.h"

void ui_main(void);
/* prep.c */
void palette(void);                      /* the table's palette, its lamp slots */
void prepare(void);                      /* the table drawn, to the screen and attic RAM */
void redraw_strips(u8 c0, u8 c1);        /* strips again (the editor) */
void show_drawn(void);                   /* the table as last drawn, from attic RAM */
/* editor.c: slot 1-24 (0: back from trying the table) */
void editor(u8 slot);
/* a line of text typed in the panel: up to max characters into buf (ASCII
 * capitals); 0 if RUN/STOP */
u8 ui_input(u8 col, u8 row, char *buf, u8 max);
u8 ui_key(void);                         /* the next key (PETSCII), waiting */
