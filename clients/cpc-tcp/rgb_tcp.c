/* Retro Game Battle - Schneider CPC client over raw TCP (M4 board).
 *
 * The same protocol as the Atari/FujiNet client (docs/protocol.md, "raw TCP"): one connection
 * for the whole match, MOVE only when the stick changes, server lines read without blocking.
 * The BASIC client polls over HTTP (|HTTPMEM), which blocks the CPC for each request (~550 ms
 * per tick on real hardware, the server runs 250 ms ticks); here steering never waits for the
 * network.
 *
 * Built with the CPC kit (SDCC + lib/m4): make SERVER=192.168.17.138, see Makefile.
 * The server compiled in is only the default: the player can type another one at the start
 * (an IP address or a host name, which the M4 looks up).
 */
#include <stdio.h>
#include <string.h>
#include "cpc.h"
#include "m4.h"

#ifndef SERVER_HOST
#define SERVER_HOST "127.0.0.1"
#endif
#ifndef SERVER_PORT
#define SERVER_PORT 6502
#endif
#ifndef PLAYER_NAME
#define PLAYER_NAME "SCHNEIDERCPC"
#endif

#define BUILD       "2"

static char host[40] = SERVER_HOST;     /* the server, changeable at the start */
static unsigned int port = SERVER_PORT;
static unsigned char ask = 1;           /* ask for the server before the next connection */
static unsigned char server[4];         /* host as an IP address */
static unsigned char sock;

static char line[256];              /* the last complete line from the server */
static unsigned char inbuf[256];    /* received bytes not yet split into lines */
static unsigned int inlen;
static char out[64];

/* ---- firmware ---------------------------------------------------------------------- */

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

static void mode1(void) __naked
{
    __asm
        ld      a, #1
        jp      0xBC0E              ; SCR SET MODE
    __endasm;
}

/* ---- network ----------------------------------------------------------------------- */

static void send_line(const char *s)
{
    unsigned char n = strlen(s);

    memcpy(out, s, n);
    out[n] = '\r';
    out[n + 1] = '\n';
    m4_send(sock, out, n + 2);
}

/* Takes what has arrived, without waiting. 1 = a line is in `line`, 0 = none yet,
   -1 = the server closed the connection. */
static signed char poll_line(void)
{
    unsigned int i;
    int r;

    for (;;) {
        for (i = 0; i < inlen; ++i) {
            if (inbuf[i] == '\r' || inbuf[i] == '\n') {
                memcpy(line, inbuf, i);
                line[i] = 0;
                while (i < inlen && (inbuf[i] == '\r' || inbuf[i] == '\n'))
                    ++i;
                inlen -= i;
                memmove(inbuf, inbuf + i, inlen);
                if (line[0])
                    return 1;
                i = (unsigned int)-1;   /* empty line: look again from the start */
            }
        }
        if (inlen == sizeof inbuf)      /* a line longer than the buffer: drop it */
            inlen = 0;
        r = m4_recv(sock, inbuf + inlen, sizeof inbuf - inlen);
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

static void read_text(char *pin, unsigned char max)
{
    unsigned char n = 0, k;

    for (;;) {
        k = cpc_getkey();
        if (k == 13)
            break;
        if (k == 127 && n) {            /* DEL */
            --n;
            cpc_print("\x08 \x08");
        } else if (k > ' ' && k < 127 && n + 1 < max) {
            pin[n++] = k;
            putchar(k);
        }
    }
    pin[n] = 0;
    putchar('\n');
}

/* a decimal number, 0 if there is anything else in s or it is larger than max */
static unsigned int number(const char *s, unsigned int max)
{
    unsigned long n = 0;

    if (!*s)
        return 0;
    for (; *s; ++s) {
        if (*s < '0' || *s > '9')
            return 0;
        n = n * 10 + (*s - '0');
        if (n > max)
            return 0;
    }
    return n;
}

/* "a.b.c.d" -> ip; 0 if host is not an IP address */
static unsigned char parse_ip(const char *s, unsigned char *ip)
{
    char part[4];
    unsigned char i, n;

    for (i = 0; i < 4; ++i) {
        for (n = 0; *s >= '0' && *s <= '9' && n < 3; )
            part[n++] = *s++;
        part[n] = 0;
        if (!n || *s != (i < 3 ? '.' : 0))
            return 0;
        if (part[0] != '0' && !number(part, 255))
            return 0;
        ip[i] = number(part, 255);
        if (i < 3)
            ++s;
    }
    return 1;
}

/* Shows the server and lets the player keep it (ENTER) or type HOST or HOST:PORT */
static void ask_server(void)
{
    char in[sizeof host + 6];
    char *p;
    unsigned int n;

    printf("SERVER: %s:%u\nENTER = OK, OR TYPE HOST[:PORT]:\n? ", host, port);
    read_text(in, sizeof in);
    if (!in[0])
        return;
    p = strchr(in, ':');
    if (p) {
        *p++ = 0;
        n = number(p, 65535);
        if (n)
            port = n;
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

    printf("\nCHOOSE A GAME:\n");
    while (*p == ' ')
        ++p;
    while (*p && *p != ' ')             /* skip the count, number the entries ourselves */
        ++p;
    while (*p) {
        while (*p == ' ')
            ++p;
        if (!*p)
            break;
        printf(" %u = ", ++count);
        while (*p && *p != ':' && *p != ' ')
            ++p;                        /* the id */
        if (*p == ':')
            ++p;
        while (*p && *p != ' ')
            putchar(*p++);              /* the label */
        putchar('\n');
    }
    do
        k = cpc_getkey();
    while (k < '1' || k > '0' + count);
    sprintf(out, "GAME %c", k);
    send_line(out);
    printf("GAME %c\n", k);
}

/* ---- the match --------------------------------------------------------------------- */

static void play(void)
{
    static const char dirs[] = "UDLR";
    unsigned char dir = 0, now, j, k, i, fire, was_fire = 0;
    signed char r;

    printf("\nUSE JOYSTICK, FIRE = BUTTON OR SPACE\n");
    for (;;) {
        /* the stick (and cursor keys / joystick codes queued in the key buffer) */
        now = 0;
        fire = 0;
        while ((k = read_key()) != 0) {
            if (k == ' ' || k == 224 || k == 225) fire = 1;     /* space, joystick fire codes */
            else if (k == 240 || k == 11) now = 1;
            else if (k == 241 || k == 10) now = 2;
            else if (k == 242 || k == 8) now = 3;
            else if (k == 243 || k == 9) now = 4;
        }
        j = joystick();
        for (i = 0; i < 4; ++i)
            if (j & (1 << i))
                now = i + 1;
        if (now && now != dir) {        /* only on a change: the server keeps the last one */
            dir = now;
            sprintf(out, "MOVE %c", dirs[dir - 1]);
            send_line(out);
        }
        /* FIRE once per press (Artillery; the other games ignore it) */
        if ((j & 0x30) && !was_fire)
            fire = 1;
        was_fire = j & 0x30;
        if (fire)
            send_line("FIRE");

        r = poll_line();
        if (r < 0) {
            printf("\nCONNECTION CLOSED\n");
            return;
        }
        if (r > 0 && (starts("END") || starts("end"))) {
            printf("\nGAME OVER\n%s\n", line);
            if (wait_line() > 0)        /* STATS */
                printf("%s\n", line);
            return;
        }
    }
}

static void session(void)
{
    char pin[12];
    unsigned char started = 0;

    mode1();
    printf(" RETRO GAME BATTLE -\n CLASSIC COMPUTING 2026\n SCHNEIDER CPC - TCP BUILD " BUILD "\n\n");
    if (ask) {
        ask_server();
        ask = 0;
    }
    printf("ENTER YOUR PIN OR PRESS ENTER:\n? ");
    read_text(pin, sizeof pin);
    if (!pin[0])
        strcpy(pin, "NONE");

    printf("CONNECTING...\n");
    if (!parse_ip(host, server) && m4_dns(host, server) != 0) {
        printf("UNKNOWN HOST %s\n", host);
        ask = 1;
        return;
    }
    sock = m4_socket();
    if (sock == M4_ERROR || m4_connect(sock, server, port) != 0) {
        printf("NO CONNECTION TO %u.%u.%u.%u:%u\n", server[0], server[1], server[2], server[3], port);
        if (sock != M4_ERROR)
            m4_close(sock);
        ask = 1;
        return;
    }
    inlen = 0;
    sprintf(line, "HELLO CPC " PLAYER_NAME " CPC %s", pin);
    send_line(line);

    while (!started) {
        if (wait_line() < 0) {
            printf("CONNECTION CLOSED\n");
            break;
        }
        if (starts("WAIT GAMES"))
            choose_game(line + 10);
        else if (starts("ERR INCOMPATIBLE GAMES"))
            choose_game(line + 22);
        else if (starts("WAIT"))
            printf("WAITING FOR OPPONENT...\n");
        else if (starts("START")) {
            started = 1;
            play();
        } else if (starts("ERR")) {
            printf("%s\n", line);
            break;
        }
    }
    send_line("BYE");
    m4_close(sock);
}

int main(void)
{
    unsigned char k;

    if (m4_init() != 0) {
        printf("NO M4 BOARD FOUND.\n");
        cpc_getkey();
        return 0;
    }
    for (;;) {
        session();
        printf("\nPRESS A KEY TO PLAY AGAIN,\nS = OTHER SERVER.");
        k = cpc_getkey();
        if (k == 's' || k == 'S')
            ask = 1;
    }
}
