# Build: make        -> build/moonlight.nes
# Run:   make run    -> opens it in your emulator (set EMU below)
# Clean: make clean

NAME    := moonlight
CC65    := cc65
CA65    := ca65
LD65    := ld65
PYTHON  := python3
EMU     ?= /mnt/c/Users/Thesu/dev/rust_projects/nes_emulator
CFLAGS  := -O -t nes --add-source

BUILD   := build
C_SRC   := $(wildcard src/*.c)
S_SRC   := $(wildcard src/*.s)
OBJS    := $(C_SRC:src/%.c=$(BUILD)/%.o) $(S_SRC:src/%.s=$(BUILD)/%.o)
CHR     := chr/bg.chr chr/sprites.chr

# cc65 2.20+ renamed the C stack pointer from `sp` to `c_sp`; detect which.
HAS_C_SP := $(shell mkdir -p $(BUILD) && printf '.include "zeropage.inc"\nlda c_sp\n' > $(BUILD)/spcheck.s && $(CA65) $(BUILD)/spcheck.s -o $(BUILD)/spcheck.o 2>/dev/null && echo yes)
ASFLAGS  := -g $(if $(HAS_C_SP),-D CC65_C_SP)

.PHONY: all run clean chr

all: $(BUILD)/$(NAME).nes

$(BUILD)/$(NAME).nes: $(OBJS) cfg/mmc1.cfg
	$(LD65) -C cfg/mmc1.cfg -o $@ $(OBJS) nes.lib -m $(BUILD)/map.txt --dbgfile $(BUILD)/$(NAME).dbg

# C -> assembly -> object (the .s output is kept so you can read what cc65 made)
$(BUILD)/%.o: src/%.c src/*.h | $(BUILD)
	$(CC65) $(CFLAGS) $< -o $(BUILD)/$*.s
	$(CA65) $(ASFLAGS) $(BUILD)/$*.s -o $@

$(BUILD)/%.o: src/%.s | $(BUILD)
	$(CA65) $(ASFLAGS) $< -o $@

$(BUILD)/crt0.o: $(CHR)

# Regenerate placeholder graphics from tools/make_chr.py
chr:
	$(PYTHON) tools/make_chr.py

$(CHR):
	$(PYTHON) tools/make_chr.py

$(BUILD):
	mkdir -p $(BUILD)

run: all
	cd $(EMU) && cargo run --release -- $(abspath $(BUILD)/$(NAME).nes)

clean:
	rm -rf $(BUILD)