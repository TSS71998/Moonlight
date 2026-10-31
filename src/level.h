#ifndef LEVEL_H
#define LEVEL_H

#include "nes.h"

#define ROOM_W 16
#define ROOM_H 15

#define MT_PAL_MASK 0x03
#define COLL_SOLID 0x10

typedef struct {
    const u8 *mt_tiles;     /* 4 tiles per metatile: TL, TR, BL, BR */
    const u8 *mt_info;      /* 1 info byte per metatile */
    const u8 *room;         /* ROOM_W * ROOM_H metatile numbers */
    const u8 *pal;          /* 16-byte background palette, or 0 */
    u16 mt_count;
} LevelDef;

extern u8 room[ROOM_W * ROOM_H];

void __fastcall__ level_load(u8 bank, const u8 *src);

void level_draw(void);

u8 __fastcall__ level_solid_at(u8 x, u8 y);

#endif