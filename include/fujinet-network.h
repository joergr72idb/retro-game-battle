/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 joergr72idb - see LICENSE */
/* fujinet-network.h for the Amstrad/Schneider CPC with the M4 board (SDCC).
 *
 * The same function names, constants and behaviour as the network part of FujiNet's
 * fujinet-lib (github.com/FujiNetWIFI/fujinet-lib), so that a program written for the
 * FujiNet on the Atari, C64 or Apple II builds for the CPC too. Underneath it talks to the
 * M4 board (lib/m4 of development-kit-cpc-and-m4). Own code, not taken from fujinet-lib.
 *
 * Supported so far: TCP client connections ("N:TCP://host:port/") and HTTP requests
 * ("N:HTTP://host[:port]/path": GET, POST, PUT, DELETE, request and response headers,
 * redirects), units N1:-N4: (N: = N1:), no translation (OPEN_TRANS_NONE); -DFN_NO_HTTP leaves
 * HTTP out (TCP only, about 5.5 KB smaller). Everything else (HTTPS, listening, UDP, JSON, ...)
 * returns FN_ERR_BAD_CMD with fn_network_error = NETWORK_ERROR_NOT_IMPLEMENTED.
 */
#ifndef FUJINET_NETWORK_H
#define FUJINET_NETWORK_H

#include <stdint.h>
#include <string.h>        /* strlen in network_http_add_header */

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
    NETWORK_SUCCESS                             = 1,
    NETWORK_ERROR_WRITE_ONLY                    = 131,
    NETWORK_ERROR_INVALID_COMMAND               = 132,
    NETWORK_ERROR_READ_ONLY                     = 135,
    NETWORK_ERROR_END_OF_FILE                   = 136,
    NETWORK_ERROR_GENERAL_TIMEOUT               = 138,
    NETWORK_ERROR_GENERAL                       = 144,
    NETWORK_ERROR_NOT_IMPLEMENTED               = 146,
    NETWORK_ERROR_INVALID_DEVICESPEC            = 165,
    NETWORK_ERROR_ACCESS_DENIED                 = 167,
    NETWORK_ERROR_FILE_NOT_FOUND                = 170,
    NETWORK_ERROR_CONNECTION_REFUSED            = 200,
    NETWORK_ERROR_NETWORK_UNREACHABLE           = 201,
    NETWORK_ERROR_NOT_CONNECTED                 = 207,
    NETWORK_ERROR_INVALID_USERNAME_OR_PASSWORD  = 212,
    NETWORK_ERROR_CLIENT_GENERAL                = 214,
    NETWORK_ERROR_SERVER_GENERAL                = 215,
    NETWORK_ERROR_NO_DEVICE_AVAILABLE           = 216,
    NETWORK_ERROR_COULD_NOT_ALLOCATE_BUFFERS    = 255
};

#define OPEN_MODE_READ          (0x04)
#define OPEN_MODE_WRITE         (0x08)
#define OPEN_MODE_RW            (0x0C)

/* HTTP: the method is chosen by the open mode */
#define OPEN_MODE_HTTP_GET      (0x04)  /* GET */
#define OPEN_MODE_HTTP_PUT      (0x08)  /* PUT of everything written (up to FN_RXBUF bytes),
                                           sent with the first read or at close */
#define OPEN_MODE_HTTP_GET_H    (0x0C)  /* GET, request headers can be added */
#define OPEN_MODE_HTTP_POST     (0x0D)  /* POST */
#define OPEN_MODE_HTTP_PUT_H    (0x0E)  /* PUT, like POST */
#define OPEN_MODE_HTTP_DELETE   (0x05)  /* DELETE */
#define OPEN_MODE_HTTP_DELETE_H (0x09)  /* DELETE, request headers can be added */

/* HTTP channel modes (network_http_set_channel_mode) */
#define HTTP_CHAN_MODE_BODY             (0) /* reads give the response body */
#define HTTP_CHAN_MODE_COLLECT_HEADERS  (1) /* before the request: each write is a response
                                               header name to keep ("Content-Type") */
#define HTTP_CHAN_MODE_GET_HEADERS      (2) /* each read gives the next kept value + 0x9B */
#define HTTP_CHAN_MODE_SET_HEADERS      (3) /* writes add request headers ("Name: value") */
#define HTTP_CHAN_MODE_POST_SET_DATA    (4) /* writes add to the POST/PUT data */

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

/* Opens "Nx:TCP://host:port/" or "Nx:HTTP://host[:port]/path" (host = name or a.b.c.d).
 * Waits until connected or failed. HTTP: the request goes out with the first network_status
 * or read in HTTP_CHAN_MODE_BODY (or with network_http_post); until then headers and POST data
 * can be added. The response is read like a TCP stream: the body, then NETWORK_ERROR_END_OF_FILE.
 * An HTTP error status shows as the err of network_status (404 = NETWORK_ERROR_FILE_NOT_FOUND,
 * ...); its body can still be read. Redirects (301, 302) are followed for GET and DELETE, up to
 * 10, to "http://..." or a path on the same server; headers added by the program are not sent
 * again. Other 3xx, or one that can't be followed, is NETWORK_ERROR_GENERAL with its body. */
FN_ERR network_open(const char *devicespec, uint8_t mode, uint8_t trans);

/* OPEN_MODE_HTTP_PUT: sends the PUT if no read did; an HTTP error is returned
 * (FN_ERR_IO_ERROR, fn_network_error = the code) */
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

/* ---- HTTP (an open HTTP unit) ---- */

/* HTTP_CHAN_MODE_BODY, _SET_HEADERS, _COLLECT_HEADERS, _POST_SET_DATA (these three before the
 * request) or _GET_HEADERS.
 * Response headers: in COLLECT_HEADERS mode each write is one name (any case, a trailing EOL
 * is dropped; together up to FN_HDR_NAMES bytes); one unit at a time can collect (another one
 * gets NETWORK_ERROR_COULD_NOT_ALLOCATE_BUFFERS). Switching to GET_HEADERS starts at the first
 * name: network_status sends the request and waits for the response head, bw = the length of
 * the next value + 1, connected = 0, err = NETWORK_ERROR_END_OF_FILE after the last one. Each
 * read gives one value and 0x9B, in the order collected (a header not in the response: 0x9B
 * alone; values together up to FN_HDR_VALUES bytes). Back in BODY mode the body is read. */
FN_ERR network_http_set_channel_mode(const char *devicespec, uint8_t mode);
#define network_http_set_mode network_http_set_channel_mode

#define network_http_start_add_headers(devicespec) network_http_set_channel_mode(devicespec, HTTP_CHAN_MODE_SET_HEADERS)
#define network_http_end_add_headers(devicespec)   network_http_set_channel_mode(devicespec, HTTP_CHAN_MODE_BODY)
/* "Accept: text/plain": between start_add_headers and end_add_headers */
#define network_http_add_header(devicespec, header) network_write(devicespec, header, strlen(header))

/* Sends the POST/PUT request with this data (text without its 0 byte, or len bytes) and
 * switches to HTTP_CHAN_MODE_BODY; then read the response. Data written before in
 * HTTP_CHAN_MODE_POST_SET_DATA is sent first (that is buffered: up to FN_RXBUF bytes). */
FN_ERR network_http_post(const char *devicespec, const char *data);
FN_ERR network_http_post_bin(const char *devicespec, const uint8_t *data, uint16_t len);
#define network_http_put(devicespec, data) network_http_post(devicespec, data)

/* opens a DELETE request; read the response, then close */
#define network_http_delete(devicespec, trans) network_open(devicespec, OPEN_MODE_HTTP_DELETE_H, trans)

#endif
