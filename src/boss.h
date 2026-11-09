#ifndef BOSS_H
#define BOSS_H

#include "nes.h"

#define BOSS_NONE    0
#define BOSS_PHANTOM 1

#define BP_OFF   0
#define BP_FIGHT 1
#define BP_DONE  2
extern u8 boss_phase;
extern u16 boss_arena_x;

void __fastcall__ boss_load(u8 type);

void boss_update(void);
void boss_draw(void);

#endif