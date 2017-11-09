/*  Copyright 2007-2017 the original vcontrold development team

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/* Umrechnung von Einheiten */

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

#ifdef __linux__
#include <asm/byteorder.h>
#endif

#ifdef __CYGWIN__
#include "byteorder.h"
#endif

#include "xmlconfig.h"
#include "common.h"
#include "arithmetic.h"

// brauchen wir bei procSet ...
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
int setCycleTime(char *string, char *sendBuf);
short bytes2Enum(enumPtr ptr, char *bytes, char **text, short len);
short text2Enum(enumPtr ptr, char *text, char **bytes, short *len);
int getErrState(enumPtr ePtr, char *recv, int len, char *result);
int getSysTime(char *recv, int len, char *result);
int setSysTime(char *input, char *sendBuf, short bufsize);

int getCycleTime(char *recv, int len, char *result)
{
    int i;
    char string[80];

    // if ((len/2)*2 !=len) {
    if (len % 2) {
        sprintf(result, "Anzahl Bytes ungerade");
        return (0);
    }

    bzero(string, sizeof(string));

    for (i = 0; i < len; i += 2) {
        if (recv[i] == (char)0xff) {
            snprintf(string, sizeof(string), "%d:An:--     Aus:--\n", (i / 2) + 1);
        } else {
            snprintf(string, sizeof(string), "%d:An:%02d:%02d  Aus:%02d:%02d\n", (i / 2) + 1,
                     (recv[i] & 0xF8) >> 3, (recv[i] & 7) * 10,
                     (recv[i + 1] & 0xF8) >> 3, (recv[i + 1] & 7) * 10);
        }
        strcat(result, string);
    }
    result[strlen(result) - 1] = '\0'; // \n verdampfen

    return 1;
}

int setCycleTime(char *input, char *sendBuf)
{
    char *sptr, *cptr;
    char *bptr = sendBuf;
    int hour, min;
    int count = 0;

    // wir trennen beim Blank
    sptr = strtok(input, " ");
    cptr = NULL;

    // zuerst fuellen wir den sendBuf mit 8 x ff
    for (count = 0; count < 8; sendBuf[count++] = 0xff);
    count = 0;

    do {
        if (sptr < cptr) {
            // da waren wir schon (durch doppelte Blank)
            continue;
        }

        while (isblank(*sptr)) {
            sptr++;
        }
        // besteht der String nur aus ein oder zwei Minus? -> diese Zeit bleibt leer
        if ((0 == strcmp(sptr, "-")) ||  (0 == strcmp(sptr, "--"))) {
            // Wir überspringen die nachste Zeitangabe, da die ja auch "-" sein muß
            bptr++;
            count++;
            sptr = strtok(NULL, " ");
            logIT(LOG_INFO, "Cycle Time: -- -- -> [%02X%02X]", 0xff, 0xff);
        } else {
            // haben wir einen Doppelpunkt im String?
            if (! strchr(sptr, ':')) {
                sprintf(sendBuf, "Falsches Zeitformat: %s", sptr);
                return 0;
            }
            sscanf(sptr, "%i:%i", &hour, &min);
            *bptr = ((hour << 3) + (min / 10)) & 0xff;
            logIT(LOG_INFO, "Cycle Time: %02d:%02d -> [%02X]", hour, min, (unsigned char) *bptr);
        }
        bptr++;
        cptr = sptr;
        count++;

    } while ((sptr = strtok(NULL, " ")) != NULL);

    if ((count / 2) * 2 != count) {
        logIT(LOG_WARNING, "Anzahl Zeiten ungerade, ignoriere %s", cptr);
        *(bptr - 1) = 0xff;
    }

    return 8;
}

int getSysTime(char *recv, int len, char *result)
{
    char day[3];
    if (len != 8) {
        sprintf(result, "Systemzeit: Len <>8 Bytes");
        return (0);
    }
    switch (recv[4]) {
    case 0:
        strcpy(day, "So");
        break;
    case 1:
        strcpy(day, "Mo");
        break;
    case 2:
        strcpy(day, "Di");
        break;
    case 3:
        strcpy(day, "Mi");
        break;
    case 4:
        strcpy(day, "Do");
        break;
    case 5:
        strcpy(day, "Fr");
        break;
    case 6:
        strcpy(day, "Sa");
        break;
    case 7:
        strcpy(day, "So");
        break;
    default:
        sprintf(result, "Fehler Tagwandlung: %02X", recv[4]);
        return (0);
    }

    sprintf(result, "%s,%02X.%02X.%02X%02X %02x:%02X:%02X", day,
            recv[3], recv[2], recv[0], recv[1],
            recv[5], recv[6], recv[7]);

    return 1;
}

int setSysTime(char *input, char *sendBuf, short bufsize)
{
    char systime[80];
    time_t tt;
    struct tm *t;

    // kein Parameter, setze aktuelle Systemzeit
    if (!*input) {
        time(&tt);
        t = localtime(&tt);
        bzero(systime, sizeof(systime));
        sprintf(systime, "%04d  %02d %02d %02d %02d %02d %02d", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_wday, t->tm_hour, t->tm_min, t->tm_sec);
        /* wir fummeln uns nun ein Blank zwischen die Jahreszahl */
        systime[4] = systime[3];
        systime[3] = systime[2];
        systime[2] = ' ';
        logIT(LOG_INFO, "aktuelle Sys.Zeit %s", systime);
        return (string2chr(systime, sendBuf, bufsize));
    } else {
        logIT1(LOG_ERR, "Setzen von explizieter Zeit noch nicht unterstuetzt");
        return (0);
    }
}

int getErrState(enumPtr ePtr, char *recv, int len, char *result)
{
    int i;
    char *errtext;
    char systime[35];
    char string[300];
    char *ptr;

    if (len % 9) {
        sprintf(result, "Anzahl Bytes nicht mod 9");
        return (0);
    }

    for (i = 0; i < len; i += 9) {
        ptr = recv + i;
        bzero(string, sizeof(string));
        bzero(systime, sizeof(systime));
        // Fehlercode: Byte 0
        if (bytes2Enum(ePtr, ptr, &errtext, 1))
            // Rest SysTime
            if (getSysTime(ptr + 1, 8, systime)) {
                snprintf(string, sizeof(string), "%s %s (%02X)\n", systime, errtext, (unsigned char) *ptr);
                strcat(result, string);
                continue;
            }
        // Formatfehler
        sprintf(result, "%s %s", errtext, systime);
        return (0);
    }

    result[strlen(result) - 1] = '\0'; // \n verdampfen
    return 1;
}

short bytes2Enum(enumPtr ptr, char *bytes, char **text, short len)
{
    enumPtr ePtr = NULL;
    char string[200];
    if (! len) {
        return 0;
    }
    // suche die passende Enum und gebe den Wert zurueck
    if (! (ePtr = getEnumNode(ptr, bytes, len))) {
        // wir suchen nach dem Default
        ePtr = getEnumNode(ptr, bytes, -1);
    }
    if (ePtr) {
        *text = ePtr->text;
        bzero(string, sizeof(string));
        char2hex(string, bytes, len);
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
    // suche die passende Enum und gebe den Wert zurueck
    if (! (ePtr = getEnumNode(ptr, text, 0))) {
        return 0;
    }

    *bytes = ePtr->bytes;
    *len = ePtr->len;
    bzero(string, sizeof(string));
    strncpy(string, text, sizeof(string));
    strcat(string, " -> ");
    bzero(string2, sizeof(string2));
    char2hex(string2, ePtr->bytes, ePtr->len);
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
    // short t;
    float erg;
    int ergI;
    char formatI[20];
    float floatV = 0;
    char *inPtr;
    char *tPtr;
    // hier die Typen fuer die Umrechnung in <type> Tag
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

    bzero(errPtr, sizeof(error));

    // wir behandeln die verschiedenen <type> Eintraege

    if (strstr(uPtr->type, "cycletime") == uPtr->type) {
        // Schaltzeit
        if (getCycleTime(recvBuf, recvLen, result)) {
            return 1;
        } else {
            return -1;
        }
    } else if (strstr(uPtr->type, "systime") == uPtr->type) {
        // Systemzeit
        if (getSysTime(recvBuf, recvLen, result)) {
            return 1;
        } else {
            return -1;
        }
    } else if (strstr(uPtr->type, "errstate") == uPtr->type) {
        // Errrocode + Systemzeit
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
            sprintf(result, "Kein passendes Enum gefunden");
            return -1;
        }
    }

    // hier kommen die ganzen numerischen Typen

    if (strstr(uPtr->type, "char") == uPtr->type) {
        // Umrechnung in Char 1Byte
        memcpy(&charV, recvBuf, 1);
        floatV = charV; // impliziete Typumnwandlung nach float fuer unsere Arithmetic
        sprintf(formatI, "%%02X %%s");
    } else if (strstr(uPtr->type, "uchar") == uPtr->type) {
        // Umrechnung in Unsigned Char 1Byte
        memcpy(&ucharV, recvBuf, 1);
        floatV = ucharV; // impliziete Typumnwandlung nach float fuer unsere Arithmetic
        sprintf(formatI, "%%02X %%s");
    } else if (strstr(uPtr->type, "short") == uPtr->type) {
        // Umrechnung in Short 2Byte
        memcpy(&tmpS, recvBuf, 2);
        // je nach CPU Typ wird hier die Wandlung vorgenommen
        shortV = __le16_to_cpu(tmpS);
        floatV = shortV; // impliziete Typumnwandlung nach float fuer unsere Arithmetic
        sprintf(formatI, "%%04X %%s");
    } else if (strstr(uPtr->type, "ushort") == uPtr->type) {
        // Umrechnung in Short 2Byte
        memcpy(&tmpUS, recvBuf, 2);
        // je nach CPU Typ wird hier die Wandlung vorgenommen
        ushortV = __le16_to_cpu(tmpUS);
        floatV = ushortV; // impliziete Typumnwandlung nach float fuer unsere Arithmetic
        sprintf(formatI, "%%04X %%s");
    } else if (strstr(uPtr->type, "int") == uPtr->type) {
        // Umrechnung in Int 4Byte
        memcpy(&tmpI, recvBuf, 4);
        // je nach CPU Typ wird hier die Wandlung vorgenommen
        intV = __le32_to_cpu(tmpI);
        floatV = intV; // impliziete Typumnwandlung nach float fuer unsere Arithmetic
        sprintf(formatI, "%%08X %%s");
    } else if (strstr(uPtr->type, "uint") == uPtr->type) {
        // Umrechnung in Unsigned Int 4Byte
        memcpy(&tmpUI, recvBuf, 4);
        // je nach CPU Typ wird hier die Wandlung vorgenommen
        uintV = __le32_to_cpu(tmpUI);
        floatV = uintV; // impliziete Typumnwandlung nach float fuer unsere Arithmetic
        sprintf(formatI, "%%08X %%s");
    } else if (uPtr->type) {
        logIT(LOG_ERR, "Unbekannter Typ %s in Unit %s", uPtr->type, uPtr->name);
        return -1;
    }

    // etwas logging
    int n;
    char *ptr;
    char res;
    ptr = recvBuf;
    bzero(buffer, sizeof(buffer));
    for (n = 0; n <= 9; n++) {
        // Bytes 0..9 sind von Interesse
        bzero(string, sizeof(string));
        unsigned char byte = *ptr++ & 255;
        snprintf(string, sizeof(string), "B%d:%02X ", n, byte);
        strcat(buffer, string);
        if (n >= MAXBUF - 3) {
            break;
        }
    }

    if (uPtr->gCalc && *uPtr->gCalc) {
        // calc im XML und get darin definiert
        logIT(LOG_INFO, "Typ: %s (in float: %f)", uPtr->type, floatV);
        inPtr = uPtr->gCalc;
        logIT(LOG_INFO, "(FLOAT) Exp:%s [%s]", inPtr, buffer);
        erg = execExpression(&inPtr, recvBuf, floatV, errPtr);
        if (*errPtr) {
            logIT(LOG_ERR, "Exec %s: %s", uPtr->gCalc, error);
            strcpy(result, string);
            return -1;
        }
        sprintf(result, "%f %s", erg, uPtr->entity);
    } else if (uPtr->gICalc && *uPtr->gICalc) {
        // icalc im XML und get darin definiert
        inPtr = uPtr->gICalc;
        logIT(LOG_INFO, "(INT) Exp:%s [BP:%d] [%s]", inPtr, bitpos, buffer);
        ergI = execIExpression(&inPtr, recvBuf, bitpos, pRecvPtr, errPtr);
        if (*errPtr) {
            logIT(LOG_ERR, "Exec %s: %s", uPtr->gCalc, error);
            strcpy(result, string);
            return -1;
        }
        logIT(LOG_INFO, "Erg: (Hex max. 4Byte) %08x", ergI);
        res = ergI;
        if ( uPtr->ePtr && bytes2Enum(uPtr->ePtr, &res, &tPtr, recvLen)) {
            strcpy(result, tPtr);
            return 1;
        } else {
            sprintf(result, formatI, ergI, uPtr->entity);
            return 1;
        }
        // hier noch das durchsuchen der enums ggf durchfuehren
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
    float erg = 0.0;
    int ergI = 0;
    short count;
    char ergType;
    float floatV;
    char *inPtr;
    // hier die Typen fuer die Umrechnung in <type> Tag
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

    bzero(errPtr, sizeof(error));
    // etwas logging
    int n = 0;
    char *ptr;
    char dumBuf[10];
    bzero(dumBuf, sizeof(dumBuf));
    bzero(buffer, sizeof(buffer));
    // wir kopieren uns den sendBuf, da dieser auch als return genutzt wird
    strncpy(input, sendBuf, sizeof(input));
    bzero(sendBuf, sizeof(sendBuf));

    if (strstr(uPtr->type, "cycletime") == uPtr->type) {
        // Schaltzeit
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
        // Schaltzeit
        if (! (*sendLen = setSysTime(input, sendBuf, *sendLen))) {
            return -1;
        } else  {
            return 1;
        }
    } else if (strstr(uPtr->type, "enum") == uPtr->type) {
        // enum
        if (! *input)
        { return -1; }
        if (!(count = text2Enum(uPtr->ePtr, input, &ptr, sendLen))) {
            sprintf(sendBuf, "Kein passendes Enum gefunden");
            return -1;
        } else {
            memcpy(sendBuf, ptr, count);
            return 1;
        }
    }

    if (! *input) {
        return -1;
    }

    // hier der uebergebene Wert
    if (uPtr->sCalc && *uPtr->sCalc) {
        // calc im XML und set darin definiert
        floatV = atof(input);
        inPtr = uPtr->sCalc;
        logIT(LOG_INFO, "Send Exp:%s [V=%f]", inPtr, floatV);
        erg = execExpression(&inPtr, dumBuf, floatV, errPtr);
        if (*errPtr) {
            logIT(LOG_ERR, "Exec %s: %s", uPtr->sCalc, error);
            strcpy(sendBuf, string);
            return (-1);
        }
        ergType = FLOAT;
    }

    if (uPtr->sICalc && *uPtr->sICalc) {
        // icalc im XML und set darin definiert
        inPtr = uPtr->sICalc;
        if ( uPtr->ePtr) {
            // es gibt enums hier
            if (! *input) {
                sprintf(sendBuf, "Input fehlt");
                return (-1);
            }
            if (!(count = text2Enum(uPtr->ePtr, input, &ptr, sendLen))) {
                sprintf(sendBuf, "Kein passendes Enum gefunden");
                return (-1);
            } else {
                bzero(dumBuf, sizeof(dumBuf));
                memcpy(dumBuf, ptr, count);
                logIT(LOG_INFO, "(INT) Exp:%s [BP:%d]", inPtr, bitpos);
                ergI = execIExpression(&inPtr, dumBuf, bitpos, pRecvPtr, errPtr);
                if (*errPtr) {
                    logIT(LOG_ERR, "Exec %s: %s", uPtr->sICalc, error);
                    strcpy(sendBuf, string);
                    return (-1);
                }
                ergType = INT;
                snprintf(string, sizeof(string), "Erg: (Hex max. 4Byte) %08x", ergI);
            }
        }
    }

    // das Ergebnis steht in erg und muss nun je nach typ umgewandelt werden
    if (uPtr->type) {
        if (strstr(uPtr->type, "char") == uPtr->type) {
            // Umrechnung in Short 2Byte
            // je nach CPU Typ wird hier die Wandlung vorgenommen
            (ergType == FLOAT) ? (charV = erg) : (charV = ergI);
            memcpy(sendBuf, &charV, 1);
            *sendLen = 1;
        } else if (strstr(uPtr->type, "uchar") == uPtr->type) {
            // je nach CPU Typ wird hier die Wandlung vorgenommen
            (ergType == FLOAT) ? (ucharV = erg) : (ucharV = ergI);
            memcpy(sendBuf, &ucharV, 1);
            *sendLen = 1;
        } else if (strstr(uPtr->type, "short") == uPtr->type) {
            // je nach CPU Typ wird hier die Wandlung vorgenommen
            (ergType == FLOAT) ? (tmpS = erg) : (tmpS = ergI);
            shortV = __cpu_to_le16(tmpS);
            memcpy(sendBuf, &shortV, 2);
            *sendLen = 2;
        } else if (strstr(uPtr->type, "ushort") == uPtr->type) {
            // je nach CPU Typ wird hier die Wandlung vorgenommen
            (ergType == FLOAT) ? (tmpUS = erg) : (tmpUS = ergI);
            ushortV = __cpu_to_le16(tmpUS);
            memcpy(sendBuf, &ushortV, 2);
            *sendLen = 2;
        } else if (strstr(uPtr->type, "int") == uPtr->type) {
            // je nach CPU Typ wird hier die Wandlung vorgenommen
            (ergType == FLOAT) ? (tmpI = erg) : (tmpI = ergI);
            intV = __cpu_to_le32(tmpI);
            memcpy(sendBuf, &intV, 2);
            *sendLen = 4;
        } else if (strstr(uPtr->type, "uint") == uPtr->type) {
            // je nach CPU Typ wird hier die Wandlung vorgenommen
            (ergType == FLOAT) ? (tmpUI = erg) : (tmpUI = ergI);
            uintV = __cpu_to_le32(tmpUI);
            memcpy(sendBuf, &uintV, 2);
        } else if (uPtr->type) {
            bzero(string, sizeof(string));
            logIT(LOG_ERR, "Unbekannter Typ %s in Unit %s", uPtr->type, uPtr->name);
            return (-1);
        }

        bzero(buffer, sizeof(buffer));
        ptr = sendBuf;
        while (*ptr) {
            bzero(string, sizeof(string));
            unsigned char byte = *ptr++ & 255;
            snprintf(string, sizeof(string), "%02X ", byte);
            strcat(buffer, string);
            if (n >= MAXBUF - 3)  /* FN Wo wird 'n' eigentlich initialisiert */
            { break; }
        }

        logIT(LOG_INFO, "Typ: %s (Bytes: %s)  ", uPtr->type, buffer);

        return 1;
    }

    return 0;
    // Wenn ich das richtig verstehe, sollten wir hier nie landen FN, deshalb; keep compiler happy
}
