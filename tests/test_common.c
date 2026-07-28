#include <stdio.h>
#include <string.h>

#include "common.h"

static int check(const char *input, size_t suppliedLength,
                 const char *expected, size_t expectedLength)
{
    char line[64];
    snprintf(line, sizeof(line), "%s", input);
    size_t length = stripLineEnding(line, suppliedLength);
    if (length != expectedLength || strcmp(line, expected) != 0) {
        fprintf(stderr, "stripLineEnding(%s): got '%s' (%zu)\n",
                input, line, length);
        return 0;
    }
    return 1;
}

int main(void)
{
    int ok = 1;
    ok &= check("", 0, "", 0);
    ok &= check("\n", 1, "", 0);
    ok &= check("\r\n", 2, "", 0);
    ok &= check("version\n", 8, "version", 7);
    ok &= check(" version \r\n", 11, " version ", 9);
    ok &= check("version", 7, "version", 7);
    ok &= stripLineEnding(NULL, 10) == 0;
    return ok ? 0 : 1;
}
