#ifndef PLAYER_H
#define PLAYER_H

#include "nes.h"

#define WALK_SPEED    0x0100
#define JUMP_SPEED    0x0100
#define JUMP_VELOCITY 0x0380
#define GRAVITY       0x0028
#define MAX_FALL      0x0400

#define PLAYER_WIDTH  16
#define PLAYER_HEIGHT 32
#define HB_LEFT       3
#define HB_RIGHT      12
#define HB_TOP        2
#define HB_BOTTOM     31

#define FACE_RIGHT 0
#define FACE_LEFT  1

extern u16 player_x;
extern u16 player_y;      
extern s16 player_vy;       
extern s16 player_jump_vx;
extern u8  player_facing;
extern u8  player_on_ground;

void __fastcall__ player_init(u8 x, u8 y);

void __fastcall__ player_update(u8 pad, u8 pressed);

void player_draw(void);

#endif