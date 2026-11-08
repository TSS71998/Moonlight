# Build: make        -> build/moonlight.nes
# Run:   make run    -> opens it in your emulator (set EMU below)
# Clean: make clean

NAME    := moonlight
CC65    := cc65
CA65    := ca65
LD65    := ld65
# Your normal C compiler, used to build tools in tools/ (not for the NES)
HOSTCC  ?= cc
EMU     ?= /mnt/c/Users/Thesu/dev/rust_projects/nes_emulator
CFLAGS  := -O -t nes --add-source -I src -I build

BUILD   := build
C_SRC   := $(wildcard src/*.c)
S_SRC   := $(wildcard src/*.s)
OBJS    := $(C_SRC:src/%.c=$(BUILD)/%.o) $(S_SRC:src/%.s=$(BUILD)/%.o) $(BUILD)/levels.o
CHR     := $(BUILD)/bg.chr $(BUILD)/bg_more.chr chr/sprites.chr chr/title.chr
LEVELS  := $(filter-out %_collision.bmp,$(wildcard levels/*.bmp))
LVL     := $(BUILD)/lvl

# cc65 2.20+ renamed the C stack pointer from `sp` to `c_sp`; detect which.
HAS_C_SP := $(shell mkdir -p $(BUILD) && printf '.include "zeropage.inc"\nlda c_sp\n' > $(BUILD)/spcheck.s && $(CA65) $(BUILD)/spcheck.s -o $(BUILD)/spcheck.o 2>/dev/null && echo yes)
ASFLAGS  := -g $(if $(HAS_C_SP),-D CC65_C_SP) --bin-include-dir audio

.PHONY: all run clean

all: $(BUILD)/$(NAME).nes

$(BUILD)/$(NAME).nes: $(OBJS) cfg/mmc1.cfg
	$(LD65) -C cfg/mmc1.cfg -o $@ $(OBJS) nes.lib -m $(BUILD)/map.txt --dbgfile $(BUILD)/$(NAME).dbg

# C -> assembly -> object (the .s output is kept so you can read what cc65 made)
$(BUILD)/%.o: src/%.c src/*.h | $(BUILD)
	$(CC65) $(CFLAGS) $< -o $(BUILD)/$*.s
	$(CA65) $(ASFLAGS) $(BUILD)/$*.s -o $@

$(BUILD)/%.o: src/%.s | $(BUILD)
	$(CA65) $(ASFLAGS) $< -o $@

# Levels: pictures (levels/NAME.bmp + NAME_collision.bmp + NAME.pal)
#   -> tools/bmp2level.c -> each level's background tiles (build/bg.chr,
#      bg_more.chr, with the status bar font from chr/bg.chr; which CHR bank
#      is whose in build/level_chr.h) and one screen per 256 pixels
#   -> tools/nam2level.c -> build/levels.c/.h
# Both tools are compiled with HOSTCC and run on your computer.
$(BUILD)/nam2level $(BUILD)/bmp2level: $(BUILD)/%: tools/%.c | $(BUILD)
	$(HOSTCC) -O2 -o $@ $<

$(LVL)/stamp: $(LEVELS) $(wildcard levels/*_collision.bmp) $(wildcard levels/*.pal) chr/bg.chr $(BUILD)/bmp2level
	rm -rf $(LVL) && mkdir -p $(LVL)
	$(BUILD)/bmp2level -font chr/bg.chr -chr $(BUILD)/bg.chr -chr-more $(BUILD)/bg_more.chr \
		-h $(BUILD)/level_chr.h -o $(LVL) $(LEVELS)
	touch $@

$(BUILD)/bg.chr $(BUILD)/bg_more.chr $(BUILD)/level_chr.h: $(LVL)/stamp ;

$(BUILD)/levels.c: $(LVL)/stamp $(BUILD)/nam2level
	$(BUILD)/nam2level -o $(BUILD)/levels -c $(LVL)/collision.txt $(LVL)/*.nam \
		|| { rm -f $(BUILD)/levels.c $(BUILD)/levels.h; exit 1; }

$(BUILD)/levels.h: $(BUILD)/levels.c ;

$(BUILD)/levels.o: $(BUILD)/levels.c $(BUILD)/levels.h src/level.h src/nes.h
	$(CC65) $(CFLAGS) $< -o $(BUILD)/levels.s
	$(CA65) $(ASFLAGS) $(BUILD)/levels.s -o $@

$(BUILD)/main.o $(BUILD)/stage.o: $(BUILD)/levels.h $(BUILD)/level_chr.h

# CHR files (from NEXXT, and the made background tiles); rebuild when they change
$(BUILD)/crt0.o: $(CHR)

$(BUILD)/title_data.o: title/title.nam title/title.pal

$(BUILD)/audio_data.o: audio/music.s audio/sfx.s
$(BUILD)/audio_engine.o: audio/famistudio_ca65.s

$(BUILD):
	mkdir -p $(BUILD)

run: all
	cd $(EMU) && cargo run --release -- $(abspath $(BUILD)/$(NAME).nes)

clean:
	rm -rf $(BUILD)