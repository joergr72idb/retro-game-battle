; demo_fx.s - the moving parts of samples/demo.c (sdasz80). C draws the screen and fills
; _fx_tbl with the colours that stay; this module does the rest, once per frame:
;
;   raster  (generated: build/demo_raster.s) - 201 scan lines of exactly 64 us. Each line
;           sets two Gate Array colours from _fx_tbl (4 bytes per line: pen, colour, pen,
;           colour) in the horizontal blank, and shifts a piece of the scroller.
;   work    (here) - the scroller's new column, the colours for the next frame (rainbow behind
;           the logo and the scroller, six raster bars) and a 3-voice AY tune.
;
; Timing: the Gate Array interrupts every 52 lines, in step with the VSYNC: 6 per frame, at
; scan lines 34, 86, 138, 190, 242 (VSYNC) and 294 of a 312-line frame (line 0 = first screen
; line). The handler counts them from the VSYNC one; the main loop HALTs until the one at 294,
; then calls raster - HALT makes the start exact to the microsecond.
;
; Mode 1. Pens: 0 background (black, bars), 1 text (rainbow / panel colours, changed per line),
; 2 panel blue, 3 cyan (the inverse panel line). Pen &10 = border.

        .module demo_fx
        .globl  _fx_init, _fx_start, _fx_font, _fx_tbl
        .globl  _scroll_text, _scroll_len
        .globl  raster, scr_src, scr_dst

SCRROW  = 0xC000 + 22*80        ; scroller: text row 22 (scan lines 176-183)
LOGO    = 8                     ; logo: scan lines 8-39
BARS1   = 40                    ; upper bars: 40-79
PANEL   = 80                    ; panel: 80-143
BARS2   = 144                   ; lower bars: 144-175 (175 stays black)
SCROLL  = 176
BLACK   = 0x54
ORDLEN  = 3

        .area   _DATA
_fx_font::      .ds     96*8    ; the firmware's font, characters 32-127
_fx_tbl::       .ds     201*4
scr_src:        .ds     2       ; the scroller copy in raster: from, to (equal = no shift)
scr_dst:        .ds     2
intc:           .ds     1       ; interrupts since the VSYNC one
frame:          .ds     1
shifted:        .ds     1       ; raster shifted the scroller this frame
half:           .ds     1       ; which half of the character comes in next (0 left, 1 right)
pos:            .ds     2
bn:             .ds     1
; music
tick:           .ds     1
stp:            .ds     1
bar:            .ds     1
ordi:           .ds     1
arpi:           .ds     1
melv:           .ds     1
bassn:          .ds     1
bassv:          .ds     1
dptr:           .ds     2       ; drum frame, 0 = none
psg:            .ds     11      ; AY registers 0-10, written every frame

        .area   _CODE

; --- before C draws: mode 1, screen at &C000 without offset, the font copied out of the ROM --
_fx_init::
        ld      a, #1
        call    0xBC0E                  ; SCR SET MODE (clears the screen)
        ld      bc, #0xBC0C             ; CRTC R12/R13: screen at &C000, offset 0
        out     (c), c
        ld      bc, #0xBD30
        out     (c), c
        ld      bc, #0xBC0D
        out     (c), c
        ld      bc, #0xBD00
        out     (c), c
        call    0xB906                  ; KL L ROM ENABLE: the font is in the lower ROM
        push    af
        ld      de, #_fx_font
        ld      c, #32
1$:     push    bc
        push    de
        ld      a, c
        call    0xBBA5                  ; TXT GET MATRIX -> HL
        pop     de
        ld      bc, #8
        ldir
        pop     bc
        inc     c
        bit     7, c
        jr      z, 1$
        pop     af
        jp      0xB90C                  ; KL ROM RESTORE

; --- take over the machine; never returns -------------------------------------------------
_fx_start::
        di
        ld      bc, #0x7F8D             ; mode 1, both ROMs off
        out     (c), c
        ld      a, #0xC3                ; JP irq at &0038 (RAM now)
        ld      (0x0038), a
        ld      hl, #irq
        ld      (0x0039), hl
        im      1
        ld      bc, #0x7F02             ; pen 2: blue, pen 3: bright cyan
        out     (c), c
        ld      a, #0x44
        out     (c), a
        ld      c, #3
        out     (c), c
        ld      a, #0x53
        out     (c), a
        ld      hl, #SCRROW
        ld      (scr_src), hl
        ld      (scr_dst), hl
        xor     a
        ld      (frame), a
        ld      (shifted), a
        ld      (half), a
        ld      (tick), a
        ld      (stp), a
        ld      (ordi), a
        ld      (arpi), a
        ld      (melv), a
        ld      (bassv), a
        ld      (pos), a
        ld      (pos+1), a
        ld      (dptr), a
        ld      (dptr+1), a
        ld      hl, #psg                ; AY: all quiet, tones on, noise off
        ld      b, #11
2$:     ld      (hl), a
        inc     hl
        djnz    2$
        ld      a, #0x38
        ld      (psg+7), a
        ld      a, #9
        ld      (intc), a
        ei
loop:   halt
        ld      a, (intc)
        cp      #1
        jr      nz, loop
        call    raster                  ; scan lines -1 to 199
        call    column
        call    colours
        call    music
        jr      loop

irq:    push    af
        push    bc
        ld      b, #0xF5                ; PPI port B, bit 0 = VSYNC
        in      a, (c)
        rra
        jr      c, 1$
        ld      a, (intc)
        inc     a
        ld      (intc), a
        pop     bc
        pop     af
        ei
        ret
1$:     xor     a
        ld      (intc), a
        pop     bc
        pop     af
        ei
        ret

; --- scroller: 4 pixels (one byte) every second frame -----------------------------------
column: ld      hl, #frame
        inc     (hl)
        ld      a, (shifted)
        or      a
        jr      z, 5$
        ; raster has moved the row one byte to the left: draw the new byte at the right
        ld      hl, (pos)
        ld      de, #_scroll_text
        add     hl, de
        ld      a, (hl)
        sub     #32
        jr      nc, 1$
        xor     a
1$:     ld      l, a                    ; font + (char - 32) * 8
        ld      h, #0
        add     hl, hl
        add     hl, hl
        add     hl, hl
        ld      de, #_fx_font
        add     hl, de
        ex      de, hl                  ; DE = glyph
        ld      hl, #SCRROW+79
        ld      bc, #0x0800
        ld      a, (half)
        or      a
        jr      nz, 3$
2$:     ld      a, (de)                 ; left half: pixels 0-3 = bits 7-4 (pen 1)
        and     #0xF0
        ld      (hl), a
        inc     de
        add     hl, bc
        jr      nc, 2$                  ; 8 scan lines: &C6E0+79 ... until past &FFFF
        jr      4$
3$:     ld      a, (de)                 ; right half: bits 3-0 to the top
        add     a, a
        add     a, a
        add     a, a
        add     a, a
        ld      (hl), a
        inc     de
        add     hl, bc
        jr      nc, 3$
        ld      hl, (pos)               ; next character
        inc     hl
        ld      de, (_scroll_len)
        or      a
        sbc     hl, de
        add     hl, de
        jr      nz, 6$
        ld      hl, #0
6$:     ld      (pos), hl
4$:     ld      a, (half)
        xor     #1
        ld      (half), a
5$:     ld      hl, #SCRROW             ; the next frame shifts when this one did not
        ld      (scr_dst), hl
        ld      a, (shifted)
        xor     #1
        ld      (shifted), a
        jr      z, 7$
        inc     hl
7$:     ld      (scr_src), hl
        ret

; --- colours for the next frame -------------------------------------------------------------
colours:
        ld      hl, #_fx_tbl + LOGO*4 + 3       ; logo: rainbow moving down
        ld      a, (frame)
        neg
        ld      c, a
        ld      b, #32
1$:     ld      a, c
        and     #31
        ld      e, a
        ld      d, #0
        push    hl
        ld      hl, #rainbow
        add     hl, de
        ld      a, (hl)
        pop     hl
        ld      (hl), a
        inc     hl
        inc     hl
        inc     hl
        inc     hl
        inc     c
        djnz    1$
        ld      hl, #_fx_tbl + SCROLL*4 + 3     ; scroller: rainbow moving up
        ld      a, (frame)
        ld      c, a
        ld      b, #8
2$:     ld      a, c
        and     #31
        ld      e, a
        ld      d, #0
        push    hl
        ld      hl, #rainbow
        add     hl, de
        ld      a, (hl)
        pop     hl
        ld      (hl), a
        inc     hl
        inc     hl
        inc     hl
        inc     hl
        inc     c
        djnz    2$
        ld      hl, #_fx_tbl + BARS1*4 + 1      ; bar areas black
        ld      b, #40
        call    clear
        ld      hl, #_fx_tbl + BARS2*4 + 1
        ld      b, #32
        call    clear
        xor     a                               ; upper bars
        ld      (bn), a
3$:     ld      a, (bn)
        ld      e, a
        ld      d, #0
        ld      hl, #phase
        add     hl, de
        ld      a, (frame)
        add     a, (hl)
        and     #63
        ld      e, a
        ld      hl, #sin1
        add     hl, de
        ld      a, (hl)
        add     a, #BARS1
        call    drawbar
        ld      hl, #bn
        inc     (hl)
        ld      a, (hl)
        cp      #3
        jr      nz, 3$
4$:     ld      a, (bn)                         ; lower bars, the other way round
        sub     #3
        ld      e, a
        ld      d, #0
        ld      hl, #phase
        add     hl, de
        ld      a, (frame)
        ld      c, a
        ld      a, (hl)
        sub     c
        and     #63
        ld      e, a
        ld      hl, #sin2
        add     hl, de
        ld      a, (hl)
        add     a, #BARS2
        call    drawbar
        ld      hl, #bn
        inc     (hl)
        ld      a, (hl)
        cp      #6
        jr      nz, 4$
        ld      a, #BLACK                       ; the line above the scroller stays black
        ld      (_fx_tbl + (SCROLL-1)*4 + 1), a
        ld      (_fx_tbl + (SCROLL-1)*4 + 3), a
        ret

; B lines from HL (border colour byte): border and pen 0 black
clear:  ld      a, #BLACK
        ld      de, #2
1$:     ld      (hl), a                 ; border
        inc     hl
        inc     hl
        ld      (hl), a                 ; pen 0
        add     hl, de                  ; on to the next line's border colour
        djnz    1$
        ret

; one bar of 13 lines from scan line A, colour ramp bn (0-5)
drawbar:
        ld      l, a
        ld      h, #0
        add     hl, hl
        add     hl, hl
        ld      de, #_fx_tbl + 1
        add     hl, de
        push    hl
        ld      a, (bn)                         ; ramp = ramps + bn * 13
        ld      e, a
        add     a, a
        add     a, e
        add     a, a
        add     a, a
        add     a, e
        ld      e, a
        ld      d, #0
        ld      hl, #ramps
        add     hl, de
        ex      de, hl
        pop     hl
        ld      b, #13
1$:     ld      a, (de)
        ld      (hl), a
        inc     hl
        inc     hl
        ld      (hl), a
        inc     hl
        inc     hl
        inc     de
        djnz    1$
        ret

; --- music ------------------------------------------------------------------------------
; The tune of the Atari and C64 demos: Am - F - C - G, 6 frames per 16th step (125 BPM).
; Channel A melody, B chord arpeggio (a new chord note every frame), C drums and bass: every
; 8th note a short drum sound (kick, hi-hat, snare, hi-hat) that turns into the bass note.
; Order: intro without melody, then twice with it.
music:  ld      hl, #tick
        dec     (hl)
        jp      p, 1$
        ld      (hl), #5
        call    step
1$:     ld      a, (arpi)               ; B: arpeggio
        inc     a
        cp      #3
        jr      c, 2$
        xor     a
2$:     ld      (arpi), a
        ld      e, a
        ld      a, (bar)
        ld      c, a
        add     a, a
        add     a, c
        add     a, e
        ld      e, a
        ld      d, #0
        ld      hl, #chords
        add     hl, de
        ld      a, (hl)
        call    period
        ld      (psg+2), hl
        ld      a, #10
        ld      (psg+9), a
        ld      a, (frame)              ; A: melody, slowly fading to a quiet sustain
        and     #3
        jr      nz, 3$
        ld      a, (melv)
        cp      #8
        jr      c, 3$
        dec     a
        ld      (melv), a
3$:     ld      a, (melv)
        ld      (psg+8), a
        ld      hl, (dptr)              ; C: a drum frame, or the bass
        ld      a, h
        or      l
        jr      z, 5$
        ld      a, (hl)
        or      a
        jr      z, 4$
        ld      (psg+7), a              ; mixer
        inc     hl
        ld      a, (hl)
        ld      (psg+6), a              ; noise period
        inc     hl
        ld      e, (hl)
        inc     hl
        ld      d, (hl)
        ld      (psg+4), de             ; tone period
        inc     hl
        ld      a, (hl)
        ld      (psg+10), a
        inc     hl
        ld      (dptr), hl
        jr      write
4$:     ld      hl, #0
        ld      (dptr), hl
5$:     ld      a, #0x38                ; tones on, noise off
        ld      (psg+7), a
        ld      a, (bassn)
        call    period
        ld      (psg+4), hl
        ld      a, (bassv)
        ld      (psg+10), a
        cp      #5
        jr      c, write
        dec     a
        ld      (bassv), a
write:  ld      hl, #psg                ; registers 0-10 to the AY
        ld      e, #0
1$:     ld      a, (hl)
        call    ay
        inc     hl
        inc     e
        ld      a, e
        cp      #11
        jr      nz, 1$
        ret

; AY register E = A, through the PPI
ay:     ld      b, #0xF4
        out     (c), e
        ld      bc, #0xF6C0             ; select the register
        out     (c), c
        ld      c, #0
        out     (c), c
        ld      b, #0xF4
        out     (c), a
        ld      bc, #0xF680             ; write
        out     (c), c
        ld      c, #0
        out     (c), c
        ret

; HL = tone period of note A (0 = C2)
period: ld      l, a
        ld      h, #0
        add     hl, hl
        ld      de, #notes
        add     hl, de
        ld      a, (hl)
        inc     hl
        ld      h, (hl)
        ld      l, a
        ret

step:   ld      a, (stp)
        rrca
        rrca
        rrca
        rrca
        and     #3
        ld      (bar), a
        ld      a, (stp)
        and     #1
        jr      nz, 2$
        ld      a, (bar)                ; C on every 8th: drum + bass (root, octave)
        ld      e, a
        ld      d, #0
        ld      hl, #bassroot
        add     hl, de
        ld      c, (hl)
        ld      a, (stp)
        and     #2
        ld      a, c
        jr      z, 1$
        add     a, #12
1$:     ld      (bassn), a
        ld      a, #14
        ld      (bassv), a
        ld      a, (stp)                ; 0 kick, 2 hat, 4 snare, 6 hat
        and     #6
        ld      e, a
        ld      d, #0
        ld      hl, #drumstart
        add     hl, de
        ld      e, (hl)
        inc     hl
        ld      d, (hl)
        ld      (dptr), de
2$:     ld      a, (ordi)               ; A: melody
        ld      e, a
        ld      d, #0
        ld      hl, #order
        add     hl, de
        ld      a, (hl)
        or      a
        jr      z, 4$
        ld      a, (stp)
        ld      e, a
        ld      hl, #melody
        add     hl, de
        ld      a, (hl)
        or      a
        jr      z, 4$
        cp      #0xFF
        jr      nz, 3$
        xor     a                       ; note off
        ld      (melv), a
        jr      4$
3$:     add     a, #11                  ; n+1 -> n, one octave above the table start (C2)
        call    period
        ld      (psg+0), hl
        ld      a, #13
        ld      (melv), a
4$:     ld      hl, #stp
        inc     (hl)
        ld      a, (hl)
        cp      #64
        ret     nz
        ld      (hl), #0
        ld      hl, #ordi
        inc     (hl)
        ld      a, (hl)
        cp      #ORDLEN
        ret     nz
        ld      (hl), #0
        ret

; --- data -------------------------------------------------------------------------------
order:  .db     0, 1, 1                 ; 1 = with melody
; notes: index into notes (0 = C2); melody as in the Atari demo: 0 nothing new,
; n+1 = note n above C3, &FF = off
bassroot:
        .db     9, 5, 0, 7              ; A2 F2 C2 G2
chords: .db     33,36,40, 29,33,36, 36,40,43, 31,35,38
melody: .db     29,0,0,0, 25,0,22,0, 25,0,27,0, 29,0,0,0        ; Am
        .db     30,0,0,0, 29,0,25,0, 22,0,25,0, 30,0,29,0       ; F
        .db     32,0,0,0, 29,0,25,0, 29,0,32,0, 34,0,32,0       ; C
        .db     32,0,0,0, 27,0,24,0, 27,0,29,0, 27,0,0xFF,0     ; G

; drums on channel C, one frame per line: mixer, noise period, tone period, volume; 0 = end
drumstart:
        .dw     kick, hat, snare, hat
kick:   .db     0x38, 0
        .dw     0x0180
        .db     15
        .db     0x38, 0
        .dw     0x0240
        .db     14
        .db     0x38, 0
        .dw     0x0300
        .db     13
        .db     0x38, 0
        .dw     0x0400
        .db     11
        .db     0x38, 0
        .dw     0x0500
        .db     9, 0
snare:  .db     0x1C, 8, 0, 0, 15
        .db     0x1C, 10, 0, 0, 13
        .db     0x1C, 12, 0, 0, 11
        .db     0x1C, 14, 0, 0, 9
        .db     0x1C, 16, 0, 0, 7, 0
hat:    .db     0x1C, 1, 0, 0, 11
        .db     0x1C, 1, 0, 0, 7, 0

; generated: AY tone periods, 1 MHz / (16 f), C2..B6
notes:  .dw     956, 902, 851, 804, 758, 716, 676, 638, 602, 568, 536, 506
        .dw     478, 451, 426, 402, 379, 358, 338, 319, 301, 284, 268, 253
        .dw     239, 225, 213, 201, 190, 179, 169, 159, 150, 142, 134, 127
        .dw     119, 113, 106, 100, 95, 89, 84, 80, 75, 71, 67, 63
        .dw     60, 56, 53, 50, 47, 45, 42, 40, 38, 36, 34, 32

phase:  .db     0, 10, 20
; Gate Array colour commands. Bars, 13 lines each: red, green, blue | orange, purple, cyan
ramps:  .db     0x5C, 0x5C, 0x4C, 0x4C, 0x4E, 0x4A, 0x4B, 0x4A, 0x4E, 0x4C, 0x4C, 0x5C, 0x5C
        .db     0x56, 0x56, 0x52, 0x52, 0x5A, 0x59, 0x4B, 0x59, 0x5A, 0x52, 0x52, 0x56, 0x56
        .db     0x44, 0x44, 0x55, 0x55, 0x57, 0x5F, 0x4B, 0x5F, 0x57, 0x55, 0x55, 0x44, 0x44
        .db     0x5C, 0x4E, 0x4E, 0x4A, 0x4A, 0x43, 0x4B, 0x43, 0x4A, 0x4A, 0x4E, 0x4E, 0x5C
        .db     0x58, 0x58, 0x4D, 0x4D, 0x4F, 0x4F, 0x4B, 0x4F, 0x4F, 0x4D, 0x4D, 0x58, 0x58
        .db     0x46, 0x46, 0x53, 0x53, 0x5B, 0x5B, 0x4B, 0x5B, 0x5B, 0x53, 0x53, 0x46, 0x46
; 4 hues, 8 lines each, white in the middle - only bright colours: dark ones vanish on black
rainbow:
        .db     0x55, 0x57, 0x5F, 0x4B, 0x5F, 0x57, 0x55, 0x57
        .db     0x4C, 0x4E, 0x4A, 0x43, 0x4B, 0x43, 0x4A, 0x4E
        .db     0x52, 0x5A, 0x59, 0x4B, 0x59, 0x5A, 0x52, 0x42
        .db     0x4D, 0x4F, 0x47, 0x4B, 0x47, 0x4F, 0x4D, 0x5D
; 13.5+13.5*sin, 64 steps (upper bar area: 40 lines - 13)
sin1:   .db     0x0E,0x0F,0x10,0x11,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x19,0x1A,0x1A,0x1B,0x1B
        .db     0x1B,0x1B,0x1B,0x1A,0x1A,0x19,0x19,0x18,0x17,0x16,0x15,0x14,0x13,0x11,0x10,0x0F
        .db     0x0E,0x0C,0x0B,0x0A,0x08,0x07,0x06,0x05,0x04,0x03,0x02,0x02,0x01,0x01,0x00,0x00
        .db     0x00,0x00,0x00,0x01,0x01,0x02,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x0A,0x0B,0x0C
; 9.5+9.5*sin (lower bar area: 32 lines - 13)
sin2:   .db     0x0A,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,0x10,0x10,0x11,0x11,0x12,0x12,0x13,0x13,0x13
        .db     0x13,0x13,0x13,0x13,0x12,0x12,0x11,0x11,0x10,0x10,0x0F,0x0E,0x0D,0x0C,0x0B,0x0A
        .db     0x0A,0x09,0x08,0x07,0x06,0x05,0x04,0x03,0x03,0x02,0x02,0x01,0x01,0x00,0x00,0x00
        .db     0x00,0x00,0x00,0x00,0x01,0x01,0x02,0x02,0x03,0x03,0x04,0x05,0x06,0x07,0x08,0x09
