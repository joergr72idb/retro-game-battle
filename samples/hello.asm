; Assembler sample (RASM): prints a line through the firmware and waits for a key.
; RASM writes HELLOA.BIN with an AMSDOS header.
;   RUN"HELLOA"               runs it; the CPC restarts when it returns (RUN resets the firmware)
;   MEMORY &3FFF:LOAD"HELLOA":CALL &4000   runs it and goes back to BASIC

TXT_OUTPUT  equ #BB5A
KM_WAIT_CHAR equ #BB06

        org #4000
start:  ld hl,msg
loop:   ld a,(hl)
        or a
        jr z,wait
        call TXT_OUTPUT
        inc hl
        jr loop
wait:   call KM_WAIT_CHAR
        ret

msg:    defb "Hello from RASM!",13,10,"Press a key.",0

        save "HELLOA.BIN",start,$-start,AMSDOS
