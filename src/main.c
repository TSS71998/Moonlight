#include "nes.h"
#include "level.h"
#include "levels.h"
#include "player.h"
#include "hud.h"
#include "enemy.h"
#include "spirit.h"
#include "trap.h"
#include "intro.h"
#include "stage.h"
#include "title.h"
#include "audio.h"

static const u8 palette[32] = {
    /* background */
    0x0F, 0x0C, 0x18, 0x21,
    0x0F, 0x11, 0x22, 0x20,
    0x0F, 0x0C, 0x07, 0x1C,
    0x0F, 0x0B, 0x19, 0x21,
    /* sprites */
    0x0F, 0x0C, 0x21, 0x11,
    0x0F, 0x00, 0x10, 0x30, 
    0x0F, 0x2D, 0x04, 0x16,
    0x0F, 0x16, 0x27, 0x37,
};

static u8 stage;
static const Stage *st;
static u16 exit_x;

static void start_stage(void) {
    st = &stages[stage];
    ppu_off();

    pal_all(palette);
    mmc1_set_chr0(st->chr);
    level_load(LEVEL_BANK, st->level);
    enemies_load(st->enemies);
    spirits_load(st->spirits);
    traps_load(st->traps);
    exit_x = st->exit_x ? st->exit_x : level_width - 24;

    player_init(st->start_x, st->start_y);
    player_hp = PLAYER_MAX_HP;
    hud_set_player_hp(player_hp);
    hud_set_enemy_hp(16);

    player_init(120, 160);
    scroll_y = 0;
    camera_snap(player_x + PLAYER_WIDTH / 2);
    hud_init();
    oam_clear();
    hud_sprite0();
    intro_begin();
    ppu_on();
    ppu_mask |= 0x60;
    intro_play(st->song);
}

static void play_game(void) {
    static u8 pad, pad_prev, pressed;

    for (;;) {
        pad = pad_poll(0);
        pressed = pad & ~pad_prev;
        pad_prev = pad;

        player_update(pad, pressed);
        enemies_update();
        spirits_update();
        traps_update();
        if (player_died) {
            player_died = 0;
            enemies_reset();
            spirits_reset();
            traps_reset();
            audio_silence();
            ppu_off();
            camera_snap(player_x + PLAYER_WIDTH / 2);
            music_play(st->song);
            oam_clear();
            hud_sprite0();
            ppu_on();
            ppu_mask |= 0x60;
            continue;
        }
        if (player_x + PLAYER_WIDTH / 2 >= exit_x) {
            stage_clear();
            if (++stage == stage_count) return;
            start_stage();
            pad_prev = 0xFF;
            continue;
        } 
        camera_update(player_x + PLAYER_WIDTH / 2);

        hud_set_player_hp(player_hp);
        hud_update();

        oam_clear();
        hud_sprite0();
        player_draw();
        traps_draw();
        enemies_draw();
        spirits_draw();

        ppu_wait_nmi();
    }
}

void main(void) {
    ppu_ctrl = CTRL_NMI | CTRL_SPR_8X16;
    audio_init();
    for (;;) {
        title_screen();
        play_game();
    }
}