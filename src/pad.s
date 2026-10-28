; pad.s - controller reading
;
; Bit layout of the returned byte (matches PAD_* in nes.h):
;   7 A, 6 B, 5 Select, 4 Start, 3 Up, 2 Down, 1 Left, 0 Right

.export _pad_poll

JOY1 = $4016

.segment "ZEROPAGE"
pad_tmp: .res 1

.segment "CODE"

; unsigned char __fastcall__ pad_poll(unsigned char port);   port 0 or 1
_pad_poll:
    tax                     ; X = port (0 -> $4016, 1 -> $4017)
    lda #1
    sta JOY1                ; strobe on: latch buttons
    sta pad_tmp             ; ring counter: the 1 shifts out after 8 reads
    lsr a                   ; A = 0
    sta JOY1                ; strobe off
@loop:
    lda JOY1,x
    and #%00000011          ; standard pad (bit 0) or Famicom expansion (bit 1)
    cmp #1                  ; carry = 1 if pressed
    rol pad_tmp
    bcc @loop
    lda pad_tmp
    ldx #0                  ; cc65 returns 16 bits in A/X
    rts