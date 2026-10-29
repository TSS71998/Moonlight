#include <string.h>
#include "level.h"

static const u8 mt_tiles[MT_COUNT][4] = {
    { 0, 0, 0, 0 },     /* MT_EMPTY */
    { 1, 2, 3, 4 },     /* MT_BRICK */
    { 5, 6, 7, 8 },     /* MT_STONE */
};

/* Background palette (0-3) for each metatile, for the attribute table */
static const u8 mt_palette[MT_COUNT] = { 0, 1, 0 };

/* Collision flags for each metatile */
static const u8 mt_collision[MT_COUNT] = {
    0,                  /* MT_EMPTY */
    COLL_SOLID,         /* MT_BRICK */
    COLL_SOLID,         /* MT_STONE */
};

u8 room[ROOM_W * ROOM_H];

void __fastcall__ level_load(u8 bank, const u8 *src) {
    banked_copy(bank, room, src, sizeof(room));
}

void level_draw(void) {
    static u8 tx, ty, half, t;
    static u8 attr[64];

    for (ty = 0; ty < ROOM_H; ++ty) {
        for (half = 0; half < 2; ++half) {
            vram_adr(NTADR_A(0, ty * 2 + half));
            for (tx = 0; tx < ROOM_W; ++tx) {
                t = room[ty * ROOM_W + tx];
                vram_put(mt_tiles[t][half * 2]);
                vram_put(mt_tiles[t][half * 2 + 1]);
            }
        }
    }

    memset(attr, 0, sizeof(attr));
    for (ty = 0; ty < ROOM_H; ++ty) {
        for (tx = 0; tx < ROOM_W; ++tx) {
            t = mt_palette[room[ty * ROOM_W + tx]];
            attr[(ty >> 1) * 8 + (tx >> 1)] |= t << (((ty & 1) << 2) | ((tx & 1) << 1));
        }
    }
    vram_adr(0x23c0);
    vram_write(attr, 64);
}

u8 __fastcall__ level_solid_at(u8 x, u8 y) {
    if (y>= 240) return 0;
    return mt_collision[room[(y & 0xF0) | (x >> 4)]] & COLL_SOLID;
}