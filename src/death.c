#include "death.h"
#include "player.h"
#include "enemy.h"
#include "spirit.h"
#include "trap.h"
#include "boss.h"
#include "hud.h"
#include "audio.h"

#define DEATH_BANK 1

#define DEATH_TEXT "YOU HAVE DIED"
#define DEATH_ROW  14

#define FREEZE_TIME   48
#define FADE_DELAY    6 
#define FADE_STEPS    4
#define TEXT_DELAY    10
#define TEXT_HOLD     150
#define TEXT_MIN      30
#define SKIP_BUTTONS  (PAD_START | PAD_A)

extern u8 full_pal[32];

static u8 player_pal[3];

#pragma code-name(push, "BANK1")
#pragma rodata-name(push, "BANK1")

static const char death_text[] = DEATH_TEXT;

static const u8 player_red[3] = { 0x06, 0x16, 0x26 };
static const u8 text_ramp[3] = { 0x0F, 0x06, 0x16 };

static void __fastcall__ fade_all(u8 level) {
    static u8 i, c, k;
    for (i = 0; i <32; ++i) {
        c = full_pal[i];
        for (k = level; k < FADE_STEPS; ++k) {
            c = c >= 0x10 ? c - 0x10 : 0x0F;
        }
        pal_buf[i] = c;
    }
    pal_update = 1;
}

static void __fastcall__ frozen_frame(u8 show_player) {
    oam_clear();
    hud_sprite0();
    if (show_player) player_draw();
    traps_draw();
    enemies_draw();
    spirits_draw();
    boss_draw();
    hud_update();
    ppu_wait_nmi();
}

static void __fastcall__ text_color(u8 c) {
    pal_buf[1] = c;
    pal_buf[2] = c;
    pal_buf[3] = c;
    pal_update = 1;
}

static void death_run(void) {
    static u8 t, i, n, c, pad, pad_prev;

    music_stop();
    sfx_play(SFX_DEATH, SFX_WORLD);
    hud_set_player_hp(0);

    for (i = 0; i < 32; ++i) full_pal[i] = pal_buf[i];
    for (i = 0; i < 3; ++i) {
        player_pal[i] = full_pal[17 + i];
        full_pal[17 + i] = player_red[i];
    }

    for (t = 0; t < FREEZE_TIME; ++t) {
        for (i = 0; i < 3; ++i) {
            pal_buf[17 + i] = (t & 4) ? player_pal[i] : player_red[i];
        }
        pal_update = 1;
        frozen_frame(1);
    }

    for (i = FADE_STEPS; i; --i) {
        fade_all(i - 1);
        for (t= 0; t < FADE_DELAY; ++t) frozen_frame(1);
    }

    ppu_off();
    hud_split = 0;
    oam_clear();
    scroll_x = 0;
    scroll_y = 0;
    ppu_ctrl &= ~(CTRL_NT_2400 | 0x02);
    vram_adr(NTADR_A(0, 0));
    vram_fill(0, 1024);
    for (n = 0; death_text[n]; ++n) {}
    vram_adr(NTADR_A((32 - n) >> 1, DEATH_ROW));
    for (i = 0; i < n; ++i) {
        c = death_text[i];
        if (c >= 'A' && c <= 'Z')      c = HUD_TILE_A + (c - 'A');
        else if (c >= '0' && c <= '9') c = HUD_TILE_0 + (c - '0');
        else if (c == '-')             c = HUD_TILE_DASH;
        else                           c = 0;
        vram_put(c);
    }

    for (i = 0; i <32; ++i) pal_buf[i] = 0x0F;
    pal_update = 1;
    ppu_on();

    for (i = 1; i < 3; ++i) {
        text_color(text_ramp[i]);
        for (t = 0; t < TEXT_DELAY; ++t) ppu_wait_nmi();
    }
    pad_prev = 0xFF;
    for (t = 0; t < TEXT_HOLD; ++t) {
        pad = pad_poll(0);
        if (t >= TEXT_MIN && (pad & ~pad_prev & SKIP_BUTTONS)) break;
        pad_prev = pad;
        ppu_wait_nmi();
    }
    for (i = 2; i; --i) {
        text_color(text_ramp[i - 1]);
        for (t = 0; t < TEXT_DELAY; ++t) ppu_wait_nmi();
    }
    audio_silence();
    ppu_off();

    for (i = 0; i < 3; ++i) full_pal[17 + i] = player_pal[i];
    for (i = 0; i < 32; ++i) pal_buf[i] = full_pal[i];
}

#pragma rodata-name(pop)
#pragma code-name(pop)

void death_screen(void) {
    static u8 old;
    old = prg_bank;
    mmc1_set_prg_bank(DEATH_BANK);
    death_run();
    mmc1_set_prg_bank(old);
}