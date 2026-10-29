# Moonlight

An NES game in C (cc65) on an MMC1 cartridge. This is the project setup: the
runtime, banking and build are in place, and `main.c` runs a movement test room
(a room loaded from a switchable bank, plus a sprite you move with the d-pad).

## Requirements

- **cc65** (2.19 or newer): `sudo apt install cc65`, `brew install cc65`, or
  the Windows snapshot from cc65.github.io
- **make** and **Python 3** (only for regenerating placeholder graphics)

## Build and run

```
make                 # -> build/game.nes
make run             # builds, then runs it in ../NES-Emulator (set EMU=path to change)
make clean
```

The build also writes `build/game.dbg` (load it in Mesen's debugger for
source-level C debugging) and `build/map.txt` (how full each bank is).

## Cartridge layout (MMC1 / mapper 1)

| Memory | Size | Contents |
|---|---|---|
| PRG bank 0–6 | 16 KB each, swapped in at `$8000` | Level data, music, extra code (`BANK0`–`BANK6` segments) |
| PRG bank 7 | 16 KB, fixed at `$C000` | All C code, runtime, NMI, vectors |
| CHR bank 0 | 4 KB at PPU `$0000` | Background tiles |
| CHR bank 1 | 4 KB at PPU `$1000` | Sprite tiles |
| CHR banks 2–7 | 24 KB | Free, for more tile sets (swap with `mmc1_set_chr0/1`) |
| PRG-RAM | 8 KB at `$6000` | `WRAM` segment (battery saves later) |
| RAM | `$0200` OAM buffer, `$0300–$06FF` variables, `$0700–$07FF` C stack | |

Mirroring is vertical, which suits horizontal scrolling.

## Files

| File | Purpose |
|---|---|
| `src/crt0.s` | iNES header, reset stubs in every bank, hardware init, C runtime start, NMI handler |
| `src/mmc1.s` | MMC1 register writes (`mmc1_set_prg_bank`, `mmc1_set_chr0/1`, `mmc1_set_control`) |
| `src/pad.s` | Controller reading |
| `src/nes.h` / `src/nes.c` | Registers, frame sync, VRAM, palette, sprite and banked-copy helpers |
| `src/level.h` / `src/level.c` | Rooms, metatiles (tiles, palette, collision), drawing, `level_solid_at` |
| `src/bank0.c` | Level data stored in PRG bank 0 (currently the movement test room) |
| `src/player.h` / `src/player.c` | Player movement: walking, committed jump, gravity, collision, drawing (tuning constants in `player.h`) |
| `src/main.c` | Setup and the main loop |
| `cfg/mmc1.cfg` | Linker layout of the banks |
| `tools/make_chr.py` | Builds placeholder `chr/*.chr` from text art |
| `tools/headless_shot.rs` | Runs the ROM in your emulator without a window and saves screenshots |

## How a frame works

1. Your C code updates the game, calls `oam_clear()` and `oam_spr()` for sprites,
   and queues tile changes with `vram_buf_put()`.
2. `ppu_wait_nmi()` waits for vblank. The NMI then DMAs sprites, uploads the
   palette (if `pal_all()` was called), applies queued tile writes, and sets
   scroll and `PPUCTRL`/`PPUMASK` from `scroll_x`, `scroll_y`, `ppu_ctrl`, `ppu_mask`.

For big VRAM writes, such as loading a level, call `ppu_off()`, write directly
with `vram_adr()`/`vram_put()`, then `ppu_on()`.

## Putting data in a bank

```c
#pragma rodata-name(push, "BANK2")
const u8 stage2_map[] = { ... };
#pragma rodata-name(pop)
```

Code in the fixed bank reads it with `banked_copy(2, ram_dest, stage2_map, size)`.
C code can also go in a bank with `#pragma code-name(push, "BANK1")`, but it can
only be called while that bank is mapped, so keep it to self-contained
routines and call them from the fixed bank after switching.

## cc65 tips for speed

- Prefer `u8` (unsigned char) everywhere. 16-bit math is much slower.
- Make loop variables and hot locals `static`, as in `main.c`. cc65 handles
  stack-based locals slowly.
- Avoid multiply, divide and modulo on variables. Use shifts and lookup tables.
- Read `build/*.s` to see what the compiler produced. When you port to
  assembly, start with whatever is slowest.

## Testing headless

`tools/headless_shot.rs` is a Cargo example for your emulator. Copy it to
`NES-Emulator/examples/` and run:

```
cargo run --release --example headless_shot -- path/to/game.nes out
```

It holds RIGHT on frames 60–119 and saves `out_59.ppm` and `out_149.ppm`.
