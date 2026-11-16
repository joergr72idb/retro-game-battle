/* Retro Game Battle - CPC client: screen, keyboard and joystick through the firmware
 * (see platform.h) */
#include <stdio.h>
#include "cpc.h"
#include "fujinet-network.h"
#include "rgb_platform.h"

static unsigned char was_fire;

/* KM GET JOYSTICK: bits 0 up, 1 down, 2 left, 3 right, 4/5 fire - joystick 0 */
static unsigned char joystick(void) __naked
{
    __asm
        call    0xBB24
        ld      l, h                ; H = joystick 0
        ld      a, h
        ret
    __endasm;
}

/* KM READ CHAR: a key from the buffer, or 0 if none (does not wait) */
static unsigned char read_key(void) __naked
{
    __asm
        call    0xBB09
        jr      c, got
        xor     a
got:    ld      l, a
        ret
    __endasm;
}

/* KM SET TRANSLATE / SHIFT / CONTROL: key number tkey gives the character tchar in all three tables */
static unsigned char tkey, tchar;
static void km_translate(void) __naked
{
    __asm
        ld      a, (_tchar)
        ld      b, a
        ld      a, (_tkey)
        call    0xBB27
        ld      a, (_tchar)
        ld      b, a
        ld      a, (_tkey)
        call    0xBB2D
        ld      a, (_tchar)
        ld      b, a
        ld      a, (_tkey)
        jp      0xBB33
    __endasm;
}

unsigned char plat_init(void)
{
    /* Joystick 0's fire buttons (key numbers 76, 77) type 'X' and 'Z' by default: after a match,
       a player still on the stick started the next one and put an X into the PIN (hands-free
       back-to-back test, 2026-11-16). Now they give codes no key has; plat_getkey() skips those
       and the stick's directions (8-11), plat_input() still reads them. */
    tkey = 76; tchar = 225; km_translate();
    tkey = 77; tchar = 224; km_translate();
    return network_init() == FN_ERR_OK;
}

/* SCR SET MODE 1: 40 columns, clears the screen */
void plat_clear(void) __naked
{
    __asm
        ld      a, #1
        jp      0xBC0E
    __endasm;
}

void plat_putc(char c)
{
    putchar(c);                         /* '\n' becomes CR LF */
}

void plat_print(const char *s)
{
    cpc_print(s);
}

void plat_cursor(unsigned char on)
{
    (void)on;                           /* KM WAIT CHAR shows the cursor itself */
}

unsigned char plat_getkey(void)
{
    unsigned char k;

    do                                  /* the keyboard only: skip joystick 0 (see plat_init) */
        k = cpc_getkey();
    while ((k >= 8 && k <= 11) || k == 224 || k == 225);
    return k == 127 ? RGB_KEY_DEL : k;  /* ENTER and RETURN are 13 already */
}

unsigned char plat_input(unsigned char *fire)
{
    unsigned char now = 0, j, k, i;

    *fire = 0;
    /* cursor keys and joystick codes queued in the key buffer, then the stick */
    while ((k = read_key()) != 0) {
        if (k == ' ' || k == 224 || k == 225) *fire = 1;   /* space, joystick fire codes */
        else if (k == 240 || k == 11) now = 1;
        else if (k == 241 || k == 10) now = 2;
        else if (k == 242 || k == 8) now = 3;
        else if (k == 243 || k == 9) now = 4;
    }
    j = joystick();
    for (i = 0; i < 4; ++i)
        if (j & (1 << i))
            now = i + 1;
    if ((j & 0x30) && !was_fire)        /* once per press */
        *fire = 1;
    was_fire = j & 0x30;
    return now;
}
