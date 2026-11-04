#include "title.h"
#include "hud.h"

extern const u8 title_nam[1024];
extern const u8 title_pal[16];

#define PRESS_X   10
#define PRESS_Y   20
#define PRESS_LEN 11

static const u8 title_sprite_pal[16] = {
    0x0F, 0x0C, 0x21, 0x11,
    0x0F, 0x00, 0x10, 0x30,
    0x0F, 0x2D, 0x04, 0x16,
    0x0F, 0x16, 0x27, 0x37,
};

#define TITTLE_EMPHASIS 0x00

#define FADE_STEPS 4
#define FADE_DELAY 4

static u8 full_pal[32];
static u8 press_tiles[PRESS_LEN];
static u8 blank_tiles[PRESS_LEN];

static void __fastcall__ set_fade(u8 level) {
    static u8 i, c, k;
    for (i = 0; i < 32; ++i) {
        c =full_pal[i];
        for (k = level; k < FADE_STEPS; ++k) {
            c = c >= 0x10 ? c - 0x10 : 0x0F;
        }
        pal_buf[i] = c;
    }
    pal_update = 1;
}

static void __fastcall__ wait_frames(u8 n) {
    while (n--) ppu_wait_nmi();
}

static void __fastcall__ show_press(u8 on) {
    vram_buf_put(NTADR_A(PRESS_X, PRESS_Y), on ? press_tiles : blank_tiles, PRESS_LEN);
}

void title_screen(void) {
    static u8 old_bank, i, pad, pad_prev, pressed, timer;

    ppu_off();

    hud_split = 0;
    oam_top = 0;
    oam_clear();
    scroll_x = 0;
    scroll_y = 0;
    ppu_ctrl &= ~(CTRL_NT_2400 | 0x02);

    mmc1_set_chr0(TITLE_CHR_BANK);
    old_bank = prg_bank;
    mmc1_set_prg_bank(TITLE_BANK);
    vram_adr(NTADR_A(0,0));
    vram_write(title_nam, 1024);
    for (i = 0; i < 16; ++i) full_pal[i] = title_pal[i];
    for (i = 0; i < PRESS_LEN; ++i) {
        press_tiles[i] = title_nam[PRESS_Y * 32 + PRESS_X + i];
        blank_tiles[i] = 0;
    }
    mmc1_set_prg_bank(old_bank);
    for (i = 0; i < 16; ++i) full_pal[16 + i] = title_sprite_pal[i];

    set_fade(0);
    ppu_on();
    ppu_mask |= TITTLE_EMPHASIS;
    for (i = 1; i <= FADE_STEPS; ++i) {
        wait_frames(FADE_DELAY);
        set_fade(i);
    }

    pad_prev = 0xFF;
    timer = 0;
    for (;;) {
        pad = pad_poll(0);
        pressed = pad & ~pad_prev;
        pad_prev = pad;
        if (pressed & PAD_START) break;

        ++timer;
        if (timer == 40) show_press(0);
        else if (timer == 60) { show_press(1); timer = 0; }
        ppu_wait_nmi();

        for (i = 0; i < 12; i++)
        {
            show_press(i & 1);
            wait_frames(4);
        }
        show_press(1);

        for (i = FADE_STEPS; i; --i) {
            set_fade(i - 1);
            wait_frames(FADE_DELAY);
        }
        ppu_off();
        mmc1_set_chr0(0);
    }
}