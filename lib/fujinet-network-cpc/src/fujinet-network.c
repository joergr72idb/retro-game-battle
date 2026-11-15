/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 joergr72idb - see LICENSE */
/* fujinet-network for the CPC + M4: see include/fujinet-network.h
 *
 * TCP: one M4 socket per unit, received bytes go through the unit's buffer.
 * HTTP: an HTTP/1.0 client on the same socket (the M4 has no TLS, so no HTTPS). network_open
 * connects and sends the request line and the standard headers; headers added by the program
 * are sent as they are written. The rest of the request (Content-Length, the empty line, the
 * POST data) goes out with the first status/read in HTTP_CHAN_MODE_BODY - as on the FujiNet,
 * where the request also runs then. POST data written in HTTP_CHAN_MODE_POST_SET_DATA waits in
 * the unit's receive buffer, which is unused until the response comes. The response head is
 * read and dropped (only the status code is kept); the body is then read like a TCP stream up
 * to the end of the connection ("Connection: close", so no chunked transfer).
 *
 * -DFN_NO_HTTP leaves HTTP out (about 3 KB less): HTTP opens return NOT_IMPLEMENTED as for
 * other protocols, and the network_http_* functions are not there (a program using them
 * does not link).
 */
#include <string.h>
#include "fujinet-network.h"
#include "m4.h"

#define UNITS   4               /* the M4 has 4 sockets */
#ifndef FN_RXBUF
#define FN_RXBUF 256            /* receive buffer per unit: what network_status reports as waiting */
#endif

/* method: 0 = a TCP unit */
enum { M_TCP, M_GET, M_POST, M_PUT, M_DELETE };
/* HTTP state */
enum { H_NONE, H_REQUEST, H_HEAD, H_BODY };

typedef struct {
    uint8_t sock;               /* M4 socket, 0 = unit not open */
    uint8_t closed;             /* the other side closed the connection (or it failed) */
    uint8_t method;             /* M_TCP or the HTTP method */
    uint8_t state;              /* HTTP: H_REQUEST (request not finished), H_HEAD (response head), H_BODY */
    uint8_t chan;               /* HTTP channel mode */
    uint8_t err;                /* NETWORK_SUCCESS, or the error from the HTTP status */
    uint8_t first;              /* response head: the status line is still to come */
    uint8_t skip;               /* response head: inside a line longer than the buffer */
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

static FN_ERR fail(FN_ERR e, uint8_t code)
{
    nw_status.avail = 0;
    nw_status.status = 0;
    nw_status.errcode = code;
    return e;
}

/* the open unit of a devicespec, or 0 (nw_status says why) */
static Unit *open_unit(const char *devicespec)
{
    uint8_t n = network_unit(devicespec);

    if (n == 0 || n > UNITS || !units[n - 1].sock) {
        fail(FN_ERR_IO_ERROR, NETWORK_ERROR_NOT_CONNECTED);
        return 0;
    }
    return &units[n - 1];
}

#ifndef FN_NO_HTTP

/* ---- sending text in small pieces ---------------------------------------------------- */

static uint8_t tx[64], txn, txsock, txerr;

static void out_flush(void)
{
    if (txn && m4_send(txsock, tx, txn) != 0)
        txerr = 1;
    txn = 0;
}

static void out_c(char c)
{
    tx[txn++] = c;
    if (txn == sizeof tx)
        out_flush();
}

static void out_s(const char *s)
{
    while (*s)
        out_c(*s++);
}

static void out_u(uint16_t v)
{
    char d[5];
    uint8_t i = 0;

    do
        d[i++] = '0' + v % 10;
    while (v /= 10);
    while (i)
        out_c(d[--i]);
}

/* ---- HTTP response -------------------------------------------------------------------- */

/* the device error for an HTTP status code, as the FujiNet firmware maps it */
static uint8_t http_error(uint16_t code)
{
    if (code >= 200 && code < 300)
        return NETWORK_SUCCESS;
    switch (code) {
    case 401: case 402: case 403: case 407:
        return NETWORK_ERROR_INVALID_USERNAME_OR_PASSWORD;
    case 404: case 410:
        return NETWORK_ERROR_FILE_NOT_FOUND;
    case 405:
        return NETWORK_ERROR_NOT_IMPLEMENTED;
    case 408:
        return NETWORK_ERROR_GENERAL_TIMEOUT;
    case 423: case 451:
        return NETWORK_ERROR_ACCESS_DENIED;
    }
    if (code >= 400 && code < 500)
        return NETWORK_ERROR_CLIENT_GENERAL;
    if (code >= 500 && code < 600)
        return NETWORK_ERROR_SERVER_GENERAL;
    return NETWORK_ERROR_GENERAL;       /* 1xx, 3xx (redirects are not followed), garbage */
}

/* works through the response head in the buffer: the status line, then the header lines up
   to the empty one. What follows it is the body. */
static void head(Unit *u)
{
    uint8_t *b;
    uint16_t nl, n, j, code;

    while (u->state == H_HEAD) {
        b = u->rx + u->start;
        for (nl = 0; nl < u->len && b[nl] != '\n'; ++nl)
            ;
        if (nl == u->len) {             /* no complete line yet */
            if (u->len == FN_RXBUF) {   /* a line longer than the buffer: drop it */
                u->skip = 1;
                u->start = u->len = 0;
            }
            return;
        }
        n = nl && b[nl - 1] == '\r' ? nl - 1 : nl;     /* the line without CR LF */
        if (u->skip) {                  /* the end of a dropped line */
            u->skip = 0;
            u->first = 0;
        } else if (u->first) {          /* "HTTP/1.1 200 OK" */
            u->first = 0;
            for (j = 0; j < n && b[j] != ' '; ++j)
                ;
            for (++j, code = 0; j < n && b[j] >= '0' && b[j] <= '9'; ++j)
                code = code * 10 + (b[j] - '0');
            u->err = http_error(code);
        } else if (n == 0)
            u->state = H_BODY;
        ++nl;                           /* the line and its '\n' */
        u->start += nl;
        u->len -= nl;
        if (!u->len)
            u->start = 0;
    }
}

/* ends the request: Content-Length and the data for POST/PUT, the empty line. 0 = sent */
static uint8_t finish_request(Unit *u, const uint8_t *data, uint16_t len)
{
    txsock = u->sock;
    txerr = 0;
    if (u->method == M_POST || u->method == M_PUT) {
        out_s("Content-Length: ");
        out_u(len);
        out_s("\r\n");
    }
    out_s("\r\n");
    out_flush();
    if (len && !txerr && m4_send(u->sock, data, len) != 0)
        txerr = 1;
    u->start = u->len = 0;              /* the POST data may have been in rx */
    u->chan = HTTP_CHAN_MODE_BODY;
    u->state = H_HEAD;
    u->first = 1;
    u->skip = 0;
    u->err = NETWORK_SUCCESS;
    if (txerr) {
        u->closed = 1;
        u->state = H_BODY;
        u->err = NETWORK_ERROR_GENERAL;
    }
    return txerr;
}

/* HTTP: the request goes out with the first status/read in body mode */
static void start_response(Unit *u)
{
    if (u->state == H_REQUEST && u->chan == HTTP_CHAN_MODE_BODY)
        finish_request(u, u->rx, u->len);
}

#else
#define start_response(u)
#endif /* FN_NO_HTTP */

/* ---- receiving ------------------------------------------------------------------------ */

/* takes what the M4 has into the unit's buffer, without waiting (HTTP: through the head) */
static void fill(Unit *u)
{
    int r;

#ifndef FN_NO_HTTP
    if (u->state == H_REQUEST)
        return;
    do {
#endif
        if (u->closed || u->len == FN_RXBUF)
#ifndef FN_NO_HTTP
            break;                      /* on to the end-of-head check */
#else
            return;
#endif
        if (u->start) {
            memmove(u->rx, u->rx + u->start, u->len);
            u->start = 0;
        }
        r = m4_recv(u->sock, u->rx + u->len, FN_RXBUF - u->len);
        if (r < 0 || (r == 0 && m4_sock_status(u->sock) >= 240))
            u->closed = 1;
        else
            u->len += r;
#ifndef FN_NO_HTTP
        if (u->state == H_HEAD)
            head(u);
    } while (u->state == H_HEAD && r > 0);
    if (u->state == H_HEAD && u->closed) {      /* closed before the end of the head */
        u->state = H_BODY;
        u->start = u->len = 0;
        if (u->first)
            u->err = NETWORK_ERROR_GENERAL;
    }
#endif
}

#ifndef FN_NO_HTTP
/* bytes the program can read: the body only */
static uint16_t avail(const Unit *u)
{
    return u->state == H_REQUEST || u->state == H_HEAD ? 0 : u->len;
}
#else
#define avail(u) ((u)->len)
#endif

static void set_status(const Unit *u)
{
    uint16_t n = avail(u);

    nw_status.avail = n;
    nw_status.status = !u->closed;
#ifndef FN_NO_HTTP
    if (u->state == H_REQUEST || u->state == H_HEAD)
        nw_status.errcode = NETWORK_SUCCESS;
    else if (u->err != NETWORK_SUCCESS)
        nw_status.errcode = u->err;     /* HTTP error status: stays, as on the FujiNet */
    else
#endif
        nw_status.errcode = u->closed && !n ? NETWORK_ERROR_END_OF_FILE : NETWORK_SUCCESS;
}

/* ---- open/close ----------------------------------------------------------------------- */

static uint8_t lower(uint8_t c)
{
    return c >= 'A' && c <= 'Z' ? c + 32 : c;
}

/* length of a case-insensitive prefix of s, or 0 */
static uint8_t prefix(const char *s, const char *pre)
{
    uint8_t i;

    for (i = 0; pre[i]; ++i)
        if (lower(s[i]) != pre[i])
            return 0;
    return i;
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

#ifndef FN_NO_HTTP
/* the HTTP method of an open mode, or M_TCP when not supported */
static uint8_t http_method(uint8_t mode)
{
    switch (mode) {
    case OPEN_MODE_HTTP_GET:
    case OPEN_MODE_HTTP_GET_H:
        return M_GET;
    case OPEN_MODE_HTTP_POST:
        return M_POST;
    case OPEN_MODE_HTTP_PUT_H:
        return M_PUT;
    case OPEN_MODE_HTTP_DELETE:
    case OPEN_MODE_HTTP_DELETE_H:
        return M_DELETE;
    }
    return M_TCP;
}
#endif

FN_ERR network_open(const char *devicespec, uint8_t mode, uint8_t trans)
{
#ifndef FN_NO_HTTP
    static const char *const names[] = { "", "GET ", "POST ", "PUT ", "DELETE " };
#endif
    char host[64];
    uint8_t ip[4], n, i, s, method;
    uint16_t port;
    const char *p;
    Unit *u;

    n = network_unit(devicespec);
    if (n == 0 || n > UNITS)
        return fail(FN_ERR_BAD_CMD, NETWORK_ERROR_INVALID_DEVICESPEC);
    if (network_init() != FN_ERR_OK)
        return fail(FN_ERR_NO_DEVICE, NETWORK_ERROR_NO_DEVICE_AVAILABLE);
    if (trans != OPEN_TRANS_NONE)
        return fail(FN_ERR_BAD_CMD, NETWORK_ERROR_NOT_IMPLEMENTED);

    p = strchr(devicespec, ':') + 1;
    if ((i = prefix(p, "tcp://")) != 0) {
        if (mode != OPEN_MODE_READ && mode != OPEN_MODE_WRITE && mode != OPEN_MODE_RW)
            return fail(FN_ERR_BAD_CMD, NETWORK_ERROR_NOT_IMPLEMENTED);
        method = M_TCP;
#ifndef FN_NO_HTTP
    } else if ((i = prefix(p, "http://")) != 0) {
        if ((method = http_method(mode)) == M_TCP)
            return fail(FN_ERR_BAD_CMD, NETWORK_ERROR_NOT_IMPLEMENTED);
#endif
    } else                              /* HTTPS, UDP, TNFS, ... */
        return fail(FN_ERR_BAD_CMD, NETWORK_ERROR_NOT_IMPLEMENTED);
    p += i;

    for (i = 0; *p && *p != ':' && *p != '/'; ++p) {
        if (i == sizeof host - 1)
            return fail(FN_ERR_BAD_CMD, NETWORK_ERROR_INVALID_DEVICESPEC);
        host[i++] = *p;
    }
    host[i] = 0;
    if (i == 0)                         /* listening (no host) not supported */
        return fail(FN_ERR_BAD_CMD, NETWORK_ERROR_INVALID_DEVICESPEC);
    port = 80;
    if (*p == ':') {
        if (*++p < '0' || *p > '9')
            return fail(FN_ERR_BAD_CMD, NETWORK_ERROR_INVALID_DEVICESPEC);
        for (port = 0; *p >= '0' && *p <= '9'; ++p)
            port = port * 10 + (*p - '0');
    }
#ifndef FN_NO_HTTP
    else if (method == M_TCP)           /* TCP needs a port, HTTP has 80 */
#else
    else
#endif
        return fail(FN_ERR_BAD_CMD, NETWORK_ERROR_INVALID_DEVICESPEC);
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

#ifndef FN_NO_HTTP
    if (method != M_TCP) {              /* request line and standard headers */
        txsock = s;
        txerr = 0;
        out_s(names[method]);
        if (!*p)
            out_c('/');
        for (; *p; ++p)
            if (*p == ' ')
                out_s("%20");
            else
                out_c(*p);
        out_s(" HTTP/1.0\r\nHost: ");
        out_s(host);
        if (port != 80) {
            out_c(':');
            out_u(port);
        }
        out_s("\r\nUser-Agent: fujinet-network-cpc\r\nConnection: close\r\n");
        out_flush();
        if (txerr) {
            m4_close(s);
            return fail(FN_ERR_IO_ERROR, NETWORK_ERROR_GENERAL);
        }
    }
#endif

    u = &units[n - 1];
    u->sock = s;
    u->closed = 0;
    u->method = method;
#ifndef FN_NO_HTTP
    u->state = method == M_TCP ? H_NONE : H_REQUEST;
#else
    u->state = H_NONE;
#endif
    u->chan = HTTP_CHAN_MODE_BODY;
    u->err = NETWORK_SUCCESS;
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

/* ---- status, read, write -------------------------------------------------------------- */

FN_ERR network_status(const char *devicespec, uint16_t *bw, uint8_t *c, uint8_t *err)
{
    Unit *u = open_unit(devicespec);
    FN_ERR e = FN_ERR_IO_ERROR;

    if (u) {
        start_response(u);
        fill(u);
        set_status(u);
        e = FN_ERR_OK;
    }
    *bw = nw_status.avail;
    *c = nw_status.status;
    *err = nw_status.errcode;
    return e;
}

/* copies up to len buffered body bytes */
static uint16_t take(Unit *u, uint8_t *buf, uint16_t len)
{
    uint16_t n = avail(u);

    if (len > n)
        len = n;
    memcpy(buf, u->rx + u->start, len);
    u->start += len;
    u->len -= len;
    if (!u->len)
        u->start = 0;
    return len;
}

/* the open unit, ready to read (an HTTP request is sent first); 0 if not */
static Unit *read_unit(const char *devicespec)
{
    Unit *u = open_unit(devicespec);

#ifndef FN_NO_HTTP
    if (u) {
        start_response(u);
        if (u->state == H_REQUEST) {    /* still adding headers or POST data */
            fail(FN_ERR_IO_ERROR, NETWORK_ERROR_WRITE_ONLY);
            return 0;
        }
    }
#endif
    return u;
}

int16_t network_read_nb(const char *devicespec, void *buf, uint16_t len)
{
    Unit *u = read_unit(devicespec);
    uint16_t got;

    if (!u)
        return -(int16_t)FN_ERR_IO_ERROR;
    if (len > 0x7FFF)
        len = 0x7FFF;
    do
        fill(u);
    while (!avail(u) && !u->closed);
    got = take(u, buf, len);
    set_status(u);
    return (int16_t)got;
}

int16_t network_read(const char *devicespec, void *buf, uint16_t len)
{
    Unit *u = read_unit(devicespec);
    uint16_t got = 0;

    if (!u)
        return -(int16_t)FN_ERR_IO_ERROR;
    if (len > 0x7FFF)
        len = 0x7FFF;
    while (got < len) {
        fill(u);
        if (!avail(u) && u->closed)
            break;
        got += take(u, (uint8_t *)buf + got, len - got);
    }
    set_status(u);
    return (int16_t)got;
}

#ifndef FN_NO_HTTP
/* HTTP: a request header ("Name: value", a trailing CR/LF/ATASCII EOL is dropped) */
static FN_ERR add_header(Unit *u, const uint8_t *buf, uint16_t len)
{
    while (len && (buf[len - 1] == '\r' || buf[len - 1] == '\n' || buf[len - 1] == 0x9B))
        --len;
    if (!len || !memchr(buf, ':', len))
        return fail(FN_ERR_BAD_CMD, NETWORK_ERROR_INVALID_COMMAND);
    if (m4_send(u->sock, buf, len) != 0 || m4_send(u->sock, "\r\n", 2) != 0)
        return fail(FN_ERR_IO_ERROR, NETWORK_ERROR_GENERAL);
    return FN_ERR_OK;
}

/* HTTP: POST/PUT data, kept in the receive buffer until the request is sent */
static FN_ERR add_data(Unit *u, const uint8_t *buf, uint16_t len)
{
    if (len > FN_RXBUF - u->len)
        return fail(FN_ERR_IO_ERROR, NETWORK_ERROR_COULD_NOT_ALLOCATE_BUFFERS);
    memcpy(u->rx + u->len, buf, len);
    u->len += len;
    return FN_ERR_OK;
}
#endif

FN_ERR network_write(const char *devicespec, const void *buf, uint16_t len)
{
    Unit *u = open_unit(devicespec);

    if (!u)
        return FN_ERR_IO_ERROR;
#ifndef FN_NO_HTTP
    if (u->method != M_TCP) {
        if (u->state != H_REQUEST)      /* the request is out: only reading now */
            return fail(FN_ERR_IO_ERROR, NETWORK_ERROR_READ_ONLY);
        if (u->chan == HTTP_CHAN_MODE_SET_HEADERS)
            return add_header(u, buf, len);
        if (u->chan == HTTP_CHAN_MODE_POST_SET_DATA && (u->method == M_POST || u->method == M_PUT))
            return add_data(u, buf, len);
        return fail(FN_ERR_BAD_CMD, NETWORK_ERROR_INVALID_COMMAND);
    }
#endif
    if (m4_send(u->sock, buf, len) != 0)
        return fail(FN_ERR_IO_ERROR, NETWORK_ERROR_GENERAL);
    return FN_ERR_OK;
}

#ifndef FN_NO_HTTP
/* ---- HTTP calls ----------------------------------------------------------------------- */

FN_ERR network_http_set_channel_mode(const char *devicespec, uint8_t mode)
{
    Unit *u = open_unit(devicespec);

    if (!u)
        return FN_ERR_IO_ERROR;
    if (u->method == M_TCP)
        return fail(FN_ERR_BAD_CMD, NETWORK_ERROR_INVALID_COMMAND);
    switch (mode) {
    case HTTP_CHAN_MODE_BODY:
        break;
    case HTTP_CHAN_MODE_SET_HEADERS:
    case HTTP_CHAN_MODE_POST_SET_DATA:
        if (u->state != H_REQUEST)      /* too late: the request is out */
            return fail(FN_ERR_IO_ERROR, NETWORK_ERROR_READ_ONLY);
        break;
    default:                            /* collecting response headers: not yet */
        return fail(FN_ERR_BAD_CMD, NETWORK_ERROR_NOT_IMPLEMENTED);
    }
    u->chan = mode;
    return FN_ERR_OK;
}

FN_ERR network_http_post_bin(const char *devicespec, const uint8_t *data, uint16_t len)
{
    Unit *u = open_unit(devicespec);
    FN_ERR e;

    if (!u)
        return FN_ERR_IO_ERROR;
    if ((u->method != M_POST && u->method != M_PUT) || u->state != H_REQUEST)
        return fail(FN_ERR_BAD_CMD, NETWORK_ERROR_INVALID_COMMAND);
    u->chan = HTTP_CHAN_MODE_BODY;
    if (u->len) {                       /* data written before: this goes after it */
        e = add_data(u, data, len);
        if (e != FN_ERR_OK)
            return e;
        data = u->rx;
        len = u->len;
    }
    if (finish_request(u, data, len) != 0)
        return fail(FN_ERR_IO_ERROR, NETWORK_ERROR_GENERAL);
    return FN_ERR_OK;
}

FN_ERR network_http_post(const char *devicespec, const char *data)
{
    return network_http_post_bin(devicespec, (const uint8_t *)data, strlen(data));
}
#endif /* FN_NO_HTTP */
