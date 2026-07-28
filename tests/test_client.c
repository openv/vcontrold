#include <stdio.h>

#include "client.h"

int inetversion = 0;

int main(void)
{
    char empty[] = "";
    char commas[] = ",,,";

    if (sendCmds(-1, empty) != NULL) {
        fputs("empty command list was accepted\n", stderr);
        return 1;
    }
    if (sendCmds(-1, commas) != NULL) {
        fputs("comma-only command list was accepted\n", stderr);
        return 1;
    }
    return 0;
}
