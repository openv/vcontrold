#include <stdio.h>
#include <string.h>

#include "common.h"
#include "xmlconfig.h"

int getCycleTime(char *recv, int len, char *result);
int setCycleTime(char *input, char *sendBuf);

void removeCompileList(compilePtr ptr)
{
    (void)ptr;
}

static int test_legacy(void)
{
    char input[] = "06:00 08:00 - - 17:30 22:00 - -";
    char encoded[MAXBUF] = {0};
    char decoded[MAXBUF] = {0};

    if (setCycleTime(input, encoded) != 8) return 0;
    if ((unsigned char)encoded[0] != 0x30 ||
            (unsigned char)encoded[1] != 0x40 ||
            (unsigned char)encoded[2] != 0xff ||
            (unsigned char)encoded[3] != 0xff) return 0;
    if (!getCycleTime(encoded, 8, decoded)) return 0;
    return strstr(decoded, "1:An:06:00  Aus:08:00") != NULL &&
           strstr(decoded, "2:An:--     Aus:--") != NULL;
}

static int test_extended(void)
{
    char input[] = "06:00 08:00 2 - - - 17:30 22:00 7 - - - - - -";
    char encoded[MAXBUF] = {0};
    char decoded[MAXBUF] = {0};

    if (setCycleTime(input, encoded) != 15) return 0;
    if ((unsigned char)encoded[2] != 2 ||
            (unsigned char)encoded[8] != 7) return 0;
    if (!getCycleTime(encoded, 15, decoded)) return 0;
    return strstr(decoded, "1:An:06:00  Aus:08:00  Mod:02") != NULL &&
           strstr(decoded, "2:An:--     Aus:--     Mod:--") != NULL &&
           strstr(decoded, "3:An:17:30  Aus:22:00  Mod:07") != NULL;
}

static int test_invalid(void)
{
    char badTime[] = "06:05 08:00 - - - - - -";
    char badMode[] = "06:00 08:00 128 - - - - - - - - - - - -";
    char encoded[MAXBUF] = {0};

    return setCycleTime(badTime, encoded) == 0 &&
           setCycleTime(badMode, encoded) == 0 &&
           getCycleTime(encoded, 9, encoded + 64) == 0;
}

int main(void)
{
    if (!test_legacy() || !test_extended() || !test_invalid()) {
        fputs("cycle time test failed\n", stderr);
        return 1;
    }
    return 0;
}
