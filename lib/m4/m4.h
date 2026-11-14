/* M4 board library for SDCC on the Amstrad/Schneider CPC.
 *
 * Talks to the M4 through its documented command interface (Duke, spinpoint.org/cpc/m4info.txt):
 * command bytes go to port &FE00, a write to &FC00 starts the command, and the M4 answers in a
 * buffer inside its own ROM. Covers HTTP downloads into memory (like |HTTPMEM), TCP client
 * sockets and DNS. IP addresses are in normal order: {192, 168, 1, 10}.
 */
#ifndef M4_H
#define M4_H

#define M4_ERROR        0xFF

/* socket status (m4_sock_status) */
#define M4_IDLE         0       /* connected and idle, or not used */
#define M4_CONNECTING   1
#define M4_SENDING      2
#define M4_CLOSED       3       /* the other side closed the connection */
#define M4_DNS_LOOKUP   5
                                /* 240-255: error codes */

unsigned char m4_init(void);                        /* finds the M4 ROM: 0 = ok, M4_ERROR = no M4 */
void m4_version(char *out, unsigned char max);      /* firmware versions, as text */

/* HTTP GET into buf (at most max bytes, max <= 16384). url like "http://host:port/path".
 * Returns the number of bytes received; buf is not 0-terminated. */
unsigned int m4_http_get(const char *url, unsigned char *buf, unsigned int max);

/* DNS: 0 = ok, ip gets the address */
unsigned char m4_dns(const char *host, unsigned char *ip);

/* TCP client sockets. Sockets are 1-4. */
unsigned char m4_socket(void);                                  /* socket number or M4_ERROR */
unsigned char m4_connect(unsigned char s, const unsigned char *ip, unsigned int port);  /* 0 = ok */
unsigned char m4_send(unsigned char s, const void *data, unsigned int len);              /* 0 = ok */
int m4_recv(unsigned char s, void *buf, unsigned int max);      /* bytes, 0 = none yet, -1 = closed */
void m4_close(unsigned char s);
unsigned char m4_sock_status(unsigned char s);

/* any M4 command: returns the response data (after the 3-byte header), copy bytes of it */
const unsigned char *m4_command(unsigned int cmd, const void *data, unsigned int len, unsigned int copy);

#endif
