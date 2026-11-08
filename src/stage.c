#include "stage.h"
#include "levels.h"
#include "level_chr.h"
#include "audio.h"

const Stage stages[] = {
    { &lvl_forest, CHR_forest, forest_spawns, forest_spirits, forest_traps,
           SONG_STAGE, 120, 160,   0 },

    /* the next one, once levels/castle.bmp and its lists exist:
    { &lvl_castle, CHR_castle, castle_spawns, castle_spirits, castle_traps,
           SONG_STAGE,  24, 160,   0 },
    */
};

const u8 stage_count = sizeof(stages) / sizeof(stages[0]);