/* Retro Game Battle - C64 client over raw TCP (FujiNet).
 *
 * The same protocol as the Atari/FujiNet client (docs/protocol.md, "raw TCP"): one connection
 * for the whole match, MOVE only when the stick changes, server lines read without blocking.
 * The BASIC clients poll over HTTP (Meatloaf ~300 ms, WiC64 ~1280 ms per tick); this one runs
 * on a C64 with a FujiNet (IEC, network device 16) and never waits for the network to steer.
 *
 * Built with the C64 kit (cc65 + fujinet-lib >= 5.1.1): make SERVER=192.168.17.138, see Makefile.
 */
#include <stdio.h>
#include <string.h>
#include <conio.h>
#include <cbm.h>
#include "fujinet-network.h"

#ifndef SERVER_HOST
#define SERVER_HOST "127.0.0.1"
#endif
#ifndef SERVER_PORT
#define SERVER_PORT "6502"
#endif
#ifndef PLAYER_NAME
#define PLAYER_NAME "C64FUJINET"
#endif

#define BUILD       "1"

/* cc65 turns string literals into PETSCII, where upper and lower case are swapped against
   ASCII. Everything that goes over the wire is converted to ASCII first, everything that
   comes back to PETSCII - then the literals below compare and print correctly. */
static char spec[48];               /* "N:TCP://host:port/" in ASCII */
static char line[200];              /* the last complete line from the server, PETSCII */
static char inbuf[200];             /* received bytes not yet split into lines, ASCII */
static unsigned char inlen;
static char out[64];

static unsigned char pet2ascii(unsigned char c)
{
    if (c >= 0xC1 && c <= 0xDA) return c - 0x80;    /* PETSCII A-Z -> ASCII A-Z */
    if (c >= 0x41 && c <= 0x5A) return c + 0x20;    /* PETSCII a-z -> ASCII a-z */
    return c;
}

static unsigned char ascii2pet(unsigned char c)
{
    if (c >= 0x61 && c <= 0x7A) return c - 0x20;    /* ASCII a-z -> PETSCII a-z */
    if (c >= 0x41 && c <= 0x5A) return c + 0x80;    /* ASCII A-Z -> PETSCII A-Z */
    return c;
}

static void to_ascii(char *dst, const char *src)
{
    while ((*dst++ = pet2ascii(*src++)) != 0)
        ;
}

/* ---- network ----------------------------------------------------------------------- */

static void send_line(const char *s)
{
    unsigned char n = strlen(s);

    to_ascii(out, s);
    out[n] = 13;                    /* CR LF as numbers: '\n' is 13 in cc65's CBM charmap */
    out[n + 1] = 10;
    network_write(spec, out, n + 2);
}

/* Takes what has arrived, without waiting. 1 = a line is in `line`, 0 = none yet,
   -1 = the connection is gone. */
static signed char poll_line(void)
{
    static uint16_t waiting;
    static uint8_t connected, err;
    unsigned char i, j;
    int16_t r;

    for (;;) {
        for (i = 0; i < inlen; ++i) {
            if (inbuf[i] == 13 || inbuf[i] == 10) {
                for (j = 0; j < i; ++j)
                    line[j] = ascii2pet(inbuf[j]);
                line[i] = 0;
                while (i < inlen && (inbuf[i] == 13 || inbuf[i] == 10))
                    ++i;
                inlen -= i;
                memmove(inbuf, inbuf + i, inlen);
                if (line[0])
                    return 1;
                i = 255;            /* empty line: look again from the start */
            }
        }
        if (inlen == sizeof inbuf)  /* a line longer than the buffer: drop it */
            inlen = 0;
        if (network_status(spec, &waiting, &connected, &err) != FN_ERR_OK)
            return -1;
        if (waiting == 0)
            return connected ? 0 : -1;
        if (waiting > sizeof inbuf - inlen)
            waiting = sizeof inbuf - inlen;
        r = network_read_nb(spec, inbuf + inlen, waiting);
        if (r < 0)
            return -1;
        if (r == 0)
            return 0;
        inlen += r;
    }
}

static signed char wait_line(void)
{
    signed char r;

    while ((r = poll_line()) == 0)
        ;
    return r;
}

static unsigned char starts(const char *prefix)
{
    return strncmp(line, prefix, strlen(prefix)) == 0;
}

/* ---- screens ----------------------------------------------------------------------- */

static void read_pin(char *pin, unsigned char max)
{
    unsigned char n = 0;
    char k;

    cursor(1);
    for (;;) {
        k = cgetc();
        if (k == '\n')              /* RETURN */
            break;
        if (k == CH_DEL && n) {
            --n;
            cputs("\x9d \x9d");
        } else if (k > ' ' && k < 0x80 && n + 1 < max) {
            pin[n++] = k;
            cputc(k);
        }
    }
    cursor(0);
    pin[n] = 0;
    cputs("\r\n");
}

/* "WAIT GAMES <n> <id>:<label> ..." or "ERR INCOMPATIBLE GAMES <n> ...": show the list,
   send GAME <n> for the key pressed */
static void choose_game(const char *list)
{
    unsigned char count = 0;
    char k;
    const char *p = list;

    cputs("\r\nCHOOSE A GAME:\r\n");
    while (*p == ' ')
        ++p;
    while (*p && *p != ' ')         /* skip the count, number the entries ourselves */
        ++p;
    while (*p) {
        while (*p == ' ')
            ++p;
        if (!*p)
            break;
        cprintf(" %u = ", ++count);
        while (*p && *p != ':' && *p != ' ')
            ++p;                    /* the id */
        if (*p == ':')
            ++p;
        while (*p && *p != ' ')
            cputc(*p++);            /* the label */
        cputs("\r\n");
    }
    do
        k = cgetc();
    while (k < '1' || k > '0' + count);
    sprintf(out, "GAME %c", k);
    send_line(out);
    cprintf("GAME %c\r\n", k);
}

/* ---- the match --------------------------------------------------------------------- */

static void play(void)
{
    static const char dirs[] = "UDLR";
    unsigned char dir = 0, now, j, i;
    char k;
    signed char r;

    cputs("\r\nUSE JOYSTICK (PORT 2)\r\n");
    for (;;) {
        /* cursor keys queued since the last pass, then the stick in port 2 */
        now = 0;
        while ((k = cbm_k_getin()) != 0) {
            if (k == CH_CURS_UP) now = 1;
            else if (k == CH_CURS_DOWN) now = 2;
            else if (k == CH_CURS_LEFT) now = 3;
            else if (k == CH_CURS_RIGHT) now = 4;
        }
        j = ~*(volatile unsigned char *)0xDC00;    /* CIA 1 port A, active low */
        for (i = 0; i < 4; ++i)
            if (j & (1 << i))
                now = i + 1;
        if (now && now != dir) {    /* only on a change: the server keeps the last one */
            dir = now;
            sprintf(out, "MOVE %c", dirs[dir - 1]);
            send_line(out);
        }

        r = poll_line();
        if (r < 0) {
            cputs("\r\nCONNECTION CLOSED\r\n");
            return;
        }
        if (r > 0 && starts("END")) {
            cprintf("\r\nGAME OVER\r\n%s\r\n", line);
            if (wait_line() > 0)    /* STATS */
                cprintf("%s\r\n", line);
            return;
        }
    }
}

static void session(void)
{
    char pin[12];
    unsigned char started = 0;

    clrscr();
    cputs(" RETRO GAME BATTLE -\r\n CLASSIC COMPUTING 2026\r\n C64 FUJINET - TCP BUILD " BUILD "\r\n\r\n");
    cputs("ENTER YOUR PIN OR PRESS RETURN:\r\n? ");
    read_pin(pin, sizeof pin);
    if (!pin[0])
        strcpy(pin, "NONE");

    cputs("CONNECTING...\r\n");
    to_ascii(spec, "N:TCP://" SERVER_HOST ":" SERVER_PORT "/");
    if (network_open(spec, OPEN_MODE_RW, OPEN_TRANS_NONE) != FN_ERR_OK) {
        cputs("NO CONNECTION TO " SERVER_HOST ":" SERVER_PORT "\r\n");
        network_close(spec);
        return;
    }
    inlen = 0;
    sprintf(line, "HELLO C64 " PLAYER_NAME " C64 %s", pin);
    send_line(line);

    while (!started) {
        if (wait_line() < 0) {
            cputs("CONNECTION CLOSED\r\n");
            break;
        }
        if (starts("WAIT GAMES"))
            choose_game(line + 10);
        else if (starts("ERR INCOMPATIBLE GAMES"))
            choose_game(line + 22);
        else if (starts("WAIT"))
            cputs("WAITING FOR OPPONENT...\r\n");
        else if (starts("START")) {
            started = 1;
            play();
        } else if (starts("ERR")) {
            cprintf("%s\r\n", line);
            break;
        }
    }
    send_line("BYE");
    network_close(spec);
}

int main(void)
{
    for (;;) {
        session();
        cputs("\r\nPRESS A KEY TO PLAY AGAIN.");
        cgetc();
    }
}
