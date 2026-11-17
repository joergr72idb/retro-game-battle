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
 * read (the status code, the response headers the program asked for, a redirect's Location)
 * and dropped; the body is then read like a TCP stream up to the end of the connection
 * ("Connection: close", so no chunked transfer).
 *
 * Response headers (HTTP_CHAN_MODE_COLLECT_HEADERS / _GET_HEADERS): the names and the values
 * are kept in one store shared by the units - one unit at a time collects headers.
 * Redirects: 301 and 302 for GET and DELETE are followed (up to FN_REDIRECTS), as the FujiNet
 * firmware does. OPEN_MODE_HTTP_PUT: the written data waits in the receive buffer like POST data.
 *
 * TCP server ("N1:TCP://:port/"): the M4 listens on a socket and takes one client on it; the
 * socket then IS that connection and no longer listens. So one client at a time per unit: when it
 * has gone (or network_close_client) a new socket listens again; until then others are refused.
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
/* TCP server: listening, no client taken (the M4 socket accepts one by itself - it is "waiting"
   until network_accept), or a client taken (sock is that connection) */
enum { S_LISTEN = 1, S_CLIENT };

#ifndef FN_NO_HTTP
#ifndef FN_HDR_NAMES
#define FN_HDR_NAMES  64        /* response headers to collect: their names together (+1 each) */
#endif
#ifndef FN_HDR_VALUES
#define FN_HDR_VALUES 192       /* their values together (+2 each), at most 250 */
#endif
#define FN_REDIRECTS  10        /* as the FujiNet firmware */
#endif
#define FN_HOST       64        /* host name */

typedef struct {
    uint8_t sock;               /* M4 socket, 0 = unit not open */
    uint8_t closed;             /* the other side closed the connection (or it failed) */
    uint8_t method;             /* M_TCP or the HTTP method */
    uint8_t state;              /* HTTP: H_REQUEST (request not finished), H_HEAD (response head), H_BODY */
    uint8_t chan;               /* HTTP channel mode */
    uint8_t err;                /* NETWORK_SUCCESS, or the error from the HTTP status */
    uint8_t first;              /* response head: the status line is still to come */
    uint8_t skip;               /* response head: inside a line longer than the buffer */
    uint8_t srv;                /* TCP server: S_LISTEN or S_CLIENT, 0 = not a server */
    uint16_t port;              /* HTTP: of the host; server: the one listened on */
#ifndef FN_NO_HTTP
    uint8_t putw;               /* OPEN_MODE_HTTP_PUT: writes in body mode are the PUT data */
    uint8_t redir;              /* response head: a redirect that is followed */
    uint8_t redirs;             /* redirects followed so far */
    char host[FN_HOST];
#endif
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

/* ---- response headers to collect ------------------------------------------------------ */

static Unit *hdr_unit;                  /* the unit that collects, 0 = none */
static char hdr_names[FN_HDR_NAMES];    /* the names, lower case, each 0-terminated */
static uint8_t hdr_nlen, hdr_count;
static uint8_t hdr_vals[FN_HDR_VALUES]; /* the values found: index, length, bytes */
static uint8_t hdr_vlen, hdr_cursor;    /* hdr_cursor: the next value to read */
static char loc[128];                   /* the Location of a redirect */

static uint8_t lower(uint8_t c)
{
    return c >= 'A' && c <= 'Z' ? c + 32 : c;
}

/* a header name to collect (one write, a trailing CR/LF/ATASCII EOL is dropped) */
static FN_ERR add_name(Unit *u, const uint8_t *buf, uint16_t len)
{
    if (hdr_unit != u) {
        if (hdr_unit)                   /* another unit collects */
            return fail(FN_ERR_IO_ERROR, NETWORK_ERROR_COULD_NOT_ALLOCATE_BUFFERS);
        hdr_unit = u;
        hdr_nlen = hdr_count = hdr_vlen = hdr_cursor = 0;
    }
    while (len && (buf[len - 1] == '\r' || buf[len - 1] == '\n' || buf[len - 1] == 0x9B))
        --len;
    if (!len || memchr(buf, ':', len))
        return fail(FN_ERR_BAD_CMD, NETWORK_ERROR_INVALID_COMMAND);
    if (len >= FN_HDR_NAMES - hdr_nlen)
        return fail(FN_ERR_IO_ERROR, NETWORK_ERROR_COULD_NOT_ALLOCATE_BUFFERS);
    while (len--)
        hdr_names[hdr_nlen++] = lower(*buf++);
    hdr_names[hdr_nlen++] = 0;
    ++hdr_count;
    return FN_ERR_OK;
}

/* where the value of collected header i is in hdr_vals (index, length, bytes), or 255 if it
   was not in the response */
static uint8_t hdr_find(uint8_t i)
{
    uint8_t j;

    for (j = 0; j < hdr_vlen; j += 2 + hdr_vals[j + 1])
        if (hdr_vals[j] == i)
            return j;
    return 255;
}

/* the index of name (n bytes, any case) among the names to collect, or 255 */
static uint8_t hdr_index(const uint8_t *name, uint16_t n)
{
    const char *p = hdr_names;
    uint8_t i;
    uint16_t k;

    for (i = 0; i < hdr_count; ++i, p += strlen(p) + 1) {
        for (k = 0; k < n && p[k] && lower(name[k]) == p[k]; ++k)
            ;
        if (k == n && !p[k])
            return i;
    }
    return 255;
}

/* a response header line "Name: value" (n bytes): a collected one is stored, a redirect's
   Location kept */
static void head_line(Unit *u, const uint8_t *b, uint16_t n)
{
    uint16_t c, v;
    uint8_t i;

    for (c = 0; c < n && b[c] != ':'; ++c)
        ;
    if (c == n)
        return;
    for (v = c + 1; v < n && (b[v] == ' ' || b[v] == '\t'); ++v)
        ;
    while (n > v && (b[n - 1] == ' ' || b[n - 1] == '\t'))
        --n;
    n -= v;                             /* the value: b[v .. v+n) */
    if (u->redir) {
        for (i = 0; i < 8 && lower(b[i]) == "location"[i]; ++i)
            ;
        if (i == 8 && c == 8) {
            if (n >= sizeof loc)
                n = 0;                  /* too long: not followed */
            memcpy(loc, b + v, n);
            loc[n] = 0;
        }
        return;
    }
    if (u != hdr_unit || (i = hdr_index(b, c)) == 255 || hdr_find(i) != 255)
        return;                         /* not asked for, or seen before (the first one counts) */
    if (hdr_vlen + 2 > FN_HDR_VALUES)
        return;                         /* no room: as if it was not there */
    if (n > FN_HDR_VALUES - 2 - hdr_vlen)
        n = FN_HDR_VALUES - 2 - hdr_vlen;       /* cut to what fits (may be nothing) */
    hdr_vals[hdr_vlen] = i;
    hdr_vals[hdr_vlen + 1] = (uint8_t)n;
    memcpy(hdr_vals + hdr_vlen + 2, b + v, n);
    hdr_vlen += 2 + n;
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
            if ((code == 301 || code == 302) && (u->method == M_GET || u->method == M_DELETE)
                && u->redirs < FN_REDIRECTS) {
                u->redir = 1;           /* followed at the end of the head (redirect) */
                loc[0] = 0;
            }
        } else if (n == 0)
            u->state = H_BODY;
        else
            head_line(u, b, n);
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
    u->state = H_HEAD;
    u->first = 1;
    u->skip = 0;
    u->redir = 0;
    u->err = NETWORK_SUCCESS;
    if (u == hdr_unit)                  /* values of an earlier response (a redirect) */
        hdr_vlen = hdr_cursor = 0;
    if (txerr) {
        u->closed = 1;
        u->state = H_BODY;
        u->err = NETWORK_ERROR_GENERAL;
    }
    return txerr;
}

/* HTTP: the request goes out with the first status/read in body or get-headers mode - for
   OPEN_MODE_HTTP_PUT with the first read only (or at close), as the data is still written */
static void start_response(Unit *u, uint8_t reading)
{
    if (u->state == H_REQUEST && (reading || !u->putw)
        && (u->chan == HTTP_CHAN_MODE_BODY || u->chan == HTTP_CHAN_MODE_GET_HEADERS))
        finish_request(u, u->rx, u->len);
}

/* the request line and the standard headers. 0 = sent */
static uint8_t request_head(uint8_t s, uint8_t method, const char *host, uint16_t port, const char *path)
{
    static const char *const names[] = { "", "GET ", "POST ", "PUT ", "DELETE " };

    txsock = s;
    txerr = 0;
    out_s(names[method]);
    if (!*path)
        out_c('/');
    for (; *path; ++path)
        if (*path == ' ')
            out_s("%20");
        else
            out_c(*path);
    out_s(" HTTP/1.0\r\nHost: ");
    out_s(host);
    if (port != 80) {
        out_c(':');
        out_u(port);
    }
    out_s("\r\nUser-Agent: fujinet-network-cpc\r\nConnection: close\r\n");
    out_flush();
    return txerr;
}

static uint8_t prefix(const char *s, const char *pre);
static const char *host_port(const char *p, char *host, uint16_t *port);
static uint8_t connect_to(const char *host, uint16_t port);

/* follows a redirect: the same request to loc ("http://host[:port]/path" or "/path" on the
   same server). Headers added by the program are not sent again. A Location that can't be
   followed (none, too long, https://, a relative path) leaves the 3xx response to be read,
   with error 144 - as when there were too many. */
static void redirect(Unit *u)
{
    const char *p = loc;
    char host[FN_HOST];
    uint16_t port = u->port;
    uint8_t s, i;

    u->redir = 0;
    strcpy(host, u->host);
    if ((i = prefix(p, "http://")) != 0) {
        p += i;                         /* before port = 80: SDCC 4.5.0 computed p + 80 here */
        port = 80;
        p = host_port(p, host, &port);
    } else if (*p != '/')
        p = 0;
    if (!p)
        return;
    ++u->redirs;
    m4_close(u->sock);
    u->start = u->len = 0;
    if ((s = connect_to(host, port)) != 0) {
        u->sock = s;
        u->port = port;
        strcpy(u->host, host);
        if (request_head(s, u->method, host, port, p) == 0) {
            finish_request(u, 0, 0);
            return;
        }
    }
    u->sock = M4_ERROR;                 /* the unit stays open, without a socket */
    u->closed = 1;
    u->state = H_BODY;
    u->err = NETWORK_ERROR_GENERAL;
}

#else
#define start_response(u, reading)
#endif /* FN_NO_HTTP */

/* ---- receiving ------------------------------------------------------------------------ */

/* TCP server: a new socket listens on u->port (M4_ERROR if that failed). The old one is closed. */
static void listen_again(Unit *u)
{
    uint8_t s;

    if (u->sock && u->sock != M4_ERROR)
        m4_close(u->sock);
    s = m4_socket();
    if (s == M4_ERROR || s == 0)
        s = M4_ERROR;
    else if (m4_listen(s, u->port) != 0) {
        m4_close(s);
        s = M4_ERROR;
    }
    u->sock = s;
}

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
        if (r < 0 || (r == 0 && m4_sock_status(u->sock) >= 240)) {
            u->closed = 1;
            if (u->srv)                 /* the client has gone: listen again at once */
                listen_again(u);
        } else
            u->len += r;
#ifndef FN_NO_HTTP
        if (u->state == H_HEAD)
            head(u);
        if (u->redir && u->state == H_BODY)     /* the head of a redirect is complete */
            redirect(u);
    } while (u->state == H_HEAD && (r > 0 || u->redir));   /* a redirect: waits for its head */
    if (u->state == H_HEAD && u->closed) {      /* closed before the end of the head */
        u->state = H_BODY;
        u->start = u->len = 0;
        if (u->first || u->redir)
            u->err = NETWORK_ERROR_GENERAL;
        u->redir = 0;
    }
#endif
}

#ifndef FN_NO_HTTP
/* HTTP: waits for the end of the response head */
static void wait_head(Unit *u)
{
    while (u->state == H_HEAD)
        fill(u);
}

/* bytes the program can read: the body only - in get-headers mode the next collected value
   with its ATASCII EOL */
static uint16_t avail(const Unit *u)
{
    uint8_t j;

    if (u->state == H_REQUEST || u->state == H_HEAD)
        return 0;
    if (u->chan == HTTP_CHAN_MODE_GET_HEADERS) {
        if (u != hdr_unit || hdr_cursor >= hdr_count)
            return 0;
        j = hdr_find(hdr_cursor);
        return j == 255 ? 1 : hdr_vals[j + 1] + 1;
    }
    return u->len;
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
    if (u->chan == HTTP_CHAN_MODE_GET_HEADERS)
        nw_status.status = 0;           /* as the FujiNet: "so that we always ask" */
    if (u->state == H_REQUEST || u->state == H_HEAD)
        nw_status.errcode = NETWORK_SUCCESS;
    else if (u->err != NETWORK_SUCCESS)
        nw_status.errcode = u->err;     /* HTTP error status: stays, as on the FujiNet */
    else if (u->chan == HTTP_CHAN_MODE_GET_HEADERS)
        nw_status.errcode = n ? NETWORK_SUCCESS : NETWORK_ERROR_END_OF_FILE;
    else
#endif
        nw_status.errcode = u->closed && !n ? NETWORK_ERROR_END_OF_FILE : NETWORK_SUCCESS;
}

/* ---- open/close ----------------------------------------------------------------------- */

#ifdef FN_NO_HTTP
static uint8_t lower(uint8_t c)
{
    return c >= 'A' && c <= 'Z' ? c + 32 : c;
}
#endif

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

/* "host[:port][/path]": the host (FN_HOST bytes), the port if there is one; returns the path
   ("" or "/..."), 0 if it is not like that */
static const char *host_port(const char *p, char *host, uint16_t *port)
{
    uint8_t i;

    for (i = 0; *p && *p != ':' && *p != '/'; ++p) {
        if (i == FN_HOST - 1)
            return 0;
        host[i++] = *p;
    }
    host[i] = 0;                        /* "" = listening */
    if (*p == ':') {
        if (*++p < '0' || *p > '9')
            return 0;
        for (*port = 0; *p >= '0' && *p <= '9'; ++p)
            *port = *port * 10 + (*p - '0');
    }
    return *p && *p != '/' ? 0 : p;
}

/* a connected M4 socket, or 0 (nw_status says why) */
static uint8_t connect_to(const char *host, uint16_t port)
{
    uint8_t ip[4], s, code;

    if (!parse_ip(host, ip) && m4_dns(host, ip) != 0)
        code = NETWORK_ERROR_NETWORK_UNREACHABLE;
    else if ((s = m4_socket()) == M4_ERROR || s == 0)
        code = NETWORK_ERROR_NO_DEVICE_AVAILABLE;
    else if (m4_connect(s, ip, port) != 0) {
        m4_close(s);
        code = NETWORK_ERROR_CONNECTION_REFUSED;
    } else
        return s;
    fail(FN_ERR_IO_ERROR, code);
    return 0;
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
    case OPEN_MODE_HTTP_PUT:
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
    char host[FN_HOST];
    uint8_t n, i, s, method;
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
    port = 0;                           /* TCP needs a port */
    if ((i = prefix(p, "tcp://")) != 0) {
        if (mode != OPEN_MODE_READ && mode != OPEN_MODE_WRITE && mode != OPEN_MODE_RW)
            return fail(FN_ERR_BAD_CMD, NETWORK_ERROR_NOT_IMPLEMENTED);
        method = M_TCP;
#ifndef FN_NO_HTTP
    } else if ((i = prefix(p, "http://")) != 0) {
        if ((method = http_method(mode)) == M_TCP)
            return fail(FN_ERR_BAD_CMD, NETWORK_ERROR_NOT_IMPLEMENTED);
        port = 80;                      /* HTTP has 80 */
#endif
    } else                              /* HTTPS, UDP, TNFS, ... */
        return fail(FN_ERR_BAD_CMD, NETWORK_ERROR_NOT_IMPLEMENTED);
    if ((p = host_port(p + i, host, &port)) == 0 || port == 0)
        return fail(FN_ERR_BAD_CMD, NETWORK_ERROR_INVALID_DEVICESPEC);

    network_close(devicespec);
    u = &units[n - 1];
    u->srv = 0;
    u->port = port;
    if (!host[0]) {                     /* "TCP://:port/": a server */
#ifndef FN_NO_HTTP
        if (method != M_TCP)
            return fail(FN_ERR_BAD_CMD, NETWORK_ERROR_INVALID_DEVICESPEC);
#endif
        u->sock = 0;
        listen_again(u);
        if (u->sock == M4_ERROR) {
            u->sock = 0;
            return fail(FN_ERR_IO_ERROR, NETWORK_ERROR_GENERAL);
        }
        s = u->sock;
        u->srv = S_LISTEN;
    } else if ((s = connect_to(host, port)) == 0)
        return FN_ERR_IO_ERROR;

#ifndef FN_NO_HTTP
    if (method != M_TCP && request_head(s, method, host, port, p) != 0) {
        m4_close(s);
        return fail(FN_ERR_IO_ERROR, NETWORK_ERROR_GENERAL);
    }
    u->state = method == M_TCP ? H_NONE : H_REQUEST;
    u->putw = mode == OPEN_MODE_HTTP_PUT && method == M_PUT;
    u->redir = u->redirs = 0;
    strcpy(u->host, host);
#else
    u->state = H_NONE;
#endif
    u->sock = s;
    u->closed = 0;
    u->method = method;
    u->chan = HTTP_CHAN_MODE_BODY;
    u->err = NETWORK_SUCCESS;
    u->start = u->len = 0;
    set_status(u);
    return FN_ERR_OK;
}

/* OPEN_MODE_HTTP_PUT: the PUT goes out here if no read sent it; an HTTP error is returned */
FN_ERR network_close(const char *devicespec)
{
    uint8_t n = network_unit(devicespec);
    FN_ERR e = FN_ERR_OK;
    Unit *u;

    if (n == 0 || n > UNITS)
        return fail(FN_ERR_BAD_CMD, NETWORK_ERROR_INVALID_DEVICESPEC);
    u = &units[n - 1];
    if (u->sock) {
#ifndef FN_NO_HTTP
        if (u->putw && u->state == H_REQUEST) {
            finish_request(u, u->rx, u->len);
            wait_head(u);
            if (u->err != NETWORK_SUCCESS)
                e = fail(FN_ERR_IO_ERROR, u->err);
        }
        if (u == hdr_unit)
            hdr_unit = 0;
#endif
        if (u->sock != M4_ERROR)        /* M4_ERROR: a redirect or listening again failed */
            m4_close(u->sock);
        u->sock = 0;
    }
    return e;
}

/* ---- TCP server ----------------------------------------------------------------------- */

/* the server unit of a devicespec, or 0 (nw_status: SERVER_NOT_RUNNING) */
static Unit *server_unit(const char *devicespec)
{
    Unit *u = open_unit(devicespec);

    if (u && !u->srv) {
        fail(FN_ERR_IO_ERROR, NETWORK_ERROR_SERVER_NOT_RUNNING);
        return 0;
    }
    return u;
}

/* listening: 1 = a client is there (taken by the M4, not yet by network_accept). One that
   came and went, or a broken socket: listen again. */
static uint8_t client_waiting(Unit *u)
{
    uint8_t st;

    if (u->sock == M4_ERROR)
        listen_again(u);
    if (u->sock == M4_ERROR)
        return 0;
    st = m4_sock_status(u->sock);
    if (st == M4_IDLE || st == M4_SENDING)
        return 1;
    if (st != M4_ACCEPTING)
        listen_again(u);
    return 0;
}

FN_ERR network_accept(const char *devicespec)
{
    Unit *u = server_unit(devicespec);

    if (!u)
        return FN_ERR_IO_ERROR;
    if (u->srv == S_CLIENT) {
        if (!u->closed)
            return fail(FN_ERR_IO_ERROR, NETWORK_ERROR_CONNECTION_ALREADY_IN_PROGRESS);
        u->srv = S_LISTEN;              /* the last one has gone (what it sent is dropped) */
    }
    if (!client_waiting(u))             /* (it listens again if it could not before) */
        return fail(FN_ERR_IO_ERROR, u->sock == M4_ERROR ? NETWORK_ERROR_SERVER_NOT_RUNNING
                                                          : NETWORK_ERROR_NO_CONNECTION_WAITING);
    u->srv = S_CLIENT;
    u->closed = 0;
    u->start = u->len = 0;
    set_status(u);
    return FN_ERR_OK;
}

FN_ERR network_close_client(const char *devicespec)
{
    Unit *u = server_unit(devicespec);

    if (!u)
        return FN_ERR_IO_ERROR;
    if (u->srv == S_LISTEN) {           /* no client: as the FujiNet, ok with NOT_CONNECTED */
        fail(FN_ERR_OK, NETWORK_ERROR_NOT_CONNECTED);
        return FN_ERR_OK;
    }
    if (!u->closed)                     /* still connected: end it, listen again */
        listen_again(u);
    u->srv = S_LISTEN;
    u->closed = 0;
    u->start = u->len = 0;
    return FN_ERR_OK;
}

/* ---- status, read, write -------------------------------------------------------------- */

FN_ERR network_status(const char *devicespec, uint16_t *bw, uint8_t *c, uint8_t *err)
{
    Unit *u = open_unit(devicespec);
    FN_ERR e = FN_ERR_IO_ERROR;

    if (u && u->srv == S_LISTEN) {      /* connected = a client is waiting for network_accept */
        nw_status.avail = 0;
        nw_status.status = client_waiting(u);
        nw_status.errcode = u->sock == M4_ERROR ? NETWORK_ERROR_SERVER_NOT_RUNNING : NETWORK_SUCCESS;
        e = FN_ERR_OK;
    } else if (u) {
        start_response(u, 0);
#ifndef FN_NO_HTTP
        if (u->chan == HTTP_CHAN_MODE_GET_HEADERS)
            wait_head(u);               /* the FujiNet has the response by now too */
#endif
        fill(u);
        set_status(u);
        if (u->srv && u->closed && !u->len)     /* the client has gone: as the FujiNet, */
            nw_status.status = client_waiting(u);   /* connected = the next one is there */
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

    if (u && u->srv == S_LISTEN) {      /* a server without a client */
        fail(FN_ERR_IO_ERROR, NETWORK_ERROR_NOT_CONNECTED);
        return 0;
    }
#ifndef FN_NO_HTTP
    if (u) {
        start_response(u, 1);
        if (u->state == H_REQUEST) {    /* still adding headers or POST data */
            fail(FN_ERR_IO_ERROR, NETWORK_ERROR_WRITE_ONLY);
            return 0;
        }
    }
#endif
    return u;
}

#ifndef FN_NO_HTTP
/* get-headers mode: the next collected value with an ATASCII EOL (one value per read, as on
   the FujiNet; a value longer than len is cut). An absent header is the EOL alone. */
static int16_t read_header(Unit *u, uint8_t *buf, uint16_t len)
{
    uint16_t n;
    uint8_t j;

    wait_head(u);
    n = avail(u);
    if (len > n)
        len = n;
    if (n) {
        j = hdr_find(hdr_cursor++);
        memcpy(buf, hdr_vals + j + 2, len < n ? len : n - 1);
        if (len == n)
            buf[n - 1] = 0x9B;
    }
    set_status(u);
    return (int16_t)len;
}
#define HEADERS(u, buf, len) if (u->chan == HTTP_CHAN_MODE_GET_HEADERS) return read_header(u, buf, len)
#else
#define HEADERS(u, buf, len)
#endif

int16_t network_read_nb(const char *devicespec, void *buf, uint16_t len)
{
    Unit *u = read_unit(devicespec);
    uint16_t got;

    if (!u)
        return -(int16_t)FN_ERR_IO_ERROR;
    if (len > 0x7FFF)
        len = 0x7FFF;
    HEADERS(u, buf, len);
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
    HEADERS(u, buf, len);
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
        if (u->chan == HTTP_CHAN_MODE_COLLECT_HEADERS)
            return add_name(u, buf, len);
        if ((u->chan == HTTP_CHAN_MODE_POST_SET_DATA && (u->method == M_POST || u->method == M_PUT))
            || (u->chan == HTTP_CHAN_MODE_BODY && u->putw))
            return add_data(u, buf, len);
        return fail(FN_ERR_BAD_CMD, NETWORK_ERROR_INVALID_COMMAND);
    }
#endif
    if (u->srv && (u->srv == S_LISTEN || u->closed))  /* no client (sock may listen again) */
        return fail(FN_ERR_IO_ERROR, NETWORK_ERROR_NOT_CONNECTED);
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
    case HTTP_CHAN_MODE_GET_HEADERS:    /* the collected values from the first one on */
        if (u == hdr_unit)
            hdr_cursor = 0;
        break;
    case HTTP_CHAN_MODE_SET_HEADERS:
    case HTTP_CHAN_MODE_COLLECT_HEADERS:
    case HTTP_CHAN_MODE_POST_SET_DATA:
        if (u->state != H_REQUEST)      /* too late: the request is out */
            return fail(FN_ERR_IO_ERROR, NETWORK_ERROR_READ_ONLY);
        break;
    default:
        return fail(FN_ERR_BAD_CMD, NETWORK_ERROR_INVALID_COMMAND);
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
