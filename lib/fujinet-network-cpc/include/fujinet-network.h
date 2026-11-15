/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 joergr72idb - see LICENSE */
/* fujinet-network.h for the Amstrad/Schneider CPC with the M4 board (SDCC).
 *
 * The same function names, constants and behaviour as the network part of FujiNet's
 * fujinet-lib (github.com/FujiNetWIFI/fujinet-lib), so that a program written for the
 * FujiNet on the Atari, C64 or Apple II builds for the CPC too. Underneath it talks to the
 * M4 board (lib/m4 of development-kit-cpc-and-m4). Own code, not taken from fujinet-lib.
 *
 * Supported so far: TCP client connections ("N:TCP://host:port/"), units N1:-N4:
 * (N: = N1:), no translation (OPEN_TRANS_NONE). Everything else returns FN_ERR_BAD_CMD
 * with fn_network_error = NETWORK_ERROR_NOT_IMPLEMENTED.
 */
#ifndef FUJINET_NETWORK_H
#define FUJINET_NETWORK_H

#include <stdint.h>

/* return values of the network_* functions */
enum {
    FN_ERR_OK        = 0x00,
    FN_ERR_IO_ERROR  = 0x01,
    FN_ERR_BAD_CMD   = 0x02,
    FN_ERR_OFFLINE   = 0x03,
    FN_ERR_WARNING   = 0x04,
    FN_ERR_NO_DEVICE = 0x05,
    FN_ERR_UNKNOWN   = 0xff
};
typedef uint8_t FN_ERR;

/* device error codes (fn_network_error, the err of network_status) */
enum {
    NETWORK_SUCCESS                    = 1,
    NETWORK_ERROR_INVALID_COMMAND      = 132,
    NETWORK_ERROR_END_OF_FILE          = 136,
    NETWORK_ERROR_GENERAL              = 144,
    NETWORK_ERROR_NOT_IMPLEMENTED      = 146,
    NETWORK_ERROR_INVALID_DEVICESPEC   = 165,
    NETWORK_ERROR_CONNECTION_REFUSED   = 200,
    NETWORK_ERROR_NETWORK_UNREACHABLE  = 201,
    NETWORK_ERROR_NOT_CONNECTED        = 207,
    NETWORK_ERROR_NO_DEVICE_AVAILABLE  = 216
};

#define OPEN_MODE_READ          (0x04)
#define OPEN_MODE_WRITE         (0x08)
#define OPEN_MODE_RW            (0x0C)

#define OPEN_TRANS_NONE         (0x00)
#define OPEN_TRANS_CR           (0x01)
#define OPEN_TRANS_LF           (0x02)
#define OPEN_TRANS_CRLF         (0x03)
#define OPEN_TRANS_PET          (0x04)

/* the status of the most recent poll (network_status, network_read, network_read_nb) */
typedef struct {
    uint16_t avail;     /* bytes waiting */
    uint8_t status;     /* 1 = connected, 0 = closed */
    uint8_t errcode;    /* NETWORK_SUCCESS, NETWORK_ERROR_END_OF_FILE, ... */
} NetworkStatus;

extern NetworkStatus nw_status;

#define fn_network_bw    (nw_status.avail)
#define fn_network_conn  (nw_status.status)
#define fn_network_error (nw_status.errcode)

/* Looks for the M4. FN_ERR_NO_DEVICE without one. Call it first. */
FN_ERR network_init(void);

/* Opens "Nx:TCP://host:port/" (host = name or a.b.c.d). Waits until connected or failed. */
FN_ERR network_open(const char *devicespec, uint8_t mode, uint8_t trans);

FN_ERR network_close(const char *devicespec);

/* bw = bytes waiting, c = connected, err = device error code. Does not wait. */
FN_ERR network_status(const char *devicespec, uint16_t *bw, uint8_t *c, uint8_t *err);

/* Waits for at least one byte, then returns what is there (up to len).
 * 0 at the end of the connection, negative FN_ERR_* if the unit is not open. */
int16_t network_read_nb(const char *devicespec, void *buf, uint16_t len);

/* Waits for len bytes, or fewer at the end of the connection. */
int16_t network_read(const char *devicespec, void *buf, uint16_t len);

FN_ERR network_write(const char *devicespec, const void *buf, uint16_t len);

/* the unit of a devicespec: "N:" = 1, "N3:" = 3 */
uint8_t network_unit(const char *devicespec);

#endif
