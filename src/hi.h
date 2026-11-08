/* hi.h — big arrays above the overlay window: the section "hi" at $8000
 * (the programs' code and small data are below $6000, the mapped overlays
 * show at $6000-$7FFF). Section memory is not cleared at the start. */
#pragma once
#ifdef __CC45_ZPCALL__
#ifdef HI_FIXED
#pragma section("hi", 0x8000)       /* (for -S, which is not linked) */
#else
#pragma section("hi")
#endif
#define HI __section("hi")
#else
#define HI
#endif
