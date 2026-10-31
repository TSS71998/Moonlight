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

const u8 hero_attack1[]        = { FRAME_16x32(0x28) };
const u8 hero_attack2[]        = { FRAME_16x32(0x30) };
const u8 hero_crouch_attack1[] = { FRAME_16x32(0x38) };
const u8 hero_crouch_attack2[] = { FRAME_16x32(0x20) };

const u8 sword_raised[] = {
    -9, -44, 0x44, 1,               /* vertical blade behind the head */
    MSPR_END
};
const u8 sword_swing[] = {
     4, -35, 0x46, 1,               /* diagonal blade, two 8x16 pieces */
    12, -35, 0x48, 1,
    MSPR_END
};
const u8 sword_thrust[] = {
     6, -23, 0x40, 1,               /* grip and guard */
    14, -23, 0x42, 1,               /* blade and tip */
    MSPR_END
};