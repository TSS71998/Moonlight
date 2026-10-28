; mmc1.s - MMC1 (mapper 1) bank switching
;
; MMC1 registers are loaded one bit at a time: five writes, lowest bit first.
; The address of the fifth write picks the register:
;   $8000 control   (mirroring, PRG mode, CHR mode)
;   $A000 CHR bank 0 (PPU $0000-$0FFF in 4 KB mode)
;   $C000 CHR bank 1 (PPU $1000-$1FFF in 4 KB mode)
;   $E000 PRG bank   (CPU $8000-$BFFF in mode 3; bit 4 = 1 disables PRG-RAM)
;
; Caution: if an NMI interrupts a 5-write sequence and writes MMC1 itself,
; the registers get corrupted. Our NMI never touches MMC1, so these are
; safe to call from C at any time. If you later switch banks inside the NMI,
; make the NMI restore _prg_bank afterwards (a common pattern).

.export _mmc1_set_control, _mmc1_set_chr0, _mmc1_set_chr1
.export _mmc1_set_prg_bank, mmc1_init
.exportzp _prg_bank

.segment "ZEROPAGE"
_prg_bank: .res 1           ; the bank currently at $8000

.macro MMC1_WRITE addr
    sta addr
    lsr a
    sta addr
    lsr a
    sta addr
    lsr a
    sta addr
    lsr a
    sta addr
.endmacro

.segment "CODE"

; void __fastcall__ mmc1_set_control(unsigned char value);
_mmc1_set_control:
    MMC1_WRITE $8000
    rts

; void __fastcall__ mmc1_set_chr0(unsigned char bank);
_mmc1_set_chr0:
    MMC1_WRITE $A000
    rts

; void __fastcall__ mmc1_set_chr1(unsigned char bank);
_mmc1_set_chr1:
    MMC1_WRITE $C000
    rts

; void __fastcall__ mmc1_set_prg_bank(unsigned char bank);
_mmc1_set_prg_bank:
    sta _prg_bank
    MMC1_WRITE $E000
    rts

; Called once from crt0 before C starts.
;   control %11110 = CHR 4 KB mode, PRG mode 3 (fix last bank at $C000,
;   switch $8000), vertical mirroring (for horizontal scrolling).
mmc1_init:
    lda #$80
    sta $8000               ; reset the shift register
    lda #%11110
    jsr _mmc1_set_control
    lda #0
    jsr _mmc1_set_chr0      ; CHR bank 0 -> background tiles
    lda #1
    jsr _mmc1_set_chr1      ; CHR bank 1 -> sprite tiles
    lda #0
    jsr _mmc1_set_prg_bank
    rts