#include "title.h"
#include "hud.h"
#include "audio.h"

extern const u8 title_nam[1024];
extern const u8 title_pal[16];

static const u8 title_sprite_pal[16] = {
    0x0F, 0x0C, 0x21, 0x11,
    0x0F, 0x00, 0x10, 0x30,
    0x0F, 0x2D, 0x04, 0x16,
    0x0F, 0x16, 0x27, 0x37,
};

#define TITLE_EMPHASIS 0x00

#define START_BUTTONS (PAD_START | PAD_SELECT)

#define FADE_STEPS 4
#define FADE_DELAY 4

static u8 full_pal[32];

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

void title_screen(void) {
    static u8 old_bank, i, pad, pad_prev;

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
    mmc1_set_prg_bank(old_bank);
    for (i = 0; i < 16; ++i) full_pal[16 + i] = title_sprite_pal[i];

    set_fade(0);
    music_play(SONG_TITLE);
    ppu_on();
    ppu_mask |= TITLE_EMPHASIS;
    for (i = 1; i <= FADE_STEPS; ++i) {
        wait_frames(FADE_DELAY);
        set_fade(i);
    }

    pad_prev = 0xFF;
    for (;;) {
        pad = pad_poll(0);
        if (pad & ~pad_prev & START_BUTTONS) break;    
        pad_prev = pad;
        ppu_wait_nmi();
    }

    music_stop();
    sfx_play(SFX_START, SFX_WORLD);
    wait_frames(30);

    for (i = FADE_STEPS; i; --i) {
        set_fade(i - 1);
        wait_frames(FADE_DELAY);
    }
    audio_silence();
    ppu_off();
    mmc1_set_chr0(0);
}