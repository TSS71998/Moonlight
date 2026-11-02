#include "enemy_sprites.h"

#define FRAME_16x32(t) \
    -8, -32, (t) + 0, 0, \
     0, -32, (t) + 2, 0, \
    -8, -16, (t) + 4, 0, \
     0, -16, (t) + 6, 0, \
    MSPR_END

#define FRAME_16x16(t) \
    -8, -16, (t) + 0, 0, \
     0, -16, (t) + 2, 0, \
    MSPR_END

const u8 skele_walk1[] = { FRAME_16x32(0x60) };
const u8 skele_walk2[] = { FRAME_16x32(0x68) };

const u8 bat_fly1[] = { FRAME_16x16(0x80) };
const u8 bat_fly2[] = { FRAME_16x16(0x84) };

const u8 flame1[] = { FRAME_16x16(0x52) };
const u8 flame2[] = { FRAME_16x16(0x56) };
const u8 flame3[] = { FRAME_16x16(0x5A) };