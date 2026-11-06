.include "zeropage.inc"

.ifdef CC65_C_SP           ; cc65 2.20+ calls the C stack pointer c_sp
    C_SP = c_sp
.else
    C_SP = sp
.endif

.import famistudio_init, famistudio_music_play, famistudio_music_pause
.import famistudio_music_stop, famistudio_update
.import famistudio_sfx_init, famistudio_sfx_play
.importzp FAMISTUDIO_SFX_CH0, FAMISTUDIO_SFX_CH1
.import audio_music, audio_sfx
.import _mmc1_set_prg_bank, incsp1
.importzp _prg_bank

.export _audio_init, _audio_update, _audio_silence
.export _music_play, _music_stop, _music_pause, _sfx_play

AUDIO_BANK = 2

.segment "ZEROPAGE"
audio_ready: .res 1         ; nonzero once audio_init has run
song_count:  .res 1
sfx_count:   .res 1

.segment "CODE"

; Switch to the audio bank, remembering the old one on the stack.
; Keeps X and Y; A is lost.
.macro BANK_IN
    lda _prg_bank
    pha
    lda #AUDIO_BANK
    jsr _mmc1_set_prg_bank
.endmacro

; Switch back to the remembered bank and return.
.macro BANK_OUT_RTS
    pla
    jmp _mmc1_set_prg_bank
.endmacro

; void audio_init(void): load the music and sound effects. Call once.
_audio_init:
    BANK_IN
    lda #1                  ; NTSC
    ldx #<audio_music
    ldy #>audio_music
    jsr famistudio_init
    ldx #<audio_sfx
    ldy #>audio_sfx
    jsr famistudio_sfx_init

    ; Count the songs and effects that were exported, so music_play and
    ; sfx_play can ignore numbers past the end. (Asking the engine for one
    ; that isn't there makes it play whatever bytes follow as music, which
    ; can hang or crash the game.)
    lda audio_music
    sta song_count
    lda audio_sfx
    sta ptr1
    lda audio_sfx+1
    sta ptr1+1
    ldy #0
    lda (ptr1),y
    sec
    sbc ptr1
    tax
    iny
    lda (ptr1),y
    sbc ptr1+1
    beq @count_ok
    ldx #$FE               ; doesn't look like a table: allow all
@count_ok:
    txa
    lsr a
    sta sfx_count
    lda #1
    sta audio_ready
    BANK_OUT_RTS

; void audio_update(void): advance music and effects by one frame.
; ppu_wait_nmi calls this, so every frame gets one update.
_audio_update:
    lda audio_ready
    beq @skip
    BANK_IN
    jsr famistudio_update
    BANK_OUT_RTS
@skip:
    rts

; void __fastcall__ music_play(u8 song)
_music_play:
    cmp song_count
    bcs @none
    sta tmp1
    BANK_IN
    lda tmp1
    jsr famistudio_music_play
    BANK_OUT_RTS
@none:
    rts

; void music_stop(void)
_music_stop:
    BANK_IN
    jsr famistudio_music_stop
    BANK_OUT_RTS

; void __fastcall__ music_pause(u8 pause): nonzero pauses, 0 resumes
_music_pause:
    sta tmp1
    BANK_IN
    lda tmp1
    jsr famistudio_music_pause
    BANK_OUT_RTS

; void audio_silence(void): stop the music and quiet the sound chip right
; away, before rendering is turned off for a while (loading a screen):
; nothing updates the sound then, so whatever was playing would hang.
_audio_silence:
    BANK_IN
    jsr famistudio_music_stop
    jsr famistudio_update
    BANK_OUT_RTS

; void __fastcall__ sfx_play(u8 sfx, u8 stream)
; Stack: (C_SP),0 = sfx. A = stream (0 or 1): effects on different streams
; can play at the same time; a new one on a busy stream replaces the old.
_sfx_play:
    tax
    ldy #0
    lda (C_SP),y
    cmp sfx_count
    bcs @skip
    sta tmp1                ; sfx number
    lda #FAMISTUDIO_SFX_CH0
    cpx #0
    beq @ch
    lda #FAMISTUDIO_SFX_CH1
@ch:
    sta tmp2                ; engine channel offset
    BANK_IN
    lda tmp1
    ldx tmp2
    jsr famistudio_sfx_play
    pla
    jsr _mmc1_set_prg_bank
    jmp incsp1
@skip:
    jmp incsp1