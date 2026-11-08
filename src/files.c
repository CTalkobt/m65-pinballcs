/* files.c — the disk, through the KERNAL (disk.s45's kcall)
 *
 * Slot n (1-24) is the file PBT01 .. PBT24: a table as table.h keeps it
 * (after the 2 bytes of a PRG's load address). The index PBINDEX names
 * every slot and keeps each table's best score. PBT00 is the empty table
 * new ones start from.
 */
#include "hw.h"
#include "files.h"
#include "table.h"
#include "video.h"

extern u8 kc_a, kc_x, kc_y, kc_c;
extern u16 kc_target, kc_ptr;
void kcall(void);
u8 drive(void);
void chain(void);

#pragma section("idxsec", 0x1C80)
__section("idxsec") struct index idx;        /* 577 bytes: $1C80-$1EC0, low RAM */

static u8 kern(u16 target, u8 a, u8 x, u8 y) {
    kc_target = target;
    kc_a = a;
    kc_x = x;
    kc_y = y;
    kcall();
    return kc_c;
}

static char fname[12];
static u8 flen;

static void name(const char *base, u8 slot, u8 save) {
    u8 i = 0;
    flen = 0;
    if (save) {
        fname[flen++] = '@';
        fname[flen++] = '0';
        fname[flen++] = ':';
    }
    while (base[i]) fname[flen++] = base[i++];
    if (slot != 0xFF) {
        fname[flen++] = (char)('0' + slot / 10);
        fname[flen++] = (char)('0' + slot % 10);
    }
}

/* a file into bank 0 at addr (its own load address skipped): 1 if loaded */
static u8 load(void *addr) {
    kc_ptr = 0;
    kern(0xFF6B, 0, 0, 0);                                         /* SETBNK */
    kern(0xFFBD, flen, (u8)(u16)fname, (u8)((u16)fname >> 8));     /* SETNAM */
    kern(0xFFBA, 1, drive(), 0);                                   /* SETLFS */
    return (u8)!kern(0xFFD5, 0, (u8)(u16)addr, (u8)((u16)addr >> 8));
}

static u8 save(void *from, void *to) {
    u8 ok;
    kc_ptr = 0;
    kern(0xFF6B, 0, 0, 0);
    kern(0xFFBD, flen, (u8)(u16)fname, (u8)((u16)fname >> 8));
    kern(0xFFBA, 1, drive(), 0);
    kc_ptr = (u16)from;
    ok = (u8)!kern(0xFFD8, 0xFB, (u8)(u16)to, (u8)((u16)to >> 8));
    kc_ptr = 0;
    return ok;
}

static void after(void) {
    video_on();                        /* the DOS locks the VIC-IV's registers */
}

u8 table_load(u8 slot) {
    u8 ok;
    name("pbt", slot, 0);
    ok = load(&tbl);
    after();
    return (u8)(ok && table_check());
}

u8 table_save(u8 slot) {
    u8 ok;
    name("pbt", slot, 1);
    ok = save(&tbl, (u8 *)&tbl + TABLE_SIZE(&tbl));
    after();
    return ok;
}

void index_load(void) {
    u8 ok, i, k;
    name("pbindex", 0xFF, 0);
    ok = load(&idx);
    after();
    if (ok && idx.magic == INDEX_MAGIC) return;
    idx.magic = INDEX_MAGIC;
    for (i = 0; i < SLOTS; i++) {
        idx.e[i].used = 0;
        idx.e[i].best = 0;
        for (k = 0; k < NAME_LEN; k++) idx.e[i].name[k] = ' ';
        for (k = 0; k < 3; k++) idx.e[i].who[k] = '-';
    }
}

u8 index_save(void) {
    u8 ok;
    name("pbindex", 0xFF, 1);
    ok = save(&idx, (u8 *)&idx + sizeof idx);
    after();
    return ok;
}

/* a file to its own address in a bank (PBGFX into bank 1) */
u8 load_bank(const char *base, u8 bank) {
    u8 ok;
    name(base, 0xFF, 0);
    kc_ptr = 0;
    kern(0xFF6B, bank, 0, 0);
    kern(0xFFBD, flen, (u8)(u16)fname, (u8)((u16)fname >> 8));
    kern(0xFFBA, 1, drive(), 1);
    ok = (u8)!kern(0xFFD5, 0, 0, 0);
    after();
    return ok;
}

/* the other program: never returns */
void run(const char *prog) {
    u8 i;
    name(prog, 0xFF, 0);
    for (i = 0; i < flen; i++) *(u8 *)(0x1F70 + i) = (u8)fname[i];
    *(u8 *)0x1F6F = flen;
    chain();
}
