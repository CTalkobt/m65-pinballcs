/* table.c — the table in memory */
#include "table.h"

#include "shared.h"

/* both programs keep the table at $C600 (it survives the change of
 * program: neither's code, data or stack is there) and talk through the
 * mailbox at $CD00. ($0800-$0FCF would be free but for the KERNAL, whose
 * screen it is.) */
#ifdef __CC45_ZPCALL__
#pragma section("tblsec", 0xC600)
#pragma section("mbox", 0xCD00)
#pragma section("hi", 0x8000)
#endif
__section("tblsec") struct table tbl;
__section("mbox") struct mailbox mb;

void table_clear(void) {
    u8 *q = (u8 *)&tbl;
    u16 i;
    for (i = 0; i < sizeof tbl; i++) q[i] = 0;
    tbl.h.magic[0] = TBL_MAGIC0;
    tbl.h.magic[1] = TBL_MAGIC1;
    tbl.h.version = TBL_VERSION;
    for (i = 0; i < NAME_LEN; i++) tbl.h.name[i] = ' ';
    tbl.h.balls = 3;
    tbl.h.options = 1;
}

/* a table from disk: the right magic, parts that are on the playfield
 * (the rest dropped), texts that end */
u8 table_check(void) {
    u8 i, k, n = 0;
    if (tbl.h.magic[0] != TBL_MAGIC0 || tbl.h.magic[1] != TBL_MAGIC1 || tbl.h.version != TBL_VERSION) return 0;
    if (tbl.h.nparts > MAX_PARTS) tbl.h.nparts = MAX_PARTS;
    for (i = 0; i < tbl.h.nparts; i++) {
        struct part *p = &tbl.p[i];
        if (p->type == P_NONE || p->type >= P_TYPES) continue;
        if (p->x < -40 || p->x > TW + 40 || p->y < -40 || p->y > TH + 40) continue;
        tbl.p[n++] = *p;
    }
    tbl.h.nparts = n;
    for (k = 0; k < NTEXTS; k++) tbl.text[k][TEXT_LEN - 1] = 0;
    if (tbl.h.balls != 5) tbl.h.balls = 3;
    return 1;
}
