; demo.s - the moving parts of samples/demo.c: two raster interrupts per frame set the
; background colour ($D021) line by line from a table (rainbow behind the logo and the
; scroller, raster bars), the scroller moves with $D016, and a 3-voice SID tune
; plays once per frame. C fills the screen, _scroll_buf and _scroll_len, then calls
; _demo_start once.
;
; Raster lines (text row r starts at $33 + 8*r):
;   IRQ A at $30: lines $33-$93, table entries 0-96
;      0-63   rows 0-7    logo: rainbow moving down
;     64-95   rows 8-11   three raster bars
;     96      row 12      panel: blue
;   IRQ B at $D1: lines $D3-$FB, table entries 97-137
;     97-120  rows 20-22  three more bars, the other way round
;    121-128  row 23      scroller: rainbow moving up; $D016 = 38 columns + fine scroll
;    129-137  row 24 + border: black
;   then (lower border) scroller, colour tables for the next frame, music.
;
; Each table entry is written as soon as its raster line begins ($D012 polled), at cycle
; 6-13 of the line: before the screen window starts (cycle 16) and before the character
; fetch of a bad line stops the CPU. The border stays black: polling jitters by 7 cycles,
; and a $D020 write that late shows up as a ragged left border.

        .export _demo_start, _scroll_buf, _scroll_len

SCRROW  = $0400 + 23*40         ; scroller row on the screen
LINES   = 138
ENTRY_B = 97
SCR_E   = ENTRY_B + 24          ; first table entry of the scroller row
ORDLEN  = 3

; ---------------------------------------------------------------------------------------
        .segment "BSS"
_scroll_buf:    .res 1024
_scroll_len:    .res 2
bk:     .res    LINES           ; $D021 per line
xs:     .res    LINES           ; $D016 per line (only used by IRQ B)
frame:  .res    1
fine:   .res    1               ; fine scroll 7..0
pos:    .res    2               ; next character of the text
bn:     .res    1
tmp:    .res    1
; music
tick:   .res    1
stp:    .res    1               ; 16th step in the 4-bar block, 0..63
bar:    .res    1
ordi:   .res    1
arpi:   .res    1
pw:     .res    2               ; pulse width of voice 1 (PWM)
pwdir:  .res    1
dptr:   .res    1               ; drum table position for voice 3, $FF = bass note
bassn:  .res    1               ; bass note (index into the frequency table)

; ---------------------------------------------------------------------------------------
        .segment "CODE"

_demo_start:
        sei
        lda     #$7F
        sta     $DC0D           ; no CIA interrupts (keyboard scan, cursor blink)
        sta     $DD0D
        lda     $DC0D
        lda     $DD0D
        ldx     #LINES-1
@tab:   lda     #0
        sta     bk,x
        lda     #$C8            ; 40 columns, no fine scroll
        sta     xs,x
        dex
        bpl     @tab
        lda     #7
        sta     fine
        lda     #0
        sta     pos
        sta     pos+1
        sta     $D020
        jsr     sid_init
        lda     #$1B            ; text mode, 25 rows, raster bit 8 = 0
        sta     $D011
        lda     #$30
        sta     $D012
        lda     #<irq_a
        sta     $0314
        lda     #>irq_a
        sta     $0315
        lda     #1
        sta     $D01A           ; raster interrupt on
        sta     $D019
        cli
        rts

; --- raster interrupts (entered through the KERNAL: $FF48 has saved A, X, Y) --------------
irq_a:  lda     #1
        sta     $D019
        ldx     #$32
        ldy     #0
@line:  lda     bk,y
        inx
@wait:  cpx     $D012
        bne     @wait
        sta     $D021
        iny
        cpy     #ENTRY_B
        bne     @line
        lda     #$D1
        sta     $D012
        lda     #<irq_b
        sta     $0314
        lda     #>irq_b
        sta     $0315
        jmp     $EA81           ; restore Y, X, A and RTI

irq_b:  lda     #1
        sta     $D019
        ldx     #$D2
        ldy     #ENTRY_B
@line:  lda     bk,y
        inx
@wait:  cpx     $D012
        bne     @wait
        sta     $D021
        lda     xs,y
        sta     $D016
        iny
        cpy     #LINES
        bne     @line
        inc     frame
        jsr     scroll
        jsr     colours
        jsr     music
        lda     #$30
        sta     $D012
        lda     #<irq_a
        sta     $0314
        lda     #>irq_a
        sta     $0315
        jmp     $EA81

; --- scroller: 1 pixel per frame, a new character every 8 --------------------------------
scroll: dec     fine
        bpl     @set
        lda     #7
        sta     fine
        ldx     #0
@shift: lda     SCRROW+1,x
        sta     SCRROW,x
        inx
        cpx     #39
        bne     @shift
        clc
        lda     #<_scroll_buf
        adc     pos
        sta     @get+1
        lda     #>_scroll_buf
        adc     pos+1
        sta     @get+2
@get:   lda     $FFFF           ; address patched above
        sta     SCRROW+39
        inc     pos
        bne     @cmp
        inc     pos+1
@cmp:   lda     pos
        cmp     _scroll_len
        bne     @set
        lda     pos+1
        cmp     _scroll_len+1
        bne     @set
        lda     #0
        sta     pos
        sta     pos+1
@set:   lda     fine
        ora     #$C0            ; 38 columns
        ldx     #SCR_E-1        ; from the last line before the row (blank there)
@xs:    sta     xs,x
        inx
        cpx     #SCR_E+8
        bne     @xs
        rts

; --- colour tables for the next frame ----------------------------------------------------
colours:
        ldx     #0              ; logo: rainbow moving down
@rb1:   txa
        sec
        sbc     frame
        and     #31
        tay
        lda     rainbow,y
        sta     bk,x
        inx
        cpx     #64
        bne     @rb1
        lda     #0              ; bar areas black
@clr1:  sta     bk,x
        inx
        cpx     #96
        bne     @clr1
        lda     #6              ; panel blue
        sta     bk+96
        lda     #0
        ldx     #ENTRY_B
@clr2:  sta     bk,x
        inx
        cpx     #SCR_E
        bne     @clr2
@rb2:   txa                     ; scroller: rainbow moving up
        clc
        adc     frame
        and     #31
        tay
        lda     rainbow,y
        sta     bk,x
        inx
        cpx     #SCR_E+8
        bne     @rb2
        lda     #0
@clr3:  sta     bk,x
        inx
        cpx     #LINES
        bne     @clr3

        lda     #0
        sta     bn
@b1:    ldy     bn              ; upper bars: entries 64..95
        lda     frame
        clc
        adc     phase,y
        and     #63
        tay
        lda     sin1,y
        clc
        adc     #64
        tax
        lda     bn
        jsr     drawbar
        inc     bn
        lda     bn
        cmp     #3
        bne     @b1
@b2:    ldy     bn              ; lower bars: entries 97..120, the other way round
        lda     phase-3,y
        sec
        sbc     frame
        and     #63
        tay
        lda     sin2,y
        clc
        adc     #ENTRY_B
        tax
        lda     bn
        jsr     drawbar
        inc     bn
        lda     bn
        cmp     #6
        bne     @b2
        rts

; one bar of 13 lines from entry X, colour ramp A (0..5)
drawbar:
        tay
        lda     rampofs,y
        tay
        lda     #13
        sta     tmp
@l:     lda     ramps,y
        sta     bk,x
        inx
        iny
        dec     tmp
        bne     @l
        rts

; --- music ------------------------------------------------------------------------------
; Am - F - C - G, 6 frames per 16th step (125 BPM), the Atari demo's tune for the SID.
; Voice 1 melody (pulse with PWM), voice 2 chord arpeggio (a new chord note every frame),
; voice 3 drums and bass: every 8th note a short drum sound (kick, hi-hat, snare, hi-hat)
; that turns into the bass note. Order: intro without melody, then twice with it.
sid_init:
        lda     #0
        ldx     #$18
@clr:   sta     $D400,x
        dex
        bpl     @clr
        lda     #$09            ; voice 1: attack 0, decay 9, sustain 10, release 9
        sta     $D405
        lda     #$A9
        sta     $D406
        lda     #$00            ; voice 2: sustain 7 - behind the melody
        sta     $D40C
        lda     #$70
        sta     $D40D
        lda     #$00            ; voice 2: pulse width $300
        sta     $D409
        lda     #$03
        sta     $D40A
        lda     #$41
        sta     $D40B
        lda     #$09            ; voice 3: a pluck, attack 0, decay 9, sustain 0
        sta     $D413
        lda     #$00
        sta     $D414
        sta     $D410
        lda     #$08
        sta     $D411
        sta     pw+1
        lda     #$0F            ; volume 15
        sta     $D418
        lda     #0
        sta     tick
        sta     stp
        sta     ordi
        sta     arpi
        sta     pwdir
        lda     #$FF
        sta     dptr
        rts

music:  dec     tick
        bpl     @nostep
        lda     #5
        sta     tick
        jsr     step
        jmp     @frame
@nostep:
        bne     @frame          ; tick 0: last frame before the next step
        jsr     prestep
@frame: ldx     arpi            ; voice 2: arpeggio
        inx
        cpx     #3
        bne     @arp
        ldx     #0
@arp:   stx     arpi
        lda     bar
        asl     a
        clc
        adc     bar
        adc     arpi
        tay
        ldx     chords,y
        lda     flo,x
        sta     $D407
        lda     fhi,x
        sta     $D408
        lda     pwdir           ; voice 1: pulse width $200..$E00 and back
        bne     @down
        clc
        lda     pw
        adc     #24
        sta     pw
        lda     pw+1
        adc     #0
        sta     pw+1
        cmp     #$0E
        bcc     @pwset
        inc     pwdir
        bne     @pwset
@down:  sec
        lda     pw
        sbc     #24
        sta     pw
        lda     pw+1
        sbc     #0
        sta     pw+1
        cmp     #$02
        bcs     @pwset
        lda     #0
        sta     pwdir
@pwset: lda     pw
        sta     $D402
        lda     pw+1
        sta     $D403
        ldy     dptr            ; voice 3: drum frames, then the bass note
        bmi     @done
        lda     drums,y
        beq     @bass
        sta     $D412
        lda     #0
        sta     $D40E
        lda     drums+1,y
        sta     $D40F
        iny
        iny
        sty     dptr
        rts
@bass:  ldx     bassn
        lda     flo,x
        sta     $D40E
        lda     fhi,x
        sta     $D40F
        lda     #$21            ; sawtooth, gate still on
        sta     $D412
        lda     #$FF
        sta     dptr
@done:  rts

; gates off one frame before a new note, so that the envelope starts again
prestep:
        lda     stp
        and     #1
        bne     @mel
        lda     #$20
        sta     $D412
@mel:   ldx     ordi
        lda     order,x
        beq     @r
        ldy     stp
        lda     melody,y
        beq     @r
        cmp     #$FF
        beq     @r
        lda     #$40
        sta     $D404
@r:     rts

step:   lda     stp
        lsr     a
        lsr     a
        lsr     a
        lsr     a
        sta     bar
        lda     stp
        and     #1
        bne     @mel
        ldy     bar             ; voice 3 on every 8th: drum + bass (root, octave)
        lda     bassroot,y
        tax
        lda     stp
        and     #2
        beq     @bn
        txa
        clc
        adc     #12
        tax
@bn:    stx     bassn
        lda     stp
        and     #7
        lsr     a               ; 0 kick, 1 hat, 2 snare, 3 hat
        tay
        lda     drumstart,y
        sta     dptr
@mel:   ldx     ordi
        lda     order,x
        beq     @next
        ldy     stp
        lda     melody,y
        beq     @next
        cmp     #$FF
        bne     @on
        lda     #$40            ; note off
        sta     $D404
        bne     @next
@on:    clc
        adc     #11             ; n+1 -> n, one octave above the table start (C2)
        tax
        lda     flo,x
        sta     $D400
        lda     fhi,x
        sta     $D401
        lda     #$41
        sta     $D404
@next:  inc     stp
        lda     stp
        cmp     #64
        bne     @r
        lda     #0
        sta     stp
        inc     ordi
        lda     ordi
        cmp     #ORDLEN
        bne     @r
        lda     #0
        sta     ordi
@r:     rts

; --- data -------------------------------------------------------------------------------
        .segment "RODATA"

order:  .byte   0,1,1           ; 1 = with melody
; notes: index into flo/fhi (0 = C2); melody as in the Atari demo: 0 nothing new,
; n+1 = note n above C3, $FF = off
bassroot:
        .byte   9,5,0,7         ; A2 F2 C2 G2
chords: .byte   33,36,40, 29,33,36, 36,40,43, 31,35,38
melody: .byte   29,0,0,0, 25,0,22,0, 25,0,27,0, 29,0,0,0        ; Am
        .byte   30,0,0,0, 29,0,25,0, 22,0,25,0, 30,0,29,0       ; F
        .byte   32,0,0,0, 29,0,25,0, 29,0,32,0, 34,0,32,0       ; C
        .byte   32,0,0,0, 27,0,24,0, 27,0,29,0, 27,0,$FF,0      ; G

; voice 3 drums: pairs of (control register, frequency high byte), 0 = on to the bass note
drums:
kick:   .byte   $81,$10, $41,$0C, $41,$09, $41,$07, $41,$05, $41,$04, 0
snare:  .byte   $81,$30, $41,$0E, $81,$28, $81,$24, $81,$20, $81,$1E, 0
hat:    .byte   $81,$E0, $81,$C0, 0
drumstart:
        .byte   kick-drums, hat-drums, snare-drums, hat-drums

phase:  .byte   0,10,20
rampofs:.byte   0,13,26,39,52,65
; colour ramps of the bars, 13 lines each: red, green, blue | orange, purple, cyan
ramps:  .byte   9,2,2,8,10,15,1,15,10,8,2,2,9
        .byte   11,12,5,5,13,13,1,13,13,5,5,12,11
        .byte   6,6,4,14,14,3,1,3,14,14,4,6,6
        .byte   9,9,2,8,8,7,1,7,8,8,2,9,9
        .byte   6,4,4,12,10,15,1,15,10,12,4,4,6
        .byte   6,14,14,3,3,13,1,13,3,3,14,14,6
; 9.5+9.5*sin, 64 steps (upper bar area: 32 lines - 13)
sin1:   .byte   10,10,11,12,13,14,15,16,16,17,17,18,18,19,19,19
        .byte   19,19,19,19,18,18,17,17,16,16,15,14,13,12,11,10
        .byte   10,9,8,7,6,5,4,3,3,2,2,1,1,0,0,0
        .byte   0,0,0,0,1,1,2,2,3,3,4,5,6,7,8,9
; 5.5+5.5*sin (lower bar area: 24 lines - 13)
sin2:   .byte   6,6,7,7,8,8,9,9,9,10,10,10,11,11,11,11
        .byte   11,11,11,11,11,10,10,10,9,9,9,8,8,7,7,6
        .byte   6,5,4,4,3,3,2,2,2,1,1,1,0,0,0,0
        .byte   0,0,0,0,0,1,1,1,2,2,2,3,3,4,4,5
; 4 hues, 8 lines each, bright in the middle (no black: the letters would vanish)
rainbow:
        .byte   6,4,14,3,1,3,14,4
        .byte   2,8,10,7,1,7,10,8
        .byte   11,5,13,3,1,3,13,5
        .byte   9,4,10,15,1,15,10,4
; generated: SID frequency registers for PAL (985248 Hz), f * 2^24 / 985248, C2..B6
flo:    .byte $5A,$9C,$E2,$2C,$7B,$CF,$27,$85,$E8,$51,$C0,$37
        .byte $B4,$38,$C4,$59,$F7,$9D,$4E,$0A,$D0,$A2,$81,$6D
        .byte $67,$70,$89,$B2,$ED,$3B,$9C,$13,$A0,$44,$02,$DA
        .byte $CE,$E0,$11,$64,$DA,$76,$39,$26,$40,$89,$04,$B4
        .byte $9C,$C0,$22,$C8,$B4,$EB,$72,$4C,$80,$12,$08,$68
fhi:    .byte $04,$04,$04,$05,$05,$05,$06,$06,$06,$07,$07,$08
        .byte $08,$09,$09,$0A,$0A,$0B,$0C,$0D,$0D,$0E,$0F,$10
        .byte $11,$12,$13,$14,$15,$17,$18,$1A,$1B,$1D,$1F,$20
        .byte $22,$24,$27,$29,$2B,$2E,$31,$34,$37,$3A,$3E,$41
        .byte $45,$49,$4E,$52,$57,$5C,$62,$68,$6E,$75,$7C,$83
