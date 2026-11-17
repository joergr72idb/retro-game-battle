/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 joergr72idb - see LICENSE */
/* fntest - tests the fujinet-network layer on the CPC + M4 against test/testserver.py.
 * Each result goes to the screen and over a second connection (N2:) to the test server,
 * which prints a summary. Build: make test SERVER=... PORT=...
 */
#include <stdio.h>
#include <string.h>
#include "fujinet-network.h"
#include "cpc.h"

#ifndef SERVER_HOST
#define SERVER_HOST "127.0.0.1"
#endif
#ifndef SERVER_PORT
#define SERVER_PORT 6510
#endif

static char spec[64], rspec[64], spec3[64], line[80];
static uint8_t buf[1500];
static uint8_t failed;

static void report(const char *name, uint8_t ok, const char *detail)
{
    sprintf(line, "%s %s %s\r\n", ok ? "PASS" : "FAIL", name, detail);
    cpc_print(line);
    network_write(rspec, line, strlen(line));
    if (!ok)
        failed = 1;
}

static uint8_t pattern(uint16_t i, uint8_t seed)
{
    return (uint8_t)(i * 7 + seed);
}

static FN_ERR open_test(const char *unit, const char *cmd)
{
    FN_ERR e;

    sprintf(spec, "%s:TCP://%s:%u/", unit, SERVER_HOST, SERVER_PORT);
    e = network_open(spec, OPEN_MODE_RW, OPEN_TRANS_NONE);
    if (e == FN_ERR_OK)
        network_write(spec, cmd, strlen(cmd));
    return e;
}

/* T1: 3000 bytes from the server, read the RGB way (status, then read_nb), then EOF */
static void t1(void)
{
    uint16_t bw, n = 0, maxbw = 0;
    uint8_t c, err, ok = 1;
    int16_t r, i;
    char d[40];

    if (open_test("N1", "T1\n") != FN_ERR_OK) {
        report("T1", 0, "open");
        return;
    }
    for (;;) {
        if (network_status(spec, &bw, &c, &err) != FN_ERR_OK) {
            ok = 0;
            break;
        }
        if (bw > maxbw)
            maxbw = bw;
        if (!bw) {
            if (!c)
                break;
            continue;
        }
        r = network_read_nb(spec, buf, bw > 100 ? 100 : bw);
        for (i = 0; i < r; ++i, ++n)
            if (buf[i] != pattern(n, 3))
                ok = 0;
    }
    sprintf(d, "bytes=%u maxbw=%u err=%u", n, maxbw, err);
    report("T1", ok && n == 3000 && err == NETWORK_ERROR_END_OF_FILE && maxbw <= 256, d);
    network_close(spec);
}

/* T2: 2000 bytes to the server in pieces of different sizes, it answers OK2 */
static void t2(void)
{
    static const uint16_t sizes[] = { 1, 99, 1400, 500 };
    uint16_t i, n = 0, k;
    int16_t r;
    char d[40];

    if (open_test("N1", "T2\n") != FN_ERR_OK) {
        report("T2", 0, "open");
        return;
    }
    for (k = 0; k < 4; ++k) {
        for (i = 0; i < sizes[k]; ++i)
            buf[i] = pattern(n + i, 5);
        if (network_write(spec, buf, sizes[k]) != FN_ERR_OK) {
            sprintf(d, "write %u bytes failed after %u", sizes[k], n);
            report("T2", 0, d);
            network_close(spec);
            return;
        }
        n += sizes[k];
    }
    r = network_read(spec, buf, 4);
    buf[r > 0 ? r : 0] = 0;
    sprintf(d, "sent=%u answer=%d", n, r);
    report("T2", n == 2000 && r == 4 && !memcmp(buf, "OK2\n", 4), d);
    network_close(spec);
}

/* T3: network_read waits for 500 bytes sent in slow pieces, then 0 at EOF */
static void t3(void)
{
    uint16_t i;
    int16_t r, r2;
    uint8_t ok = 1;
    char d[40];

    if (open_test("N1", "T3\n") != FN_ERR_OK) {
        report("T3", 0, "open");
        return;
    }
    r = network_read(spec, buf, 500);
    for (i = 0; i < 500; ++i)
        if (buf[i] != pattern(i, 9))
            ok = 0;
    r2 = network_read(spec, buf, 10);
    sprintf(d, "read=%d then=%d err=%u", r, r2, fn_network_error);
    report("T3", ok && r == 500 && r2 == 0 && fn_network_error == NETWORK_ERROR_END_OF_FILE, d);
    network_close(spec);
}

/* T4: host name instead of an address, unit N3 */
static void t4(void)
{
    int16_t r = -1;
    char d[40];

    sprintf(spec, "N3:TCP://localhost:%u/", SERVER_PORT);
    if (network_open(spec, OPEN_MODE_RW, OPEN_TRANS_NONE) == FN_ERR_OK) {
        network_write(spec, "T4\n", 3);
        r = network_read(spec, buf, 3);
    }
    sprintf(d, "read=%d", r);
    report("T4", r == 3 && !memcmp(buf, "HI\n", 3), d);
    network_close(spec);
}

/* T5: the CPC as a TCP server on SERVER_PORT + 4 - the test server connects twice */
static uint8_t bad;

static void check(uint8_t cond, uint8_t step)
{
    if (!cond && !bad)
        bad = step;
}

/* waits until a client is there for network_accept; 1 = it came */
static uint8_t wait_client(const char *s)
{
    uint16_t bw, i;
    uint8_t c = 0, err;

    for (i = 0; i < 30000 && !c; ++i)
        network_status(s, &bw, &c, &err);
    return c && bw == 0;
}

static void t5(void)
{
    static char lspec[24], cmd[16];
    uint16_t bw;
    uint8_t c, err;
    int16_t r;
    char d[60];

    bad = 0;
    sprintf(lspec, "N4:TCP://:%u/", SERVER_PORT + 4);
    if (network_open(lspec, OPEN_MODE_RW, OPEN_TRANS_NONE) != FN_ERR_OK) {
        sprintf(d, "open err=%u", fn_network_error);
        report("T5-server", 0, d);
        return;
    }
    /* nobody there yet */
    network_status(lspec, &bw, &c, &err);
    check(!c && !bw && err == NETWORK_SUCCESS, 1);
    check(network_accept(lspec) == FN_ERR_IO_ERROR && fn_network_error == NETWORK_ERROR_NO_CONNECTION_WAITING, 2);
    check(network_write(lspec, "x", 1) == FN_ERR_IO_ERROR && fn_network_error == NETWORK_ERROR_NOT_CONNECTED, 3);
    check(network_read(lspec, buf, 1) < 0, 4);
    check(network_accept("N2:") == FN_ERR_IO_ERROR && fn_network_error == NETWORK_ERROR_SERVER_NOT_RUNNING, 5);

    sprintf(cmd, "T5 %u\n", SERVER_PORT + 4);
    if (open_test("N1", cmd) != FN_ERR_OK) {
        report("T5-server", 0, "open N1");
        network_close(lspec);
        return;
    }
    /* first client: PING -> PONG, it closes */
    check(wait_client(lspec), 6);
    check(network_accept(lspec) == FN_ERR_OK, 7);
    check(network_accept(lspec) == FN_ERR_IO_ERROR
          && fn_network_error == NETWORK_ERROR_CONNECTION_ALREADY_IN_PROGRESS, 8);
    r = network_read(lspec, buf, 5);
    check(r == 5 && !memcmp(buf, "PING\n", 5), 9);
    check(network_write(lspec, "PONG\n", 5) == FN_ERR_OK, 10);
    r = network_read(lspec, buf, 1);
    check(r == 0 && fn_network_error == NETWORK_ERROR_END_OF_FILE, 11);
    network_status(lspec, &bw, &c, &err);
    check(!c && !bw && err == NETWORK_ERROR_END_OF_FILE, 12);
    check(network_write(lspec, "x", 1) == FN_ERR_IO_ERROR && fn_network_error == NETWORK_ERROR_NOT_CONNECTED, 13);
    /* second client: AGAIN -> BYE, the CPC ends it */
    network_write(spec, "NEXT\n", 5);
    check(wait_client(lspec), 14);
    check(network_accept(lspec) == FN_ERR_OK, 15);
    r = network_read(lspec, buf, 6);
    check(r == 6 && !memcmp(buf, "AGAIN\n", 6), 16);
    check(network_write(lspec, "BYE\n", 4) == FN_ERR_OK, 17);
    check(network_close_client(lspec) == FN_ERR_OK, 18);
    network_status(lspec, &bw, &c, &err);
    check(!c && err == NETWORK_SUCCESS, 19);
    check(network_close_client(lspec) == FN_ERR_OK && fn_network_error == NETWORK_ERROR_NOT_CONNECTED, 20);
    /* the test server's verdict */
    r = network_read(spec, buf, 5);
    check(r == 5 && !memcmp(buf, "T5OK\n", 5), 21);
    network_close(spec);
    network_close(lspec);               /* stops listening */
    check(network_accept(lspec) == FN_ERR_IO_ERROR, 22);
    sprintf(d, "failed at step %u (err=%u)", bad, fn_network_error);
    report("T5-server", !bad, bad ? d : "two clients");
}

#ifndef FN_NO_HTTP
/* ---- HTTP (the test server's HTTP port is SERVER_PORT + 2) ---- */

static void hspec(const char *path)
{
    sprintf(spec, "N1:HTTP://%s:%u%s", SERVER_HOST, SERVER_PORT + 2, path);
}

/* reads the response body the usual way (status, read_nb) into buf, 0-terminated */
static uint16_t body(uint8_t *err)
{
    uint16_t bw, n = 0;
    uint8_t c;
    int16_t r;

    *err = 0;
    for (;;) {
        if (network_status(spec, &bw, &c, err) != FN_ERR_OK)
            break;
        if (!bw) {
            if (!c)
                break;
            continue;
        }
        if (bw > sizeof buf - 1 - n)
            bw = sizeof buf - 1 - n;
        if (!bw || (r = network_read_nb(spec, buf + n, bw)) <= 0)
            break;
        n += r;
    }
    buf[n] = 0;
    return n;
}

/* opens path with mode; for POST/PUT sends data (0 = nothing here); compares the body */
static void http(const char *name, const char *path, uint8_t mode, const char *data,
                 const char *want, uint8_t want_err)
{
    uint8_t err;
    char d[60];

    hspec(path);
    if (network_open(spec, mode, OPEN_TRANS_NONE) != FN_ERR_OK) {
        report(name, 0, "open");
        return;
    }
    if (data && network_http_post(spec, data) != FN_ERR_OK) {
        report(name, 0, "post");
        network_close(spec);
        return;
    }
    body(&err);
    sprintf(d, "err=%u body=%.30s", err, (char *)buf);
    report(name, !strcmp((char *)buf, want) && err == want_err, d);
    network_close(spec);
}

/* reads one collected response header in get-headers mode: compares it with want (with the
   EOL), returns 1 if it matches */
static uint8_t header(const char *want)
{
    uint16_t bw;
    uint8_t c, err;
    int16_t r;
    char d[60];

    network_status(spec, &bw, &c, &err);
    r = network_read(spec, buf, bw);
    buf[r > 0 ? r : 0] = 0;
    if (bw == strlen(want) && r == bw && !strcmp((char *)buf, want) && err == NETWORK_SUCCESS && !c)
        return 1;
    sprintf(d, "bw=%u r=%d c=%u err=%u got=%.20s", bw, r, c, err, (char *)buf);
    report("H14-headers", 0, d);
    return 0;
}

/* H14/H15: response headers, collected through a redirect; then the body as usual */
static void http_headers(void)
{
    uint16_t bw;
    uint8_t c, err, ok;
    char d[60];

    hspec("/redir-info");
    network_open(spec, OPEN_MODE_HTTP_GET_H, OPEN_TRANS_NONE);
    ok = network_http_set_channel_mode(spec, HTTP_CHAN_MODE_COLLECT_HEADERS) == FN_ERR_OK;
    ok &= network_write(spec, "X-Answer", 8) == FN_ERR_OK;
    ok &= network_write(spec, "content-type\x9b", 13) == FN_ERR_OK;   /* EOL dropped */
    ok &= network_write(spec, "X-Missing\r\n", 11) == FN_ERR_OK;
    ok &= network_write(spec, "Bad:", 4) == FN_ERR_BAD_CMD;
    /* another unit can't collect at the same time */
    sprintf(spec3, "N3:HTTP://%s:%u/info", SERVER_HOST, SERVER_PORT + 2);
    network_open(spec3, OPEN_MODE_HTTP_GET_H, OPEN_TRANS_NONE);
    network_http_set_channel_mode(spec3, HTTP_CHAN_MODE_COLLECT_HEADERS);
    ok &= network_write(spec3, "X-Answer", 8) == FN_ERR_IO_ERROR
          && fn_network_error == NETWORK_ERROR_COULD_NOT_ALLOCATE_BUFFERS;
    network_close(spec3);
    ok &= network_http_set_channel_mode(spec, HTTP_CHAN_MODE_GET_HEADERS) == FN_ERR_OK;
    if (!ok) {
        report("H14-headers", 0, "collect");
        network_close(spec);
        return;
    }
    /* in the order collected; the first X-Answer counts, spaces around it dropped */
    if (!header("42\x9b") || !header("text/plain\x9b") || !header("\x9b")) {
        network_close(spec);
        return;
    }
    network_status(spec, &bw, &c, &err);
    sprintf(d, "end bw=%u err=%u", bw, err);
    report("H14-headers", bw == 0 && err == NETWORK_ERROR_END_OF_FILE, d);
    /* again from the first one, then the body */
    network_http_set_channel_mode(spec, HTTP_CHAN_MODE_GET_HEADERS);
    ok = header("42\x9b");
    network_http_set_channel_mode(spec, HTTP_CHAN_MODE_BODY);
    body(&err);
    sprintf(d, "err=%u body=%.20s", err, (char *)buf);
    report("H15-headers-body", ok && !strcmp((char *)buf, "INFO") && err == NETWORK_ERROR_END_OF_FILE, d);
    network_close(spec);
}

/* H16/H17: OPEN_MODE_HTTP_PUT - the data written is sent with the first read or at close */
static void http_put(void)
{
    uint8_t err, ok;
    int16_t r;
    FN_ERR e;
    char d[60];

    hspec("/echo");
    network_open(spec, OPEN_MODE_HTTP_PUT, OPEN_TRANS_NONE);
    ok = network_write(spec, "abc", 3) == FN_ERR_OK && network_write(spec, "def", 3) == FN_ERR_OK;
    r = network_read(spec, buf, 100);
    buf[r > 0 ? r : 0] = 0;
    sprintf(d, "r=%d err=%u body=%.20s", r, fn_network_error, (char *)buf);
    report("H16-put-read", ok && !strcmp((char *)buf, "PUT LEN=6:abcdef")
           && fn_network_error == NETWORK_ERROR_END_OF_FILE, d);
    network_close(spec);

    hspec("/putclose");
    network_open(spec, OPEN_MODE_HTTP_PUT, OPEN_TRANS_NONE);
    network_write(spec, "hello ", 6);
    network_write(spec, "cpc", 3);
    ok = network_close(spec) == FN_ERR_OK;
    hspec("/putcheck");
    network_open(spec, OPEN_MODE_HTTP_GET, OPEN_TRANS_NONE);
    body(&err);
    network_close(spec);
    ok &= !strcmp((char *)buf, "hello cpc");
    hspec("/nope");                     /* an HTTP error comes back from the close */
    network_open(spec, OPEN_MODE_HTTP_PUT, OPEN_TRANS_NONE);
    network_write(spec, "x", 1);
    e = network_close(spec);
    sprintf(d, "got=%.12s e=%u err=%u", (char *)buf, e, fn_network_error);
    report("H17-put-close", ok && e == FN_ERR_IO_ERROR && fn_network_error == NETWORK_ERROR_FILE_NOT_FOUND, d);
}

static void http_tests(void)
{
    uint16_t i, n;
    uint8_t err, ok;
    int16_t r;
    char d[60];

    /* H1: 3000 bytes through the 256-byte buffer, a 400-byte header line dropped, then EOF */
    hspec("/big");
    network_open(spec, OPEN_MODE_HTTP_GET, OPEN_TRANS_NONE);
    for (ok = 1, n = 0;;) {             /* checked as it comes: more than buf holds */
        uint16_t bw;
        uint8_t c;

        if (network_status(spec, &bw, &c, &err) != FN_ERR_OK)
            break;
        if (!bw) {
            if (!c)
                break;
            continue;
        }
        r = network_read_nb(spec, buf, bw > 100 ? 100 : bw);
        for (i = 0; i < (uint16_t)r; ++i, ++n)
            if (buf[i] != pattern(n, 11))
                ok = 0;
    }
    sprintf(d, "bytes=%u err=%u", n, err);
    report("H1-get", ok && n == 3000 && err == NETWORK_ERROR_END_OF_FILE, d);
    network_close(spec);

    /* H2: a request header; reading while adding headers is refused */
    hspec("/hdr");
    network_open(spec, OPEN_MODE_HTTP_GET_H, OPEN_TRANS_NONE);
    network_http_start_add_headers(spec);
    r = network_read_nb(spec, buf, 10);
    ok = r < 0 && fn_network_error == NETWORK_ERROR_WRITE_ONLY;
    ok &= network_http_add_header(spec, "X-Test: cpc") == FN_ERR_OK;
    ok &= network_http_add_header(spec, "no colon") == FN_ERR_BAD_CMD;
    network_http_end_add_headers(spec);
    body(&err);
    sprintf(d, "r=%d err=%u body=%.20s", r, err, (char *)buf);
    report("H2-header", ok && !strcmp((char *)buf, "X=cpc") && err == NETWORK_ERROR_END_OF_FILE, d);
    network_close(spec);

    http("H3-404", "/missing", OPEN_MODE_HTTP_GET, 0, "nope", NETWORK_ERROR_FILE_NOT_FOUND);
    http("H4-post", "/echo", OPEN_MODE_HTTP_POST, "hello world", "POST LEN=11:hello world",
         NETWORK_ERROR_END_OF_FILE);

    /* H5: POST data in pieces (channel mode), sent with the first status */
    hspec("/echo");
    network_open(spec, OPEN_MODE_HTTP_POST, OPEN_TRANS_NONE);
    network_http_set_channel_mode(spec, HTTP_CHAN_MODE_POST_SET_DATA);
    network_write(spec, "abc", 3);
    network_write(spec, "def", 3);
    network_http_set_channel_mode(spec, HTTP_CHAN_MODE_BODY);
    body(&err);
    sprintf(d, "err=%u body=%.30s", err, (char *)buf);
    report("H5-post-data", !strcmp((char *)buf, "POST LEN=6:abcdef") && err == NETWORK_ERROR_END_OF_FILE, d);
    network_close(spec);

    /* H6: 1000 bytes of binary POST data, more than the buffer: sent directly */
    for (i = 0; i < 1000; ++i)
        buf[i] = pattern(i, 13);
    hspec("/echo");
    network_open(spec, OPEN_MODE_HTTP_POST, OPEN_TRANS_NONE);
    ok = network_http_post_bin(spec, buf, 1000) == FN_ERR_OK;
    body(&err);
    sprintf(d, "err=%u body=%.30s", err, (char *)buf);
    report("H6-post-bin", ok && !strcmp((char *)buf, "POST LEN=1000:OK") && err == NETWORK_ERROR_END_OF_FILE, d);
    network_close(spec);

    http("H7-put", "/echo", OPEN_MODE_HTTP_PUT_H, "xyz", "PUT LEN=3:xyz", NETWORK_ERROR_END_OF_FILE);

    /* H8: DELETE the fujinet-lib way */
    hspec("/item");
    network_http_delete(spec, OPEN_TRANS_NONE);
    body(&err);
    sprintf(d, "err=%u body=%.20s", err, (char *)buf);
    report("H8-delete", !strcmp((char *)buf, "DELETED") && err == NETWORK_ERROR_END_OF_FILE, d);
    network_close(spec);

    http("H9-space", "/a b", OPEN_MODE_HTTP_GET, 0, "SPACE", NETWORK_ERROR_END_OF_FILE);

    /* redirects: 301/302 for GET and DELETE, absolute (to another host name) or a path */
    http("H10-redir", "/redir", OPEN_MODE_HTTP_GET, 0, "TARGET", NETWORK_ERROR_END_OF_FILE);
    http("H11-redir-path", "/redir-rel", OPEN_MODE_HTTP_GET, 0, "TARGET", NETWORK_ERROR_END_OF_FILE);
    hspec("/moved");
    network_http_delete(spec, OPEN_TRANS_NONE);
    body(&err);
    sprintf(d, "err=%u body=%.20s", err, (char *)buf);
    report("H12-redir-delete", !strcmp((char *)buf, "DELETED") && err == NETWORK_ERROR_END_OF_FILE, d);
    network_close(spec);
    /* not followed: more than 10, to https, for POST (that one's body stays readable) */
    http("H13-redir-loop", "/loop", OPEN_MODE_HTTP_GET, 0, "moved", NETWORK_ERROR_GENERAL);
    http("H13-redir-https", "/redir-https", OPEN_MODE_HTTP_GET, 0, "moved", NETWORK_ERROR_GENERAL);
    http("H13-redir-post", "/redir", OPEN_MODE_HTTP_POST, "x", "moved", NETWORK_ERROR_GENERAL);

    http_headers();
    http_put();
}
#endif

static void expect(const char *name, const char *s, uint8_t mode, uint8_t trans, FN_ERR want, uint8_t code)
{
    FN_ERR e = network_open(s, mode, trans);
    char d[40];

    sprintf(d, "e=%u err=%u", e, fn_network_error);
    report(name, e == want && (!code || fn_network_error == code), d);
    network_close(s);
}

int main(void)
{
    uint16_t bw;
    uint8_t c, err;
    char d[40];

    cpc_print("FNTEST - fujinet-network on the M4\r\n");
    if (network_init() != FN_ERR_OK) {
        cpc_print("FAIL init: no M4\r\n");
        return 0;
    }
    sprintf(rspec, "N2:TCP://%s:%u/", SERVER_HOST, SERVER_PORT);
    if (network_open(rspec, OPEN_MODE_RW, OPEN_TRANS_NONE) != FN_ERR_OK) {
        printf("FAIL no test server at %s:%u\r\n", SERVER_HOST, SERVER_PORT);
        return 0;
    }
    network_write(rspec, "REPORT\n", 7);

    t1();
    t2();
    t3();
    t4();
    t5();
    sprintf(spec, "N1:TCP://127.0.0.1:%u/", SERVER_PORT + 1);   /* nobody listens there */
    expect("refused", spec, OPEN_MODE_RW, OPEN_TRANS_NONE, FN_ERR_IO_ERROR, NETWORK_ERROR_CONNECTION_REFUSED);
    expect("no-host", "N1:TCP://nosuch.invalid:80/", OPEN_MODE_RW, OPEN_TRANS_NONE,
           FN_ERR_IO_ERROR, NETWORK_ERROR_NETWORK_UNREACHABLE);
    expect("unit5", "N5:TCP://127.0.0.1:80/", OPEN_MODE_RW, OPEN_TRANS_NONE,
           FN_ERR_BAD_CMD, NETWORK_ERROR_INVALID_DEVICESPEC);
#ifndef FN_NO_HTTP
    http_tests();
#else
    expect("no-http", "N1:HTTP://127.0.0.1/", OPEN_MODE_HTTP_GET, OPEN_TRANS_NONE,
           FN_ERR_BAD_CMD, NETWORK_ERROR_NOT_IMPLEMENTED);
#endif
    expect("https", "N1:HTTPS://127.0.0.1/", OPEN_MODE_HTTP_GET, OPEN_TRANS_NONE,
           FN_ERR_BAD_CMD, NETWORK_ERROR_NOT_IMPLEMENTED);
    expect("no-port", "N1:TCP://127.0.0.1/", OPEN_MODE_RW, OPEN_TRANS_NONE,
           FN_ERR_BAD_CMD, NETWORK_ERROR_INVALID_DEVICESPEC);
    expect("trans", "N1:TCP://127.0.0.1:80/", OPEN_MODE_RW, OPEN_TRANS_CRLF,
           FN_ERR_BAD_CMD, NETWORK_ERROR_NOT_IMPLEMENTED);
    sprintf(d, "r=%d", network_read_nb("N4:", buf, 10));
    report("not-open", network_read_nb("N4:", buf, 10) < 0
           && network_status("N4:", &bw, &c, &err) != FN_ERR_OK, d);

    sprintf(line, "DONE %s\r\n", failed ? "FAIL" : "PASS");
    cpc_print(line);
    network_write(rspec, line, strlen(line));
    network_close(rspec);
    return 0;
}
