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

    memset(simIn, 0, sizeof(simIn));
    memset(simOut, 0, sizeof(simOut));

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
                memset(string, 0, sizeof(string));
                char2hex(string, cmpPtr->send, cmpPtr->len);
                strcat(simIn, string);
                strcat(simIn, " ");
                break;
            case SEND:
                out_len = 0;
                while (1) {
                    if (out_len + cmpPtr->len > sizeof(out_buff)) {
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

                memset(string, 0, sizeof(string));
                char2hex(string, out_buff, out_len);
                strcat(simOut, string);
                strcat(simOut, " ");
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

                memset(string, 0, sizeof(string));
                char2hex(string, recvBuf, cmpPtr->len);
                strcat(simIn, string);
                strcat(simIn, " ");

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
                    char2hex(string, sendBuf, sendLen);
                    strcat(simOut, string);
                    strcat(simOut, " ");
                } else if (cmpPtr->len) {
                    // A unit to use is already defined, and we already converted it
                    if (! my_send(fd, cmpPtr->send, cmpPtr->len)) {
                        logIT1(LOG_ERR, "Error in send unit bytes, terminating");
                        free(cmpPtr->send);
                        cmpPtr->send = NULL;
                        cmpPtr->len = 0;
                        return -1;
                    }
                    memset(string, 0, sizeof(string));
                    char2hex(string, cmpPtr->send, cmpPtr->len);
                    strcat(simOut, string);
                    strcat(simOut, " ");
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
        if (! my_send(fd, hex, hexlen)) {
            logIT1(LOG_ERR, "Error send, terminating");
            exit(1);
        }
        break;
    case RECV:
        if (hexlen > recvLen) {
            logIT(LOG_ERR, "Recv buffer too small. Is: %d should be %d", recvLen, hexlen);
            hexlen = recvLen;
        }
        etime = 0;
        if (receive(fd, recvBuf, hexlen, &etime) <= 0) {
            logIT1(LOG_ERR, "Error recv, terminating");
            exit(1);
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

int expand(commandPtr cPtr, protocolPtr pPtr)
{
    // Recursion
    if (! cPtr) {
        return 0;
    }
    if (cPtr->next) {
        expand(cPtr->next, pPtr);
    }

    // If no address has been set, we do nothing
    if (! cPtr->addr) {
        return 0;
    }

    char eString[2000];
    char *ePtr = eString;;
    char var[100];
    char name[100];
    char *ptr, *bptr;
    macroPtr mFPtr;
    char string[256];
    char *sendPtr, *sendStartPtr;
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
    sendStartPtr = iPtr->send;
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
        strncpy(ePtr, sendPtr, ptr - sendPtr);
        ePtr += ptr - sendPtr;
        memset(var, 0, sizeof(var));
        strncpy(var, ptr + 1, bptr - ptr - 1);
        //snprintf(string, sizeof(string),"   Var: %s",var);
        //logIT(LOG_INFO,string);
        if (*var) {
            // Do we have variabls to expand at all?
            if (strstr(var, "addr") == var) {
                // Always two bytes
                int i;
                memset(string, 0, sizeof(string));
                for (i = 0; i < strlen(cPtr->addr) - 1; i += 2) {
                    strncpy(ePtr, cPtr->addr + i, 2);
                    ePtr += 2;
                    *ePtr++ = ' ';
                }
                ePtr--;
            } else if (strstr(var, "len") == var) {
                snprintf(string, sizeof(string), "%d", cPtr->len);
                strncpy(ePtr, string, strlen(string));
                ePtr += strlen(string);
            } else if (strstr(var, "hexlen") == var) {
                snprintf(string, sizeof(string), "%02X", cPtr->len);
                strncpy(ePtr, string, strlen(string));
                ePtr += strlen(string);
            } else if (strstr(var, "unit") == var) {
                if (cPtr->unit) {
                    strncpy(ePtr, cPtr->unit, strlen(cPtr->unit));
                    ePtr += strlen(cPtr->unit);
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
    sendStartPtr = sendPtr;

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
        strncpy(name, sendPtr, ptr - sendPtr);
        if ((mFPtr = getMacroNode(pPtr->mPtr, name))) {
            strncpy(ePtr, mFPtr->command, strlen(mFPtr->command));
            ePtr += strlen(mFPtr->command);
            *ePtr++ = *ptr;
        } else {
            strncpy(ePtr, sendPtr, ptr - sendPtr + 1);
            ePtr += ptr - sendPtr + 1;
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
    // Recursion
    if (!cPtr) {
        return 0;
    }
    if (cPtr->next) {
        buildByteCode(cPtr->next, uPtr);
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

    char *sendPtr, *sendStartPtr;
    sendPtr = cPtr->send;
    sendStartPtr = cPtr->send;

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
        strncpy(cmd, sendPtr, ptr - sendPtr);
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
        cmpPtr->send = calloc(hexlen, sizeof(char));
        memcpy(cmpPtr->send, hex, hexlen);

        if (*uSPtr && !(cmpPtr->uPtr = getUnitNode(uPtr, uSPtr))) {
            logIT(LOG_ERR, "Unit %s not defined", uSPtr);
            exit(3);
        }

        sendPtr += strlen(cmd) + 1;
    } while (*sendPtr);

    cPtr->cmpPtr = cmpStartPtr;
    return cmpStartPtr;
}

void compileCommand(devicePtr dPtr, unitPtr uPtr)
{
    if (! dPtr) {
        return;
    }
    if (dPtr->next) {
        compileCommand(dPtr->next, uPtr);
    }

    logIT(LOG_INFO, "Expanding command for device %s", dPtr->id);
    expand(dPtr->cmdPtr, dPtr->protoPtr);
    buildByteCode(dPtr->cmdPtr, uPtr);
}
