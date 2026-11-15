/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 joergr72idb - see LICENSE */
/* fujinet-network for the CPC + M4: see include/fujinet-network.h */
#include <string.h>
#include "fujinet-network.h"
#include "m4.h"

#define UNITS   4               /* the M4 has 4 sockets */
#ifndef FN_RXBUF
#define FN_RXBUF 256            /* receive buffer per unit: what network_status reports as waiting */
#endif

typedef struct {
    uint8_t sock;               /* M4 socket, 0 = unit not open */
    uint8_t closed;             /* the other side closed the connection (or it failed) */
    uint16_t start, len;        /* buffered bytes: rx[start .. start+len) */
    uint8_t rx[FN_RXBUF];
} Unit;

NetworkStatus nw_status;

static Unit units[UNITS];
static uint8_t ready;           /* network_init found the M4 */

FN_ERR network_init(void)
{
    if (!ready && m4_init() == 0)
        ready = 1;
    return ready ? FN_ERR_OK : FN_ERR_NO_DEVICE;
}

uint8_t network_unit(const char *devicespec)
{
    if (devicespec[0] != 'N' && devicespec[0] != 'n')
        return 0;
    if (devicespec[1] == ':')
        return 1;
    if (devicespec[1] >= '1' && devicespec[1] <= '9' && devicespec[2] == ':')
        return devicespec[1] - '0';
    return 0;
}

/* the open unit of a devicespec, or 0 (nw_status says why) */
static Unit *open_unit(const char *devicespec)
{
    uint8_t n = network_unit(devicespec);

    if (n == 0 || n > UNITS || !units[n - 1].sock) {
        nw_status.avail = 0;
        nw_status.status = 0;
        nw_status.errcode = NETWORK_ERROR_NOT_CONNECTED;
        return 0;
    }
    return &units[n - 1];
}

/* takes what the M4 has into the unit's buffer, without waiting */
static void fill(Unit *u)
{
    int r;

    if (u->closed || u->len == FN_RXBUF)
        return;
    if (u->start) {
        memmove(u->rx, u->rx + u->start, u->len);
        u->start = 0;
    }
    r = m4_recv(u->sock, u->rx + u->len, FN_RXBUF - u->len);
    if (r < 0 || (r == 0 && m4_sock_status(u->sock) >= 240))
        u->closed = 1;
    else
        u->len += r;
}

static void set_status(const Unit *u)
{
    nw_status.avail = u->len;
    nw_status.status = !u->closed;
    nw_status.errcode = u->closed && !u->len ? NETWORK_ERROR_END_OF_FILE : NETWORK_SUCCESS;
}

static FN_ERR fail(FN_ERR e, uint8_t code)
{
    nw_status.avail = 0;
    nw_status.status = 0;
    nw_status.errcode = code;
    return e;
}

static uint8_t lower(uint8_t c)
{
    return c >= 'A' && c <= 'Z' ? c + 32 : c;
}

/* "a.b.c.d" -> ip, 1 = it was one */
static uint8_t parse_ip(const char *s, uint8_t *ip)
{
    uint8_t i;
    uint16_t v;

    for (i = 0; i < 4; ++i) {
        if (*s < '0' || *s > '9')
            return 0;
        for (v = 0; *s >= '0' && *s <= '9'; ++s)
            if ((v = v * 10 + (*s - '0')) > 255)
                return 0;
        ip[i] = (uint8_t)v;
        if (i < 3 && *s++ != '.')
            return 0;
    }
    return *s == 0;
}

FN_ERR network_open(const char *devicespec, uint8_t mode, uint8_t trans)
{
    static const char tcp[] = "tcp://";
    char host[64];
    uint8_t ip[4], n, i, s;
    uint16_t port;
    const char *p;
    Unit *u;

    n = network_unit(devicespec);
    if (n == 0 || n > UNITS)
        return fail(FN_ERR_BAD_CMD, NETWORK_ERROR_INVALID_DEVICESPEC);
    if (network_init() != FN_ERR_OK)
        return fail(FN_ERR_NO_DEVICE, NETWORK_ERROR_NO_DEVICE_AVAILABLE);
    if (trans != OPEN_TRANS_NONE
        || (mode != OPEN_MODE_READ && mode != OPEN_MODE_WRITE && mode != OPEN_MODE_RW))
        return fail(FN_ERR_BAD_CMD, NETWORK_ERROR_NOT_IMPLEMENTED);

    p = strchr(devicespec, ':') + 1;
    for (i = 0; tcp[i]; ++i)
        if (lower(p[i]) != tcp[i])
            return fail(FN_ERR_BAD_CMD, NETWORK_ERROR_NOT_IMPLEMENTED);
    p += i;
    for (i = 0; *p && *p != ':' && *p != '/'; ++p) {
        if (i == sizeof host - 1)
            return fail(FN_ERR_BAD_CMD, NETWORK_ERROR_INVALID_DEVICESPEC);
        host[i++] = *p;
    }
    host[i] = 0;
    if (i == 0 || *p++ != ':' || *p < '0' || *p > '9')   /* listening (no host) not supported */
        return fail(FN_ERR_BAD_CMD, NETWORK_ERROR_INVALID_DEVICESPEC);
    for (port = 0; *p >= '0' && *p <= '9'; ++p)
        port = port * 10 + (*p - '0');
    if (*p && *p != '/')
        return fail(FN_ERR_BAD_CMD, NETWORK_ERROR_INVALID_DEVICESPEC);

    network_close(devicespec);
    if (!parse_ip(host, ip) && m4_dns(host, ip) != 0)
        return fail(FN_ERR_IO_ERROR, NETWORK_ERROR_NETWORK_UNREACHABLE);
    s = m4_socket();
    if (s == M4_ERROR || s == 0)
        return fail(FN_ERR_IO_ERROR, NETWORK_ERROR_NO_DEVICE_AVAILABLE);
    if (m4_connect(s, ip, port) != 0) {
        m4_close(s);
        return fail(FN_ERR_IO_ERROR, NETWORK_ERROR_CONNECTION_REFUSED);
    }
    u = &units[n - 1];
    u->sock = s;
    u->closed = 0;
    u->start = u->len = 0;
    set_status(u);
    return FN_ERR_OK;
}

FN_ERR network_close(const char *devicespec)
{
    uint8_t n = network_unit(devicespec);

    if (n == 0 || n > UNITS)
        return fail(FN_ERR_BAD_CMD, NETWORK_ERROR_INVALID_DEVICESPEC);
    if (units[n - 1].sock) {
        m4_close(units[n - 1].sock);
        units[n - 1].sock = 0;
    }
    return FN_ERR_OK;
}

FN_ERR network_status(const char *devicespec, uint16_t *bw, uint8_t *c, uint8_t *err)
{
    Unit *u = open_unit(devicespec);
    FN_ERR e = FN_ERR_IO_ERROR;

    if (u) {
        fill(u);
        set_status(u);
        e = FN_ERR_OK;
    }
    *bw = nw_status.avail;
    *c = nw_status.status;
    *err = nw_status.errcode;
    return e;
}

/* copies up to len buffered bytes */
static uint16_t take(Unit *u, uint8_t *buf, uint16_t len)
{
    if (len > u->len)
        len = u->len;
    memcpy(buf, u->rx + u->start, len);
    u->start += len;
    u->len -= len;
    if (!u->len)
        u->start = 0;
    return len;
}

int16_t network_read_nb(const char *devicespec, void *buf, uint16_t len)
{
    Unit *u = open_unit(devicespec);
    uint16_t got;

    if (!u)
        return -(int16_t)FN_ERR_IO_ERROR;
    if (len > 0x7FFF)
        len = 0x7FFF;
    do
        fill(u);
    while (!u->len && !u->closed);
    got = take(u, buf, len);
    set_status(u);
    return (int16_t)got;
}

int16_t network_read(const char *devicespec, void *buf, uint16_t len)
{
    Unit *u = open_unit(devicespec);
    uint16_t got = 0;

    if (!u)
        return -(int16_t)FN_ERR_IO_ERROR;
    if (len > 0x7FFF)
        len = 0x7FFF;
    while (got < len) {
        fill(u);
        if (!u->len && u->closed)
            break;
        got += take(u, (uint8_t *)buf + got, len - got);
    }
    set_status(u);
    return (int16_t)got;
}

FN_ERR network_write(const char *devicespec, const void *buf, uint16_t len)
{
    Unit *u = open_unit(devicespec);

    if (!u)
        return FN_ERR_IO_ERROR;
    if (m4_send(u->sock, buf, len) != 0)
        return fail(FN_ERR_IO_ERROR, NETWORK_ERROR_GENERAL);
    return FN_ERR_OK;
}
