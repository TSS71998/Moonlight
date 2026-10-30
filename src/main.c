#include "nes.h"
#include "level.h"
#include "player.h"

static const u8 palette[32] = {
    /* background */
    0x0F, 0x00, 0x10, 0x07,     /* stone grays + dark brown */
    0x0F, 0x06, 0x16, 0x26,     /* brick reds */
    0x0F, 0x01, 0x11, 0x21,
    0x0F, 0x09, 0x19, 0x29,
    /* sprites */
    0x0F, 0x0C, 0x21, 0x11,     /* hero: dark brown, brown, skin */
    0x0F, 0x14, 0x24, 0x34,
    0x0F, 0x1A, 0x2A, 0x3A,
    0x0F, 0x12, 0x22, 0x32,
};

void main(void) {
    static u8 pad, pad_prev, pressed;

    ppu_off();
    ppu_ctrl = CTRL_NMI | CTRL_SPR_8X16;

    level_load(LEVEL_BANK, room_test);
    level_draw();
    pal_all(palette);

    player_init(120, 160);
    scroll_x = 0;
    scroll_y = 0;
    ppu_on();

    for (;;) {
        pad = pad_poll(0);
        pressed = pad & ~pad_prev;
        pad_prev = pad;

        player_update(pad, pressed);

        oam_clear();
        player_draw();

        ppu_wait_nmi();
    }
}