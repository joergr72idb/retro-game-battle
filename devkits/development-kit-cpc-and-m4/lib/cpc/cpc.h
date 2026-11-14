/* CPC firmware helpers for SDCC: text output (printf works through putchar) and keyboard. */
#ifndef CPC_H
#define CPC_H

int putchar(int c);              /* firmware TXT OUTPUT; '\n' becomes CR+LF */
unsigned char cpc_getkey(void);  /* waits for a key (KM WAIT CHAR) */
void cpc_print(const char *s);

#endif
