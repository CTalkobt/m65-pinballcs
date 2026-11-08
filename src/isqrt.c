/* isqrt.c — the integer square root (the physics and the renderer) */
#include "geom.h"

u16 isqrt32(u32 v) {
    u32 r = 0, bit = (u32)1 << 30;
    while (bit > v) bit >>= 2;
    while (bit) {
        if (v >= r + bit) {
            v -= r + bit;
            r = (r >> 1) + bit;
        } else
            r >>= 1;
        bit >>= 2;
    }
    return (u16)r;
}

