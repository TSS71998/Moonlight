#include <string.h>
#include "level.h"

u8 room[ROOM_W * ROOM_H];

static u8 mt_info[256];
static const u8 *cur_mt_tiles;
static u8 cur_bank;

static const u8 quarter_bit[4] = { 0x10, 0x20, 0x40, 0x80 };

void __fastcall__ level_load(u8 bank, const LevelDef *lvl) {
    static LevelDef def;

    banked_copy(bank, &def, lvl, sizeof(def));
    banked_copy(bank, room, def.room, sizeof(room));
    banked_copy(bank, mt_info, def.mt_info, def.mt_count);
    if (def.pal) {
        banked_copy(bank, pal_buf, def.pal, 16);
        pal_update = 1;
    }
    cur_mt_tiles = def.mt_tiles;
    cur_bank = bank;
}

void level_draw(void) {
    static u8 tx, ty, half, t, old_bank;
    static const u8 *mt;
    static u8 attr[64];

    old_bank = prg_bank;
    mmc1_set_prg_bank(cur_bank);

    for (ty = 0; ty < ROOM_H; ++ty) {
        for (half = 0; half < 2; ++half) {
            vram_adr(NTADR_A(0, ty * 2 + half));
            for (tx = 0; tx < ROOM_W; ++tx) {
                mt = cur_mt_tiles + ((u16)room[ty * ROOM_W + tx] << 2) + half * 2;
                vram_put(mt[0]);
                vram_put(mt[1]);
            }
        }
    }
    mmc1_set_prg_bank(old_bank);

    memset(attr, 0, sizeof(attr));
    for (ty = 0; ty < ROOM_H; ++ty) {
        for (tx = 0; tx < ROOM_W; ++tx) {
            t = mt_info[room[ty * ROOM_W + tx]] & MT_PAL_MASK;
            attr[(ty >> 1) * 8 + (tx >> 1)] |=
                t << (((ty & 1) << 2) | ((tx & 1) << 1));
        }
    }
    vram_adr(0x23C0);
    vram_write(attr, 64);
}

u8 __fastcall__ level_solid_at(u8 x, u8 y) {
    if (y >= 240) return 0;
    return mt_info[room[(y & 0xF0) | (x >> 4)]]
        & quarter_bit[((y >> 2) & 2) | ((x >> 3) & 1)];
}