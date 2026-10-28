/* level.h - room format and banked level data */
#ifndef LEVEL_H
#define LEVEL_H

#include "nes.h"

#define ROOM_W 16   /* metatiles across one screen */
#define ROOM_H 15   /* metatiles down (240 px) */

#define LEVEL_BANK 0
extern const u8 room_test[ROOM_W * ROOM_H];   /* lives in PRG bank 0 */

#endif