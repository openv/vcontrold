#include <stdio.h>
#include <string.h>

#include "common.h"
#include "xmlconfig.h"
#include "unit.h"

int getCycleTime(char *recv, int len, char *result);
int getW01CCycleTime(char *recv, int len, char *result, int waterMode);
int setCycleTime(char *input, char *sendBuf);
int getSysTime(char *recv, int len, char *result);
int setSysTime(char *input, char *sendBuf);
int getErrState(enumPtr ePtr, char *recv, int len, char *result);
short bytes2Enum(enumPtr ptr, char *bytes, char **text, short len);
short text2Enum(enumPtr ptr, char *text, char **bytes, short *len);

void removeCompileList(compilePtr ptr)
{
    (void)ptr;
}

int compileCommand(devicePtr devices, unitPtr units)
{
    (void)devices;
    (void)units;
    return 1;
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

static int test_w01c(void)
{
    char heating[] = {0x30, 0x40, 0x01};
    char water[] = {0x33, 0x45, 0x01};
    char invalid[] = {0x37, 0x40, 0x02};
    char result[MAXBUF] = {0};

    if (!getW01CCycleTime(heating, 3, result, 0) ||
            strcmp(result, "06:00 - 08:00 Reduziert") != 0) return 0;
    if (!getW01CCycleTime(water, 3, result, 1) ||
            strcmp(result, "06:30 - 08:50 Oben") != 0) return 0;
    return getW01CCycleTime(invalid, 3, result, 0) == 0 &&
           getW01CCycleTime(heating, 2, result, 0) == 0;
}

static int test_numeric_units(void)
{
    static const char *types[] = {
        "char", "uchar", "short", "ushort", "int", "uint"
    };
    static const int lengths[] = {1, 1, 2, 2, 4, 4};
    char result[MAXBUF];
    char previous[8] = {0};

    for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); ++i) {
        Unit unit = {
            .name = "numeric",
            .type = (char *)types[i],
            .gCalc = "V+1",
            .sCalc = "V"
        };
        char bytes[8] = {5, 0, 0, 0};
        memset(result, 0, sizeof(result));
        if (procGetUnit(&unit, bytes, lengths[i], result, 0, previous) != 1 ||
                strncmp(result, "6.000000", 8) != 0) return 0;

        char input[32] = "7";
        short inputLength = (short)sizeof(input);
        if (procSetUnit(&unit, input, &inputLength, 0, previous) != 1 ||
                inputLength != lengths[i] ||
                (unsigned char)input[0] != 7) return 0;
    }

    Unit invalid = {.name = "invalid", .type = "unknown"};
    char byte = 0;
    return procGetUnit(&invalid, &byte, 1, result, 0, previous) == -1 &&
        procGetUnit(NULL, &byte, 1, result, 0, previous) == -1;
}

static int test_enum_units(void)
{
    char onByte = 1;
    char defaultByte = 0;
    Enumerate fallback = {
        .bytes = &defaultByte, .len = -1, .text = "unknown", .next = NULL
    };
    Enumerate on = {
        .bytes = &onByte, .len = 1, .text = "on", .next = &fallback
    };
    Unit unit = {.name = "switch", .type = "enum", .ePtr = &on};
    char result[MAXBUF] = {0};
    char previous[2] = {0};
    if (procGetUnit(&unit, &onByte, 1, result, 0, previous) != 1 ||
            strcmp(result, "on") != 0) return 0;

    char input[32] = "on";
    short length = (short)sizeof(input);
    return procSetUnit(&unit, input, &length, 0, previous) == 1 &&
        length == 1 && input[0] == onByte;
}

static int test_system_time(void)
{
    char encoded[8] = {0x20, 0x26, 0x07, 0x28, 0x02, 0x12, 0x34, 0x56};
    char result[MAXBUF] = {0};
    char invalid[4] = {0};
    Unit unit = {.name = "clock", .type = "systime"};
    if (procGetUnit(&unit, encoded, 8, result, 0, NULL) != 1 ||
            strstr(result, "2026-07-28T12:34:56") != result) return 0;
    return procGetUnit(&unit, invalid, 4, result, 0, NULL) == -1;
}

static int test_unit_failures_and_integer_calculations(void)
{
    char previous[8] = {0};
    char result[MAXBUF] = {0};
    char byte = 3;
    Unit integer = {
        .name = "bits", .type = "uchar",
        .gICalc = "B0+2", .sICalc = "5"
    };
    if (procGetUnit(&integer, &byte, 1, result, 0, previous) != 1 ||
            strcmp(result, "05") != 0) return 0;
    char input[32] = "ignored";
    short length = (short)sizeof(input);
    if (procSetUnit(&integer, input, &length, 0, previous) != 1 ||
            length != 1 || input[0] != 5) return 0;

    Unit small = {.name = "small", .type = "char", .sCalc = "V"};
    snprintf(input, sizeof(input), "128");
    length = (short)sizeof(input);
    if (procSetUnit(&small, input, &length, 0, previous) != -1) return 0;
    snprintf(input, sizeof(input), "not-a-number");
    length = (short)sizeof(input);
    if (procSetUnit(&small, input, &length, 0, previous) != -1) return 0;

    Unit noCalculation = {.name = "none", .type = "uchar"};
    snprintf(input, sizeof(input), "1");
    length = (short)sizeof(input);
    return procSetUnit(&noCalculation, input, &length, 0, previous) == -1;
}

static int test_special_unit_dispatch(void)
{
    char cycleBytes[8] = {0x30, 0x40, -1, -1, -1, -1, -1, -1};
    char w01c[3] = {0x30, 0x40, 2};
    char result[MAXBUF] = {0};
    Unit cycle = {.name = "cycle", .type = "cycletime"};
    Unit heating = {.name = "heating", .type = "heatingtime"};
    Unit water = {.name = "water", .type = "watertime"};
    return procGetUnit(&cycle, cycleBytes, 8, result, 0, NULL) == 1 &&
        procGetUnit(&heating, w01c, 3, result, 0, NULL) == 1 &&
        procGetUnit(&water, w01c, 3, result, 0, NULL) == 1;
}

static int test_enumeration_boundaries(void)
{
    char one = 1;
    Enumerate fallback = {
        .bytes = NULL, .len = -1, .text = "fallback"
    };
    Enumerate known = {
        .bytes = &one, .len = 1, .text = "known", .next = &fallback
    };
    char *text = NULL;
    char *bytes = NULL;
    short length = 0;
    char unknown = 9;
    if (!bytes2Enum(&known, &one, &text, 1) ||
            strcmp(text, "known") != 0) return 0;
    if (!bytes2Enum(&known, &unknown, &text, 1) ||
            strcmp(text, "fallback") != 0) return 0;
    if (bytes2Enum(&known, &one, &text, 0) != 0 ||
            bytes2Enum(NULL, &one, &text, 1) != 0) return 0;
    if (text2Enum(&known, "known", &bytes, &length) != 1 ||
            bytes != &one || length != 1) return 0;
    return text2Enum(&known, "missing", &bytes, &length) == 0;
}

static int test_time_setters(void)
{
    char bytes[16] = {0};
    char empty[] = "";
    char invalid[] = "not-a-time";
    char explicitTime[] = "2026-07-28T12:34:56+0200";
    if (setSysTime(empty, bytes) != 8) return 0;
    if (setSysTime(invalid, bytes) != 0) return 0;
    if (setSysTime(explicitTime, bytes) != 8) return 0;

    Unit unit = {.name = "clock", .type = "systime"};
    char input[64] = "2026-07-28T12:34:56+0200";
    short length = (short)sizeof(input);
    return procSetUnit(&unit, input, &length, 0, NULL) == 1 &&
        length == 8;
}

static int test_error_state(void)
{
    char code = 1;
    Enumerate error = {
        .bytes = &code, .len = 1, .text = "test-error"
    };
    char record[9] = {
        1, 0x20, 0x26, 0x07, 0x28, 0x02, 0x12, 0x34, 0x56
    };
    char result[MAXBUF] = {0};
    if (!getErrState(&error, record, sizeof(record), result) ||
            strstr(result, "test-error") == NULL) return 0;
    if (getErrState(&error, record, 8, result) != 0) return 0;
    Unit unit = {.name = "errors", .type = "errstate", .ePtr = &error};
    memset(result, 0, sizeof(result));
    return procGetUnit(&unit, record, sizeof(record), result, 0, NULL) == 1;
}

static int test_set_dispatch_and_ranges(void)
{
    char previous[8] = {0};
    char input[MAXBUF];
    short length;
    Unit cycle = {.name = "cycle", .type = "cycletime"};
    snprintf(input, sizeof(input), "06:00 08:00 - - - - - -");
    length = (short)sizeof(input);
    if (procSetUnit(&cycle, input, &length, 0, previous) != 1 ||
            length != 8) return 0;
    input[0] = '\0';
    length = (short)sizeof(input);
    if (procSetUnit(&cycle, input, &length, 0, previous) != -1) return 0;

    char enumByte = 1;
    Enumerate value = {.bytes = &enumByte, .len = 1, .text = "enabled"};
    Unit enumeration = {
        .name = "enum", .type = "enum", .ePtr = &value
    };
    input[0] = '\0';
    length = (short)sizeof(input);
    if (procSetUnit(&enumeration, input, &length, 0, previous) != -1)
        return 0;
    snprintf(input, sizeof(input), "missing");
    length = (short)sizeof(input);
    if (procSetUnit(&enumeration, input, &length, 0, previous) != -1)
        return 0;

    struct {
        const char *type;
        const char *tooLarge;
    } ranges[] = {
        {"char", "128"}, {"uchar", "256"}, {"short", "32768"},
        {"ushort", "65536"}, {"int", "2147483648"},
        {"uint", "4294967296"}
    };
    for (size_t i = 0; i < sizeof(ranges) / sizeof(ranges[0]); ++i) {
        Unit numeric = {
            .name = "range", .type = (char *)ranges[i].type, .sCalc = "V"
        };
        snprintf(input, sizeof(input), "%s", ranges[i].tooLarge);
        length = (short)sizeof(input);
        if (procSetUnit(&numeric, input, &length, 0, previous) != -1)
            return 0;
    }
    return 1;
}

static int test_conversion_failures(void)
{
    char byte[8] = {1};
    char previous[8] = {0};
    char result[MAXBUF] = {0};
    const char *types[] = {"short", "ushort", "int", "uint"};
    for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); ++i) {
        Unit unit = {.name = "short-buffer", .type = (char *)types[i]};
        if (procGetUnit(&unit, byte, 1, result, 0, previous) != -1)
            return 0;
    }
    Unit badFloat = {
        .name = "bad-float", .type = "uchar", .gCalc = "1/0"
    };
    Unit badInteger = {
        .name = "bad-integer", .type = "uchar", .gICalc = "1/0"
    };
    if (procGetUnit(&badFloat, byte, 1, result, 0, previous) != -1 ||
            procGetUnit(&badInteger, byte, 1, result, 0, previous) != -1)
        return 0;
    Unit missingEnum = {.name = "enum", .type = "enum"};
    return procGetUnit(&missingEnum, byte, 1, result, 0, previous) == -1;
}

int main(void)
{
    if (!test_legacy()) return fputs("legacy failed\n", stderr), 1;
    if (!test_extended()) return fputs("extended failed\n", stderr), 1;
    if (!test_invalid()) return fputs("invalid failed\n", stderr), 1;
    if (!test_w01c()) return fputs("w01c failed\n", stderr), 1;
    if (!test_numeric_units()) return fputs("numeric failed\n", stderr), 1;
    if (!test_enum_units()) return fputs("enum failed\n", stderr), 1;
    if (!test_system_time()) return fputs("system time failed\n", stderr), 1;
    if (!test_unit_failures_and_integer_calculations())
        return fputs("integer unit failed\n", stderr), 1;
    if (!test_special_unit_dispatch())
        return fputs("special dispatch failed\n", stderr), 1;
    if (!test_enumeration_boundaries())
        return fputs("enumeration boundaries failed\n", stderr), 1;
    if (!test_time_setters())
        return fputs("time setters failed\n", stderr), 1;
    if (!test_error_state())
        return fputs("error state failed\n", stderr), 1;
    if (!test_set_dispatch_and_ranges())
        return fputs("set dispatch failed\n", stderr), 1;
    if (!test_conversion_failures())
        return fputs("conversion failures failed\n", stderr), 1;
    return 0;
}
