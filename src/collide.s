; collide.s - level_solid_at, the collision lookup, in assembly: the player
; and every walking enemy call it several times a frame (~350 CPU cycles
; each in C, ~75 here).
;
; u8 __fastcall__ level_solid_at(u16 x, u8 y);   (level.h)
;   Nonzero if level pixel (x, y) is inside a solid 8x8 tile.
;   y >= 240 (below the level) is empty, so pits work; x past the level's
;   end is solid (x < 0 wraps around to a big number, so that is too).
;
; The tables it reads are filled by level_load in level.c:
;   screen_ptr[s]   start of screen s in the level map (16x15 metatiles)
;   mt_info[m]      metatile m: bits 4-7 = solid TL, TR, BL, BR 8x8 tiles

.include "zeropage.inc"

.ifdef CC65_C_SP           ; cc65 2.20+ calls the C stack pointer c_sp
    C_SP = c_sp
.else
    C_SP = sp
.endif

.import incsp2
.import _level_width, _screen_ptr, _mt_info
.export _level_solid_at

.segment "RODATA"
quarter_bit: .byte $10, $20, $40, $80   ; [y bit 3 * 2 + x bit 3]

.segment "CODE"

; A = y, (C_SP),0-1 = x
_level_solid_at:
    cmp #240
    bcs @empty
    sta tmp1                ; y
    ldy #0
    lda (C_SP),y
    sta tmp2                ; x low
    iny
    lda (C_SP),y
    sta tmp3                ; x high = screen number

    lda tmp2                ; x >= level_width: solid
    cmp _level_width
    lda tmp3
    sbc _level_width+1
    bcs @solid

    lda tmp3                ; ptr1 = screen_ptr[screen]
    asl a
    tax
    lda _screen_ptr,x
    sta ptr1
    lda _screen_ptr+1,x
    sta ptr1+1

    lda tmp2                ; metatile in the screen: (y & $F0) | (x low >> 4)
    lsr a
    lsr a
    lsr a
    lsr a
    sta tmp4
    lda tmp1
    and #$F0
    ora tmp4
    tay
    lda (ptr1),y
    tax                     ; X = metatile number

    lda tmp1                ; which quarter: (y bit 3) * 2 + (x bit 3)
    lsr a
    lsr a
    and #2
    sta tmp4
    lda tmp2
    lsr a
    lsr a
    lsr a
    and #1
    ora tmp4
    tay
    lda _mt_info,x
    and quarter_bit,y
    ldx #0
    jmp incsp2

@empty:
    lda #0
    tax
    jmp incsp2
@solid:
    lda #1
    ldx #0
    jmp incsp2