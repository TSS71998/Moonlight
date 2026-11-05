; audio_engine.s - configures and builds the FamiStudio sound engine
; (audio/famistudio_ca65.s, unchanged from FamiStudio).
;
; The engine is set up to play anything a FamiStudio song can use on the
; NES's own 5 channels, so songs and effects made in FamiStudio play without
; touching this file. Two exceptions:
;   - DPCM samples are off: they must sit in $C000-$FFFF, where the game's
;     code lives. Turn on FAMISTUDIO_CFG_DPCM_SUPPORT if you add some later.
;   - FamiStudio tempo mode only (the default for new FamiStudio projects).
; When FamiStudio exports music it lists any "Required flags" at the top of
; music.s: if one isn't set below, add it here.

FAMISTUDIO_CFG_EXTERNAL = 1

; where the engine's variables and code go
.define FAMISTUDIO_CA65_ZP_SEGMENT   ZEROPAGE
.define FAMISTUDIO_CA65_RAM_SEGMENT  AUDIORAM ; 205 bytes at the top of WRAM (cfg/mmc1.cfg)
.define FAMISTUDIO_CA65_CODE_SEGMENT BANK2   ; with the music (audio.s switches it in)

FAMISTUDIO_CFG_NTSC_SUPPORT      = 1
FAMISTUDIO_CFG_SFX_SUPPORT       = 1
FAMISTUDIO_CFG_SFX_STREAMS       = 2    ; effects that can play at once
FAMISTUDIO_CFG_SMOOTH_VIBRATO    = 1    ; no "pops" on square vibrato

; every FamiStudio effect a song can use
FAMISTUDIO_USE_RELEASE_NOTES     = 1
FAMISTUDIO_USE_VOLUME_TRACK      = 1
FAMISTUDIO_USE_VOLUME_SLIDES     = 1
FAMISTUDIO_USE_PITCH_TRACK       = 1
FAMISTUDIO_USE_SLIDE_NOTES       = 1
FAMISTUDIO_USE_NOISE_SLIDE_NOTES = 1
FAMISTUDIO_USE_VIBRATO           = 1
FAMISTUDIO_USE_ARPEGGIO          = 1
FAMISTUDIO_USE_DUTYCYCLE_EFFECT  = 1
FAMISTUDIO_USE_PHASE_RESET       = 1

.include "../audio/famistudio_ca65.s"