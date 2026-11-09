#ifndef INTRO_H
#define INTRO_H

#include "nes.h"

void intro_begin(void);
void __fastcall__ intro_play(u8 song);
void __fastcall__ stage_clear(u8 walk);

#endif