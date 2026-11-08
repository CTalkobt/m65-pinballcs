/* robot.c — a player for the attract mode (and the simulator): it pulls
 * the plunger, flips when a ball comes down near a flipper's sweet spot */
#include "robot.h"
#include "physics.h"

static u8 hold[MAX_FLIP], wait_lane, pulling;

u8 robot(u8 *plunge) {
    u8 i, b, keys = 0, on = 0;
    struct ball *bl;
    for (i = 0; i < nflips; i++) {
        struct flip *f = &flips[i];
        for (b = 0, bl = balls; b < MAX_BALLS; b++, bl++) {
            s16 dx = (s16)((bl->x >> 10) - f->px), dy = (s16)((bl->y >> 10) - f->py);
            if (f->side) dx = (s16)-dx;
            if (bl->live && !bl->held && bl->vy > 0 && dx > f->len / 3 && dx < f->len + 4 && dy > -22 && dy < 14 &&
                !hold[i] && (phys_seed & 3))
                hold[i] = 14;
        }
        if (hold[i]) {
            hold[i]--;
            keys |= (u8)(f->side ? 2 : 1);
        }
    }
    /* a ball resting on the plunger: pull, then let go */
    for (b = 0, bl = balls; b < MAX_BALLS; b++, bl++)
        if (bl->live && !bl->held && (u16)((bl->x >> 10) - plunger_x + 10) < 20 &&
            (bl->y >> 10) > plunger_y - 24 && (u16)(bl->vy + 80) < 160)
            on = 1;
    *plunge = 0;
    if (pulling) *plunge = (u8)(--pulling != 0);
    else if (on && ++wait_lane > 40) {
        wait_lane = 0;
        pulling = (u8)(36 + (phys_seed & 7));
        *plunge = 1;
    }
    return keys;
}
