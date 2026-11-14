#include "cpc.h"

static void txt_output(unsigned char c) __naked __z88dk_fastcall
{
    (void)c;
    __asm
        ld      a, l
        jp      0xBB5A              ; TXT OUTPUT, keeps all registers
    __endasm;
}

unsigned char cpc_getkey(void) __naked
{
    __asm
        call    0xBB06              ; KM WAIT CHAR -> A
        ld      l, a                ; for both SDCC calling conventions
        ret
    __endasm;
}

int putchar(int c)
{
    if (c == '\n')
        txt_output('\r');
    txt_output((unsigned char)c);
    return c;
}

void cpc_print(const char *s)
{
    while (*s)
        putchar(*s++);
}
