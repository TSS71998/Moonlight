/* nes.c - small runtime library: frame sync, VRAM, palettes, sprites, banking.
   Written in C to keep it readable; move hot paths to assembly later. */
#include "nes.h"
#include <string.h>

static u8 oam_idx;

void ppu_wait_nmi(void) {
    nmi_ready = 1;
    while (nmi_ready) {
    } /* NMI clears it when done */
}

void ppu_on(void) {
    ppu_mask = MASK_BG | MASK_SPR | MASK_BG_LEFT | MASK_SPR_LEFT;
    ppu_wait_nmi(); /* takes effect at the next vblank */
}

void ppu_off(void) {
    ppu_mask = 0;
    ppu_wait_nmi(); /* rendering is off once this returns */
}

void __fastcall__ vram_adr(u16 adr) {
    (void)PPUSTATUS; /* reset the address latch */
    PPUADDR = adr >> 8;
    PPUADDR = (u8)adr;
}

void __fastcall__ vram_put(u8 value) { PPUDATA = value; }

void __fastcall__ vram_fill(u8 value, u16 len) {
    while (len--)
        PPUDATA = value;
}

void __fastcall__ vram_write(const u8 *src, u16 len) {
    while (len--)
        PPUDATA = *src++;
}

void __fastcall__ vram_buf_put(u16 adr, const u8 *src, u8 len) {
    u8 i = vram_buf_len;
    if ((u16)i + len + 3 > sizeof(vram_buf))
    return; /* full: drop it */
    vram_buf[i++] = adr >> 8;
    vram_buf[i++] = (u8)adr;
    vram_buf[i++] = len;
    memcpy(&vram_buf[i], src, len);
    vram_buf_len = i + len;
}

void __fastcall__ pal_all(const u8 *pal) {
    memcpy(pal_buf, pal, 32);
    pal_update = 1;
}

void oam_clear(void) {
    memset(oam_buf, 0xFF, 256); /* Y = $FF puts a sprite off-screen */
    oam_idx = 0;
}

void __fastcall__ oam_spr(u8 x, u8 y, u8 tile, u8 attr) {
    u8 *s;
    if (oam_idx == 0 && oam_buf[0] != 0xFF)
        return; /* all 64 used */
    s = &oam_buf[oam_idx];
    s[0] = y - 1; /* sprites appear one line lower */
    s[1] = tile;
    s[2] = attr;
    s[3] = x;
    oam_idx += 4;
}

void __fastcall__ oam_meta_spr(u8 x, u8 y, const u8 *data, u8 flip) {
    static const u8 *p;
    static u8 dx;

    p = data;
    while (p[0] != MSPR_END) {
        dx = p[0];
        if (flip) {
            dx = (u8)(-8 - (s8)dx);
        }
        oam_spr(x + dx, y + p[1], p[2] | 1, p[3] ^ flip);
        p += 4;
    }
}

void __fastcall__ banked_copy(u8 bank, void *dst, const void *src, u16 len) {
    u8 old = prg_bank;
    mmc1_set_prg_bank(bank);
    memcpy(dst, src, len);
    mmc1_set_prg_bank(old);
}
