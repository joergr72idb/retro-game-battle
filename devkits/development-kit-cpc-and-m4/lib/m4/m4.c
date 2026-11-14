#include <string.h>
#include "m4.h"

#define C_VERSION       0x4326
#define C_HTTPGETMEM    0x4328
#define C_COPYBUF       0x4329
#define C_NETSOCKET     0x4331
#define C_NETCONNECT    0x4332
#define C_NETCLOSE      0x4333
#define C_NETSEND       0x4334
#define C_NETRECV       0x4335
#define C_NETHOSTIP     0x4336

#define ROM_RESPONSE    0xFF02  /* pointers in the M4 ROM (link table) */
#define ROM_SOCKINFO    0xFF06

#define CHUNK           0x400   /* bytes per C_COPYBUF / C_NETSEND / C_NETRECV */

__sfr __banked __at(0xFE00) M4_DATA;
__sfr __banked __at(0xFC00) M4_ACK;

static unsigned char m4_rom = M4_ERROR;
static unsigned char resp[CHUNK + 8];
static unsigned char req[CHUNK + 8];

/* previous ROM state from KL ROM SELECT, for KL ROM RESTORE */
unsigned char m4_old_b, m4_old_c;

static void rom_on(unsigned char rom) __naked __z88dk_fastcall
{
    (void)rom;
    __asm
        ld      c, l
        call    0xB90F              ; KL ROM SELECT: upper ROM c at &C000
        ld      a, b
        ld      (_m4_old_b), a
        ld      a, c
        ld      (_m4_old_c), a
        ret
    __endasm;
}

static void rom_off(void) __naked
{
    __asm
        ld      a, (_m4_old_b)
        ld      b, a
        ld      a, (_m4_old_c)
        ld      c, a
        jp      0xB90C              ; KL ROM RESTORE
    __endasm;
}

static unsigned int rom_word(unsigned int addr)
{
    return *(volatile unsigned char *)addr | (*(volatile unsigned char *)(addr + 1) << 8);
}

/* "M4 BOARD" with bit 7 set on the last letter, as in the ROM's RSX name table */
static const char m4_name[] = "M4 BOARD";

unsigned char m4_init(void)
{
    unsigned char r, i, ok;
    const volatile unsigned char *name;

    for (r = 127; r > 0; --r) {
        rom_on(r);
        ok = 0;
        if (*(volatile unsigned char *)0xC000 == 1) {       /* background ROM */
            name = (const volatile unsigned char *)rom_word(0xC004);
            for (i = 0; i < 8; ++i)
                if ((name[i] & 0x7F) != m4_name[i])
                    break;
            ok = i == 8 && (name[7] & 0x80);
        }
        rom_off();
        if (ok) {
            m4_rom = r;
            return 0;
        }
    }
    return M4_ERROR;
}

const unsigned char *m4_command(unsigned int cmd, const void *data, unsigned int len, unsigned int copy)
{
    const unsigned char *p = data;
    const volatile unsigned char *r;
    unsigned int i;

    M4_DATA = (unsigned char)(len + 2);     /* size byte, 8 bits even for longer commands */
    M4_DATA = (unsigned char)cmd;
    M4_DATA = (unsigned char)(cmd >> 8);
    for (i = 0; i < len; ++i)
        M4_DATA = p[i];
    M4_ACK = 0;                             /* the M4 runs the command */

    if (copy > sizeof resp - 3)
        copy = sizeof resp - 3;
    rom_on(m4_rom);
    r = (const volatile unsigned char *)rom_word(ROM_RESPONSE);
    for (i = 0; i < copy + 3; ++i)
        resp[i] = r[i];
    rom_off();
    return resp + 3;
}

void m4_version(char *out, unsigned char max)
{
    const unsigned char *r = m4_command(C_VERSION, 0, 0, max);
    unsigned char i;

    for (i = 0; i + 1 < max && r[i]; ++i)
        out[i] = r[i];
    out[i] = 0;
}

unsigned int m4_http_get(const char *url, unsigned char *buf, unsigned int max)
{
    const unsigned char *r;
    unsigned int n, got, part;
    unsigned int ulen = strlen(url);

    if (max > 0x4000)
        max = 0x4000;
    req[0] = (unsigned char)max;
    req[1] = (unsigned char)(max >> 8);
    memcpy(req + 2, url, ulen + 1);
    r = m4_command(C_HTTPGETMEM, req, ulen + 3, 2);
    got = r[0] | (r[1] << 8);
    if (got > max)
        got = max;

    for (n = 0; n < got; n += part) {       /* from the M4's buffer into ours */
        part = got - n > CHUNK ? CHUNK : got - n;
        req[0] = (unsigned char)n;
        req[1] = (unsigned char)(n >> 8);
        req[2] = (unsigned char)part;
        req[3] = (unsigned char)(part >> 8);
        r = m4_command(C_COPYBUF, req, 4, part);
        memcpy(buf + n, r, part);
    }
    return got;
}

unsigned char m4_sock_status(unsigned char s)
{
    unsigned char st;

    rom_on(m4_rom);
    st = *(volatile unsigned char *)(rom_word(ROM_SOCKINFO) + s * 16);
    rom_off();
    return st;
}

unsigned char m4_dns(const char *host, unsigned char *ip)
{
    const volatile unsigned char *info;
    unsigned char st, i;

    if (m4_command(C_NETHOSTIP, host, strlen(host) + 1, 1)[0] != 1)
        return M4_ERROR;
    while ((st = m4_sock_status(0)) == M4_DNS_LOOKUP)
        ;
    if (st != M4_IDLE)
        return M4_ERROR;
    rom_on(m4_rom);
    info = (const volatile unsigned char *)rom_word(ROM_SOCKINFO);
    for (i = 0; i < 4; ++i)
        ip[i] = info[4 + 3 - i];            /* the M4 keeps the last octet first */
    rom_off();
    return 0;
}

unsigned char m4_socket(void)
{
    static const unsigned char tcp[3] = { 0, 0, 6 };   /* domain, type, protocol = TCP */
    return m4_command(C_NETSOCKET, tcp, 3, 1)[0];
}

unsigned char m4_connect(unsigned char s, const unsigned char *ip, unsigned int port)
{
    unsigned char st;

    req[0] = s;
    req[1] = ip[3];                         /* last octet first */
    req[2] = ip[2];
    req[3] = ip[1];
    req[4] = ip[0];
    req[5] = (unsigned char)port;
    req[6] = (unsigned char)(port >> 8);
    if (m4_command(C_NETCONNECT, req, 7, 1)[0] != 0)
        return M4_ERROR;
    while ((st = m4_sock_status(s)) == M4_CONNECTING)
        ;
    return st == M4_IDLE ? 0 : M4_ERROR;
}

unsigned char m4_send(unsigned char s, const void *data, unsigned int len)
{
    const unsigned char *p = data;
    unsigned int part;

    while (len) {
        while (m4_sock_status(s) == M4_SENDING)
            ;
        part = len > CHUNK ? CHUNK : len;
        req[0] = s;
        req[1] = (unsigned char)part;
        req[2] = (unsigned char)(part >> 8);
        memcpy(req + 3, p, part);
        if (m4_command(C_NETSEND, req, part + 3, 1)[0] != 0)
            return M4_ERROR;
        p += part;
        len -= part;
    }
    return 0;
}

int m4_recv(unsigned char s, void *buf, unsigned int max)
{
    const unsigned char *r;
    unsigned int n;

    if (max > CHUNK)
        max = CHUNK;
    req[0] = s;
    req[1] = (unsigned char)max;
    req[2] = (unsigned char)(max >> 8);
    r = m4_command(C_NETRECV, req, 3, max + 3);
    n = r[1] | (r[2] << 8);
    if (n > max)
        n = max;
    memcpy(buf, r + 3, n);
    if (n == 0 && m4_sock_status(s) == M4_CLOSED)
        return -1;
    return (int)n;
}

void m4_close(unsigned char s)
{
    m4_command(C_NETCLOSE, &s, 1, 0);
}
