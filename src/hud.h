#ifndef HUD_H
#define HUD_H

#include "nes.h"

#define HUD_ROWS    4
#define HUD_HEIGHTS (HUD_ROWS * 8)
#define HUD_PAL     1
#define HUD_BAR_LEN 16

#define HUD_TILE_BLANK  0xC0
#define HUD_TILE_PLAYER 0xC1            /* full segment, player bar */
#define HUD_TILE_ENEMY  0xC2            /* full segment, enemy bar */
#define HUD_TILE_EMPTY  0xC3            /* empty segment (outline) */
#define HUD_TILE_DASH   0xC4
#define HUD_TILE_SPLIT  0xC5            /* "E" of ENEMY, its stem 1 line longer */
#define HUD_TILE_0      0xD0            /* digits 0-9: $D0-$D9 */
#define HUD_TILE_A      0xE0            /* letters A-Z: $E0-$F9 */

#define HUD_SPRITE0_TILE 0x51

void hud_init(void);

void __fastcall__ hud_add_score(u16 points);
void __fastcall__ hud_set_player_hp(u8 hp);
void __fastcall__ hud_set_enemy_hp(u8 hp);

extern u8 hud_player_hp;
extern u8 hud_enemy_hp;

void hud_update(void);
void hud_sprite0(void);

#endif