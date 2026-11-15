/* Retro Game Battle - Atari XL/XE client: screen, keyboard and joystick (see platform.h) */
#include <conio.h>
#include <atari.h>
#include "fujinet-network.h"
#include "rgb_platform.h"

static unsigned char was_fire;

unsigned char plat_init(void)
{
    OS.soundr = 0;                      /* quiet SIO: the client polls the FujiNet all the time */
    return network_init() == FN_ERR_OK;
}

void plat_clear(void)
{
    clrscr();
}

void plat_putc(char c)
{
    if (c == '\n')
        cputc('\r');
    cputc(c);
}

void plat_print(const char *s)
{
    while (*s)
        plat_putc(*s++);
}

void plat_cursor(unsigned char on)
{
    cursor(on);
}

unsigned char plat_getkey(void)
{
    char k = cgetc();

    if (k == CH_ENTER)
        return RGB_KEY_ENTER;
    if (k == CH_DEL || k == CH_CURS_LEFT)
        return RGB_KEY_DEL;
    return k;
}

unsigned char plat_input(unsigned char *fire)
{
    unsigned char now = 0, j, trig;
    char k;

    *fire = 0;
    while (kbhit()) {
        k = cgetc();
        if (k == ' ') *fire = 1;
        else if (k == CH_CURS_UP) now = 1;
        else if (k == CH_CURS_DOWN) now = 2;
        else if (k == CH_CURS_LEFT) now = 3;
        else if (k == CH_CURS_RIGHT) now = 4;
    }
    /* the hardware, not the OS copies STICK0/STRIG0: the OS skips updating them while SIO
       runs (CRITIC), and this client talks to the FujiNet nearly all the time */
    j = ~PIA.porta & 0x0F;              /* joystick 1: bits 0-3 up/down/left/right, active low */
    if (j & 1) now = 1;
    else if (j & 2) now = 2;
    else if (j & 4) now = 3;
    else if (j & 8) now = 4;
    trig = GTIA_READ.trig0 == 0;        /* 0 = pressed */
    if (trig && !was_fire)              /* once per press */
        *fire = 1;
    was_fire = trig;
    return now;
}
