/* Retro Game Battle - the shared part of the C clients (Atari, C64, CPC, Apple II).
 *
 * The raw TCP protocol (docs/protocol.md): one connection for the whole match, MOVE only when
 * the stick changes, server lines read without blocking. The server compiled in is only the
 * default: the player can type another one at the start (HOST or HOST:PORT).
 *
 * The network goes through the fujinet-lib calls (network_open/status/read_nb/write/close);
 * everything that differs between the machines is in each client's platform.c (rgb_platform.h).
 */
#include <stdio.h>
#include <string.h>
#include "fujinet-network.h"
#include "rgb_platform.h"
#include "platform.h"

static char host[40] = SERVER_HOST;    /* the server, changeable at the start */
static char port[6] = SERVER_PORT;
static unsigned char ask = 1;           /* ask for the server before the next connection */
static char spec[64];                   /* "N:TCP://host:port/" (ASCII) */
static char line[200];                  /* the last complete line from the server */
static char inbuf[200];                 /* received bytes not yet split into lines (ASCII) */
static unsigned char inlen;
static char out[64];
static char text[64];

/* ---- character set ------------------------------------------------------------------- */

#ifdef PLAT_PETSCII
/* cc65 turns string literals into PETSCII, where upper and lower case are swapped against
   ASCII. Everything that goes over the wire is converted to ASCII first, everything that
   comes back to PETSCII - then the literals compare and print correctly. */
static unsigned char to_wire(unsigned char c)
{
    if (c >= 0xC1 && c <= 0xDA) return c - 0x80;    /* PETSCII A-Z -> ASCII A-Z */
    if (c >= 0x41 && c <= 0x5A) return c + 0x20;    /* PETSCII a-z -> ASCII a-z */
    return c;
}

static unsigned char from_wire(unsigned char c)
{
    if (c >= 0x61 && c <= 0x7A) return c - 0x20;    /* ASCII a-z -> PETSCII a-z */
    if (c >= 0x41 && c <= 0x5A) return c + 0x80;    /* ASCII A-Z -> PETSCII A-Z */
    return c;
}
#else
#define to_wire(c)   (c)
#define from_wire(c) (c)
#endif

/* ---- network ------------------------------------------------------------------------- */

static void send_line(const char *s)
{
    unsigned char n = 0;

    while (s[n]) {
        out[n] = to_wire(s[n]);
        ++n;
    }
    out[n] = 13;                        /* CR LF as numbers: '\n' is 13 in cc65's CBM charmap */
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
                    line[j] = from_wire(inbuf[j]);
                line[i] = 0;
                while (i < inlen && (inbuf[i] == 13 || inbuf[i] == 10))
                    ++i;
                inlen -= i;
                memmove(inbuf, inbuf + i, inlen);
                if (line[0])
                    return 1;
                i = 255;                /* empty line: look again from the start */
            }
        }
        if (inlen == sizeof inbuf)      /* a line longer than the buffer: drop it */
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

/* ---- screens ------------------------------------------------------------------------- */

static void read_text(char *in, unsigned char max)
{
    unsigned char n = 0, k;

    plat_cursor(1);
    for (;;) {
        k = plat_getkey();
        if (k == RGB_KEY_ENTER)
            break;
        if (k == RGB_KEY_DEL) {
            if (n) {
                --n;
                plat_print(PLAT_BACKSPACE);
            }
        } else if (k > ' ' && k < 0x7F && n + 1 < max) {
            in[n++] = k;
            plat_putc(k);
        }
    }
    plat_cursor(0);
    in[n] = 0;
    plat_print("\n");
}

/* Shows the server and lets the player keep it (RETURN) or type HOST or HOST:PORT */
static void ask_server(void)
{
    char in[sizeof host + sizeof port];
    char *p;

    sprintf(text, "SERVER: %s:%s\n", host, port);
    plat_print(text);
    plat_print(PLAT_ENTER " = OK, OR TYPE HOST[:PORT]:\n? ");
    read_text(in, sizeof in);
    if (!in[0])
        return;
    p = strchr(in, ':');
    if (p) {
        *p++ = 0;
        if (*p && strlen(p) < sizeof port)
            strcpy(port, p);
    }
    if (in[0] && strlen(in) < sizeof host)
        strcpy(host, in);
}

/* "WAIT GAMES <n> <id>:<label> ..." or "ERR INCOMPATIBLE GAMES <n> ...": show the list,
   send GAME <n> for the key pressed */
static void choose_game(const char *list)
{
    unsigned char count = 0, k;
    const char *p = list;

    plat_print("\nCHOOSE A GAME:\n");
    while (*p == ' ')
        ++p;
    while (*p && *p != ' ')             /* skip the count, number the entries ourselves */
        ++p;
    while (*p) {
        while (*p == ' ')
            ++p;
        if (!*p)
            break;
        sprintf(text, " %u = ", ++count);
        plat_print(text);
        while (*p && *p != ':' && *p != ' ')
            ++p;                        /* the id */
        if (*p == ':')
            ++p;
        while (*p && *p != ' ')
            plat_putc(*p++);            /* the label */
        plat_print("\n");
    }
    do
        k = plat_getkey();
    while (k < '1' || k > '0' + count);
    sprintf(text, "GAME %c", k);
    send_line(text);
    plat_print(text);
    plat_print("\n");
}

/* ---- the match ----------------------------------------------------------------------- */

static void play(void)
{
    static const char dirs[] = "UDLR";
    unsigned char dir = 0, now, fire;
    signed char r;

    plat_print("\n" PLAT_CONTROLS "\n");
    for (;;) {
        now = plat_input(&fire);
        if (now && now != dir) {        /* only on a change: the server keeps the last one */
            dir = now;
            sprintf(text, "MOVE %c", dirs[dir - 1]);
            send_line(text);
        }
        if (fire)                       /* Artillery; the other games ignore it */
            send_line("FIRE");

        r = poll_line();
        if (r < 0) {
            plat_print("\nCONNECTION CLOSED\n");
            return;
        }
        if (r > 0 && starts("END")) {
            plat_print("\nGAME OVER\n");
            plat_print(line);
            plat_print("\n");
            if (wait_line() > 0) {      /* STATS */
                plat_print(line);
                plat_print("\n");
            }
            return;
        }
    }
}

static void session(void)
{
    char pin[12];
    unsigned char started = 0;
    unsigned char i;

    plat_clear();
    plat_print(" RETRO GAME BATTLE -\n CLASSIC COMPUTING 2026\n " PLAT_TITLE "\n\n");
    if (ask) {
        ask_server();
        ask = 0;
    }
    plat_print("ENTER YOUR PIN OR PRESS " PLAT_ENTER ":\n? ");
    read_text(pin, sizeof pin);
    if (!pin[0])
        strcpy(pin, "NONE");

    plat_print("CONNECTING...\n");
    sprintf(line, "N:TCP://%s:%s/", host, port);
    for (i = 0; (spec[i] = to_wire(line[i])) != 0; ++i)
        ;
    if (network_open(spec, OPEN_MODE_RW, OPEN_TRANS_NONE) != FN_ERR_OK) {
        sprintf(line, "NO CONNECTION TO %s:%s\n", host, port);
        plat_print(line);
        network_close(spec);
        ask = 1;
        return;
    }
    inlen = 0;
    sprintf(line, "HELLO " PLAT_ID " " PLAYER_NAME " " PLAT_ID " %s", pin);
    send_line(line);

    while (!started) {
        if (wait_line() < 0) {
            plat_print("CONNECTION CLOSED\n");
            break;
        }
        if (starts("WAIT GAMES"))
            choose_game(line + 10);
        else if (starts("ERR INCOMPATIBLE GAMES"))
            choose_game(line + 22);
        else if (starts("WAIT"))
            plat_print("WAITING FOR OPPONENT...\n");
        else if (starts("START")) {
            started = 1;
            play();
        } else if (starts("ERR")) {
            plat_print(line);
            plat_print("\n");
            break;
        }
    }
    send_line("BYE");
    network_close(spec);
}

int main(void)
{
    unsigned char k;

    plat_clear();
    if (!plat_init()) {
        plat_print(PLAT_NO_DEVICE "\n");
        plat_getkey();
        return 0;
    }
    for (;;) {
        session();
        plat_print("\nPRESS A KEY TO PLAY AGAIN,\nS = OTHER SERVER.");
        k = plat_getkey();
        if (k == 's' || k == 'S')
            ask = 1;
    }
}
