// SPDX-FileCopyrightText: 2007-2016 The original vcontrold authors (cf. doc/original_authors.txt)
//
// SPDX-License-Identifier: GPL-3.0-or-later

// Routines for reading commands

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

#include "xmlconfig.h"
#include "parser.h"
#include "unit.h"
#include "common.h"
#include "io.h"
#include "framer.h"

extern FILE *iniFD; // For creation of the Sim. INI Files

void *getUnit(char *str)
{
    (void)str;
    // We parse the input for a known unit and return a pointer to the struct
    return NULL;
}

int parseLine(char *line, char *hex, int *hexlen, char *uSPtr, ssize_t uSPtrLen)
{
    int token = 0;

    if (strstr(line, "WAIT") == line) {
        token = WAIT;
    } else if (strstr(line, "SEND BYTES") == line) {
        token = BYTES;
    } else if (strstr(line, "SEND") == line) {
        token = SEND;
    } else if (strstr(line, "RECV") == line) {
        token = RECV;
    } else if (strstr(line, "PAUSE") == line) {
        token = PAUSE;
    }

    if (token) {
        // We parse a hex string and return it processed
        // Search first blank
        char *ptr;
        ptr = strchr(line, ' ');
        if (! ptr) {
            logIT(LOG_ERR, "Parse error: Found no blanks: %s", line);
        } else {
            // We process the rst of the line
            char pString[MAXBUF];
            char *pPtr;
            while (*ptr != '\0') {
                int i;
                pPtr = pString;
                for (i = 0; i < MAXBUF; i++) {
                    pPtr[i] = '\0';
                }
                pPtr = pString;
                // Remove blanks
                int endFound = 0;
                while (*++ptr == ' ') {
                    if (*ptr == '\0') {
                        endFound = 1;
                        break;
                    }
                }
                if (endFound) {
                    break;
                }
                // Now, we read the characters to the next blank or end
                while ((*ptr != ' ') && (*ptr != '\0') && (*ptr != '\n') && (*ptr != '\r')) {
                    *pPtr++ = *ptr++;
                }

                if (! *pString) {
                    break;
                }

                *pPtr = '\0';
                ++*hexlen;
                // If it's Pause or Reveice, we tread it as an integer
                if (token == PAUSE || token == RECV) {
                    *hexlen = atoi(pString);
                    // With RECV, one can give the unit to be converted
                    if (token == RECV) {
                        pPtr = pString;
                        while (*ptr && (isdigit(*ptr) || isspace(*ptr))) {
                            ptr++;
                        }
                        strncpy(uSPtr, ptr, uSPtrLen);
                    }
                    return token;
                } else if (token == BYTES ) {
                    // Here follows the unit
                    pPtr = pString;
                    while (*ptr && (isdigit(*ptr) || isspace(*ptr))) {
                        ptr++;
                    }
                    strncpy(uSPtr, ptr, uSPtrLen);
                    return token;
                } else  {
                    *hex++ = hex2chr(pString);
                }
                *pString = '\0';
            }
        }
    }

    return token;
}

static int appendSimulation(char *destination, size_t destinationSize,
                            const char *value)
{
    size_t used;
    size_t valueLength;

    if (!destination || !destinationSize || !value) return 0;
    used = strlen(destination);
    valueLength = strlen(value);
    if (used + valueLength + 2 > destinationSize) {
        logIT1(LOG_ERR, "Simulation output buffer too small");
        return 0;
    }
    memcpy(destination + used, value, valueLength);
    destination[used + valueLength] = ' ';
    destination[used + valueLength + 1] = '\0';
    return 1;
}

int execByteCode(compilePtr cmpPtr, int fd, char *recvBuf, short recvLen,
                 char *sendBuf, short sendLen, short supressUnit,
                 char bitpos, int retry,
                 char *pRecvPtr, unsigned short recvTimeout)
{
    char string[256];
    char result[MAXBUF];
    char simIn[500];
    char simOut[500];
    short len;
    compilePtr cPtr;
    unsigned long etime;
    char out_buff[1024];
    int out_len;

    if (iniFD) {
        memset(simIn, 0, sizeof(simIn));
        memset(simOut, 0, sizeof(simOut));
    }

    // First copy or convert the bytes of sendBuf to the BYTES token of cmpPtr
    // to be sure not to abort right in the middle of it
    cPtr = cmpPtr; // do not change cmpPtr, use local cPtr
    while (cPtr) {
        if (cPtr->token != BYTES) {
            cPtr = cPtr->next;
            continue;
        }
        if (supressUnit) {
            // No unit conversion needed, just copy the bytes
            if (sendLen != cPtr->len) {
                // This should never happen!
                logIT(LOG_ERR,
                      "Error in length of the hex string (%d) != send length of the command (%d), terminating", sendLen, cPtr->len);
                return -1;
            }
            if (cPtr->send) {
                free(cPtr->send);
            }
            cPtr->send = calloc(cPtr->len, sizeof(char));
            sendLen = 0; // We don't send the converted sendBuf
            memcpy(cPtr->send, sendBuf, cPtr->len);
        } else if (cPtr->uPtr) {
            len = sendLen; // we need this in procSetUnit() to clear sendBuf
            if (procSetUnit(cPtr->uPtr, sendBuf, &len, bitpos, pRecvPtr) <= 0) {
                logIT(LOG_ERR, "Error in unit conversion: %s, terminating", sendBuf);
                return -1;
            }
            if (cPtr->send) {
                free(cPtr->send);
            }
            cPtr->send = calloc(len, sizeof(char));
            cPtr->len = len;
            sendLen = 0; // We don't send the converted sendBuf
            memcpy(cPtr->send, sendBuf, len);
        }
        cPtr = cPtr->next;
    }

    do {
        cPtr = cmpPtr; // We need the starting point for the next round
        while (cmpPtr) {
            switch (cmpPtr->token) {
            case WAIT:
                if (! framer_waitfor(fd, cmpPtr->send, cmpPtr->len)) {
                    logIT1(LOG_ERR, "Error in wait, terminating");
                    return -1;
                }
                if (iniFD) {
                    memset(string, 0, sizeof(string));
                    if (!char2hex(string, sizeof(string), cmpPtr->send, cmpPtr->len) ||
                            !appendSimulation(simIn, sizeof(simIn), string)) {
                        return -1;
                    }
                }
                break;
            case SEND:
                out_len = 0;
                while (1) {
                    if ((size_t)out_len + cmpPtr->len > sizeof(out_buff)) {
                        // Hopefully, we never end up here
                        logIT1(LOG_ERR, "Error out_buff buffer overflow, terminating");
                        return -1;
                    }

                    // Copy all SEND data and BYTES data to out_buff, that CRC calculation
                    // works in framer_send()
                    memcpy(out_buff + out_len, cmpPtr->send, cmpPtr->len);
                    out_len += cmpPtr->len;

                    if (! (cmpPtr->next && cmpPtr->next->token == BYTES)) {
                        break;
                    }
                    cmpPtr = cmpPtr->next;
                }

                if (! framer_send(fd, out_buff, out_len)) {
                    logIT1(LOG_ERR, "Error in send, terminating");
                    return -1;
                }

                if (iniFD && *simIn && *simOut) {
                    // We already sent and received something, so we output it
                    fprintf(iniFD, "%s= %s\n", simOut, simIn);
                    memset(simOut, 0, sizeof(simOut));
                    memset(simIn, 0, sizeof(simIn));
                }

                if (iniFD) {
                    memset(string, 0, sizeof(string));
                    if (!char2hex(string, sizeof(string), out_buff, out_len) ||
                            !appendSimulation(simOut, sizeof(simOut), string)) {
                        return -1;
                    }
                }
                break;
            case RECV:
                if (cmpPtr->len > recvLen) {
                    // Hopefully, we don't end up here
                    logIT(LOG_ERR, "Recv buffer too small. Is: %d, should be %d",
                          recvLen, cmpPtr->len);
                    cmpPtr->len = recvLen;
                }
                etime = 0;
                memset(recvBuf, 0, recvLen);
                if (framer_receive(fd, recvBuf, cmpPtr->len, &etime) <= 0) {
                    logIT1(LOG_ERR, "Error in recv, terminating");
                    return -1;
                }
                // If receiving took longer than the timeout, we start the next round
                if (recvTimeout && (etime > recvTimeout)) {
                    logIT(LOG_NOTICE, "Recv Timeout: %ld ms > %d ms (Retry: %d)",
                          etime, recvTimeout, (int)(retry - 1));
                    if (retry <= 1) {
                        logIT1(LOG_ERR, "Recv timeout, terminating");
                        return -1;
                    }
                    goto RETRY;
                }

                // If some errStr is defined, we check if the result is correct
                if (cmpPtr->errStr && *cmpPtr->errStr) {
                    if (memcmp(recvBuf, cmpPtr->errStr, cmpPtr->len) == 0) {
                        // Wrong answer
                        logIT(LOG_NOTICE, "Errstr matched, wrong result (Retry: %d)", retry - 1);
                        if (retry <= 1) {
                            logIT1(LOG_ERR, "Wrong result, terminating");
                            return -1;
                        }
                        goto RETRY;
                    }
                }

                if (iniFD) {
                    memset(string, 0, sizeof(string));
                    if (!char2hex(string, sizeof(string), recvBuf, cmpPtr->len) ||
                            !appendSimulation(simIn, sizeof(simIn), string)) {
                        return -1;
                    }
                }

                // If we have a Unit (== uPtr), we convert the received value and also
                // return the converted value to uPtr
                memset(result, 0, sizeof(result));
                if (! supressUnit && cmpPtr->uPtr) {
                    if (procGetUnit(cmpPtr->uPtr, recvBuf, cmpPtr->len, result, bitpos, pRecvPtr)
                        <= 0) {
                        logIT(LOG_ERR, "Error in unit conversion: %s, terminating", result);
                        return -1;
                    }
                    strncpy(recvBuf, result, recvLen);

                    if (iniFD && *simIn && *simOut) {
                        // We already sent and received, now we output it.
                        fprintf(iniFD, "%s= %s \n", simOut, simIn);
                    }

                    return 0; // 0 == converted to unit
                }

                if (iniFD && *simIn && *simOut) {
                    // We already sent and received, now we output it.
                    fprintf(iniFD, "%s= %s \n", simOut, simIn);
                }
                return cmpPtr->len;
                break;
            case PAUSE:
                logIT(LOG_INFO, "Waiting %i ms", cmpPtr->len);
		struct timespec sleepTime = {
		    .tv_sec = cmpPtr->len / 1000L,
		    .tv_nsec = (cmpPtr->len % 1000L) * 1000000L};
                nanosleep(&sleepTime, NULL);
                break;
            case BYTES:
                // We send the forwarded sendBuffer. No converting has been done.
                if (sendLen) {
                    if (!my_send(fd, sendBuf, sendLen)) {
                        logIT1(LOG_ERR, "Error in send, terminating");
                        return -1;
                    }
                    if (iniFD) {
                        memset(string, 0, sizeof(string));
                        if (!char2hex(string, sizeof(string), sendBuf, sendLen) ||
                                !appendSimulation(simOut, sizeof(simOut), string)) {
                            return -1;
                        }
                    }
                } else if (cmpPtr->len) {
                    // A unit to use is already defined, and we already converted it
                    if (! my_send(fd, cmpPtr->send, cmpPtr->len)) {
                        logIT1(LOG_ERR, "Error in send unit bytes, terminating");
                        free(cmpPtr->send);
                        cmpPtr->send = NULL;
                        cmpPtr->len = 0;
                        return -1;
                    }
                    if (iniFD) {
                        memset(string, 0, sizeof(string));
                        if (!char2hex(string, sizeof(string), cmpPtr->send, cmpPtr->len) ||
                                !appendSimulation(simOut, sizeof(simOut), string)) {
                            return -1;
                        }
                    }
                }
                break;
            default:
                logIT(LOG_ERR, "Unknown token: %d, terminating", cmpPtr->token);
                return -1;
            }
            cmpPtr = cmpPtr->next;
        }
RETRY:
        retry--;
        cmpPtr = cPtr; // One more time, please
    } while ((cmpPtr->errStr || recvTimeout) && (retry > 0));

    return 0;
}

int execCmd(char *cmd, int fd, char *recvBuf, int recvLen)
{
    char uString[100];
    // We parse the individual lines
    char hex[MAXBUF];
    int token;
    int hexlen = 0;
    int t;
    unsigned long etime;

    logIT(LOG_INFO, "Execute %s", cmd);

    token = parseLine(cmd, hex, &hexlen, uString, sizeof(uString));
    // If noOpen is set for debugging, we do nothing here.

    switch (token) {
    case WAIT:
        if (! waitfor(fd, hex, hexlen)) {
            logIT1(LOG_ERR, "Error wait, terminating");
            return -1;
        }
        break;
    case SEND:
        if (! framer_send(fd, hex, hexlen)) {
            logIT1(LOG_ERR, "Error send, terminating");
            return -1;
        }
        break;
    case RECV:
        if (hexlen > recvLen) {
            logIT(LOG_ERR, "Recv buffer too small. Is: %d should be %d", recvLen, hexlen);
            hexlen = recvLen;
        }
        etime = 0;
        if (framer_receive(fd, recvBuf, hexlen, &etime) <= 0) {
            logIT1(LOG_ERR, "Error recv, terminating");
            return -1;
        }
        logIT(LOG_INFO, "Recv: %ld ms", etime);
        // If we have a unit (== uPtr), we convert the received value and also return it to uPtr
        return hexlen;
        break;
    case PAUSE:
        t = (int) hexlen / 1000;
        logIT(LOG_INFO, "Waiting %i s", t);
        sleep(t);
        break;
    default:
        logIT(LOG_INFO, "Unknown command: %s", cmd);

    }

    return 0;
}

compilePtr newCompileNode(compilePtr ptr)
{
    compilePtr nptr;

    if (ptr && ptr->next) {
        return newCompileNode(ptr->next);
    }

    nptr = calloc(1, sizeof(Compile));
    if (! nptr) {
        fprintf(stderr, "malloc failed\n");
        exit(1);
    }

    if (ptr) {
        ptr->next = nptr;
    }

    nptr->token = 0;
    nptr->len = 0;
    nptr->uPtr = NULL;
    nptr->next = NULL;

    return nptr;
}

void removeCompileList(compilePtr ptr)
{
    if (ptr && ptr->next) {
        removeCompileList(ptr->next);
    }

    if (ptr) {
        free(ptr->send);
        ptr->send = NULL;
        free(ptr);
        ptr = NULL;
    }
}

static int append_bytes(char **dst, char *end, const char *src, size_t len)
{
    if (!dst || !*dst || !src || *dst > end || len >= (size_t)(end - *dst)) {
        return 0;
    }
    memcpy(*dst, src, len);
    *dst += len;
    **dst = '\0';
    return 1;
}

int expand(commandPtr cPtr, protocolPtr pPtr)
{
    if (! cPtr) {
        return 0;
    }

    // If no address has been set, we do nothing
    if (! cPtr->addr) {
        return 0;
    }

    char eString[2000];
    char *ePtr = eString;;
    char *eEnd = eString + sizeof(eString);
    char var[100];
    char name[100];
    char *ptr, *bptr;
    macroPtr mFPtr;
    char string[256];
    char *sendPtr;
    char *tmpPtr;

    icmdPtr iPtr;

    // The command pointer's send has to be assembled

    // 1. Search command pcmd at the protocol's command
    if (! (iPtr = (icmdPtr) getIcmdNode( pPtr->icPtr, cPtr->pcmd))) {
        logIT(LOG_ERR, "Protocol command %s (at %s) not defined", cPtr->pcmd, cPtr->name);
        exit(3);
    }

    // 2. Parse the line and replace the variables with values from cPtr
    sendPtr = iPtr->send;
    if (! sendPtr) {
        return 0;
    }

    logIT(LOG_INFO, "protocmd line: %s", sendPtr);
    memset(eString, 0, sizeof(eString));
    cPtr->retry = iPtr->retry; // We take the Retry value from the protocol command
    cPtr->recvTimeout = iPtr->recvTimeout; // Same for the receive timeout
    do {
        ptr = sendPtr;
        while (*ptr++) {
            if ((*ptr == '$') || (*ptr == '\0')) {
                break;
            }
        }
        bptr = ptr;
        while (*bptr++) {
            if ((*bptr == ' ') || (*bptr == ';') || (*bptr == '\0')) {
                break;
            }
        }
        // Don't copy the converted string
        if (!append_bytes(&ePtr, eEnd, sendPtr, (size_t)(ptr - sendPtr))) {
            logIT(LOG_ERR, "Expanded command %s is too long", cPtr->name);
            return 0;
        }
        memset(var, 0, sizeof(var));
        size_t varlen = (size_t)(bptr - ptr - 1);
        if (varlen >= sizeof(var)) {
            logIT(LOG_ERR, "Variable name in command %s is too long", cPtr->name);
            return 0;
        }
        memcpy(var, ptr + 1, varlen);
        //snprintf(string, sizeof(string),"   Var: %s",var);
        //logIT(LOG_INFO,string);
        if (*var) {
            // Do we have variabls to expand at all?
            if (strstr(var, "addr") == var) {
                // Always two bytes
                int i;
                memset(string, 0, sizeof(string));
                for (i = 0; (size_t)i + 1 < strlen(cPtr->addr); i += 2) {
                    if (!append_bytes(&ePtr, eEnd, cPtr->addr + i, 2) ||
                            !append_bytes(&ePtr, eEnd, " ", 1)) {
                        return 0;
                    }
                }
                if (ePtr > eString) *--ePtr = '\0';
            } else if (strstr(var, "len") == var) {
                snprintf(string, sizeof(string), "%d", cPtr->len);
                if (!append_bytes(&ePtr, eEnd, string, strlen(string))) return 0;
            } else if (strstr(var, "hexlen") == var) {
                snprintf(string, sizeof(string), "%02X", cPtr->len);
                if (!append_bytes(&ePtr, eEnd, string, strlen(string))) return 0;
            } else if (strstr(var, "unit") == var) {
                if (cPtr->unit) {
                    if (!append_bytes(&ePtr, eEnd, cPtr->unit, strlen(cPtr->unit))) return 0;
                }
            } else {
                logIT(LOG_ERR, "Variable %s Unknown", var);
                exit(3);
            }
        }
        sendPtr = bptr;
    } while (*sendPtr);

    logIT(LOG_INFO, "  After substitution: %s", eString);
    tmpPtr = calloc(strlen(eString) + 1, sizeof(char));
    strcpy(tmpPtr, eString);

    // 3. Expand the line as usual
    sendPtr = tmpPtr;

    // We search for words and look if we have them as macros
    memset(eString, 0, sizeof(eString));
    ePtr = eString;
    do {
        ptr = sendPtr;
        while (*ptr++) {
            if ((*ptr == ' ') || (*ptr == ';') || (*ptr == '\0')) {
                break;
            }
        }
        memset(name, 0, sizeof(name));
        size_t namelen = (size_t)(ptr - sendPtr);
        if (namelen >= sizeof(name)) {
            logIT(LOG_ERR, "Macro name in command %s is too long", cPtr->name);
            free(tmpPtr);
            return 0;
        }
        memcpy(name, sendPtr, namelen);
        if ((mFPtr = getMacroNode(pPtr->mPtr, name))) {
            if (!append_bytes(&ePtr, eEnd, mFPtr->command, strlen(mFPtr->command)) ||
                    !append_bytes(&ePtr, eEnd, ptr, 1)) {
                free(tmpPtr);
                return 0;
            }
        } else {
            if (!append_bytes(&ePtr, eEnd, sendPtr, (size_t)(ptr - sendPtr + 1))) {
                free(tmpPtr);
                return 0;
            }
        }
        if (*ptr == '\0') {
            break;
        }
        sendPtr = ptr + 1;
    } while (*sendPtr);

    free(tmpPtr);
    logIT(LOG_INFO, "   after EXPAND:%s", eString);
    if (! (cPtr->send = calloc(strlen(eString) + 1, sizeof(char)))) {
        logIT1(LOG_ERR, "calloc failed");
        exit(1);
    }

    strcpy(cPtr->send, eString);
    return 1;
}

compilePtr buildByteCode(commandPtr cPtr, unitPtr uPtr)
{
    if (!cPtr) {
        return 0;
    }

    // If no address has been given, we do nothing
    if (! cPtr->addr) {
        return 0;
    }

    char eString[2000];
    char cmd[200];
    char *ptr;

    char hex[MAXBUF];
    int hexlen;
    int token;
    char uString[100];
    char *uSPtr = uString;

    compilePtr cmpPtr, cmpStartPtr;
    cmpStartPtr = NULL;

    char *sendPtr;
    sendPtr = cPtr->send;

    if (! sendPtr) {
        // Nothing to do here
        return 0;
    }

    logIT(LOG_INFO, "BuildByteCode: %s", sendPtr);
    memset(eString, 0, sizeof(eString));
    do {
        ptr = sendPtr;
        while (*ptr++) {
            if ((*ptr == '\0') || (*ptr == ';')) {
                break;
            }
        }
        memset(cmd, 0, sizeof(cmd));
        size_t cmdlen = (size_t)(ptr - sendPtr);
        if (cmdlen >= sizeof(cmd)) {
            logIT(LOG_ERR, "Bytecode segment in command %s is too long", cPtr->name);
            return NULL;
        }
        memcpy(cmd, sendPtr, cmdlen);
        hexlen = 0;
        memset(uSPtr, 0, sizeof(uString));
        token = parseLine(cmd, hex, &hexlen, uString, sizeof(uString));
        logIT(LOG_INFO, "        Token: %d Hexlen: %d, Unit: %s", token, hexlen, uSPtr);
        cmpPtr = newCompileNode(cmpStartPtr);

        if (!cmpStartPtr) {
            cmpStartPtr = cmpPtr;
        }
        cmpPtr->token = token;
        cmpPtr->len = hexlen;
        cmpPtr->errStr = cPtr->errStr;
        if (hexlen > 0) {
            cmpPtr->send = calloc((size_t)hexlen, sizeof(char));
            if (!cmpPtr->send) {
                logIT1(LOG_ERR, "calloc failed");
                removeCompileList(cmpStartPtr);
                return NULL;
            }
            memcpy(cmpPtr->send, hex, (size_t)hexlen);
        }

        if (*uSPtr && !(cmpPtr->uPtr = getUnitNode(uPtr, uSPtr))) {
            logIT(LOG_ERR, "Unit %s not defined", uSPtr);
            exit(3);
        }

        if (*ptr == '\0') {
            break;
        }
        sendPtr = ptr + 1;
    } while (*sendPtr);

    cPtr->cmpPtr = cmpStartPtr;
    return cmpStartPtr;
}

void compileCommand(devicePtr dPtr, unitPtr uPtr)
{
    for (devicePtr device = dPtr; device; device = device->next) {
        logIT(LOG_INFO, "Expanding command for device %s", device->id);
        for (commandPtr command = device->cmdPtr; command; command = command->next) {
            expand(command, device->protoPtr);
            buildByteCode(command, uPtr);
        }
    }
}
