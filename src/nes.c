/* nes.c - small runtime library: frame sync, VRAM, palettes, sprites, banking.
   Written in C to keep it readable; move hot paths to assembly later. */
#include "nes.h"
#include <string.h>

static u8 oam_idx;
u8 oam_top;

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

void __fastcall__ banked_copy(u8 bank, void *dst, const void *src, u16 len) {
    u8 old = prg_bank;
    mmc1_set_prg_bank(bank);
    memcpy(dst, src, len);
    mmc1_set_prg_bank(old);
}
