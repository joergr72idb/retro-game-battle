/* Retro Game Battle - Apple II client: screen, keyboard and joystick (see platform.h) */
#include <conio.h>
#include <joystick.h>
#include "fujinet-network.h"
#include "rgb_platform.h"

static unsigned char have_joystick, was_fire;

unsigned char plat_init(void)
{
    if (network_init() != FN_ERR_OK)
        return 0;
    have_joystick = joy_install(a2e_stdjoy_joy) == JOY_ERR_OK;
    return 1;
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
    unsigned char now = 0, j;
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
    j = have_joystick ? joy_read(JOY_1) : 0;
    if (JOY_UP(j)) now = 1;
    else if (JOY_DOWN(j)) now = 2;
    else if (JOY_LEFT(j)) now = 3;
    else if (JOY_RIGHT(j)) now = 4;
    if (JOY_BTN_1(j) && !was_fire)      /* once per press */
        *fire = 1;
    was_fire = JOY_BTN_1(j);
    return now;
}
