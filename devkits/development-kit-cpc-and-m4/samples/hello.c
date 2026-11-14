/* C sample: M4 firmware version, an HTTP download into memory (like |HTTPMEM) and the same
   request over a raw TCP socket. Needs the test server: make www (serves samples/www). */
#include <stdio.h>
#include "cpc.h"
#include "m4.h"

/* the Mac running the emulator; on a real CPC use the Mac's LAN address instead */
static const unsigned char server[4] = { 127, 0, 0, 1 };
#define PORT 8765
#define URL  "http://127.0.0.1:8765/hello.txt"

static unsigned char buf[512];

static void print_text(const unsigned char *p, unsigned int n)
{
    unsigned int i;

    for (i = 0; i < n; ++i)
        if (p[i] != '\r')                   /* putchar makes CR+LF from '\n' */
            putchar(p[i]);
}

int main(void)
{
    static const char request[] = "GET /hello.txt HTTP/1.0\r\nHost: cpc\r\n\r\n";
    char version[64];
    unsigned int n;
    unsigned char s;
    int r;

    printf("Hello from SDCC + M4!\n\n");
    if (m4_init() != 0) {
        printf("No M4 board found.\n");
        return 0;
    }
    m4_version(version, sizeof version);
    printf("M4: %s\n\n", version);

    n = m4_http_get(URL, buf, sizeof buf);
    printf("HTTP GET: %u bytes\n", n);
    print_text(buf, n);

    s = m4_socket();
    if (s == M4_ERROR || m4_connect(s, server, PORT) != 0) {
        printf("\nTCP: no connection\n");
    } else {
        m4_send(s, request, sizeof request - 1);
        n = 0;
        while ((r = m4_recv(s, buf + n, sizeof buf - n)) >= 0 && n < sizeof buf)
            n += r;
        m4_close(s);
        printf("\nTCP socket: %u bytes\n", n);
    }

    printf("\nPress a key.");
    cpc_getkey();
    return 0;
}
