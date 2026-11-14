; Assembler sample (ca65): prints a line through the KERNAL and returns to BASIC.
; Build: cl65 -t c64 -C c64-asm.cfg -u __EXEHDR__ (the cc65 library adds the BASIC "SYS" line)

CHROUT  = $FFD2

        .segment "CODE"
        ldx     #0
loop:   lda     msg,x
        beq     done
        jsr     CHROUT
        inx
        bne     loop
done:   rts

msg:    .byte   "HELLO FROM CA65!", 13, 0
