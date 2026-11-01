#include <string.h>
#include "level.h"

u16 level_width;
u16 camera_x;

#pragma bss-name(push, "WRAM")
static u8 level_map[MAX_SCREENS * ROOM_W * ROOM_H];
static u8 mt_info[256];
#pragma bss-name(pop)

static u8 *screen_ptr[MAX_SCREENS];
static u16 level_cols;
static u16 cam_col;
static const u8 *cur_mt_tiles;
static u8 cur_bank;

static const u8 quarter_bit[4] = { 0x10, 0x20, 0x40, 0x80 };

static u8 col_left[ROOM_H * 2];
static u8 col_right[ROOM_H * 2];
static u8 col_attr[8];
static u16 col_addr;
static u16 col_attr_addr;

void __fastcall__ level_load(u8 bank, const LevelDef *lvl) {
    static LevelDef def;
    static u8 i;

    banked_copy(bank, &def, lvl, sizeof(def));
    banked_copy(bank, level_map, def.map, (u16)def.screens * (ROOM_W * ROOM_H));
    banked_copy(bank, mt_info, def.mt_info, def.mt_count);
    if (def.pal) {
        banked_copy(bank, pal_buf, def.pal, 16);
        pal_update = 1;
    }
    for (i = 0; i < def.screens; ++i) {
        screen_ptr[i] = level_map + (u16)i * (ROOM_W * ROOM_H);
    }
    level_cols = (u16)def.screens * ROOM_W;
    level_width = (u16)def.screens << 8;
    cur_mt_tiles = def.mt_tiles;
    cur_bank = bank;
}

static void __fastcall__ build_column(u16 mcol) {
    static const u8 *p, *pe;
    static const u8 *mt;
    static u8 r, a, v, old_bank, slot;
    static u16 even;

    p = screen_ptr[mcol >> 4] + (mcol & 15);
    old_bank = prg_bank;
    mmc1_set_prg_bank(cur_bank);
    for (r = 0; r < ROOM_H; ++r) {
        mt = cur_mt_tiles + ((u16)p[r << 4] << 2);
        col_left[r * 2] = mt[0];
        col_right[r * 2] = mt[1];
        col_left[r * 2 + 1] = mt[2];
        col_right[r * 2 + 1] = mt[3];
    }
    mmc1_set_prg_bank(old_bank);

    even = mcol & ~1;
    pe = screen_ptr[even >> 4] + (even & 15);
    for (a = 0; a < 8; ++a) {
        r = a << 1;
        v = (mt_info[pe[r << 4]] & 3) | ((mt_info[pe[(r << 4) + 1]] & 3) << 2);
        if (r + 1 < ROOM_H) {
            v |= ((mt_info[pe[(r + 1) << 4]] & 3) << 4)
               | ((mt_info[pe[((r + 1) << 4) + 1]] & 3) << 6);
        }
        col_attr[a] = v;
    }

    slot = (u8)(mcol << 1) & 63;
    col_addr = ((slot & 32) ? 0x2400 : 0x2000) + (slot & 31);
    col_attr_addr = ((slot & 32) ? 0x27C0 : 0x23C0) + ((slot & 31) >> 2);
}

static void __fastcall__ queue_column(u16 mcol) {
    static u8 a;
    if (mcol >= level_cols) return;
    build_column(mcol);
    vram_buf_put(col_addr | VRAM_DOWN, col_left, ROOM_H * 2);
    vram_buf_put((col_addr + 1) | VRAM_DOWN, col_right, ROOM_H * 2);
    for (a = 0; a < 8; ++a) {
        vram_buf_put(col_attr_addr + (a << 3), &col_attr[a], 1);
    }
}

static void set_scroll(void) {
    scroll_x = (u8)camera_x;
    ppu_ctrl = (ppu_ctrl & ~CTRL_NT_2400) | ((u8)(camera_x >> 8) & 1);
}

void __fastcall__ level_draw(u16 cam_x) {
    static u16 c, first;
    static u8 i, a;

    camera_x = cam_x;
    cam_col = cam_x >> 4;

    first = cam_col > 14 ? cam_col - 14 : 0;
    for (i = 0; i < 32; ++i) {
        c = first + i;
        if (c >= level_cols) break;
        build_column(c);
        PPUCTRL = ppu_ctrl | CTRL_INC_32;
        vram_adr(col_addr);
        vram_write(col_left, ROOM_H * 2);
        vram_adr(col_addr + 1);
        vram_write(col_right, ROOM_H * 2);
        PPUCTRL = ppu_ctrl;
        for (a = 0; a < 8; ++a) {
            vram_adr(col_attr_addr + (a << 3));
            vram_put(col_attr[a]);
        }
    }
    set_scroll();
}

void __fastcall__ camera_update(u16 target) {
    static u16 want, new_col;

    want = target > 128 ? target - 128 : 0;
    if (want > level_width - 256) want = level_width - 256;
    if (want > camera_x) camera_x += (want - camera_x > 8) ? 8 : want - camera_x;
    else if (want < camera_x) camera_x -= (camera_x - want > 8) ? 8 : camera_x - want;

    new_col = camera_x >> 4;
    if (new_col > cam_col) {
        cam_col = new_col;
        queue_column(cam_col + 17);
    } else if (new_col < cam_col) {
        cam_col = new_col;
        if (cam_col) queue_column(cam_col - 1);
    }
    set_scroll();
}

u8 __fastcall__ level_solid_at(u16 x, u8 y) {
    if (y >= 240) return 0;
    if (x >= level_width) return 1;

    return mt_info[screen_ptr[x >> 8][(y & 0xF0) | ((u8)x >> 4)]]
        & quarter_bit[((y >> 2) & 2) | ((x >> 3) & 1)];
}