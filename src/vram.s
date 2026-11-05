.include "zeropage.inc"

.ifdef CC65_C_SP           ; cc65 2.20+ calls the C stack pointer c_sp
    C_SP = c_sp
.else
    C_SP = sp
.endif

.import incsp2
.export _vram_write

PPUDATA = $2007

.segment "CODE"

; A/X = len, (C_SP),0-1 = src
_vram_write:
    sta tmp1                ; len low: bytes after the whole pages
    stx tmp2                ; len high: whole 256-byte pages
    ldy #0
    lda (C_SP),y
    sta ptr1
    iny
    lda (C_SP),y
    sta ptr1+1
    ldy #0
    ldx tmp2
    beq @rest
@page:
    lda (ptr1),y
    sta PPUDATA
    iny
    bne @page
    inc ptr1+1
    dex
    bne @page
@rest:
    ldx tmp1
    beq @done
@byte:
    lda (ptr1),y
    sta PPUDATA
    iny
    dex
    bne @byte
@done:
    jmp incsp2