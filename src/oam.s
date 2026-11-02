; oam.s - sprite drawing into the OAM buffer at $0200, in assembly because
; it runs for every sprite piece every frame (the C version cost ~1100 CPU
; cycles per 8x16 piece, this one ~70).
;
; C prototypes are in nes.h:
;   void oam_clear(void);
;   void __fastcall__ oam_spr(u8 x, u8 y, u8 tile, u8 attr);
;   void __fastcall__ oam_meta_spr(s16 x, u8 y, const u8 *data, u8 attr);
;   extern u8 oam_top;
;
; cc65 calling convention (__fastcall__): the last argument is in A (and X
; for 16 bits); the others are on the C stack, the first one deepest, and
; the function removes them (incspN) before returning.

.include "zeropage.inc"

.ifdef CC65_C_SP           ; cc65 2.20+ calls the C stack pointer c_sp
    C_SP = c_sp
.else
    C_SP = sp
.endif

.import incsp3, incsp5
.import _oam_buf
.export _oam_clear, _oam_spr, _oam_meta_spr, _oam_top

MSPR_END    = 128
SPR_FLIP_H  = $40

.segment "ZEROPAGE"
oam_idx:   .res 1           ; next free byte in _oam_buf (4 per sprite)
oam_full:  .res 1           ; nonzero once all 64 sprites are used
m_attr:    .res 1           ; oam_meta_spr: attribute XOR
m_x:       .res 2           ; oam_meta_spr: anchor x (signed)
m_y:       .res 1           ; oam_meta_spr: anchor y
m_off:     .res 1           ; oam_meta_spr: offset into the data
m_sx:      .res 1           ; piece x on screen

.segment "BSS"
_oam_top:  .res 1           ; pieces starting above this line are skipped (HUD)

.segment "CODE"

; ---------------------------------------------------------------------------
; void oam_clear(void)
; Hide the sprites used last frame (Y = $FF is below the screen). Sprites
; past oam_idx are already hidden, so only the used part is cleared.
; ---------------------------------------------------------------------------
_oam_clear:
    lda #$FF
    ldx oam_full
    beq @partial
    ldx #0                  ; all 64 were used: clear them all
@all:
    sta _oam_buf,x
    inx
    inx
    inx
    inx
    bne @all
    beq @done
@partial:
    ldx oam_idx
    beq @done
@loop:
    dex
    dex
    dex
    dex
    sta _oam_buf,x
    bne @loop
@done:
    lda #0
    sta oam_idx
    sta oam_full
    rts

; ---------------------------------------------------------------------------
; void __fastcall__ oam_spr(u8 x, u8 y, u8 tile, u8 attr)
; Stack: (C_SP),0 = tile, 1 = y, 2 = x.
; ---------------------------------------------------------------------------
_oam_spr:
    ldx oam_full
    bne @skip
    ldx oam_idx
    sta _oam_buf+2,x        ; attr
    ldy #0
    lda (C_SP),y
    sta _oam_buf+1,x        ; tile
    iny
    lda (C_SP),y
    sec
    sbc #1                  ; sprites appear one line lower than their Y
    sta _oam_buf+0,x
    iny
    lda (C_SP),y
    sta _oam_buf+3,x        ; x
    txa
    clc
    adc #4
    sta oam_idx
    bne @skip
    inc oam_full            ; wrapped to 0: all 64 used
@skip:
    jmp incsp3

; ---------------------------------------------------------------------------
; void __fastcall__ oam_meta_spr(s16 x, u8 y, const u8 *data, u8 attr)
; Stack: (C_SP),0-1 = data, 2 = y, 3-4 = x.
; data: [dx, dy, tile, attr]... MSPR_END. dx and dy are signed. Each piece
; is an 8x16 sprite: its tile gets bit 0 set (sprite pattern table).
; Pieces that would be off the screen (x outside 0-255, y outside
; oam_top-239) are left out instead of wrapping around.
; ---------------------------------------------------------------------------
_oam_meta_spr:
    sta m_attr
    ldy #0
    lda (C_SP),y
    sta ptr1
    iny
    lda (C_SP),y
    sta ptr1+1
    iny
    lda (C_SP),y
    sta m_y
    iny
    lda (C_SP),y
    sta m_x
    iny
    lda (C_SP),y
    sta m_x+1
    jsr incsp5              ; arguments read: drop them now

    lda #0
    sta m_off
@piece:
    ldy m_off
    lda (ptr1),y            ; dx
    cmp #MSPR_END
    beq @end

    bit m_attr              ; mirrored? dx = -8 - dx
    bvc @no_flip
    eor #$FF
    sec
    sbc #7
@no_flip:
    ; sx = x + dx (16-bit, dx sign-extended): high byte must come out 0
    ldx #0
    cmp #$80
    bcc @dx_pos
    dex                     ; X = $FF: dx negative
@dx_pos:
    clc
    adc m_x
    sta m_sx
    txa
    adc m_x+1
    bne @next               ; sx < 0 or sx > 255

    ; sy = y + dy: dy >= 0 must not carry, dy < 0 must carry
    iny
    lda (ptr1),y            ; dy
    bmi @dy_neg
    clc
    adc m_y
    bcs @next
    bcc @dy_ok
@dy_neg:
    clc
    adc m_y
    bcc @next
@dy_ok:
    cmp _oam_top            ; inside oam_top..239?
    bcc @next
    cmp #240
    bcs @next

    ldx oam_full
    bne @end                ; no sprites left
    ldx oam_idx
    sec
    sbc #1                  ; sprites appear one line lower
    sta _oam_buf+0,x
    iny
    lda (ptr1),y            ; tile: bit 0 = sprite pattern table
    ora #1
    sta _oam_buf+1,x
    iny
    lda (ptr1),y            ; attr ^ m_attr
    eor m_attr
    sta _oam_buf+2,x
    lda m_sx
    sta _oam_buf+3,x
    txa
    clc
    adc #4
    sta oam_idx
    bne @next
    inc oam_full

@next:
    lda m_off
    clc
    adc #4
    sta m_off
    bne @piece              ; (at most 63 pieces)
@end:
    rts