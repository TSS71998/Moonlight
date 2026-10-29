/* bank0.c - data stored in switchable PRG bank 0 ($8000-$BFFF).
   Anything under this pragma is only visible while bank 0 is mapped,
   so read it with banked_copy() or switch with mmc1_set_prg_bank(0). */
#include "nes.h"
#include "level.h"

#pragma rodata-name(push, "BANK0")

/* Test room: 16 x 15 metatiles (16x16 px each) = one screen.
   0 = empty, 1 = brick wall, 2 = stone floor/platform */
const u8 room_test[ROOM_W * ROOM_H] = {
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
    1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
    1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
    1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
    1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
    1,0,0,0,0,0,0,0,0,0,0,2,2,2,0,1,
    1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
    1,0,0,0,0,0,2,2,2,0,0,0,0,0,0,1,
    1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
    1,0,2,2,0,0,0,0,0,0,0,0,0,0,0,1,
    1,0,2,2,0,0,0,0,0,0,0,2,2,2,0,1,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
};

#pragma rodata-name(pop)