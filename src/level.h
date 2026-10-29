/* level.h - rooms, metatiles and collision */
#ifndef LEVEL_H
#define LEVEL_H

#include "nes.h"

#define ROOM_W 16
#define ROOM_H 15

#define MT_EMPTY 0
#define MT_BRICK 1
#define MT_STONE 2
#define MT_COUNT 3

#define COLL_SOLID 0x01

#define LEVEL_BANK 0
extern const u8 room_test[ROOM_W * ROOM_H];

extern u8 room[ROOM_W * ROOM_H];

void __fastcall__ level_load(u8 bank, const u8 *src);

void level_draw(void);

u8 __fastcall__ level_solid_at(u8 x, u8 y);

#endif