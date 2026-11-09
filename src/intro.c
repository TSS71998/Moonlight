#include "intro.h"
#include "player.h"
#include "hud.h"
#include "audio.h"
#include "level.h"
#include "enemy.h"
#include "spirit.h"
#include "trap.h"

#define INTRO_DARK        40
#define PLAYER_FADE_DELAY 10
#define INTRO_HOLD        60
#define WORLD_FADE_DELAY  8
#define FADE_STEPS        4

#define MUSIC_WITH_LEVEL 1

#define SKIP_BUTTONS (PAD_START | PAD_A)

#define CLEAR_TIME       48
#define CLEAR_FADE_START 8
#define CLEAR_FADE_DELAY 10
#define CLEAR_BLACK      100

extern u8 full_pal[32];

static u8 pad_prev;
static u8 skipping;

static void set_fade(u8 world, u8 player) {
    static u8 i, c, k, lv;
    for (i = 0; i < 32; ++i) {
        c = full_pal[i];
        lv = (i >= 17 && i <= 19) ? player : world;
        for (k = lv; k < FADE_STEPS; ++k) {
            c = c >= 0x10 ? c - 0x10 : 0x0F;
        }
        pal_buf[i] = c;
    }
    pal_update = 1;
}

static void intro_frame(void) {
    static u8 pad;
    pad = pad_poll(0);
    if (pad & ~pad_prev & SKIP_BUTTONS) skipping = 1;
    pad_prev = pad;

    oam_clear();
    hud_sprite0();
    player_draw();
    ppu_wait_nmi();
}

static void __fastcall__ intro_wait(u8 n) {
    while (n-- && !skipping) intro_frame();
}

void intro_begin(void) {
    static u8 i;
    for (i = 0; i < 32; ++i) full_pal[i] = pal_buf[i];
    set_fade(0, 0);
}

void __fastcall__ intro_play(u8 song) {
    static u8 i;

    skipping = 0;
    pad_prev = 0xFF;

#if !MUSIC_WITH_LEVEL
    music_play(song);
#endif

    intro_wait(INTRO_DARK);
    for(i = 1; i <= FADE_STEPS && !skipping; ++i) {
        set_fade(0, i);
        intro_wait(PLAYER_FADE_DELAY);
    }
    intro_wait(INTRO_HOLD);

#if MUSIC_WITH_LEVEL
    music_play(song);
#endif

    for (i = 1; i <= FADE_STEPS; ++i) {
        set_fade(i, FADE_STEPS);
        intro_frame();
        if (!skipping) intro_wait(WORLD_FADE_DELAY - 1);
        else intro_frame();
    }
}

void __fastcall__ stage_clear(u8 walk) {
    static u8 t, step, i, pad;
    static u16 old_x;

    music_stop();
    sfx_play(SFX_CLEAR, SFX_WORLD);
    for (i = 0; i < 32; ++i) full_pal[i] = pal_buf[i];

    pad = walk ? PAD_RIGHT : 0;
    step = FADE_STEPS;
    for (t = 0; t < CLEAR_TIME; ++t) {
        old_x = player_x;
        player_update(pad, 0);
        if (player_x == old_x) pad = 0;
        player_died = 0;
        if (walk) camera_update(player_x + PLAYER_WIDTH / 2);
        hud_update();

        if (step && t >= CLEAR_FADE_START
            && (u8)(t - CLEAR_FADE_START) % CLEAR_FADE_DELAY == 0) {
            set_fade(step - 1, step - 1);
            --step;
        }

        oam_clear();
        hud_sprite0();
        player_draw();
        traps_draw();
        enemies_draw();
        spirits_draw();
        ppu_wait_nmi();
    }
    set_fade(0, 0);
    for (t = 0; t < CLEAR_BLACK; ++t) ppu_wait_nmi();
    audio_silence();
}