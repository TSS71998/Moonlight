#include "hero_sprites.h"

#define FRAME_16x32(t) \
    -8, -32, (t) + 0, 0, \
     0, -32, (t) + 2, 0, \
    -8, -16, (t) + 4, 0, \
     0, -16, (t) + 6, 0, \
    MSPR_END

const u8 hero_stand[]  = { FRAME_16x32(0x00) };
const u8 hero_walk1[]  = { FRAME_16x32(0x08) };
const u8 hero_walk2[]  = { FRAME_16x32(0x10) };
const u8 hero_walk3[]  = { FRAME_16x32(0x18) };
const u8 hero_crouch[] = { FRAME_16x32(0x20) };