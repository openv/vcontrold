// SPDX-FileCopyrightText: 2007-2016 The original vcontrold authors (cf. doc/original_authors.txt)
//
// SPDX-License-Identifier: GPL-3.0-or-later

// Conversion of units

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <syslog.h>
#include <unistd.h>
#include <termios.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <float.h>
#include <limits.h>
#include <math.h>

#if defined __linux__ || defined __CYGWIN__
#include <asm/byteorder.h>
#endif

#ifdef __CYGWIN__
#define __le64_to_cpu(x) (x)
#define __le32_to_cpu(x) (x)
#define __le16_to_cpu(x) (x)
#define __cpu_to_le64(x) (x)
#define __cpu_to_le32(x) (x)
#define __cpu_to_le16(x) (x)
#endif

#include "xmlconfig.h"
#include "common.h"
#include "arithmetic.h"

// We need this at procSet ...
#define FLOAT 1
#define INT 2

#if defined (__APPLE__)
// this is needed to emulate the linux API calls

#include <libkern/OSByteOrder.h>

#define __cpu_to_be64(x) OSSwapHostToBigInt64(x)
#define __cpu_to_le64(x) OSSwapHostToLittleInt64(x)
#define __cpu_to_be32(x) OSSwapHostToBigInt32(x)
#define __cpu_to_le32(x) OSSwapHostToLittleInt32(x)
#define __cpu_to_be16(x) OSSwapHostToBigInt16(x)
#define __cpu_to_le16(x) OSSwapHostToLittleInt16(x)

#define __be64_to_cpu(x) OSSwapBigToHostInt64(x)
#define __le64_to_cpu(x) OSSwapLittleToHostInt64(x)
#define __be32_to_cpu(x) OSSwapBigToHostInt32(x)
#define __le32_to_cpu(x) OSSwapLittleToHostInt32(x)
#define __be16_to_cpu(x) OSSwapBigToHostInt16(x)
#define __le16_to_cpu(x) OSSwapLittleToHostInt16(x)

#endif

int getCycleTime(char *recv, int len, char *result);
int getW01CCycleTime(char *recv, int len, char *result, int waterMode);
int setCycleTime(char *string, char *sendBuf);
short bytes2Enum(enumPtr ptr, char *bytes, char **text, short len);
short text2Enum(enumPtr ptr, char *text, char **bytes, short *len);
int getErrState(enumPtr ePtr, char *recv, int len, char *result);
int getSysTime(char *recv, int len, char *result);
int setSysTime(char *input, char *sendBuf);

static int parseCycleTimeToken(const char *token, unsigned char emptyValue,
                               unsigned char *value)
{
    unsigned int hour;
    unsigned int minute;
    char trailing;

    if (!token || !value) return 0;
    if (strcmp(token, "-") == 0 || strcmp(token, "--") == 0) {
        *value = emptyValue;
        return 1;
    }
    if (sscanf(token, "%u:%u%c", &hour, &minute, &trailing) != 2 ||
            hour > 23 || minute > 59 || minute % 10 != 0) {
        return 0;
    }
    *value = (unsigned char)((hour << 3) + minute / 10);
    return 1;
}

int getCycleTime(char *recv, int len, char *result)
{
    int i;
    size_t used = 0;

    if (!recv || !result || (len != 8 && len != 15)) {
        snprintf(result, MAXBUF, "Byte count is not 8 or 15");
        return 0;
    }
    result[0] = '\0';

    if (len == 8) {
        for (i = 0; i < len; i += 2) {
            int written;
            if ((unsigned char)recv[i] == 0xff) {
                written = snprintf(result + used, MAXBUF - used,
                                   "%d:An:--     Aus:--\n", i / 2 + 1);
            } else {
                written = snprintf(result + used, MAXBUF - used,
                                   "%d:An:%02d:%02d  Aus:%02d:%02d\n", i / 2 + 1,
                                   ((unsigned char)recv[i] & 0xf8) >> 3,
                                   ((unsigned char)recv[i] & 7) * 10,
                                   ((unsigned char)recv[i + 1] & 0xf8) >> 3,
                                   ((unsigned char)recv[i + 1] & 7) * 10);
            }
            if (written < 0 || (size_t)written >= MAXBUF - used) return 0;
            used += (size_t)written;
        }
    } else {
        for (i = 0; i < len; i += 3) {
            unsigned char on = (unsigned char)recv[i];
            unsigned char off = (unsigned char)recv[i + 1];
            unsigned char mode = (unsigned char)recv[i + 2] & 0x7f;
            char onTime[8] = "--";
            char offTime[8] = "--";
            int written;

            if (on != 0) {
                snprintf(onTime, sizeof(onTime), "%02d:%02d",
                         (on & 0xf8) >> 3, (on & 7) * 10);
            }
            if (off != 0) {
                snprintf(offTime, sizeof(offTime), "%02d:%02d",
                         (off & 0xf8) >> 3, (off & 7) * 10);
            }
            if (on == 0 && off == 0) {
                written = snprintf(result + used, MAXBUF - used,
                                   "%d:An:--     Aus:--     Mod:--\n", i / 3 + 1);
            } else {
                written = snprintf(result + used, MAXBUF - used,
                                   "%d:An:%-5s  Aus:%-5s  Mod:%02u\n",
                                   i / 3 + 1, onTime, offTime, mode);
            }
            if (written < 0 || (size_t)written >= MAXBUF - used) return 0;
            used += (size_t)written;
        }
    }
    if (used > 0) result[used - 1] = '\0';
    return 1;
}

int getW01CCycleTime(char *recv, int len, char *result, int waterMode)
{
    const char *mode;
    unsigned char start;
    unsigned char end;

    if (!recv || !result || len != 3) {
        if (result) snprintf(result, MAXBUF, "Byte count is not 3");
        return 0;
    }
    start = (unsigned char)recv[0];
    end = (unsigned char)recv[1];
    if ((start & 7) > 5 || (end & 7) > 5 ||
            (start >> 3) > 23 || (end >> 3) > 23) {
        snprintf(result, MAXBUF, "Invalid W01C cycle time");
        return 0;
    }
    switch ((unsigned char)recv[2]) {
    case 1:
        mode = waterMode ? "Oben" : "Reduziert";
        break;
    case 2:
        mode = "Normal";
        break;
    case 3:
        mode = "Sollwert2";
        break;
    default:
        mode = "Aus";
        break;
    }
    snprintf(result, MAXBUF, "%02u:%02u - %02u:%02u %s",
             start >> 3, (start & 7) * 10,
             end >> 3, (end & 7) * 10, mode);
    return 1;
}

int setCycleTime(char *input, char *sendBuf)
{
    char *tokens[15];
    char *save = NULL;
    char *token;
    int tokenCount = 0;

    if (!input || !sendBuf) return 0;
    for (token = strtok_r(input, " \t", &save); token && tokenCount < 15;
            token = strtok_r(NULL, " \t", &save)) {
        tokens[tokenCount++] = token;
    }
    if (token != NULL || (tokenCount != 8 && tokenCount != 15)) {
        snprintf(sendBuf, 16, "BadCount");
        return 0;
    }

    if (tokenCount == 8) {
        memset(sendBuf, 0xff, 8);
        for (int i = 0; i < 8; i += 2) {
            unsigned char on;
            unsigned char off;
            if (!parseCycleTimeToken(tokens[i], 0xff, &on) ||
                    !parseCycleTimeToken(tokens[i + 1], 0xff, &off) ||
                    ((on == 0xff) != (off == 0xff))) {
                snprintf(sendBuf, 16, "BadTime");
                return 0;
            }
            sendBuf[i] = (char)on;
            sendBuf[i + 1] = (char)off;
        }
        return 8;
    }

    memset(sendBuf, 0, 15);
    for (int i = 0; i < 15; i += 3) {
        unsigned char on;
        unsigned char off;
        char *end = NULL;
        long mode;
        if (!parseCycleTimeToken(tokens[i], 0, &on) ||
                !parseCycleTimeToken(tokens[i + 1], 0, &off)) {
            snprintf(sendBuf, 16, "BadTime");
            return 0;
        }
        if (on == 0 && off == 0) {
            if (strcmp(tokens[i + 2], "-") != 0 &&
                    strcmp(tokens[i + 2], "--") != 0) {
                snprintf(sendBuf, 16, "BadMode");
                return 0;
            }
            mode = 0;
        } else {
            errno = 0;
            mode = strtol(tokens[i + 2], &end, 0);
            if (errno || end == tokens[i + 2] || *end || mode < 0 || mode > 0x7f) {
                snprintf(sendBuf, 16, "BadMode");
                return 0;
            }
        }
        sendBuf[i] = (char)on;
        sendBuf[i + 1] = (char)off;
        sendBuf[i + 2] = (char)mode;
    }
    return 15;
}

int bcd2dec(int bcd) {
    int dec = bcd - ((int)(bcd / 16) * 6);
    return dec;
}

int getSysTime(char *recv, int len, char *result)
{
    struct tm *t;
    time_t tt;

    if (len != 8) {
        sprintf(result, "System time: Len <> 8 bytes");
        return 0;
    }

    // Use timezone information from the host system
    time(&tt);
    t = localtime(&tt);
    t->tm_year = bcd2dec(recv[0]) * 100 + bcd2dec(recv[1]) - 1900;
    t->tm_mon = bcd2dec(recv[2]) - 1;
    t->tm_mday = bcd2dec(recv[3]);
    t->tm_wday = bcd2dec(recv[4]) % 7;
    t->tm_hour = bcd2dec(recv[5]);
    t->tm_min = bcd2dec(recv[6]);
    t->tm_sec = bcd2dec(recv[7]);

    // This might break existing applications. But changing the string to some custom format
    // that is not recognized by strptime for parsing dates has a symmetry impact that the
    // format for getTime is different from the format required by setTime.
    // I would consider the command symmetry more important for the future.
    strftime(result, 48, "%FT%T%z", t);

    return 1;
}

int setSysTime(char *input, char *sendBuf)
{
    char systime[80];
    time_t tt;
    struct tm t_in = {0};
    struct tm *t;
    struct tm *th;

    memset(systime, 0, sizeof(systime));

    time(&tt);
    th = localtime(&tt);

    // No parameter, set the current system time
    if (!*input) {
        t = th;
    } else {
#ifdef _XOPEN_SOURCE
        char *parseEnd = strptime(input, "%FT%T%z", &t_in);
        // If the string is fully parsed, parseEnd should be the terminating '0' character of the input string.
        if (!parseEnd || *parseEnd) {
            logIT(LOG_ERR, "Can not parse time string '%s'. Use the same ISO 8601 time format for setting a time as you get when getting a time.", input);
            return 0;
        }
#else
        logIT1(LOG_ERR, "Setting an explicit time is not supported yet");
        return 0;
#endif

        // Recalculate the input time adjusted to the hosts timezone.
        // It is probably not the best idea to use internal variables, but
        // there doesn't seem to be an efficient way to transform times
        // between timezones.
#if defined(__CYGWIN__) || defined(__APPLE__)
        t_in.tm_sec += (th->tm_gmtoff - t_in.tm_gmtoff);
        t_in.tm_gmtoff = th->tm_gmtoff;
#else
        t_in.tm_sec += (th->__tm_gmtoff - t_in.__tm_gmtoff);
        t_in.__tm_gmtoff = th->__tm_gmtoff;
#endif
        t_in.tm_isdst = th->tm_isdst;
        mktime(&t_in);

        t = &t_in;
    }

    int year = t->tm_year + 1900;
    snprintf(systime, sizeof(systime), "%02d %02d %02d %02d %02d %02d %02d %02d",
             year / 100, year % 100, t->tm_mon + 1, t->tm_mday,
             t->tm_wday, t->tm_hour, t->tm_min, t->tm_sec);
    return string2chr(systime, sendBuf, 8);
}

int getErrState(enumPtr ePtr, char *recv, int len, char *result)
{
    int i;
    char *errtext;
    char systime[35];
    char string[300];
    char *ptr;

    if (len % 9) {
        sprintf(result, "Bytes count is not mod 9");
        return 0;
    }

    for (i = 0; i < len; i += 9) {
        ptr = recv + i;
        memset(string, 0, sizeof(string));
        memset(systime, 0, sizeof(systime));
        // Error code: Byte 0
        if (bytes2Enum(ePtr, ptr, &errtext, 1))
            // Rest SysTime
            if (getSysTime(ptr + 1, 8, systime)) {
                snprintf(string, sizeof(string),
                         "%s %s (%02X)\n", systime, errtext, (unsigned char) *ptr);
                strcat(result, string);
                continue;
            }
        // Format error
        sprintf(result, "%s %s", errtext, systime);
        return 0;
    }

    result[strlen(result) - 1] = '\0'; // Remove \n
    return 1;
}

short bytes2Enum(enumPtr ptr, char *bytes, char **text, short len)
{
    enumPtr ePtr = NULL;
    char string[200];

    if (! len) {
        return 0;
    }

    // Search for the appropriate enum and return the value
    if (! (ePtr = getEnumNode(ptr, bytes, len))) {
        // We search for the default
        ePtr = getEnumNode(ptr, bytes, -1);
    }
    if (ePtr) {
        *text = ePtr->text;
        memset(string, 0, sizeof(string));
        char2hex(string, sizeof(string), bytes, len);
        strcat(string, " -> ");
        strcat(string, ePtr->text);
        logIT1(LOG_INFO, string);
        return 1;
    } else {
        return 0;
    }
}

short text2Enum(enumPtr ptr, char *text, char **bytes, short *len)
{
    enumPtr ePtr = NULL;
    char string[200];
    char string2[1000];

    // Search for the appropriate enum and return the value
    if (! (ePtr = getEnumNode(ptr, text, 0))) {
        return 0;
    }

    *bytes = ePtr->bytes;
    *len = ePtr->len;
    memset(string, 0, sizeof(string));
    snprintf(string, sizeof(string), "%s", text);
    strcat(string, " -> ");
    memset(string2, 0, sizeof(string2));
    char2hex(string2, sizeof(string2), ePtr->bytes, ePtr->len);
    strcat(string, string2);
    logIT1(LOG_INFO, string);

    return *len;
}

int procGetUnit(unitPtr uPtr, char *recvBuf, int recvLen, char *result, char bitpos, char *pRecvPtr)
{
    char string[256];
    char error[1000];
    char buffer[MAXBUF];
    char *errPtr = error;
    //short t;
    double erg;
    int ergI;
    int hexWidth = 0;
    double floatV = 0;
    char *inPtr;
    char *tPtr;
    // Here the types for the conversion in <type> tags
    int8_t charV;
    uint8_t ucharV;
    int16_t shortV;
    int16_t tmpS;
    uint16_t ushortV;
    uint16_t tmpUS;
    int32_t intV;
    int32_t tmpI;
    uint32_t tmpUI;
    uint32_t uintV;

    memset(errPtr, 0, sizeof(error));

    if (!uPtr || !uPtr->type || !recvBuf || recvLen < 0 || !result) {
        logIT1(LOG_ERR, "Invalid unit conversion arguments");
        return -1;
    }

    // We tread the different <type> entries
    if (strstr(uPtr->type, "cycletime") == uPtr->type) {
        // Cycle time
        if (getCycleTime(recvBuf, recvLen, result)) {
            return 1;
        } else {
            return -1;
        }
    } else if (strstr(uPtr->type, "heatingtime") == uPtr->type) {
        return getW01CCycleTime(recvBuf, recvLen, result, 0) ? 1 : -1;
    } else if (strstr(uPtr->type, "watertime") == uPtr->type) {
        return getW01CCycleTime(recvBuf, recvLen, result, 1) ? 1 : -1;
    } else if (strstr(uPtr->type, "systime") == uPtr->type) {
        // System time
        if (getSysTime(recvBuf, recvLen, result)) {
            return 1;
        } else {
            return -1;
        }
    } else if (strstr(uPtr->type, "errstate") == uPtr->type) {
        // Error code + System time
        if (getErrState(uPtr->ePtr, recvBuf, recvLen, result)) {
            return 1;
        } else {
            return -1;
        }
    } else if (strstr(uPtr->type, "enum") == uPtr->type) {
        // enum
        if (bytes2Enum(uPtr->ePtr, recvBuf, &tPtr, recvLen)) {
            strcpy(result, tPtr);
            return 1;
        } else {
            sprintf(result, "Didn't find an appropriate enum");
            return -1;
        }
    }

    // Here are all the numeric types
    if (strstr(uPtr->type, "char") == uPtr->type) {
        if (recvLen < 1) return -1;
        // Conversion to Char 1Byte
        memcpy(&charV, recvBuf, 1);
        floatV = charV; // Implicit type conversion to float for our arithmetic
        hexWidth = 2;
    } else if (strstr(uPtr->type, "uchar") == uPtr->type) {
        if (recvLen < 1) return -1;
        // Conversion to Unsigned Char 1Byte
        memcpy(&ucharV, recvBuf, 1);
        floatV = ucharV; // Implicit type conversion to float for our arithmetic
        hexWidth = 2;
    } else if (strstr(uPtr->type, "short") == uPtr->type) {
        if (recvLen < 2) return -1;
        // Conversion to Short 2Byte
        memcpy(&tmpS, recvBuf, 2);
        // According to the CPU, the conversion is done here
        shortV = __le16_to_cpu(tmpS);
        floatV = shortV; // Implicit type conversion to float for our arithmetic
        hexWidth = 4;
    } else if (strstr(uPtr->type, "ushort") == uPtr->type) {
        if (recvLen < 2) return -1;
        // Conversion to  Short 2Byte
        memcpy(&tmpUS, recvBuf, 2);
        // According to the CPU, the conversion is done here
        ushortV = __le16_to_cpu(tmpUS);
        floatV = ushortV; // Implicit type conversion to float for our arithmetic
        hexWidth = 4;
    } else if (strstr(uPtr->type, "int") == uPtr->type) {
        if (recvLen < 4) return -1;
        // Conversion to Int 4Byte
        memcpy(&tmpI, recvBuf, 4);
        // According to the CPU, the conversion is done here
        intV = __le32_to_cpu(tmpI);
        floatV = intV; // Implicit type conversion to float for our arithmetic
        hexWidth = 8;
    } else if (strstr(uPtr->type, "uint") == uPtr->type) {
        if (recvLen < 4) return -1;
        // Conversion to Unsigned Int 4Byte
        memcpy(&tmpUI, recvBuf, 4);
        // According to the CPU, the conversion is done here
        uintV = __le32_to_cpu(tmpUI);
        floatV = uintV; // Implicit type conversion to float for our arithmetic
        hexWidth = 8;
    } else if (uPtr->type) {
        logIT(LOG_ERR, "Unknown type %s in unit %s", uPtr->type, uPtr->name);
        return -1;
    }

    // Some logging
    int n;
    char *ptr;
    char res;

    ptr = recvBuf;
    memset(buffer, 0, sizeof(buffer));
    for (n = 0; n < recvLen && n < 16; n++) {
        // The bytes 0..9 are of interest
        memset(string, 0, sizeof(string));
        unsigned char byte = *ptr++ & 255;
        snprintf(string, sizeof(string), "B%01X:%02X ", n, byte);
        strcat(buffer, string);
        if (n >= MAXBUF - 3) {
            break;
        }
    }

    if (uPtr->gCalc && *uPtr->gCalc) {
        // calc in XML and get defined within
        logIT(LOG_INFO, "Typ: %s (in float: %f)", uPtr->type, floatV);
        inPtr = uPtr->gCalc;
        logIT(LOG_INFO, "(FLOAT) Exp: %s [%s]", inPtr, buffer);
        erg = execExpression(&inPtr, (unsigned char *)recvBuf, recvLen,
                             floatV, errPtr);
        if (*errPtr) {
            logIT(LOG_ERR, "Exec %s: %s", uPtr->gCalc, error);
            strcpy(result, string);
            return -1;
        }
        sprintf(result, "%f%s%s", erg, uPtr->entity ? " " : "",
                uPtr->entity ? uPtr->entity : "");
    } else if (uPtr->gICalc && *uPtr->gICalc) {
        // icalc in XML and get defined within
        inPtr = uPtr->gICalc;
        logIT(LOG_INFO, "(INT) Exp: %s [BP:%d] [%s]", inPtr, bitpos, buffer);
        ergI = execIExpression(&inPtr, (unsigned char *)recvBuf, recvLen,
                               bitpos, pRecvPtr, errPtr);
        if (*errPtr) {
            logIT(LOG_ERR, "Exec %s: %s", uPtr->gCalc, error);
            strcpy(result, string);
            return -1;
        }
        logIT(LOG_INFO, "Res: (Hex max. 4 bytes) %08x", ergI);
        res = (char)(ergI & 0xff);
        if (uPtr->ePtr && bytes2Enum(uPtr->ePtr, &res, &tPtr, 1)) {
            strcpy(result, tPtr);
            return 1;
        } else {
            snprintf(result, MAXBUF, "%0*X%s%s", hexWidth, (unsigned int)ergI,
                     uPtr->entity ? " " : "",
                     uPtr->entity ? uPtr->entity : "");
            return 1;
        }
        // Probably do the enum search here
    }
    return 1;
}

int procSetUnit(unitPtr uPtr, char *sendBuf, short *sendLen, char bitpos, char *pRecvPtr)
{
    char string[256];
    char error[1000];
    char buffer[MAXBUF];
    char input[MAXBUF];
    char *errPtr = error;
    // short t;
    double erg = 0.0;
    int ergI = 0;
    short count;
    char ergType = 0;
    double floatV;
    char *inPtr;
    // Here the types for the <type> tag conversion
    int8_t charV;
    uint8_t ucharV;
    int16_t shortV;
    int16_t tmpS;
    uint16_t ushortV;
    uint16_t tmpUS;
    int32_t intV;
    int32_t tmpI;
    uint32_t tmpUI;
    uint32_t uintV;

    memset(errPtr, 0, sizeof(error));
    // Some logging
    int n = 0;
    char *ptr;
    char dumBuf[10];
    memset(dumBuf, 0, sizeof(dumBuf));
    memset(buffer, 0, sizeof(buffer));
    if (!uPtr || !uPtr->type || !sendBuf || !sendLen || *sendLen < 0) {
        logIT1(LOG_ERR, "Invalid set-unit arguments");
        return -1;
    }
    // We copy the sendBuf, as this one is also used for return
    snprintf(input, sizeof(input), "%s", sendBuf);
    memset(sendBuf, 0, *sendLen);

    if (strstr(uPtr->type, "cycletime") == uPtr->type) {
        // Cycle time
        if (! *input) {
            return -1;
        }
        if (! (*sendLen = setCycleTime(input, sendBuf))) {
            return -1;
        } else  {
            return 1;
        }
    }
    if (strstr(uPtr->type, "systime") == uPtr->type) {
        // System time
        if (! (*sendLen = setSysTime(input, sendBuf))) {
            return -1;
        } else  {
            return 1;
        }
    } else if (strstr(uPtr->type, "enum") == uPtr->type) {
        // enum
        if (! *input)
        { return -1; }
        if (!(count = text2Enum(uPtr->ePtr, input, &ptr, sendLen))) {
            sprintf(sendBuf, "Did not find an appropriate enum");
            return -1;
        } else {
            memcpy(sendBuf, ptr, count);
            return 1;
        }
    }

    if (! *input) {
        return -1;
    }

    // Here the forwarded value
    if (uPtr->sCalc && *uPtr->sCalc) {
        // calc in XML and get defined within
        char *end = NULL;
        errno = 0;
        floatV = strtod(input, &end);
        while (end && isspace((unsigned char)*end)) end++;
        if (errno == ERANGE || !end || end == input || *end || !isfinite(floatV)) {
            logIT(LOG_ERR, "Invalid numeric value: %s", input);
            return -1;
        }
        inPtr = uPtr->sCalc;
        logIT(LOG_INFO, "Send Exp: %s [V=%f]", inPtr, floatV);
        erg = execExpression(&inPtr, (unsigned char *)dumBuf, sizeof(dumBuf),
                             floatV, errPtr);
        if (*errPtr) {
            logIT(LOG_ERR, "Exec %s: %s", uPtr->sCalc, error);
            strcpy(sendBuf, string);
            return -1;
        }
        ergType = FLOAT;
    }

    if (uPtr->sICalc && *uPtr->sICalc) {
        // icalc in XML and get defined within
        inPtr = uPtr->sICalc;
        if (uPtr->ePtr) {
            // There are enums
            if (! (count = text2Enum(uPtr->ePtr, input, &ptr, sendLen))) {
                sprintf(sendBuf, "Did not find an appropriate enum");
                return -1;
            } else {
                memset(dumBuf, 0, sizeof(dumBuf));
                memcpy(dumBuf, ptr, count);
            }
        }

        logIT(LOG_INFO, "(INT) Exp: %s [BP:%d]", inPtr, bitpos);
        ergI = execIExpression(&inPtr, (unsigned char *)dumBuf, sizeof(dumBuf),
                               bitpos, pRecvPtr, errPtr);
        if (*errPtr) {
            logIT(LOG_ERR, "Exec %s: %s", uPtr->sICalc, error);
            strcpy(sendBuf, string);
            return -1;
        }
        ergType = INT;
        snprintf(string, sizeof(string), "Res: (Hex max. 4 bytes) %08x", ergI);
    }

    if (!ergType) {
        logIT(LOG_ERR, "Unit %s has no set calculation", uPtr->name);
        return -1;
    }

    double value = (ergType == FLOAT) ? (double)erg : (double)ergI;
    if (!isfinite(value)) {
        logIT1(LOG_ERR, "Set calculation produced a non-finite value");
        return -1;
    }

    // The result is in erg and has to be converted according to the type
    if (uPtr->type) {
        if (strstr(uPtr->type, "char") == uPtr->type) {
            if (value < INT8_MIN || value > INT8_MAX) return -1;
            // Conversion to Short 2Byte
            // According to the CPU, the conversion is done here
            (ergType == FLOAT) ? (charV = erg) : (charV = ergI);
            memcpy(sendBuf, &charV, 1);
            *sendLen = 1;
        } else if (strstr(uPtr->type, "uchar") == uPtr->type) {
            if (value < 0 || value > UINT8_MAX) return -1;
            // According to the CPU, the conversion is done here
            (ergType == FLOAT) ? (ucharV = erg) : (ucharV = ergI);
            memcpy(sendBuf, &ucharV, 1);
            *sendLen = 1;
        } else if (strstr(uPtr->type, "short") == uPtr->type) {
            if (value < INT16_MIN || value > INT16_MAX) return -1;
            // According to the CPU, the conversion is done here
            (ergType == FLOAT) ? (tmpS = erg) : (tmpS = ergI);
            shortV = __cpu_to_le16(tmpS);
            memcpy(sendBuf, &shortV, 2);
            *sendLen = 2;
        } else if (strstr(uPtr->type, "ushort") == uPtr->type) {
            if (value < 0 || value > UINT16_MAX) return -1;
            // According to the CPU, the conversion is done here
            (ergType == FLOAT) ? (tmpUS = erg) : (tmpUS = ergI);
            ushortV = __cpu_to_le16(tmpUS);
            memcpy(sendBuf, &ushortV, 2);
            *sendLen = 2;
        } else if (strstr(uPtr->type, "int") == uPtr->type) {
            if (value < INT32_MIN || value > INT32_MAX) return -1;
            // According to the CPU, the conversion is done here
            (ergType == FLOAT) ? (tmpI = erg) : (tmpI = ergI);
            intV = __cpu_to_le32(tmpI);
            memcpy(sendBuf, &intV, 4);
            *sendLen = 4;
        } else if (strstr(uPtr->type, "uint") == uPtr->type) {
            if (value < 0 || value > UINT32_MAX) return -1;
            // According to the CPU, the conversion is done here
            (ergType == FLOAT) ? (tmpUI = erg) : (tmpUI = ergI);
            uintV = __cpu_to_le32(tmpUI);
            memcpy(sendBuf, &uintV, 4);
            *sendLen = 4;
        } else if (uPtr->type) {
            memset(string, 0, sizeof(string));
            logIT(LOG_ERR, "Unknown type %s in unit %s", uPtr->type, uPtr->name);
            return -1;
        }

        memset(buffer, 0, sizeof(buffer));
        ptr = sendBuf;
        for (n = 0; n < *sendLen && (size_t)(n * 3 + 1) < sizeof(buffer); n++) {
            memset(string, 0, sizeof(string));
            unsigned char byte = *ptr++ & 255;
            snprintf(string, sizeof(string), "%02X ", byte);
            strcat(buffer, string);
        }

        logIT(LOG_INFO, "Type: %s (bytes: %s)  ", uPtr->type, buffer);

        return 1;
    }

    // We should never reach here. But we want to keep the compiler happy.
    return 0;
}
