#include "nes.h"
#include "level.h"
#include "levels.h"
#include "player.h"
#include "hud.h"

static const u8 palette[32] = {
    /* background */
    0x0F, 0x0B, 0x18, 0x21,
    0x0F, 0x11, 0x22, 0x20,
    0x0F, 0x0C, 0x07, 0x1C,
    0x0F, 0x0B, 0x19, 0x21,
    /* sprites */
    0x0F, 0x0C, 0x21, 0x11,
    0x0F, 0x00, 0x10, 0x30, 
    0x0F, 0x1A, 0x2A, 0x3A,
    0x0F, 0x12, 0x22, 0x32,
};

void main(void) {
    static u8 pad, pad_prev, pressed;

    ppu_off();
    ppu_ctrl = CTRL_NMI | CTRL_SPR_8X16;

    pal_all(palette);
    level_load(LEVEL_BANK, &lvl_forest);

    player_init(120, 160);
    scroll_y = 0;
    level_draw(0);
    hud_init();
    oam_clear();
    hud_sprite0();
    ppu_on();
    ppu_mask |= 0x60; 

    for (;;) {
        pad = pad_poll(0);
        pressed = pad & ~pad_prev;
        pad_prev = pad;

        player_update(pad, pressed);
        camera_update(player_x + PLAYER_WIDTH / 2);

        //TESTING SCORE
        if (pressed & PAD_SELECT) {
            hud_add_score(150);
            hud_set_player_hp(hud_player_hp ? hud_player_hp - 1 : HUD_BAR_LEN);
            hud_set_enemy_hp(hud_enemy_hp >= 3 ? hud_enemy_hp - 3 : HUD_BAR_LEN);
        }
        hud_update();

        oam_clear();
        hud_sprite0();
        player_draw();

        ppu_wait_nmi();
    }
}