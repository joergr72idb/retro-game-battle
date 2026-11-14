/* C sample: asks the FujiNet for its network settings and prints them. */
#include <stdio.h>
#include <conio.h>
#include "fujinet-fuji.h"

static AdapterConfig ac;

int main(void)
{
    printf("Hello from cc65 + fujinet-lib!\n\n");
    if (fuji_get_adapter_config(&ac)) {
        printf("FujiNet %s\n", ac.fn_version);
        printf("Host:    %s\n", ac.hostname);
        printf("IP:      %u.%u.%u.%u\n",
               ac.localIP[0], ac.localIP[1], ac.localIP[2], ac.localIP[3]);
    } else {
        printf("No FujiNet found.\n");
    }
    printf("\nPress a key.");
    cgetc();
    return 0;
}
