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
static u8 mt_info[256];
#pragma bss-name(pop)

static u8 *screen_ptr[MAX_SCREENS];
static u16 level_cols;
static u16 cam_col;
static const u8 *cur_mt_tiles;
static u8 cur_bank;

static const u8 quarter_bit[4] = { 0x10, 0x20, 0x40, 0x80 };

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

/* Queue metatile column q_col for the next vblank, built straight into
   vram_buf: two 26-tile columns (58 bytes) and, if q_attrs is set, the
   7 attribute bytes below the HUD (28 more). Attributes cover a 32 px pair
   of columns, so they're only needed with the first column of a pair that
   comes into view. Arguments are globals and everything is static: cc65's
   argument stack is slow, and this runs in the middle of a frame. */
static u16 q_col;
static u8 q_attrs;

static void __fastcall__ queue_column(void) {
    static const u8 *p, *pe;
    static const u8 *mt;
    static u8 r, n, nl, nr, t, v, old_bank, slot;
    static u16 adr, even;

    if (q_col >= level_cols) return;

    slot = (u8)(q_col << 1) & 63;
    adr = ((slot & 32) ? 0x2400 : 0x2000) + (slot & 31) + TOP_ROW * 32;

    n = vram_buf_len;
    nl = n + 3;
    nr = nl + COL_LEN + 3;
    vram_buf[n] = (u8)(adr >> 8) | (VRAM_DOWN >> 8);
    vram_buf[n + 1] = (u8)adr;
    vram_buf[n + 2] = COL_LEN;
    ++adr;
    vram_buf[nr - 3] = (u8)(adr >> 8) | (VRAM_DOWN >> 8);
    vram_buf[nr - 2] = (u8)adr;
    vram_buf[nr - 1] = COL_LEN;

    p = screen_ptr[q_col >> 4] + (q_col & 15) + (TOP_ROW / 2) * ROOM_W;
    old_bank = prg_bank;
    mmc1_set_prg_bank(cur_bank);
    for (r = ROOM_H - TOP_ROW / 2; r; --r) {
        mt = cur_mt_tiles + ((u16)*p << 2);
        t = mt[0]; vram_buf[nl] = t; ++nl;     /* TL */
        t = mt[2]; vram_buf[nl] = t; ++nl;     /* BL */
        t = mt[1]; vram_buf[nr] = t; ++nr;     /* TR */
        t = mt[3]; vram_buf[nr] = t; ++nr;     /* BR */
        p += ROOM_W;
    }
    mmc1_set_prg_bank(old_bank);
    n = nr;

    if (q_attrs) {
        even = q_col & ~1;
        p = screen_ptr[even >> 4] + (even & 15) + TOP_ATTR * 2 * ROOM_W;
        adr = ((slot & 32) ? 0x27C0 : 0x23C0) + ((slot & 31) >> 2) + TOP_ATTR * 8;
        for (r = TOP_ATTR; r < 8; ++r) {
            t = p[0]; v = mt_info[t] & 3;
            t = p[1]; v |= (mt_info[t] & 3) << 2;
            if (r < 7) {    /* the last row of bytes has only its top half */
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

static void flush_vram_buf(void) {
    static u8 i, len;
    i = 0;
    while (i < vram_buf_len) {
        PPUCTRL = (vram_buf[i] & 0x80) ? (ppu_ctrl | CTRL_INC_32) : (ppu_ctrl & ~CTRL_INC_32);
        vram_adr(((u16)(vram_buf[i] & 0x3F) << 8) | vram_buf[i + 1]);
        len = vram_buf[i + 2];
        i += 3;
        while (len--) PPUDATA = vram_buf[i++];
    }
    PPUCTRL = ppu_ctrl;
    vram_buf_len = 0;
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
    flush_vram_buf();
    q_attrs = 1;
    for (i = 0; i < 32; ++i) {
        q_col = first + 1;
        queue_column();
        flush_vram_buf();
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
        q_col = cam_col + 17;
        q_attrs = !(q_col & 1);
        queue_column();
    } else if (new_col < cam_col) {
        cam_col = new_col;
        if (cam_col) {
            q_col = cam_col - 1;
            q_attrs = q_col & 1;
            queue_column();
        }
    }
    set_scroll();
}

u8 __fastcall__ level_solid_at(u16 x, u8 y) {
    if (y >= 240) return 0;
    if (x >= level_width) return 1;

    return mt_info[screen_ptr[x >> 8][(y & 0xF0) | ((u8)x >> 4)]]
        & quarter_bit[((y >> 2) & 2) | ((x >> 3) & 1)];
}