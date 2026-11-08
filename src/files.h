/* files.h — tables, the index, the other program */
#pragma once
#include "pb.h"
#include "table.h"

#define SLOTS 24
#define INDEX_MAGIC 0xB1

struct index_entry {
    char name[NAME_LEN];
    u32 best;
    char who[3];
    u8 used;
};
struct index {
    u8 magic;
    struct index_entry e[SLOTS];
};
extern struct index idx;

u8 table_load(u8 slot);                  /* 1-24 (0: the empty table) into tbl: 1 if it is one */
u8 table_save(u8 slot);
void index_load(void);
u8 index_save(void);
u8 load_bank(const char *name, u8 bank);
void run(const char *prog);              /* "pinball" or "play": never returns */
