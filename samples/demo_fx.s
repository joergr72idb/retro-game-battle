; demo_fx.s - the moving parts of samples/demo.c (ca65, Apple //e enhanced). Never returns.
;
; Lo-res mixed mode: 40 x 40 colour blocks (text rows 0-19, each byte = two blocks, low nibble
; on top) and the text lines 20-23. Every frame:
;   colours  a foreground and a background colour for each of the 40 block rows: rainbow for the
;            logo and the scroller, raster bars in the background
;   scroll   every second frame the scroller rows move one block to the left
;   draw     the 20 rows into the hidden page: per text row, the 4 possible bytes (letter or not,
;            top and bottom block) come from those colours, _fx_mask picks one per column
;   then     wait for the vertical blank ($C019), show the page just drawn, play the music
;
; Music: a Mockingboard in slot 4, both AY chips get the same registers (sound in the middle).

        .export _fx_start, _fx_mask
        .import _scroll_cols, _scroll_len

KBD     = $C000
VBLBAR  = $C019                 ; bit 7 = 0 during the vertical blank (//e)

; block rows
LOGO_END   = 19                 ; logo: rows 0-18
BARS       = 19                 ; bars: rows 19-31 (13 rows)
SCROLL     = 32                 ; scroller: rows 32-39 (letters in 33-37)
ORDLEN     = 3
STEPFRAMES = 7                  ; 60 Hz: 7 frames per 16th = 129 BPM (the Atari's 125)

MB1     = $C400                 ; Mockingboard slot 4: VIA 1 -> AY 1, VIA 2 -> AY 2
MB2     = $C480

; ---------------------------------------------------------------------------------------
        .segment "BSS"
_fx_mask:       .res 20*40
fg:     .res    40
bg:     .res    40
vtab:   .res    4
tr:     .res    1
frame:  .res    1
pageoff:.res    1               ; $04 = drawing page 2 ($0800), $00 = page 1 ($0400)
pos:    .res    2
soff:   .res    1               ; start of the scroller's ring buffers
ring16: .res    80
ring17: .res    80
ring18: .res    80
tmp:    .res    1
bn:     .res    1
; music
tick:   .res    1
stp:    .res    1
bar:    .res    1
ordi:   .res    1
arpi:   .res    1
melv:   .res    1
bassn:  .res    1
bassv:  .res    1
dptr:   .res    2               ; drum frame, high byte 0 = none
psg:    .res    14              ; AY registers 0-13, written when they change
last:   .res    14

; ---------------------------------------------------------------------------------------
        .segment "CODE"

_fx_start:
        sei
        sta     $C000           ; 80STORE off: PAGE2 switches the display page
        sta     $C00C           ; 40 columns
        sta     $C00E           ; normal character set
        lda     #0
        sta     frame
        sta     pos
        sta     pos+1
        sta     soff
        ldx     #239            ; scroller empty
@ring:  sta     ring16,x
        dex
        bne     @ring
        sta     ring16
        jsr     scroll_ptrs
        lda     #0              ; (scroll_ptrs changes A)
        sta     tick
        sta     stp
        sta     ordi
        sta     arpi
        sta     melv
        sta     bassv
        sta     dptr+1
        ldx     #13
@psg:   sta     psg,x
        lda     #$FF            ; "changed": all registers go out in the first frame
        sta     last,x
        lda     #0
        dex
        bpl     @psg
        lda     #$38            ; tones on, noise off
        sta     psg+7
        jsr     ay_init
        lda     #$04            ; draw page 2 first, page 1 still shows the text
        sta     pageoff
        jsr     colours
        jsr     draw
        bit     $C056           ; lo-res
        bit     $C053           ; mixed: 4 text lines
        bit     $C055           ; page 2
        bit     $C050           ; graphics
        lda     #$00
        sta     pageoff
loop:   inc     frame
        jsr     colours
        lda     frame
        lsr     a
        bcc     @noscroll
        jsr     scroll
@noscroll:
        jsr     draw
        jsr     vbl
        lda     pageoff         ; show what was drawn, draw the other page next
        bne     @p2
        bit     $C054
        lda     #$04
        bne     @set
@p2:    bit     $C055
        lda     #$00
@set:   sta     pageoff
        jsr     music
        jmp     loop

; wait for the start of the vertical blank
vbl:    lda     VBLBAR
        bpl     vbl             ; still in the last one
@w:     lda     VBLBAR
        bmi     @w
        rts

; --- colours for this frame ---------------------------------------------------------------
colours:
        ldx     #39
        lda     #0
@clr:   sta     fg,x
        sta     bg,x
        dex
        bpl     @clr
        lda     frame           ; logo: rainbow moving down one block row every 4 frames
        lsr     a
        lsr     a
        sta     tmp
        ldx     #0
@logo:  txa
        sec
        sbc     tmp
        and     #15
        tay
        lda     rainbow,y
        sta     fg,x
        inx
        cpx     #LOGO_END
        bne     @logo
        ldx     #SCROLL         ; scroller: moving up
@scr:   txa
        clc
        adc     tmp
        and     #15
        tay
        lda     rainbow,y
        sta     fg,x
        inx
        cpx     #40
        bne     @scr
        lda     #0              ; three bars of 5 rows
        sta     bn
@bar:   ldy     bn
        lda     frame
        clc
        adc     phase,y
        and     #63
        tay
        lda     sinb,y
        clc
        adc     #BARS
        tax                     ; first row of the bar
        lda     bn              ; ramp = bn * 5
        asl     a
        asl     a
        adc     bn
        tay
        lda     #5
        sta     tmp
@row:   lda     ramps,y
        sta     bg,x
        inx
        iny
        dec     tmp
        bne     @row
        inc     bn
        lda     bn
        cmp     #3
        bne     @bar
        rts

; --- scroller: one block to the left, the next column of the text at the right ------------
; Block rows 33-37 = text row 16 bottom, 17 top/bottom, 18 top/bottom. Each of those three rows
; is a ring buffer of 80 bytes (the 40 columns twice): a step writes the new column twice and
; moves the start by one, instead of copying 120 bytes.
scroll: ldx     soff
        clc                     ; column = scroll_cols[pos]
        lda     #<_scroll_cols
        adc     pos
        sta     @get+1
        lda     #>_scroll_cols
        adc     pos+1
        sta     @get+2
@get:   lda     $FFFF
        sta     tmp
        and     #1              ; row 33 = bottom of text row 16
        asl     a
        sta     ring16,x
        sta     ring16+40,x
        lda     tmp             ; rows 34, 35 = text row 17
        lsr     a
        and     #3
        sta     ring17,x
        sta     ring17+40,x
        lda     tmp             ; rows 36, 37 = text row 18
        lsr     a
        lsr     a
        lsr     a
        and     #3
        sta     ring18,x
        sta     ring18+40,x
        inx                     ; the new start
        cpx     #40
        bne     @off
        ldx     #0
@off:   stx     soff
        jsr     scroll_ptrs
        inc     pos
        bne     @cmp
        inc     pos+1
@cmp:   lda     pos
        cmp     _scroll_len
        bne     @r
        lda     pos+1
        cmp     _scroll_len+1
        bne     @r
        lda     #0
        sta     pos
        sta     pos+1
@r:     rts

; draw reads rows 16-18 from ring + soff (and - 1)
scroll_ptrs:
        ldx     soff
        txa
        clc
        adc     #<ring16
        sta     masklo+16
        lda     #>ring16
        adc     #0
        sta     maskhi+16
        txa
        clc
        adc     #<ring17
        sta     masklo+17
        lda     #>ring17
        adc     #0
        sta     maskhi+17
        txa
        clc
        adc     #<ring18
        sta     masklo+18
        lda     #>ring18
        adc     #0
        sta     maskhi+18
        ldy     #16             ; the "- 1" pointers
@m1:    lda     masklo,y
        sec
        sbc     #1
        sta     masklo1,y
        lda     maskhi,y
        sbc     #0
        sta     maskhi1,y
        iny
        cpy     #19
        bne     @m1
        rts

; --- the 20 lo-res rows into the hidden page ---------------------------------------------
draw:   ldx     #0
@row:   stx     tr
        txa                     ; vtab: 0 none, 1 top, 2 bottom, 3 both blocks are letter
        asl     a
        tay
        ldx     bg+1,y          ; bottom block colours to the high nibble
        lda     shl4,x
        sta     tmp
        ora     bg,y
        sta     vtab
        lda     fg,y
        ora     tmp
        sta     vtab+1
        ldx     fg+1,y
        lda     shl4,x
        sta     tmp
        ora     bg,y
        sta     vtab+2
        lda     fg,y
        ora     tmp
        sta     vtab+3
        ldx     tr
        lda     rowhi,x         ; the page: high byte + pageoff
        clc
        adc     pageoff
        ldy     hasmask,x
        beq     @fill
        sta     @st1+2          ; letters: two bytes a turn, 16.5 cycles a byte
        lda     rowlo,x
        sta     @st1+1
        lda     rowlo1,x        ; screen - 1 for column x-1 (row 0 starts at $0400: borrow)
        sta     @st2+1
        lda     rowhi1,x
        clc
        adc     pageoff
        sta     @st2+2
        lda     masklo,x
        sta     @ld1+1
        lda     maskhi,x
        sta     @ld1+2
        lda     masklo1,x
        sta     @ld2+1
        lda     maskhi1,x
        sta     @ld2+2
        ldx     #39
@ld1:   ldy     $FFFF,x
        lda     vtab,y
@st1:   sta     $FFFF,x
@ld2:   ldy     $FFFF,x         ; mask - 1: column x-1
        lda     vtab,y
@st2:   sta     $FFFF,x         ; screen - 1
        dex
        dex
        bpl     @ld1
        bmi     @next
@fill:  sta     @f0+2           ; no letters: four bytes a turn, 6.25 cycles a byte
        sta     @f1+2
        sta     @f2+2
        sta     @f3+2
        lda     rowlo,x         ; +10/+20/+30 never carry: rows start at $x00 to $xD0
        sta     @f0+1
        adc     #10             ; carry is clear (adc pageoff above did not overflow)
        sta     @f1+1
        adc     #10
        sta     @f2+1
        adc     #10
        sta     @f3+1
        lda     vtab
        ldx     #9
@f0:    sta     $FFFF,x
@f1:    sta     $FFFF,x
@f2:    sta     $FFFF,x
@f3:    sta     $FFFF,x
        dex
        bpl     @f0
@next:  ldx     tr
        inx
        cpx     #20
        beq     @done
        jmp     @row
@done:  rts

; --- music ------------------------------------------------------------------------------
; The tune of the Atari, C64 and CPC demos: Am - F - C - G. Channel A melody, B chord arpeggio
; (a new chord note every frame), C drums and bass: every 8th note a short drum sound (kick,
; hi-hat, snare, hi-hat) that turns into the bass note. Intro without melody, then twice with it.
ay_init:
        lda     #$FF            ; VIA port A out (data), port B bits 0-2 out (AY control)
        sta     MB1+3
        sta     MB2+3
        lda     #$07
        sta     MB1+2
        sta     MB2+2
        lda     #$00            ; reset the AYs
        sta     MB1
        sta     MB2
        lda     #$04            ; inactive
        sta     MB1
        sta     MB2
        rts

; AY register X = A, on both chips
ay:     stx     MB1+1
        ldy     #$07            ; latch address
        sty     MB1
        ldy     #$04
        sty     MB1
        sta     MB1+1
        ldy     #$06            ; write
        sty     MB1
        ldy     #$04
        sty     MB1
        stx     MB2+1
        ldy     #$07
        sty     MB2
        ldy     #$04
        sty     MB2
        sta     MB2+1
        ldy     #$06
        sty     MB2
        ldy     #$04
        sty     MB2
        rts

music:  dec     tick
        bpl     @frame
        lda     #STEPFRAMES-1
        sta     tick
        jsr     step
@frame: ldx     arpi            ; B: arpeggio
        inx
        cpx     #3
        bne     @arp
        ldx     #0
@arp:   stx     arpi
        lda     bar
        asl     a
        adc     bar
        adc     arpi
        tay
        lda     chords,y
        jsr     period
        stx     psg+2
        sta     psg+3
        lda     #10
        sta     psg+9
        lda     frame           ; A: melody, slowly fading to a quiet sustain
        and     #3
        bne     @mv
        lda     melv
        cmp     #8
        bcc     @mv
        dec     melv
@mv:    lda     melv
        sta     psg+8
        lda     dptr+1          ; C: a drum frame, or the bass
        beq     @bass
        sta     @d1+2
        lda     dptr
        sta     @d1+1
        ldy     #0
@d1:    lda     $FFFF,y         ; patched: the drum frame
        beq     @dend
        sta     psg+7           ; mixer
        jsr     dnext
        sta     psg+6           ; noise period
        jsr     dnext
        sta     psg+4           ; tone period
        jsr     dnext
        sta     psg+5
        jsr     dnext
        sta     psg+10          ; volume
        clc
        lda     dptr
        adc     #5
        sta     dptr
        bcc     @write
        inc     dptr+1
        bne     @write
@dend:  lda     #0
        sta     dptr+1
@bass:  lda     #$38            ; tones on, noise off
        sta     psg+7
        lda     bassn
        jsr     period
        stx     psg+4
        sta     psg+5
        lda     bassv
        sta     psg+10
        cmp     #5
        bcc     @write
        dec     bassv
@write: ldx     #13             ; the registers that changed
@w:     lda     psg,x
        cmp     last,x
        beq     @same
        sta     last,x
        jsr     ay
@same:  dex
        bpl     @w
        rts

; next byte of the drum frame (through the patched address in music)
dnext:  iny
        lda     dptr
        sta     @d+1
        lda     dptr+1
        sta     @d+2
@d:     lda     $FFFF,y
        rts

; X = low, A = high byte of the tone period of note A (0 = C2)
period: asl     a
        tay
        ldx     notes,y
        lda     notes+1,y
        rts

step:   lda     stp
        lsr     a
        lsr     a
        lsr     a
        lsr     a
        sta     bar
        lda     stp
        and     #1
        bne     @mel
        ldy     bar             ; C on every 8th: drum + bass (root, octave)
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
        lda     #14
        sta     bassv
        lda     stp             ; 0 kick, 2 hat, 4 snare, 6 hat
        and     #6
        tay
        lda     drumstart,y
        sta     dptr
        lda     drumstart+1,y
        sta     dptr+1
@mel:   ldx     ordi            ; A: melody
        lda     order,x
        beq     @next
        ldy     stp
        lda     melody,y
        beq     @next
        cmp     #$FF
        bne     @on
        lda     #0              ; note off
        sta     melv
        beq     @next
@on:    clc
        adc     #11             ; n+1 -> n, one octave above the table start (C2)
        jsr     period
        stx     psg+0
        sta     psg+1
        lda     #13
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

; text rows 0-19 on page 1 ($0400); page 2 is +$0400
rowlo:
        .repeat 20, I
        .byte   <($0400 + (I .mod 8) * 128 + (I / 8) * 40)
        .endrepeat
rowhi:
        .repeat 20, I
        .byte   >($0400 + (I .mod 8) * 128 + (I / 8) * 40)
        .endrepeat
rowlo1:
        .repeat 20, I
        .byte   <($0400 + (I .mod 8) * 128 + (I / 8) * 40 - 1)
        .endrepeat
rowhi1:
        .repeat 20, I
        .byte   >($0400 + (I .mod 8) * 128 + (I / 8) * 40 - 1)
        .endrepeat
shl4:
        .repeat 16, I
        .byte   I * 16
        .endrepeat
        .segment "DATA"
; where draw reads the letter bits of each row (rows 16-18 move with the scroller)
masklo:
        .repeat 20, I
        .byte   <(_fx_mask + I * 40)
        .endrepeat
maskhi:
        .repeat 20, I
        .byte   >(_fx_mask + I * 40)
        .endrepeat
masklo1:
        .repeat 20, I
        .byte   <(_fx_mask + I * 40 - 1)
        .endrepeat
maskhi1:
        .repeat 20, I
        .byte   >(_fx_mask + I * 40 - 1)
        .endrepeat
        .segment "RODATA"
; rows with letters: the logo (block rows 1-17) and the scroller (33-37)
hasmask:.byte   1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,1,1,1,0

; lo-res colours: 0 black 1 magenta 2 dark blue 3 purple 4 dark green 5 grey 6 medium blue
; 7 light blue 8 brown 9 orange 10 grey 11 pink 12 green 13 yellow 14 aqua 15 white
rainbow:.byte   1, 9, 13, 15, 13, 12, 14, 15, 14, 7, 6, 7, 15, 11, 3, 11
ramps:  .byte   1, 9, 13, 9, 1              ; fire
        .byte   4, 12, 15, 12, 4            ; green
        .byte   2, 6, 7, 6, 2               ; blue
phase:  .byte   0, 21, 42
; 4+4*sin, 64 steps (13 rows - 5)
sinb:   .byte   4,4,5,5,6,6,6,7,7,7,8,8,8,8,8,8,8,8,8,8,8,8,8,7,7,7,6,6,6,5,5,4
        .byte   4,4,3,3,2,2,2,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,2,2,2,3,3,4

order:  .byte   0, 1, 1                     ; 1 = with melody
; notes: index into notes (0 = C2); melody as in the Atari demo: 0 nothing new,
; n+1 = note n above C3, $FF = off
bassroot:
        .byte   9, 5, 0, 7                  ; A2 F2 C2 G2
chords: .byte   33,36,40, 29,33,36, 36,40,43, 31,35,38
melody: .byte   29,0,0,0, 25,0,22,0, 25,0,27,0, 29,0,0,0       ; Am
        .byte   30,0,0,0, 29,0,25,0, 22,0,25,0, 30,0,29,0      ; F
        .byte   32,0,0,0, 29,0,25,0, 29,0,32,0, 34,0,32,0      ; C
        .byte   32,0,0,0, 27,0,24,0, 27,0,29,0, 27,0,$FF,0     ; G

; drums on channel C, one frame per line: mixer, noise period, tone period, volume; 0 = end
drumstart:
        .word   kick, hat, snare, hat
kick:   .byte   $38, 0
        .word   $0180
        .byte   15
        .byte   $38, 0
        .word   $0240
        .byte   14
        .byte   $38, 0
        .word   $0300
        .byte   13
        .byte   $38, 0
        .word   $0400
        .byte   11
        .byte   $38, 0
        .word   $0500
        .byte   9, 0
snare:  .byte   $1C, 8, 0, 0, 15
        .byte   $1C, 10, 0, 0, 13
        .byte   $1C, 12, 0, 0, 11
        .byte   $1C, 14, 0, 0, 9
        .byte   $1C, 16, 0, 0, 7, 0
hat:    .byte   $1C, 1, 0, 0, 11
        .byte   $1C, 1, 0, 0, 7, 0

; generated: AY tone periods for the Mockingboard, 1.023 MHz / (16 f), C2..B6
notes:  .word   978, 923, 871, 822, 776, 732, 691, 652, 616, 581, 549, 518
        .word   489, 461, 435, 411, 388, 366, 346, 326, 308, 291, 274, 259
        .word   244, 231, 218, 206, 194, 183, 173, 163, 154, 145, 137, 129
        .word   122, 115, 109, 103, 97, 92, 86, 82, 77, 73, 69, 65
        .word   61, 58, 54, 51, 48, 46, 43, 41, 38, 36, 34, 32
