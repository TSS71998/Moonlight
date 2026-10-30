/* nes.h - hardware definitions and the small runtime library for C code */
#ifndef NES_H
#define NES_H

/* ---- Types ------------------------------------------------------------- */
typedef unsigned char u8;
typedef signed char s8;
typedef unsigned int u16;
typedef signed int s16;

/* ---- PPU registers ------------------------------------------------------ */
#define PPUCTRL (*(volatile u8 *)0x2000)
#define PPUMASK (*(volatile u8 *)0x2001)
#define PPUSTATUS (*(volatile u8 *)0x2002)
#define PPUADDR (*(volatile u8 *)0x2006)
#define PPUDATA (*(volatile u8 *)0x2007)

/* PPUCTRL bits */
#define CTRL_NMI 0x80
#define CTRL_SPR_8X16 0x20
#define CTRL_BG_1000 0x10  /* background tiles from $1000 */
#define CTRL_SPR_1000 0x08 /* 8x8 sprite tiles from $1000 */
#define CTRL_INC_32 0x04   /* PPUDATA steps by 32 (down a column) */
#define CTRL_NT_2400 0x01  /* base nametable bit (scroll X high bit) */

/* PPUMASK bits */
#define MASK_SPR 0x10
#define MASK_BG 0x08
#define MASK_SPR_LEFT 0x04
#define MASK_BG_LEFT 0x02

/* Nametable address of tile (x, y) in nametable 0 ($2000) or 1 ($2400) */
#define NTADR_A(x, y) (0x2000 | ((y) << 5) | (x))
#define NTADR_B(x, y) (0x2400 | ((y) << 5) | (x))

/* ---- Controller --------------------------------------------------------- */
#define PAD_A 0x80
#define PAD_B 0x40
#define PAD_SELECT 0x20
#define PAD_START 0x10
#define PAD_UP 0x08
#define PAD_DOWN 0x04
#define PAD_LEFT 0x02
#define PAD_RIGHT 0x01

u8 __fastcall__ pad_poll(u8 port);

/* ---- Sprite attributes -------------------------------------------------- */
#define SPR_FLIP_H 0x40
#define SPR_FLIP_V 0x80
#define SPR_BEHIND_BG 0x20

/* ---- MMC1 -------------------------------------------------------------- */
void __fastcall__ mmc1_set_control(u8 value);
void __fastcall__ mmc1_set_chr0(u8 bank);
void __fastcall__ mmc1_set_chr1(u8 bank);
void __fastcall__ mmc1_set_prg_bank(u8 bank);
extern u8 prg_bank; /* bank currently mapped at $8000 */
#pragma zpsym("prg_bank")

/* ---- State shared with the NMI handler (crt0.s) ------------------------- */
extern u8 frame_count;
extern u8 nmi_ready;
extern u8 ppu_ctrl;
extern u8 ppu_mask;
extern u8 scroll_x;
extern u8 scroll_y;
extern u8 pal_update;
extern u8 vram_buf_len;
#pragma zpsym("frame_count")
#pragma zpsym("nmi_ready")
#pragma zpsym("ppu_ctrl")
#pragma zpsym("ppu_mask")
#pragma zpsym("scroll_x")
#pragma zpsym("scroll_y")
#pragma zpsym("pal_update")
#pragma zpsym("vram_buf_len")

extern u8 pal_buf[32];
extern u8 vram_buf[128];
extern u8 oam_buf[256];

/* ---- Runtime functions (nes.c) ----------------------------------------- */

/* Frame sync: hand this frame's changes to the NMI and wait for it. */
void ppu_wait_nmi(void);

/* Rendering on/off. Turn it off for big VRAM writes (loading a level). */
void ppu_on(void);
void ppu_off(void);

/* Direct VRAM access: only while rendering is off. */
void __fastcall__ vram_adr(u16 adr);
void __fastcall__ vram_put(u8 value);
void __fastcall__ vram_fill(u8 value, u16 len);
void __fastcall__ vram_write(const u8 *src, u16 len);

/* Buffered VRAM writes, uploaded during the next NMI (rendering on).
   Keep the total under ~100 bytes per frame. */
void __fastcall__ vram_buf_put(u16 adr, const u8 *src, u8 len);

/* Palettes: 32 bytes (4 BG + 4 sprite palettes), uploaded next NMI. */
void __fastcall__ pal_all(const u8 *pal);

/* Sprites: oam_clear() then oam_spr() for each sprite, every frame. */
void oam_clear(void);
void __fastcall__ oam_spr(u8 x, u8 y, u8 tile, u8 attr);

#define MSPR_END 128
void __fastcall__ oam_meta_spr(u8 x, u8 y, const u8 *data, u8 flip);


/* Banked reads: copy len bytes from src in PRG bank `bank` into dst,
   restoring the previous bank afterwards. */
void __fastcall__ banked_copy(u8 bank, void *dst, const void *src, u16 len);

#endif
