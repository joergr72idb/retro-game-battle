/* C sample: asks the FujiNet (IEC device 30) for its network settings and prints them. */
#include <stdio.h>
#include <conio.h>
#include "fujinet-fuji.h"

static AdapterConfig ac;

/* FujiNet sends ASCII; the C64 shows PETSCII, where the two cases are swapped
   and ASCII lowercase lands on graphics characters. */
static char *ascii2pet(char *s)
{
    char *p;

    for (p = s; *p; ++p) {
        if ((unsigned char)*p >= 0x61 && (unsigned char)*p <= 0x7A)
            *p -= 0x20;                 /* ASCII a-z -> PETSCII a-z */
        else if ((unsigned char)*p >= 0x41 && (unsigned char)*p <= 0x5A)
            *p += 0x80;                 /* ASCII A-Z -> PETSCII A-Z */
    }
    return s;
}

int main(void)
{
    printf("hello from cc65 + fujinet-lib!\n\n");
    if (fuji_get_adapter_config(&ac)) {
        printf("fujinet %s\n", ascii2pet(ac.fn_version));
        printf("host:   %s\n", ascii2pet(ac.hostname));
        printf("ip:     %u.%u.%u.%u\n",
               ac.localIP[0], ac.localIP[1], ac.localIP[2], ac.localIP[3]);
    } else {
        printf("no fujinet found.\n");
    }
    printf("\npress a key.");
    cgetc();
    return 0;
}
