; demo.s - the moving parts of samples/demo.c: display list, raster bars by display list
; interrupts (DLI), fine scroller, rainbow logo and a 4-voice POKEY tune, all driven from
; the deferred vertical blank interrupt (VBI). C fills _logo, _panel, _scroll_buf and
; _scroll_len, then calls _demo_start once.
;
; Screen (PAL lines after the 24 blank ones at the top):
;   32  logo, 2 lines ANTIC mode 7, colour 0 changes every line (rainbow)   \ DLI A
;   40  blank lines with three moving raster bars (COLBK)                   /
;   64  info panel, 8 lines ANTIC mode 2 (40 columns, OS font)
;   32  blank lines with three more bars                                    \ DLI B
;   16  scroller, ANTIC mode 7 with HSCROL, rainbow                         /
;    8  blank
; The tables bk/pf (one colour per line) are drawn by the VBI and written to the
; hardware by the DLIs, one line after the other (sta WSYNC).

        .export _demo_start, _logo, _panel, _scroll_buf, _scroll_len

RTCLOK  = $12           ; RTCLOK+2 counts frames
ATRACT  = $4D
VDSLST  = $0200
SDMCTL  = $022F
SDLSTL  = $0230
COLOR0  = $02C4         ; OS shadows of COLPF0-3, COLBK (copied at every VBI)
COLPF0  = $D016
COLBK   = $D01A
AUDF1   = $D200
AUDC1   = $D201
AUDF2   = $D202
AUDC2   = $D203
AUDF3   = $D204
AUDC3   = $D205
AUDF4   = $D206
AUDC4   = $D207
AUDCTL  = $D208
SKCTL   = $D20F
HSCROL  = $D404
WSYNC   = $D40A
NMIEN   = $D40E
SETVBV  = $E45C
XITVBV  = $E462

LINES_A = 72            ; logo + upper bars
LINES_B = 48            ; lower bars + scroller
ORDLEN  = 3

; ---------------------------------------------------------------------------------------
        .segment "DATA"
        .align  256     ; a display list must not cross a 1 KB boundary
dlist:  .byte   $70,$70,$F0             ; 24 blank lines, DLI A on the last one
        .byte   $47                     ; mode 7 + LMS
        .word   _logo
        .byte   $07
        .byte   $70,$70,$70,$70,$70     ; 40 lines for the bars
        .byte   $42                     ; mode 2 + LMS
        .word   _panel
        .byte   $02,$02,$02,$02,$02,$02,$82     ; last panel line: DLI B
        .byte   $70,$70,$70,$70
        .byte   $57                     ; mode 7 + LMS + HSCROL
dl_scr: .word   _scroll_buf             ; the VBI moves this along the text
        .byte   $70
        .byte   $41                     ; jump and wait for vertical blank
        .word   dlist

; ---------------------------------------------------------------------------------------
        .segment "BSS"
        .align  1024    ; screen data must not cross a 4 KB boundary within a line
_scroll_buf:    .res 1024
_panel:         .res 320
_logo:          .res 40
_scroll_len:    .res 2  ; where the scroller starts over (set by C)
bk:     .res    LINES_A+LINES_B         ; COLBK per line
pf:     .res    LINES_A+LINES_B         ; COLPF0 per line
frame:  .res    1
hs:     .res    1       ; HSCROL value (colour clocks, 15..0)
pos:    .res    2       ; first character of the scroller
tmp:    .res    1
bn:     .res    1
; music
tick:   .res    1
stp:    .res    1       ; 16th step in the 4-bar block, 0..63
bar:    .res    1
ordi:   .res    1
arpi:   .res    1
melf:   .res    1
melv:   .res    1
bassf:  .res    1
bassv:  .res    1
dtype:  .res    1       ; 0 none, 1 kick, 2 snare, 3 hi-hat
dfreq:  .res    1
dvol:   .res    1

; ---------------------------------------------------------------------------------------
        .segment "CODE"

_demo_start:
        lda     #0
        sta     AUDCTL          ; 64 kHz clock, no joined channels
        lda     #3
        sta     SKCTL
        ldx     #4
@col:   lda     colors,x
        sta     COLOR0,x
        dex
        bpl     @col
        lda     RTCLOK+2        ; switch right after a vertical blank
@wait:  cmp     RTCLOK+2
        beq     @wait
        lda     #<dli_a
        sta     VDSLST
        lda     #>dli_a
        sta     VDSLST+1
        lda     #<dlist
        sta     SDLSTL
        lda     #>dlist
        sta     SDLSTL+1
        lda     #$22            ; normal playfield, display list DMA
        sta     SDMCTL
        ldy     #<vbi
        ldx     #>vbi
        lda     #7              ; deferred VBI
        jsr     SETVBV
        lda     #$C0            ; DLI + VBI
        sta     NMIEN
        rts

; PF0 (logo/scroller, overwritten by the DLIs), PF1 (panel text brightness, scroller
; accent '~'), PF2 (panel background), PF3 (scroller accent '^'), COLBK
colors: .byte   $0E,$1E,$92,$AE,$00

; --- display list interrupts ------------------------------------------------------------
dli_a:  pha
        txa
        pha
        tya
        pha
        ldx     #0
@line:  ldy     pf,x
        lda     bk,x
        sta     WSYNC
        sta     COLBK
        sty     COLPF0
        inx
        cpx     #LINES_A
        bne     @line
        lda     #0
        sta     WSYNC
        sta     COLBK           ; black border around the panel
        lda     #<dli_b
        sta     VDSLST
        lda     #>dli_b
        sta     VDSLST+1
        pla
        tay
        pla
        tax
        pla
        rti

dli_b:  pha
        txa
        pha
        tya
        pha
        ldx     #LINES_A
@line:  ldy     pf,x
        lda     bk,x
        sta     WSYNC
        sta     COLBK
        sty     COLPF0
        inx
        cpx     #LINES_A+LINES_B
        bne     @line
        lda     #0
        sta     WSYNC
        sta     COLBK
        pla
        tay
        pla
        tax
        pla
        rti

; --- vertical blank ---------------------------------------------------------------------
vbi:    lda     #<dli_a
        sta     VDSLST
        lda     #>dli_a
        sta     VDSLST+1
        lda     #0
        sta     ATRACT          ; no attract mode (colour cycling after 9 minutes)
        inc     frame
        jsr     scroll
        jsr     bars
        jsr     music
        jmp     XITVBV

; scroller: 2 colour clocks per frame to the left; a mode 7 character is 16 wide
scroll: lda     hs
        sec
        sbc     #2
        bpl     @set
        and     #15             ; -2 -> 14
        sta     hs
        inc     pos
        bne     @cmp
        inc     pos+1
@cmp:   lda     pos
        cmp     _scroll_len
        bne     @lms
        lda     pos+1
        cmp     _scroll_len+1
        bne     @lms
        lda     #0
        sta     pos
        sta     pos+1
@lms:   clc
        lda     #<_scroll_buf
        adc     pos
        sta     dl_scr
        lda     #>_scroll_buf
        adc     pos+1
        sta     dl_scr+1
        lda     hs
@set:   sta     hs
        sta     HSCROL
        rts

; colour tables for the next frame
bars:   lda     #0
        ldx     #LINES_A+LINES_B-1
@clr:   sta     bk,x
        sta     pf,x
        dex
        bpl     @clr
        ldx     #0              ; logo: rainbow moving down
@rb1:   txa
        sec
        sbc     frame
        and     #127
        tay
        lda     rainbow,y
        sta     pf,x
        inx
        cpx     #32
        bne     @rb1
        ldx     #LINES_A+32     ; scroller: rainbow moving up
@rb2:   txa
        clc
        adc     frame
        and     #127
        tay
        lda     rainbow,y
        sta     pf,x
        inx
        cpx     #LINES_A+LINES_B
        bne     @rb2
        lda     #0
        sta     bn
@b1:    ldy     bn              ; upper bars: lines 32..71
        lda     frame
        clc
        adc     phase,y
        and     #63
        tay
        lda     sin1,y
        clc
        adc     #32
        tax
        ldy     bn
        lda     hue1,y
        jsr     drawbar
        inc     bn
        lda     bn
        cmp     #3
        bne     @b1
        lda     #0
        sta     bn
@b2:    ldy     bn              ; lower bars: lines 72..103, the other way round
        lda     phase,y
        sec
        sbc     frame
        and     #63
        tay
        lda     sin2,y
        clc
        adc     #LINES_A
        tax
        ldy     bn
        lda     hue2,y
        jsr     drawbar
        inc     bn
        lda     bn
        cmp     #3
        bne     @b2
        rts

; one bar: 13 lines from bk+X, hue in A
drawbar:
        sta     tmp
        ldy     #0
@l:     lda     grad,y
        ora     tmp
        sta     bk,x
        inx
        iny
        cpy     #13
        bne     @l
        rts

; --- music ------------------------------------------------------------------------------
; Am - F - C - G, 6 frames per 16th step (125 BPM). Voice 1 melody, 2 chord arpeggio
; (a new chord note every frame), 3 bass, 4 drums. Order: intro without melody, then twice
; with it, then from the top.
music:  dec     tick
        bpl     @frame
        lda     #5
        sta     tick
        jsr     step
@frame: ldx     arpi            ; arpeggio
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
        lda     chords,y
        tay
        lda     freq,y
        sta     AUDF2
        lda     #$A4
        sta     AUDC2
        lda     melf            ; melody, slowly fading to a quiet sustain
        sta     AUDF1
        lda     melv
        ora     #$A0
        sta     AUDC1
        lda     frame
        and     #3
        bne     @bass
        lda     melv
        cmp     #5
        bcc     @bass
        dec     melv
@bass:  lda     bassf           ; bass, plucked
        sta     AUDF3
        lda     bassv
        ora     #$A0
        sta     AUDC3
        lda     bassv
        beq     @drum
        dec     bassv
@drum:  lda     dtype
        bne     @d1
        sta     AUDC4
        rts
@d1:    cmp     #1
        bne     @d2
        lda     dfreq           ; kick: a tone falling fast
        sta     AUDF4
        lda     dvol
        ora     #$A0
        sta     AUDC4
        lda     dfreq
        clc
        adc     #32
        bcc     @kf
        lda     #$FF
@kf:    sta     dfreq
        lda     #2
        bne     @fade
@d2:    cmp     #2
        bne     @d3
        lda     #6              ; snare: noise
        sta     AUDF4
        lda     dvol
        ora     #$80
        sta     AUDC4
        lda     #1
        bne     @fade
@d3:    lda     #0              ; hi-hat: high noise, short
        sta     AUDF4
        lda     dvol
        ora     #$80
        sta     AUDC4
        lda     #2
@fade:  sta     tmp
        lda     dvol
        sec
        sbc     tmp
        bcs     @fv
        lda     #0
@fv:    sta     dvol
        bne     @done
        sta     dtype
@done:  rts

step:   lda     stp
        lsr     a
        lsr     a
        lsr     a
        lsr     a
        sta     bar
        lda     stp
        and     #1
        bne     @drums
        ldy     bar             ; bass on every 8th note, root and octave
        lda     bassroot,y
        tay
        lda     stp
        and     #2
        beq     @bf
        tya
        clc
        adc     #12
        tay
@bf:    lda     freq,y
        sta     bassf
        lda     #10
        sta     bassv
@drums: lda     stp
        and     #7
        bne     @sn
        lda     #1              ; kick on 1 and 3
        sta     dtype
        lda     #24
        sta     dfreq
        lda     #14
        sta     dvol
        bne     @mel
@sn:    cmp     #4
        bne     @hat
        lda     #2              ; snare on 2 and 4
        sta     dtype
        lda     #12
        sta     dvol
        bne     @mel
@hat:   and     #1
        bne     @mel
        lda     #3              ; hi-hat on the off-beats
        sta     dtype
        lda     #6
        sta     dvol
@mel:   ldx     ordi
        lda     order,x
        beq     @next
        ldy     stp
        lda     melody,y
        beq     @next
        cmp     #$FF
        bne     @on
        lda     #0
        sta     melv
        beq     @next
@on:    tay
        dey
        lda     freq,y
        sta     melf
        lda     #11
        sta     melv
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
; notes: semitones from C3; in melody 0 = nothing new, n+1 = note n, $FF = off
bassroot:
        .byte   9,5,0,7         ; A F C G
chords: .byte   21,24,28, 17,21,24, 24,28,31, 19,23,26
melody: .byte   29,0,0,0, 25,0,22,0, 25,0,27,0, 29,0,0,0        ; Am
        .byte   30,0,0,0, 29,0,25,0, 22,0,25,0, 30,0,29,0       ; F
        .byte   32,0,0,0, 29,0,25,0, 29,0,32,0, 34,0,32,0       ; C
        .byte   32,0,0,0, 27,0,24,0, 27,0,29,0, 27,0,$FF,0      ; G

phase:  .byte   0,10,20
hue1:   .byte   $30,$C0,$80     ; red, green, blue
hue2:   .byte   $F0,$50,$A0     ; orange, purple, turquoise
grad:   .byte   $02,$04,$06,$08,$0A,$0C,$0E,$0C,$0A,$08,$06,$04,$02

; generated: POKEY 64 kHz clock (PAL 1773447/28), pure tone f = clk/(2*(AUDF+1)), C3..B5
freq:
        .byte $F1,$E4,$D7,$CB,$BF,$B4,$AA,$A1,$98,$8F,$87,$7F
        .byte $78,$71,$6B,$65,$5F,$5A,$55,$50,$4B,$47,$43,$3F
        .byte $3C,$38,$35,$32,$2F,$2C,$2A,$27,$25,$23,$21,$1F
; 13.5+13.5*sin, 64 steps (upper bar area: 40 lines - 13)
sin1:
        .byte $0E,$0F,$10,$11,$13,$14,$15,$16,$17,$18,$19,$19,$1A,$1A,$1B,$1B
        .byte $1B,$1B,$1B,$1A,$1A,$19,$19,$18,$17,$16,$15,$14,$13,$11,$10,$0F
        .byte $0E,$0C,$0B,$0A,$08,$07,$06,$05,$04,$03,$02,$02,$01,$01,$00,$00
        .byte $00,$00,$00,$01,$01,$02,$02,$03,$04,$05,$06,$07,$08,$0A,$0B,$0C
; 9.5+9.5*sin (lower bar area: 32 lines - 13)
sin2:
        .byte $0A,$0A,$0B,$0C,$0D,$0E,$0F,$10,$10,$11,$11,$12,$12,$13,$13,$13
        .byte $13,$13,$13,$13,$12,$12,$11,$11,$10,$10,$0F,$0E,$0D,$0C,$0B,$0A
        .byte $0A,$09,$08,$07,$06,$05,$04,$03,$03,$02,$02,$01,$01,$00,$00,$00
        .byte $00,$00,$00,$00,$01,$01,$02,$02,$03,$03,$04,$05,$06,$07,$08,$09
; 16 hues, 8 lines each with a brightness ramp
rainbow:
        .byte $04,$06,$08,$0A,$0C,$0E,$0C,$08,$14,$16,$18,$1A,$1C,$1E,$1C,$18
        .byte $24,$26,$28,$2A,$2C,$2E,$2C,$28,$34,$36,$38,$3A,$3C,$3E,$3C,$38
        .byte $44,$46,$48,$4A,$4C,$4E,$4C,$48,$54,$56,$58,$5A,$5C,$5E,$5C,$58
        .byte $64,$66,$68,$6A,$6C,$6E,$6C,$68,$74,$76,$78,$7A,$7C,$7E,$7C,$78
        .byte $84,$86,$88,$8A,$8C,$8E,$8C,$88,$94,$96,$98,$9A,$9C,$9E,$9C,$98
        .byte $A4,$A6,$A8,$AA,$AC,$AE,$AC,$A8,$B4,$B6,$B8,$BA,$BC,$BE,$BC,$B8
        .byte $C4,$C6,$C8,$CA,$CC,$CE,$CC,$C8,$D4,$D6,$D8,$DA,$DC,$DE,$DC,$D8
        .byte $E4,$E6,$E8,$EA,$EC,$EE,$EC,$E8,$F4,$F6,$F8,$FA,$FC,$FE,$FC,$F8
