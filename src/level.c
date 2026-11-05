#include <string.h>
#include "level.h"
#include "hud.h"

#define TOP_ROW     HUD_ROWS
#define TOP_ATTR    (HUD_ROWS / 4)
#define COL_LEN     (ROOM_H * 2 - TOP_ROW)

u16 level_width;
u16 camera_x;

#pragma bss-name(push, "WRAM")
static u8 level_map[MAX_SCREENS * ROOM_W * ROOM_H];
u8 mt_info[256];
#pragma bss-name(pop)

u8 *screen_ptr[MAX_SCREENS];
static u16 level_cols;
static u16 cam_tile;
static const u8 *cur_mt_tiles;
static u8 cur_bank;

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

/* Queue 8 px tile column q_tcol of the level (two per metatile column)
   for the next vblank, built straight into vram_buf: 26 tiles below the
   HUD (29 bytes) and, if q_attrs is set, the 7 attribute bytes of its
   32 px group (28 more). Writing half a metatile column at a time keeps
   the work per frame small: the camera crosses at most one 8 px column a
   frame. Attributes cover 4 tile columns, so they're only written with the
   first column of a group that comes into view. Arguments are globals and
   everything is static: cc65's argument stack is slow. */
static u16 q_tcol;
static u8 q_attrs;

static void queue_tile_column(void) {
    static const u8 *p;
    static const u8 *mt;
    static u8 r, n, t, v, half, old_bank, slot;
    static u16 adr, mcol;

    mcol = q_tcol >> 1;                 /* metatile column */
    if (mcol >= level_cols) return;
    half = (u8)q_tcol & 1;              /* 0 = its left tiles, 1 = right */

    slot = (u8)q_tcol & 63;             /* tile column in the 64-wide ring */
    adr = ((slot & 32) ? 0x2400 : 0x2000) + (slot & 31) + TOP_ROW * 32;

    /* one entry: [addr | VRAM_DOWN, len, tiles...] */
    n = vram_buf_len;
    vram_buf[n] = (u8)(adr >> 8) | (VRAM_DOWN >> 8);
    vram_buf[n + 1] = (u8)adr;
    vram_buf[n + 2] = COL_LEN;
    n += 3;

    /* tiles: read the metatile table straight from its bank */
    p = screen_ptr[mcol >> 4] + ((u8)mcol & 15) + (TOP_ROW / 2) * ROOM_W;
    old_bank = prg_bank;
    mmc1_set_prg_bank(cur_bank);
    for (r = ROOM_H - TOP_ROW / 2; r; --r) {
        mt = cur_mt_tiles + ((u16)*p << 2) + half;
        t = mt[0]; vram_buf[n] = t; ++n;
        t = mt[2]; vram_buf[n] = t; ++n;
        p += ROOM_W;
    }
    mmc1_set_prg_bank(old_bank);

    if (q_attrs) {
        mcol &= ~1;
        p = screen_ptr[mcol >> 4] + ((u8)mcol & 15) + TOP_ATTR * 2 * ROOM_W;
        adr = ((slot & 32) ? 0x27C0 : 0x23C0) + ((slot & 31) >> 2) + TOP_ATTR * 8;
        for (r = TOP_ATTR; r < 8; ++r) {
            t = p[0]; v = mt_info[t] & 3;
            t = p[1]; v |= (mt_info[t] & 3) << 2;
            if (r < 7) {
                t = p[ROOM_W];     v |= (mt_info[t] & 3) << 4;
                t = p[ROOM_W + 1]; v |= (mt_info[t] & 3) << 6;
            }
            t = (u8)(adr >> 8); vram_buf[n] = t; ++n;
            t = (u8)adr;        vram_buf[n] = t; ++n;
            vram_buf[n] = 1; ++n;
            vram_buf[n] = v; ++n;
            adr += 8;
            p += ROOM_W * 2;
        }
    }
    vram_buf_len = n;
}

static void set_scroll(void) {
    scroll_x = (u8)camera_x;
    ppu_ctrl = (ppu_ctrl & ~CTRL_NT_2400) | ((u8)(camera_x >> 8) & 1);
}

void __fastcall__ level_draw(u16 cam_x) {
    static u16 first;
    static u8 i;

    camera_x = cam_x;
    cam_tile = cam_x >> 3;

    first = cam_tile > 2 ? cam_tile - 2 : 0;
    vram_buf_flush();
    for (i = 0; i < 37; ++i) {
        q_tcol = first + i;
        q_attrs = !i || !(q_tcol & 3);
        queue_tile_column();
        vram_buf_flush();
     }
    set_scroll();
}

static u16 want;

static void __fastcall__ camera_want(u16 target) {
    want = target > 128 ? target - 128 : 0;
    if (want > level_width - 256) want = level_width - 256;
}

void __fastcall__ camera_snap(u16 target) {
    camera_want(target);
    level_draw(want);
}

void __fastcall__ camera_update(u16 target) {
    static u16 new_tile;

    camera_want(target);
    if (want > camera_x) camera_x += (want - camera_x > 8) ? 8 : want - camera_x;
    else if (want < camera_x) camera_x -= (camera_x - want > 8) ? 8 : camera_x - want;

    new_tile = camera_x >> 3;
    if (new_tile > cam_tile) {
        cam_tile = new_tile;
        q_tcol = cam_tile + 34;
        q_attrs = !(q_tcol & 3);
        queue_tile_column();
    } else if (new_tile < cam_tile) {
        cam_tile = new_tile;
        if (cam_tile >= 2) {
            q_tcol = cam_tile - 2;
            q_attrs = (q_tcol & 3) == 3;
            queue_tile_column();
        }
    }
    set_scroll();
}