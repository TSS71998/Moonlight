# Moonlight Sorrow: developer guide

How the game is built, how it works, and how to add to it. For the game
itself, see the [README](../README.md).

The game is written in C with cc65, plus assembly for the parts that run every
frame, on an MMC1 (mapper 1) cartridge. Graphics come from Aseprite and NEXXT, music from
FamiStudio and the build tools,
`tools/bmp2level.c` and `tools/nam2level.c`, are in C.

## Building

Requirements:

- **cc65** (2.19 or newer): `sudo apt install cc65`, `brew install cc65`, or
  the Windows snapshot from cc65.github.io
- **make** and a normal C compiler (`gcc`, `clang` or `cc`; on WSL/Ubuntu:
  `sudo apt install gcc`) to build the level converters. Set `HOSTCC=` to use
  a different compiler.

```
make                 # -> build/moonlight.nes
make run             # builds, then runs it in your emulator (set EMU in the Makefile)
make clean
```

The build also writes `build/moonlight.dbg` (load it in Mesen's debugger for
source-level C debugging) and `build/map.txt` (how full each bank is).

## Files

| File | Purpose |
|---|---|
| `src/main.c` | Setup, then the title screen and the game loop |
| `src/crt0.s` | iNES header, reset stubs in every bank, hardware init, NMI handler (with the HUD split) |
| `src/mmc1.s` | MMC1 register writes (`mmc1_set_prg_bank`, `mmc1_set_chr0/1`) |
| `src/pad.s` | Controller reading |
| `src/nes.h` / `nes.c` | Registers, frame sync, VRAM and palette helpers, banked copies |
| `src/oam.s` | Sprite drawing: `oam_clear`, `oam_spr`, `oam_meta_spr` (assembly) |
| `src/vram.s` | `vram_write` (assembly) |
| `src/bank0.c` | Data kept in PRG bank 0 (levels) |
| `src/level.h` / `level.c` | Loading levels, drawing them and scrolling the camera |
| `src/collide.s` | `level_solid_at`, the collision lookup (assembly) |
| `src/player.h` / `player.c` | Movement, animation, the sword, getting hurt (tuning in `player.h`) |
| `src/hero_sprites.h` / `hero_sprites.c` | The knight's frames and the sword pieces |
| `src/enemy.h` / `enemy.c` | Enemy types, spawning, behaviour and fighting |
| `src/enemy_sprites.h` / `enemy_sprites.c` | Enemy frames |
| `src/spawns.c` | Where enemies appear in each level |
| `src/hud.h` / `hud.c` | The status bar: score, player and enemy health |
| `src/title.h` / `title.c`, `title_data.s` | The title screen |
| `src/audio.h` / `audio.s`, `audio_data.s`, `audio_engine.s` | Music and sound effects |
| `chr/sprites.chr`, `chr/title.chr` | Sprite and title tiles (from NEXXT) |
| `chr/bg.chr` | Only its tiles `$C0`–`$FF` are used: the status bar font. The rest of the background tiles are made from the levels |
| `levels/` | The level pictures, collision pictures and palettes |
| `title/` | The title screen and its palette (from NEXXT) |
| `audio/` | The FamiStudio projects, their exports, and the sound engine |
| `cfg/mmc1.cfg` | Linker layout of the banks |
| `tools/bmp2level.c` | Turns `levels/*.bmp` into background tiles (`build/bg.chr`) and screens; built and run by `make` |
| `tools/nam2level.c` | Turns those screens into level data; built and run by `make`

## Cartridge layout

| Memory | Size | Contents |
|---|---|---|
| PRG bank 0 | 16 KB at `$8000` (swapped) | Level data |
| PRG bank 1 | 16 KB at `$8000` (swapped) | Title screen |
| PRG bank 2 | 16 KB at `$8000` (swapped) | Sound engine, music and sound effects |
| PRG banks 3–6 | 16 KB each | Free (`BANK3`–`BANK6` segments) |
| PRG bank 7 | 16 KB, fixed at `$C000` | All other code, runtime, NMI, vectors |
| CHR bank 0 | 4 KB at PPU `$0000` | Background tiles |
| CHR bank 1 | 4 KB at PPU `$1000` | Sprite tiles |
| CHR bank 2 | 4 KB, swapped in at PPU `$0000` on the title | Title screen tiles |
| CHR banks 3–7 | 20 KB | Free |
| PRG-RAM | 8 KB at `$6000` | The level map (`WRAM`); the sound engine's variables in the top 256 bytes (`AUDIORAM`) |
| RAM | `$0200` sprite buffer, `$0300–$06FF` variables, `$0700–$07FF` C stack | |

Mirroring is vertical: the two nametables sit side by side for horizontal
scrolling.

## How a frame works

1. The game updates the player and enemies, moves the camera, then draws
   sprites with `oam_clear()` and `oam_meta_spr()`. Tile changes (a new
   column of the level, HUD updates) are queued with `vram_buf_put()`.
2. `ppu_wait_nmi()` waits for vblank. The NMI sends the sprites, the palette
   and the queued tiles to the PPU, shows the HUD unscrolled, waits for the
   sprite-0 hit at the bottom of the HUD, and switches to the level's scroll
   for the rest of the screen.
3. Back in `ppu_wait_nmi()`, the sound engine plays one frame of music and
   effects.

For big screen changes (loading a level or the title), rendering is turned
off with `ppu_off()`, the screen is written directly, then `ppu_on()`.

### Frame budget

A frame has 29,780 CPU cycles. The HUD split takes about 6,000 and the
music about 1,600, leaving roughly 22,000 for the game. Walking uses about
11,000; the worst case measured (four enemies on screen while scrolling) is
right at the limit. To stay there, each enemy updates every other frame, the
level streams in 8 pixels at a time, and the routines that run most
(`oam.s`, `collide.s`, `vram.s`) are assembly. More on-screen action will
need more of the game in assembly.

## Levels

A level is one picture, drawn in Aseprite (or any paint program), plus a
second picture saying what's solid:

| File | What it is |
|---|---|
| `levels/NAME.bmp` | The level: 240 pixels tall, 256 pixels wide per screen (up to 32 screens), saved as an indexed 4- or 8-bit BMP |
| `levels/NAME_collision.bmp` | Same size: paint anything solid in any colour on black. Works in 8x8 cells; a cell with any coloured pixel is solid |
| `levels/NAME.pal` | The 16 NES colours of the background palettes (from NEXXT: Palettes > Save palettes) |

Then `make`. `tools/bmp2level.c` turns the picture into tiles and screens,
and `tools/nam2level.c` turns those into the game's level data
(`build/levels.c`/`.h`). Load a level with `level_load(LEVEL_BANK, &lvl_NAME)`.

Rules for the picture (`make` stops and says where if one is broken):

- **Palette layout:** the picture's first 16 colours are the 4 NES
  background palettes in order: colours 0–3 are palette 0, 4–7 palette 1,
  and so on. Colours 0, 4, 8 and 12 are the background (black). The shades
  in the picture only show which colour is which; what the NES shows comes
  from `NAME.pal`.
- **One palette per 16x16 square** (on the 16x16 grid). Using the same shade
  from another palette is fine.
- **192 different 8x8 tiles** for all levels together: tiles `$C0`–`$FF`
  are the status bar font, taken from `chr/bg.chr`. A tile that's solid in
  one place and not in another counts twice.
- **256 different 16x16 blocks** per level. If a level has more, `make`
  stops, says how many each screen adds, and writes
  `build/lvl/NAME_blocks.bmp`: the level with every block that's used only
  once in red, which shows where reusing pieces would help most.
- **Palette 1** is also the status bar's colours.
- The top 32 pixels are under the status bar and are left out, and so are
  empty screens at the end, so the canvas can be wider than what's drawn.

While playing, the level map lives in PRG-RAM and the camera streams in each
8-pixel column of tiles just before it comes into view. Positions in the
level are in level pixels; things are drawn at `x - camera_x`.

## The HUD

The top 32 pixels are a status bar that doesn't scroll:

```
SCORE-000000
PLAYER ||||||||||||||||
ENEMY  ||||||||||||||||
```

- Change it with `hud_add_score(points)`, `hud_set_player_hp(0-16)` and
  `hud_set_enemy_hp(0-16)`; `hud_update()` puts the changes on screen.
- The split relies on sprite 0: one pixel (tile `$50` of `sprites.chr`)
  hidden behind the long-stemmed "E" of ENEMY (tile `$C5` of `bg.chr`).
  **Keep both tiles** when editing the sheets in NEXXT. If either goes
  missing, the split turns itself off and the HUD scrolls with the level.
- The HUD font is in `bg.chr` at `$C0`–`$FF` (digits at `$D0`, A–Z at
  `$E0`); edit it in `chr/bg.chr`, which the build takes it from.

## Enemies

Each level has a spawn list in `src/spawns.c`:

```c
const EnemySpawn forest_spawns[] = {
    { 300, 184, ENEMY_SKELETON },   /* x (its middle), y (top of the floor), type */
    { 470, 176, ENEMY_BAT },
    { 0, 0, ENEMY_NONE }            /* end */
};
```

- An enemy appears just before its spot scrolls into view and goes away
  once it's well off screen; it comes back next time unless it was killed.
  Up to 4 are alive at once.
- Each type is a row of `enemy_types[]` in `enemy.c`: its frames, behaviour
  (`BEH_WALK` walks, turns at walls, climbs 8 px steps, falls off ledges;
  `BEH_FLY` flies in a wave), speed, animation speed, size, hit points, damage, palette
  and points.
- The sword hits each enemy once per swing. Touching an enemy hurts the
  knight and knocks him back; at 0 health, or in a pit, he goes back to
  the start of the level.
- Sprite sheet: knight `$00`–`$3F`, sword `$40`–`$49`, sprite 0 `$50`
  (don't touch), death flame `$52`–`$5D`, skeleton `$60`–`$6F`, bat
  `$80`–`$87`, free from `$A0`.
- To add an enemy: draw it, add its frames to `enemy_sprites.c`, give it an
  `ENEMY_` number in `enemy.h` and a row in `enemy_types[]` (same order).

## The title screen

The title is one full screen with its own 256 tiles, so it doesn't use any
of the level's tiles:

| File | From NEXXT |
|---|---|
| `chr/title.chr` | the tile sheet (4 KB) |
| `title/title.nam` | the nametable, with attributes, RLE off |
| `title/title.pal` | the palette (16 bytes) |

Use a separate NEXXT session for it, so it can't overwrite the game's tile
sheets. In `title.c`: `START_BUTTONS` (which buttons start the game),
`TITLE_EMPHASIS` (colour tint, 0 = none) and the fade speed.

## Music and sound effects

Sound uses the FamiStudio sound engine, with the engine and all sound data
in PRG bank 2.

To change the music or effects:

1. Edit `audio/moonlight_music.fms` and `audio/moonlight_sfx.fms` in
   [FamiStudio](https://famistudio.org). Effects are one short, non-looping
   song each.
2. Use only the NES's 5 channels, FamiStudio tempo mode, and no DPCM samples.
3. Export each as FamiStudio sound engine code, CA65 format: music to
   `audio/music.s`, effects to `audio/sfx.s`. Then `make`.
4. Songs and effects are numbered in project order; keep the `SONG_...` and
   `SFX_...` lists in `src/audio.h` in step.
5. If the top of `music.s` lists "Required flags", make sure each is set in
   `src/audio_engine.s`.

In the game:

```c
music_play(SONG_STAGE);
music_stop();
sfx_play(SFX_HIT, SFX_WORLD);   /* SFX_PLAYER or SFX_WORLD: two effects can play at once */
audio_silence();                /* before turning rendering off to load */
```

`music_play` and `sfx_play` ignore numbers that aren't in the exports, so a
missing effect is silent instead of crashing the game.

## Putting data in a bank

```c
#pragma rodata-name(push, "BANK3")
const u8 stage2_map[] = { ... };
#pragma rodata-name(pop)
```

Code in the fixed bank reads it with `banked_copy(3, ram_dest, stage2_map, size)`.

## cc65 tips for speed

- Prefer `u8` everywhere; 16-bit math is much slower.
- Make loop variables and hot locals `static`; cc65 handles stack locals slowly.
- Avoid multiply, divide and modulo on variables; use shifts and tables.
- Write `buf[i] = t; ++i;` with static `u8` values, not `buf[i++] = ptr[0];`:
  the first is one instruction, the second a slow runtime call.
- Pass arguments to hot functions in globals; the C argument stack is slow.
- Read `build/*.s` to see what the compiler produced. When porting to
  assembly, start with whatever is slowest.
