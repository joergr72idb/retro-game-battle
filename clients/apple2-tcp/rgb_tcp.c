/* Retro Game Battle - Apple II client over raw TCP (FujiNet).
 *
 * The same protocol as the Atari/FujiNet client (docs/protocol.md, "raw TCP"): one connection
 * for the whole match, MOVE only when the stick changes, server lines read without blocking.
 * Runs on an Apple //e (enhanced) or later with a FujiNet on the SmartPort; a ProDOS SYS
 * program (RGBTCP.SYSTEM) that starts by itself from the boot disk.
 *
 * Built with the Apple II kit (cc65 + fujinet-lib >= 5.1.1): make SERVER=192.168.17.138.
 */
#include <stdio.h>
#include <string.h>
#include <conio.h>
#include <joystick.h>
#include "fujinet-network.h"

#ifndef SERVER_HOST
#define SERVER_HOST "127.0.0.1"
#endif
#ifndef SERVER_PORT
#define SERVER_PORT "6502"
#endif
#ifndef PLAYER_NAME
#define PLAYER_NAME "APPLE2"
#endif

#define BUILD       "1"

static const char spec[] = "N:TCP://" SERVER_HOST ":" SERVER_PORT "/";
static char line[200];              /* the last complete line from the server */
static char inbuf[200];             /* received bytes not yet split into lines */
static unsigned char inlen;
static char out[64];
static unsigned char have_joystick;

/* ---- network ----------------------------------------------------------------------- */

static void send_line(const char *s)
{
    unsigned char n = strlen(s);

    memcpy(out, s, n);
    out[n] = 13;
    out[n + 1] = 10;
    network_write(spec, out, n + 2);
}

/* Takes what has arrived, without waiting. 1 = a line is in `line`, 0 = none yet,
   -1 = the connection is gone. */
static signed char poll_line(void)
{
    static uint16_t waiting;
    static uint8_t connected, err;
    unsigned char i;
    int16_t r;

    for (;;) {
        for (i = 0; i < inlen; ++i) {
            if (inbuf[i] == 13 || inbuf[i] == 10) {
                memcpy(line, inbuf, i);
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
        if (k == CH_ENTER)
            break;
        if ((k == CH_DEL || k == CH_CURS_LEFT) && n) {
            --n;
            cputs("\b \b");
        } else if (k > ' ' && k < 0x7F && n + 1 < max) {
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
    unsigned char dir = 0, now, j, fire, was_fire = 0;
    char k;
    signed char r;

    cputs("\r\nUSE JOYSTICK OR ARROW KEYS,\r\nFIRE = BUTTON 0 OR SPACE\r\n");
    for (;;) {
        /* keys typed since the last pass, then the stick */
        now = 0;
        fire = 0;
        while (kbhit()) {
            k = cgetc();
            if (k == ' ') fire = 1;
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
        if (now && now != dir) {    /* only on a change: the server keeps the last one */
            dir = now;
            sprintf(out, "MOVE %c", dirs[dir - 1]);
            send_line(out);
        }
        /* FIRE once per press (Artillery; the other games ignore it) */
        if (JOY_BTN_1(j) && !was_fire)
            fire = 1;
        was_fire = JOY_BTN_1(j);
        if (fire)
            send_line("FIRE");

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
    cputs(" RETRO GAME BATTLE -\r\n CLASSIC COMPUTING 2026\r\n APPLE II FUJINET - TCP BUILD " BUILD "\r\n\r\n");
    cputs("ENTER YOUR PIN OR PRESS RETURN:\r\n? ");
    read_pin(pin, sizeof pin);
    if (!pin[0])
        strcpy(pin, "NONE");

    cputs("CONNECTING...\r\n");
    if (network_open(spec, OPEN_MODE_RW, OPEN_TRANS_NONE) != FN_ERR_OK) {
        cputs("NO CONNECTION TO " SERVER_HOST ":" SERVER_PORT "\r\n");
        network_close(spec);
        return;
    }
    inlen = 0;
    sprintf(line, "HELLO APPLE2 " PLAYER_NAME " APPLE2 %s", pin);
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
    clrscr();
    if (network_init() != FN_ERR_OK) {
        cputs("NO FUJINET FOUND.\r\n");
        cgetc();
        return 0;
    }
    have_joystick = joy_install(a2e_stdjoy_joy) == JOY_ERR_OK;
    for (;;) {
        session();
        cputs("\r\nPRESS A KEY TO PLAY AGAIN.");
        cgetc();
    }
}
