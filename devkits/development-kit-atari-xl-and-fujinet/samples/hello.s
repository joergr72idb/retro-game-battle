; Assembler sample (ca65): prints a line through CIO channel 0 (the screen editor E:)
; and waits for a key. Build: cl65 -t atari -C atari-asm.cfg

        .include "atari.inc"
        .export start               ; the cc65 library adds the XEX headers and RUNAD

        .segment "CODE"
start:  ldx     #0                  ; channel 0 = E:
        lda     #PUTREC
        sta     ICCOM,x
        lda     #<msg
        sta     ICBAL,x
        lda     #>msg
        sta     ICBAH,x
        lda     #<(msg_end - msg)
        sta     ICBLL,x
        lda     #>(msg_end - msg)
        sta     ICBLH,x
        jsr     CIOV

        lda     #255                ; wait for a key
        sta     CH
wait:   lda     CH
        cmp     #255
        beq     wait
        lda     #255
        sta     CH
        rts                         ; back to DOS

msg:    .byte   "Hello from ca65!", ATEOL
msg_end:

