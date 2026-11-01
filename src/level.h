#ifndef LEVEL_H
#define LEVEL_H

#include "nes.h"

#define ROOM_W 16
#define ROOM_H 15

#define MAX_SCREENS 32

/* Metatile info byte (made by tools/nam2level.c from your NEXXT screens):
    bits 0-1  background palette
    bits 4-7  which 8x8 tiles of the block are solid (from levels/collision.txt):
              bit 4 top-left, 5 top-right, 6 bottom-left, 7 bottom-right */
#define MT_PAL_MASK 0x03
#define COLL_MASK   0xF0

typedef struct {
    const u8 *mt_tiles;
    const u8 *mt_info;
    const u8 *map;
    const u8 *pal;
    u16 mt_count;
    u8 screens;
} LevelDef;

extern u16 level_width;
extern u16 camera_x;

void __fastcall__ level_load(u8 bank, const LevelDef *lvl);

void level_draw(void);

void __fastcall__ camera_update(u16 target);

u8 __fastcall__ level_solid_at(u8 x, u8 y);

#endif