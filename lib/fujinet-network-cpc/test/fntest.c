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

static char spec[64], rspec[64], line[80];
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
    sprintf(spec, "N1:TCP://127.0.0.1:%u/", SERVER_PORT + 1);   /* nobody listens there */
    expect("refused", spec, OPEN_MODE_RW, OPEN_TRANS_NONE, FN_ERR_IO_ERROR, NETWORK_ERROR_CONNECTION_REFUSED);
    expect("no-host", "N1:TCP://nosuch.invalid:80/", OPEN_MODE_RW, OPEN_TRANS_NONE,
           FN_ERR_IO_ERROR, NETWORK_ERROR_NETWORK_UNREACHABLE);
    expect("unit5", "N5:TCP://127.0.0.1:80/", OPEN_MODE_RW, OPEN_TRANS_NONE,
           FN_ERR_BAD_CMD, NETWORK_ERROR_INVALID_DEVICESPEC);
    expect("http", "N1:HTTP://127.0.0.1/", OPEN_MODE_READ, OPEN_TRANS_NONE,
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
