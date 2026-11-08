#include "nes.h"
#include "level.h"
#include "levels.h"
#include "player.h"
#include "hud.h"
#include "enemy.h"
#include "spirit.h"
#include "trap.h"
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

static void play_game(void) {
    static u8 pad, pad_prev, pressed;

    ppu_off();

    pal_all(palette);
    level_load(LEVEL_BANK, &lvl_forest);
    enemies_load(forest_spawns);
    spirits_load(forest_spirits);
    traps_load(forest_traps);

    player_init(120, 160);
    scroll_y = 0;
    level_draw(0);
    hud_init();
    oam_clear();
    hud_sprite0();
    music_play(SONG_STAGE);
    ppu_on();
    ppu_mask |= 0x60; 

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
            music_play(SONG_STAGE);
            oam_clear();
            hud_sprite0();
            ppu_on();
            ppu_mask |= 0x60;
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