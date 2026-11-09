#ifndef AUDIO_H
#define AUDIO_H

#include "nes.h"

#define SONG_TITLE 0
#define SONG_STAGE 1
#define SONG_BOSS  2


#define SFX_ENEMY_DIE 0
#define SFX_HIT       1
#define SFX_HURT      2
#define SFX_JUMP      3
#define SFX_START     4
#define SFX_SWING     5
#define SFX_BREAK     6
#define SFX_PICKUP    7
#define SFX_SPIKE     8
#define SFX_CLEAR     9

#define SFX_PLAYER 0
#define SFX_WORLD  1

void audio_init(void);

void audio_update(void);

void __fastcall__ music_play(u8 song);
void music_stop(void);
void __fastcall__ music_pause(u8 pause);

void __fastcall__ sfx_play(u8 sfx, u8 stream);

void audio_silence(void);

#endif