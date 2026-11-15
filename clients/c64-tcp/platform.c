/* Retro Game Battle - C64 client: screen, keyboard and joystick (see platform.h) */
#include <conio.h>
#include <cbm.h>
#include <string.h>
#include "rgb_platform.h"

static unsigned char was_fire;

unsigned char plat_init(void)
{
    return 1;                           /* the FujiNet answers on device 16 without setup */
}

void plat_clear(void)
{
    clrscr();
}

/* cc65's conio does not scroll: past the last line the cursor wraps and the text lands in the
   lines above (seen on the end screen, 2026-11-16). Scroll screen and colour RAM by hand. */
#define SCREEN  ((unsigned char *)0x0400)
#define COLOURS ((unsigned char *)0xD800)

static void scroll_up(void)
{
    memmove(SCREEN, SCREEN + 40, 24 * 40);
    memmove(COLOURS, COLOURS + 40, 24 * 40);
    memset(SCREEN + 24 * 40, ' ', 40);
    memset(COLOURS + 24 * 40, *(unsigned char *)0x0286, 40);   /* the current text colour */
}

static void next_line(void)
{
    if (wherey() >= 24) {
        scroll_up();
        gotoxy(0, 24);
    } else
        gotoxy(0, wherey() + 1);
}

void plat_putc(char c)
{
    unsigned char x;

    if (c == '\n') {
        next_line();
        return;
    }
    x = wherex();
    if (x == 39 && wherey() >= 24) {    /* the last cell: print it, then scroll instead of wrapping */
        cputc(c);
        scroll_up();
        gotoxy(0, 24);
        return;
    }
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

    if (k == '\n')                      /* RETURN */
        return RGB_KEY_ENTER;
    if (k == CH_DEL)
        return RGB_KEY_DEL;
    return k;
}

unsigned char plat_input(unsigned char *fire)
{
    unsigned char now = 0, j, i;
    char k;

    *fire = 0;
    while ((k = cbm_k_getin()) != 0) {  /* cursor keys queued since the last pass */
        if (k == ' ') *fire = 1;
        else if (k == CH_CURS_UP) now = 1;
        else if (k == CH_CURS_DOWN) now = 2;
        else if (k == CH_CURS_LEFT) now = 3;
        else if (k == CH_CURS_RIGHT) now = 4;
    }
    j = ~*(volatile unsigned char *)0xDC00;    /* the stick in port 2: CIA 1 port A, active low */
    for (i = 0; i < 4; ++i)
        if (j & (1 << i))
            now = i + 1;
    if ((j & 0x10) && !was_fire)        /* once per press */
        *fire = 1;
    was_fire = j & 0x10;
    return now;
}
