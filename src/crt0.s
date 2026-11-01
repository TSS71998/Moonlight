; crt0.s - startup code for an MMC1 cartridge running cc65 C code
;
; Contains: the iNES header, MMC1 reset stubs for every bank, hardware init,
; the C runtime setup, and the NMI handler that uploads sprites, palette,
; VRAM updates and scroll each frame.

.include "zeropage.inc"

.ifdef CC65_C_SP           ; cc65 2.20+ calls the C stack pointer c_sp
    C_SP = c_sp
.else
    C_SP = sp
.endif

.import _main
.import copydata, initlib
.import __RAM_START__, __RAM_SIZE__, __STACKSIZE__
.import mmc1_init

.export __STARTUP__ : absolute = 1    ; stops nes.lib's own crt0 being linked
.export _oam_buf     = $0200

.exportzp _frame_count, _nmi_ready, _ppu_ctrl, _ppu_mask
.exportzp _scroll_x, _scroll_y, _pal_update, _vram_buf_len, _hud_split
.export   _pal_buf, _vram_buf

PPUCTRL   = $2000
PPUMASK   = $2001
PPUSTATUS = $2002
OAMADDR   = $2003
PPUSCROLL = $2005
PPUADDR   = $2006
PPUDATA   = $2007
OAMDMA    = $4014
APUFRAME  = $4017
DMCFREQ   = $4010

; ---------------------------------------------------------------------------
; iNES header: mapper 1, 128 KB PRG, 32 KB CHR, PRG-RAM at $6000
; ---------------------------------------------------------------------------
.segment "HEADER"
    .byte "NES", $1A
    .byte 8                 ; PRG-ROM in 16 KB units (8 = 128 KB)
    .byte 4                 ; CHR-ROM in 8 KB units  (4 = 32 KB)
    .byte $10               ; mapper 1 low nibble; set bit 1 for battery saves
    .byte $00               ; mapper high nibble
    .byte 1                 ; PRG-RAM in 8 KB units
    .res 7, 0

; ---------------------------------------------------------------------------
; Zero page variables shared with C (see nes.h)
; ---------------------------------------------------------------------------
.segment "ZEROPAGE"
_frame_count:  .res 1       ; incremented every NMI
_nmi_ready:    .res 1       ; C sets 1, NMI does its updates and clears it
_ppu_ctrl:     .res 1       ; shadow of PPUCTRL, written every NMI
_ppu_mask:     .res 1       ; shadow of PPUMASK, written every NMI
_scroll_x:     .res 1
_scroll_y:     .res 1
_pal_update:   .res 1       ; nonzero = upload _pal_buf this NMI
_vram_buf_len: .res 1       ; bytes used in _vram_buf
_hud_split:    .res 1
split_x:       .res 1
split_ctrl:    .res 1

.segment "BSS"
_pal_buf:  .res 32          ; 32 palette entries
_vram_buf: .res 128         ; [hi, lo, len, data...]... terminated by $FF

; ---------------------------------------------------------------------------
; MMC1 reset stubs. On power-up some MMC1 revisions map any bank at $C000,
; so every bank carries this stub and vectors at $FFF0. Writing a byte with
; bit 7 set resets MMC1 into "last bank fixed at $C000" mode, then we jump
; into the real startup code in the fixed bank.
; ---------------------------------------------------------------------------
.macro RESET_STUB seg
    .segment seg
    .scope
    stub:
        sei
        ldx #$FF
        txs
        stx $8000           ; bit 7 set: reset MMC1 shift register + mode
        jmp start
        .addr nmi, stub, irq
    .endscope
.endmacro

RESET_STUB "STUB0"
RESET_STUB "STUB1"
RESET_STUB "STUB2"
RESET_STUB "STUB3"
RESET_STUB "STUB4"
RESET_STUB "STUB5"
RESET_STUB "STUB6"
RESET_STUB "STUB7"

; ---------------------------------------------------------------------------
; Startup
; ---------------------------------------------------------------------------
.segment "STARTUP"
start:
    sei
    cld
    ldx #$40
    stx APUFRAME            ; disable APU frame IRQ
    ldx #$FF
    txs
    inx                     ; X = 0
    stx PPUCTRL             ; NMI off
    stx PPUMASK             ; rendering off
    stx DMCFREQ             ; DMC IRQ off

    bit PPUSTATUS           ; clear the vblank flag
@vblank1:
    bit PPUSTATUS           ; wait for the first vblank (PPU warming up)
    bpl @vblank1

    ; clear 2 KB of RAM, fill the OAM buffer with $FF (sprites off-screen)
    lda #0
@clear:
    sta $0000,x
    sta $0100,x
    sta $0300,x
    sta $0400,x
    sta $0500,x
    sta $0600,x
    sta $0700,x
    inx
    bne @clear
    lda #$FF
@clear_oam:
    sta $0200,x
    inx
    bne @clear_oam

@vblank2:
    bit PPUSTATUS           ; second vblank: PPU is ready
    bpl @vblank2

    jsr mmc1_init           ; mirroring, PRG/CHR banking mode, initial banks

    ; C runtime: parameter stack at the top of RAM, copy DATA, constructors
    lda #<(__RAM_START__ + __RAM_SIZE__ + __STACKSIZE__)
    sta C_SP
    lda #>(__RAM_START__ + __RAM_SIZE__ + __STACKSIZE__)
    sta C_SP+1
    jsr copydata
    jsr initlib

    lda #%10000000          ; NMI on, BG at $0000, sprites at $1000 (set in C)
    sta _ppu_ctrl
    sta PPUCTRL

    jmp _main

; ---------------------------------------------------------------------------
; NMI: runs at the start of every vblank. If C has a frame ready
; (_nmi_ready = 1) it uploads sprites, palette, buffered VRAM writes and
; scroll; otherwise it only counts the frame.
; ---------------------------------------------------------------------------
.segment "CODE"
nmi:
    pha
    txa
    pha
    tya
    pha

    lda _nmi_ready
    bne @upload
    jmp @done
@upload:
    ; sprites: DMA the 256-byte OAM buffer at $0200
    lda #0
    sta OAMADDR
    lda #>_oam_buf
    sta OAMDMA

    ; palette
    lda _pal_update
    beq @no_pal
    bit PPUSTATUS
    lda #$3F
    sta PPUADDR
    lda #$00
    sta PPUADDR
    ldx #0
@pal_loop:
    lda _pal_buf,x
    sta PPUDATA
    inx
    cpx #32
    bne @pal_loop
    lda #0
    sta _pal_update
@no_pal:

    ; VRAM update buffer: entries of [addr hi, addr lo, length, bytes...]
    ldx #0
@vram_entry:
    cpx _vram_buf_len
    bcs @vram_done
    lda _ppu_ctrl
    and #%11111011          ; +1 per byte...
    ldy _vram_buf,x
    bpl @vram_across
    ora #%00000100    
@vram_across:
    sta PPUCTRL
    bit PPUSTATUS
    lda _vram_buf,x
    and #$3F
    sta PPUADDR
    inx
    lda _vram_buf,x
    sta PPUADDR
    inx
    ldy _vram_buf,x         ; length
    inx
@vram_byte:
    lda _vram_buf,x
    sta PPUDATA
    inx
    dey
    bne @vram_byte
    jmp @vram_entry
@vram_done:
    lda #0
    sta _vram_buf_len

    ; scroll and control registers must come after all PPUADDR writes
    bit PPUSTATUS
    lda _scroll_x
    sta PPUSCROLL
    lda _scroll_y
    sta PPUSCROLL
    lda _ppu_ctrl
    sta PPUCTRL
    lda _ppu_mask
    sta PPUMASK

    ; keep this frame's scroll for the HUD split, which also runs on frames
    ; where C wasn't ready (and may be halfway through changing _scroll_x)
    lda _scroll_x
    sta split_x
    lda _ppu_ctrl
    sta split_ctrl

    lda #0
    sta _nmi_ready

@done:
    inc _frame_count

    ; HUD split (hud.c): the top of the screen shows nametable 0 unscrolled,
    ; then sprite 0 hits the HUD's last line and the rest uses the level's
    ; scroll. Only while the HUD is on and rendering is on, or the hit
    ; never comes.

    lda _hud_split
    beq @no_split
    lda _ppu_mask
    and #%00011000
    beq @no_split

    bit PPUSTATUS
    lda #0
    sta PPUSCROLL
    sta PPUSCROLL
    lda split_ctrl
    and #%11111100
    sta PPUCTRL

    ; wait for last frame's hit flag to clear (end of vblank), then for this
    ; frame's hit. Each wait gives up after ~11000 cycles in case it never
    ; comes (sprite 0 not where it should be).

    ldx #4
    ldy #0
@wait_clear:
    bit PPUSTATUS
    bvc @wait_hit_start
    dey
    bne @wait_clear
    dex
    bne @wait_clear
    jmp @no_split
@wait_hit_start:
    ldx #4
@wait_hit:
    bit PPUSTATUS
    bvc @split
    dey
    bne @wait_hit
    dex
    bne @wait_hit
    jmp @no_split
@split:
    ; mid-frame only the horizontal scroll (x and the nametable bit) can be
    ; changed with these two writes; the PPU picks them up at the end of
    ; this line. (The bit PPUSTATUS above reset the write latch.)
    lda split_ctrl
    sta PPUCTRL
    lda split_x
    sta PPUSCROLL
@no_split:
    pla
    tay
    pla
    tax
    pla
irq:
    rti

; ---------------------------------------------------------------------------
; OAM buffer (page $0200) and CHR-ROM
; ---------------------------------------------------------------------------
.segment "OAM"
    .res 256

.segment "CHARS"
    .incbin "chr/bg.chr"        ; CHR bank 0 (4 KB): background tiles
    .incbin "chr/sprites.chr"   ; CHR bank 1 (4 KB): sprite tiles
    ; the remaining 24 KB (banks 2-7) is left empty for later graphics