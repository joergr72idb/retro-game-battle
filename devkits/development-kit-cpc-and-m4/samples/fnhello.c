/* C sample for the optional fujinet-network-cpc library (tools/setup-linux.sh fujinet or
   tools/setup-macos.sh fujinet, then make fujinet): the same calls as FujiNet's fujinet-lib on
   the Atari, C64 or Apple II. An HTTP GET and the same request over a TCP connection.
   Needs the test server: make www (serves samples/www).
   Note: the library is GPL-3.0-or-later, so FNHELLO.BIN (which links it) is too. */
#include <stdio.h>
#include "cpc.h"
#include "fujinet-network.h"

/* the Mac/PC running the emulator; on a real CPC use its LAN address instead */
#define HTTP_URL "N1:HTTP://127.0.0.1:8765/hello.txt"
#define TCP_URL  "N2:TCP://127.0.0.1:8765/"

static unsigned char buf[256];

static void print_text(const unsigned char *p, int n)
{
    int i;

    for (i = 0; i < n; ++i)
        if (p[i] != '\r')                   /* putchar makes CR+LF from '\n' */
            putchar(p[i]);
}

/* the usual FujiNet loop: status (bytes waiting), read them, until err 136 = end of data */
static unsigned int read_all(const char *dev, unsigned char print)
{
    uint16_t waiting;
    uint8_t connected, err;
    unsigned int total = 0;
    int n;

    for (;;) {
        network_status(dev, &waiting, &connected, &err);
        if (waiting == 0) {
            if (err == NETWORK_ERROR_END_OF_FILE || !connected)
                break;
            continue;
        }
        n = network_read_nb(dev, buf, waiting > sizeof buf ? sizeof buf : waiting);
        if (n < 0)
            break;
        if (print)
            print_text(buf, n);
        total += n;
    }
    return total;
}

int main(void)
{
    static const char request[] = "GET /hello.txt HTTP/1.0\r\nHost: cpc\r\n\r\n";
    unsigned int n;

    printf("Hello from fujinet-network on the M4!\n\n");
    if (network_init() != FN_ERR_OK) {
        printf("No M4 board found.\n");
        goto out;
    }

    if (network_open(HTTP_URL, OPEN_MODE_HTTP_GET, OPEN_TRANS_NONE) != FN_ERR_OK) {
        printf("HTTP: open failed, error %u\n", fn_network_error);
    } else {
        printf("HTTP GET:\n");
        n = read_all(HTTP_URL, 1);
        network_close(HTTP_URL);
        printf("(%u bytes)\n", n);
    }

    if (network_open(TCP_URL, OPEN_MODE_RW, OPEN_TRANS_NONE) != FN_ERR_OK) {
        printf("\nTCP: no connection, error %u\n", fn_network_error);
    } else {
        network_write(TCP_URL, request, sizeof request - 1);
        n = read_all(TCP_URL, 0);
        network_close(TCP_URL);
        printf("\nTCP: %u bytes (headers + body)\n", n);
    }

out:
    printf("\nPress a key.");
    cpc_getkey();
    return 0;
}
