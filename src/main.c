/* main.c - setup test: loads a room from a switchable bank, draws it,
   and moves a 16x16 test sprite with the d-pad. Replace with your game. */
#include <string.h>
#include "nes.h"
#include "level.h"

static const u8 palette[32] = {
    /* background */
    0x0F, 0x00, 0x10, 0x07,     /* stone grays + dark brown */
    0x0F, 0x06, 0x16, 0x26,     /* brick reds */
    0x0F, 0x01, 0x11, 0x21,
    0x0F, 0x09, 0x19, 0x29,
    /* sprites */
    0x0F, 0x17, 0x27, 0x37,     /* test sprite: browns/skin */
    0x0F, 0x14, 0x24, 0x34,
    0x0F, 0x1A, 0x2A, 0x3A,
    0x0F, 0x12, 0x22, 0x32,
};

/* Each metatile is 4 tiles: top-left, top-right, bottom-left, bottom-right.
   Tile numbers refer to CHR bank 0 (see tools/make_chr.py). */
static const u8 metatiles[3][4] = {
    { 0, 0, 0, 0 },             /* 0: empty */
    { 1, 2, 3, 4 },             /* 1: brick wall */
    { 5, 6, 7, 8 },             /* 2: stone block */
};
/* Palette (0-3) for each metatile, used for the attribute table */
static const u8 metatile_pal[3] = { 0, 1, 0 };

static u8 room[ROOM_W * ROOM_H];    /* the current room, copied to RAM */

static void draw_room(void) {
    static u8 tx, ty, half, t;
    static u8 attr[64];

    /* nametable 0: two rows of tiles per metatile row */
    for (ty = 0; ty < ROOM_H; ++ty) {
        for (half = 0; half < 2; ++half) {
            vram_adr(NTADR_A(0, ty * 2 + half));
            for (tx = 0; tx < ROOM_W; ++tx) {
                t = room[ty * ROOM_W + tx];
                vram_put(metatiles[t][half * 2]);
                vram_put(metatiles[t][half * 2 + 1]);
            }
        }
    }

    /* attribute table: one byte per 2x2 metatiles (32x32 px) */
    memset(attr, 0, sizeof(attr));
    for (ty = 0; ty < ROOM_H; ++ty) {
        for (tx = 0; tx < ROOM_W; ++tx) {
            t = metatile_pal[room[ty * ROOM_W + tx]];
            /* bit pair: bit 0 of x and y pick which quarter of the byte */
            attr[(ty >> 1) * 8 + (tx >> 1)] |=
                t << (((ty & 1) << 2) | ((tx & 1) << 1));
        }
    }
    vram_adr(0x23C0);
    vram_write(attr, 64);
}

void main(void) {
    static u8 pad, x, y;

    ppu_off();
    ppu_ctrl = CTRL_NMI | CTRL_SPR_1000;    /* BG tiles at $0000, sprites $1000 */

    banked_copy(LEVEL_BANK, room, room_test, sizeof(room));
    draw_room();
    pal_all(palette);

    x = 120;
    y = 176;
    scroll_x = 0;
    scroll_y = 0;
    ppu_on();

    for (;;) {
        pad = pad_poll(0);
        if (pad & PAD_LEFT)  --x;
        if (pad & PAD_RIGHT) ++x;
        if (pad & PAD_UP)    --y;
        if (pad & PAD_DOWN)  ++y;

        oam_clear();
        oam_spr(x,     y,     0, 0);        /* 16x16 test sprite: 4 tiles */
        oam_spr(x + 8, y,     1, 0);
        oam_spr(x,     y + 8, 2, 0);
        oam_spr(x + 8, y + 8, 3, 0);

        ppu_wait_nmi();
    }
}