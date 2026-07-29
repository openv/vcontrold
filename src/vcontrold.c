// SPDX-FileCopyrightText: 2007-2016 The original vcontrold authors (cf. doc/original_authors.txt)
//
// SPDX-License-Identifier: GPL-3.0-or-later

// Vito control daemon for vito queries

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <syslog.h>
#include <unistd.h>
#include <termios.h>
#include <string.h>
#include <time.h>
#include <getopt.h>
#include <grp.h>
#include <pwd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <sys/time.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>

#include "io.h"
#include "common.h"
#include "xmlconfig.h"
#include "parser.h"
#include "socket.h"
#include "prompt.h"
#include "semaphore.h"
#include "framer.h"

#ifdef __CYGWIN__
#define XMLFILE "vcontrold.xml"
#define INIOUTFILE "sim-%s.ini"
#else
#define XMLFILE "/etc/vcontrold/vcontrold.xml"
#define INIOUTFILE "/tmp/sim-%s.ini"
#endif

#include "version.h"

// Global variables
char *xmlfile = XMLFILE;
FILE *iniFD = NULL;
int makeDaemon = 1;
int inetversion = 0;

// Defined in xmlconfig.c
extern protocolPtr protoPtr;
extern unitPtr uPtr;
extern devicePtr devPtr;
extern configPtr cfgPtr;

// Declarations
int readCmdFile(char *filename, char *result, size_t resultCapacity,
                size_t *resultLen, char *device);
int interactive(int socketfd, char *device);
void printHelp(int socketfd);
int rawModus (int socketfd, char *device);
static void sigHupHandler(int signo);
int reloadConfig();

void usage()
{
    //      1       10        20        30        40        50        60        70        80

    printf("usage: vcontrold [-x|--xmlfile xml-file] [-d|--device <device>]\n");
    printf("                 [-l|--logfile <logfile>] [-s|--syslog]\n");
    printf("                 [-p|--port <port>] [-L|--listen <address>]\n");
    printf("                 [-n|--nodaemon] [-v|--verbose] [-V|--Version]\n");
    printf("                 [-c|--commandfile <command-file>] [-P|--pidfile <pid-file>]\n");
    printf("                 [-U|--username <username>] [-G|--groupname <groupname>]\n");
    printf("                 [-?|--help] [-i|--vsim] [-g|--debug]\n");
    printf("                 [-4|--inet4] [-6|--inet6]\n\n");

    exit(1);
}

int reloadConfig()
{
    if (parseXMLFile(xmlfile)) {
        logIT(LOG_NOTICE, "XML file %s reloaded", xmlfile);
        return 1;
    } else {
        logIT(LOG_ERR, "Loading of XML file %s failed", xmlfile);
        return 0;
    }
}

int readCmdFile(char *filename, char *result, size_t resultCapacity,
                size_t *resultLen, char *device)
{
    FILE *cmdPtr;
    char line[MAXBUF];
    char string[256];
    char recvBuf[MAXBUF];
    char *resultPtr = result;
    int fd;
    int count = 0;
    if (!filename || !result || !resultLen || resultCapacity == 0 || !device) {
        return 0;
    }
    *resultLen = 0;
    result[0] = '\0';

    // Open the device only if we have something to do
    if (!vcontrol_semget()) {
        logIT1(LOG_ERR, "Could not acquire device lock");
        return 0;
    }
    if ((fd = framer_openDevice(device, cfgPtr->devPtr->protoPtr->id)) == -1) {
        vcontrol_semrelease();
        logIT(LOG_ERR, "Error opening %s", device);
        *resultLen = 0;
        return 0;
    }

    cmdPtr = fopen(filename, "r");
    if (! cmdPtr) {
        logIT(LOG_ERR, "Could not open cmd file %s", filename);
        *resultLen = 0;
        framer_closeDevice(fd);
        vcontrol_semrelease();
        return 0;
    }
    logIT(LOG_INFO, "Reading cmd file %s", filename);
    // Empty queue
    tcflush(fd, TCIOFLUSH);
    while (fgets(line, MAXBUF - 1, cmdPtr)) {
        size_t lineLength = strlen(line);
        int completeLine = lineLength > 0 && line[lineLength - 1] == '\n';
        stripLineEnding(line, lineLength);
        if (!completeLine && !feof(cmdPtr)) {
            logIT1(LOG_ERR, "Command file line too long");
            framer_closeDevice(fd);
            vcontrol_semrelease();
            fclose(cmdPtr);
            return 0;
        }
        if (!*line) continue;
        memset(recvBuf, 0, sizeof(recvBuf));
        count = execCmd(line, fd, recvBuf, sizeof(recvBuf));
        if (count < 0) {
            logIT(LOG_ERR, "Error executing command-file line: %s", line);
            framer_closeDevice(fd);
            vcontrol_semrelease();
            fclose(cmdPtr);
            return 0;
        }
        if ((size_t)count > resultCapacity - *resultLen) {
            logIT1(LOG_ERR, "Command-file result buffer too small");
            framer_closeDevice(fd);
            vcontrol_semrelease();
            fclose(cmdPtr);
            return 0;
        }
        int n;
        char *ptr;
        ptr = recvBuf;
        char buffer[MAXBUF];
        memset(buffer, 0, sizeof(buffer));
        for (n = 0; n < count; n++) {
            // We received characters
            *resultPtr++ = *ptr;
            (*resultLen)++;
            memset(string, 0, sizeof(string));
            unsigned char byte = *ptr++ & 255;
            snprintf(string, sizeof(string), "%02X ", byte);
            strcat(buffer, string);
            if (n >= MAXBUF - 3) {
                break;
            }
        }
        if (count - 1) {
            // timeout
        }
        if (count) {
            logIT(LOG_INFO, "Received: %s", buffer);
        }

    }
    framer_closeDevice(fd);
    vcontrol_semrelease();
    fclose(cmdPtr);
    return 1;
}

void printHelp(int socketfd)
{
//      10        20        30        40        50        60        70        80
    char string[] = " \
close              Close connection to device\n \
commands           List all commands for the protocol listed in the XML file\n \
debug on|off       Toggle debug information\n \
detail <command>   Show detailed information about <command>\n \
device             The device set in the XML file\n \
protocol           Active protocol\n \
raw                Raw mode, commands WAIT,SEND,RECV,PAUSE terminated with END\n \
reload             Reload XML configuration\n \
unit on|off        Toggle conversion to given unit\n \
version            Show the version number\n \
quit               Close the session\n";
    Writen(socketfd, string, strlen(string));
}

int rawModus(int socketfd, char *device)
{
    // Here, we write all incoming commands in a temporary file, which is forwarded to readCmdFile
    char readBuf[MAXBUF];
    // FIXME: I think we should be using some PID or such instead of "XXXXXX".
#ifdef __CYGWIN__
    char tmpfile[] = "vitotmp-XXXXXX";
#else
    char tmpfile[] = "/tmp/vitotmp-XXXXXX";
#endif

    FILE *filePtr;
    int tmpfd;
    char result[MAXBUF];
    int resultLen;

    tmpfd = mkstemp(tmpfile);
    if (tmpfd < 0) {
        logIT(LOG_ERR, "Error creating temporary file: %s", strerror(errno));
        return 0;
    }
    filePtr = fdopen(tmpfd, "w+");
    if (! filePtr) {
        logIT(LOG_ERR, "Could not create temp file %s", tmpfile);
        close(tmpfd);
        unlink(tmpfile);
        return 0;
    }

    logIT(LOG_INFO, "Raw mode: Temp file: %s", tmpfile);

    while (Readline(socketfd, readBuf, sizeof(readBuf))) {
        // Here, we parse the particular commands
        if (strcmp(readBuf, "END\n") == 0 || strcmp(readBuf, "END\r\n") == 0 ||
                strcmp(readBuf, "END") == 0) {
            fclose(filePtr);
            size_t used = 0;
            readCmdFile(tmpfile, result, sizeof(result), &used, device);
            resultLen = (int)used;
            if (resultLen) {
                // Re received characters
                char buffer[MAXBUF];
                memset(buffer, 0, sizeof(buffer));
                char2hex(buffer, sizeof(buffer), result, resultLen);
                Writen(socketfd, "Result: ", 8);
                Writen(socketfd, buffer, strlen(buffer));
                Writen(socketfd, "\n", 1);
            }
            remove(tmpfile);
            return 1;
        }
        logIT(LOG_INFO, "Raw: Read: %s", readBuf);
        if (fputs(readBuf, filePtr) == EOF) {
            logIT1(LOG_ERR, "Error writing to temp file");
        } else {
            // debug stuff
        }
    }
    fclose(filePtr);
    unlink(tmpfile);
    return 0;
}

int interactive(int socketfd, char *device)
{
    char readBuf[1000];
    char *readPtr;
    char bye[] = BYE;
    // has to fit hex dump incl. prefix like "\tError at (Hex): "
    char string[MAXBUF + 32];
    commandPtr cPtr;
    commandPtr pcPtr;
    int fd = -1;
    short count = 0;
    short rcount = 0;
    short noUnit = 0;
    char recvBuf[MAXBUF];
    char pRecvBuf[MAXBUF];
    char sendBuf[MAXBUF];
    char cmd[MAXBUF];
    char para[MAXBUF];
    char *ptr;
    short sendLen = 0;
    int lockOwned = 0;
    char buffer[MAXBUF];

    Writen(socketfd, PROMPT, strlen(PROMPT));
    memset(readBuf, 0, sizeof(readBuf));

    while ((rcount = Readline(socketfd, readBuf, sizeof(readBuf)))) {
        sendLen = 0;
        sendErrMsg(socketfd);
        stripLineEnding(readBuf, (size_t)rcount);
        logIT(LOG_INFO, "Command: %s", readBuf);

        // We separate the command and possible options at the first blank
        memset(cmd, 0, sizeof(cmd));
        memset(para, 0, sizeof(para));
        if ((ptr = strchr(readBuf, ' '))) {
            strncpy(cmd, readBuf, ptr - readBuf);
            strcpy(para, ptr + 1);
        } else {
            strcpy(cmd, readBuf);
        }

        // Here, the particular commands are parsed
        if (strcmp(cmd, "help") == 0) {
            printHelp(socketfd);
        } else if (strcmp(cmd, "quit") == 0) {
            Writen(socketfd, bye, strlen(bye));
            framer_closeDevice(fd);
            fd = -1;
            if (lockOwned) {
                vcontrol_semrelease();
                lockOwned = 0;
            }
            return 1;
        } else if (strcmp(readBuf, "debug on") == 0) {
            setDebugFD(socketfd);
        } else if (strcmp(readBuf, "debug off") == 0) {
            setDebugFD(-1);
        } else if (strcmp(readBuf, "unit off") == 0) {
            noUnit = 1;
        } else if (strcmp(readBuf, "unit on") == 0) {
            noUnit = 0;
        } else if (strcmp(cmd, "reload") == 0) {
            if (reloadConfig()) {
                device = cfgPtr->tty;
                memset(string, 0, sizeof(string));
                snprintf(string, sizeof(string), "XML file %s reloaded\n", xmlfile);
                Writen(socketfd, string, strlen(string));
                // If we have a parent (daemon mode), it reveives a SIGHUP
                if (makeDaemon) {
                    kill(getppid(), SIGHUP);
                }
            } else {
                memset(string, 0, sizeof(string));
                snprintf(string, sizeof(string),
                         "Loading of XML file %s failed, using old configuration\n", xmlfile);
                Writen(socketfd, string, strlen(string));
            }
        } else if (strcmp(cmd, "raw") == 0) {
            if (cfgPtr->readonly) {
                snprintf(string, sizeof(string), "ERR: raw mode disabled by readonly configuration\n");
                Writen(socketfd, string, strlen(string));
            } else {
                if (fd >= 0) {
                    framer_closeDevice(fd);
                    fd = -1;
                }
                if (lockOwned) {
                    vcontrol_semrelease();
                    lockOwned = 0;
                }
                rawModus(socketfd, device);
            }
        } else if (strcmp(cmd, "close") == 0) {
            framer_closeDevice(fd);
            fd = -1;
            if (lockOwned) {
                vcontrol_semrelease();
                lockOwned = 0;
            }
            snprintf(string, sizeof(string), "%s closed\n", device);
            Writen(socketfd, string, strlen(string));
            fd = -1;
        } else if (strcmp(cmd, "commands") == 0) {
            cPtr = cfgPtr->devPtr->cmdPtr;
            while (cPtr) {
                if (cPtr->addr) {
                    memset(string, 0, sizeof(string));
                    snprintf(string, sizeof(string), "%s: %s\n", cPtr->name, cPtr->description);
                    Writen(socketfd, string, strlen(string));
                }
                cPtr = cPtr->next;
            }
        } else if (strcmp(cmd, "protocol") == 0) {
            memset(string, 0, sizeof(string));
            snprintf(string, sizeof(string), "%s\n", cfgPtr->devPtr->protoPtr->name);
            Writen(socketfd, string, strlen(string));
        } else if (strcmp(cmd, "device") == 0) {
            memset(string, 0, sizeof(string));
            snprintf(string, sizeof(string), "%s (ID=%s) (Protocol=%s)\n", cfgPtr->devPtr->name,
                     cfgPtr->devPtr->id,
                     cfgPtr->devPtr->protoPtr->name);
            Writen(socketfd, string, strlen(string));
        } else if (strcmp(cmd, "version") == 0) {
            memset(string, 0, sizeof(string));
            snprintf(string, sizeof(string), "Version: %s\n", VERSION);
            Writen(socketfd, string, strlen(string));
        } else if ((cPtr = getCommandNode(cfgPtr->devPtr->cmdPtr, cmd)) && (cPtr->addr)) {
            // The command is defined in XML, so we take care of it ...
            if (cfgPtr->readonly && (!cPtr->pcmd || strcmp(cPtr->pcmd, "getaddr") != 0)) {
                snprintf(string, sizeof(string),
                         "ERR: command %s disabled by readonly configuration\n", cPtr->name);
                Writen(socketfd, string, strlen(string));
                Writen(socketfd, PROMPT, strlen(PROMPT));
                continue;
            }
            memset(string, 0, sizeof(string));
            memset(recvBuf, 0, sizeof(recvBuf));
            memset(sendBuf, 0, sizeof(sendBuf));
            memset(pRecvBuf, 0, sizeof(pRecvBuf));

            // If unit off is set or no unit is defined, we pass the parameters in hex
            memset(sendBuf, 0, sizeof(sendBuf));
            if ((noUnit || !cPtr->unit) && *para) {
                if ((sendLen = string2chr(para, sendBuf, sizeof(sendBuf))) == -1) {
                    logIT(LOG_ERR, "No hex string: %s", para);
                    sendErrMsg(socketfd);
                    if (! Writen(socketfd, PROMPT, strlen(PROMPT))) {
                        sendErrMsg(socketfd);
                        framer_closeDevice(fd);
                        if (lockOwned) {
                            vcontrol_semrelease();
                        }
                        return 0;
                    }
                    continue;
                }
                // If sendLen > len of the command, we use len
                if (sendLen > cPtr->len) {
                    logIT(LOG_WARNING,
                          "Length of the hex string > send length of the command, sending only %d bytes", cPtr->len);
                    sendLen = cPtr->len;
                }
            } else if (*para) {
                // We copy the parameter, execByteCode itself takes care of it
                strcpy(sendBuf, para);
                sendLen = strlen(sendBuf);
            }
            if (iniFD) {
                fprintf(iniFD, ";%s\n", readBuf);
            }

            // We only open the device if we have something to do. But only if it's not open yet.
            if (fd < 0) {
                /* As one vclient call opens the link once, all is seen a transaction
                 * This may cause trouble for telnet sessions, as the whole session is
                 * one link activity, even more commands are given within.
                 * This is related to a accept/close on a server socket
                 */
                // everything on link is a transaction - all commands
                if (!vcontrol_semget()) {
                    logIT1(LOG_ERR, "Could not acquire device lock");
                    sendErrMsg(socketfd);
                    continue;
                }
                lockOwned = 1;

                if ((fd = framer_openDevice(device, cfgPtr->devPtr->protoPtr->id)) == -1) {
                    logIT(LOG_ERR, "Error opening %s", device);
                    sendErrMsg(socketfd);
                    framer_closeDevice(fd);
                    vcontrol_semrelease();
                    lockOwned = 0;
                    if (!Writen(socketfd, PROMPT, strlen(PROMPT))) {
                        sendErrMsg(socketfd);
                        return 0;
                    }
                    continue;
                }
            }

            // If there's a pre command, we execute this first
            if (cPtr->precmd && (pcPtr = getCommandNode(cfgPtr->devPtr->cmdPtr, cPtr->precmd))) {
                logIT(LOG_INFO, "Executing pre command %s", cPtr->precmd);

                // if there is no parameter, precommand and calc/icalc will set the parameter
                if (! *para) {
                    // dummy sendBuffer required for procSetUnit input check later
                    strcpy(sendBuf, " ");
                    sendLen = strlen(sendBuf);
                }

                if (execByteCode(pcPtr->cmpPtr, fd, pRecvBuf, sizeof(pRecvBuf), sendBuf, sendLen, 1, pcPtr->bit, pcPtr->retry, pRecvBuf, pcPtr->recvTimeout) == -1) {
                    logIT(LOG_ERR, "Error executing %s", readBuf);
                    sendErrMsg(socketfd);
                    break;
                } else {
                    memset(buffer, 0, sizeof(buffer));
                    char2hex(buffer, sizeof(buffer), pRecvBuf, pcPtr->len);
                    logIT(LOG_INFO, "Result of pre command: %s", buffer);
                }
            }

            // We execute the bytecode:
            // -1: Error
            //  0: Preformatted string
            //  n: raw bytes
            count = execByteCode(cPtr->cmpPtr, fd, recvBuf, sizeof(recvBuf), sendBuf, sendLen, noUnit, cPtr->bit, cPtr->retry, pRecvBuf, cPtr->recvTimeout);

            if (count == -1) {
                logIT(LOG_ERR, "Error executing %s", readBuf);
                sendErrMsg(socketfd);
            } else if (*recvBuf && (count == 0)) {
                // Unit converted
                logIT1(LOG_INFO, recvBuf);
                    Writen(socketfd, recvBuf, strlen(recvBuf));
                    Writen(socketfd, "\n", 1);
            } else {
                int n;
                char *ptr;
                ptr = recvBuf;
                char buffer[MAXBUF];
                memset(buffer, 0, sizeof(buffer));
                for (n = 0; n < count; n++) {
                    // We received a character
                    memset(string, 0, sizeof(string));
                    unsigned char byte = *ptr++ & 255;
                    snprintf(string, sizeof(string), "%02X ", byte);
                    strcat(buffer, string);
                    if (n >= MAXBUF - 3) {
                        break;
                    }
                }
                if (count) {
                    Writen(socketfd, buffer, strlen(buffer));
                    Writen(socketfd, "\n", 1);
                    logIT(LOG_INFO, "Received: %s", buffer);
                }
            }
            if (iniFD) {
                fflush(iniFD);
            }
        } else if (strcmp(cmd, "detail") == 0) {
            readPtr = readBuf + strlen("detail");
            while (isspace(*readPtr)) {
                readPtr++;
            }
            // Is the command defined in the XML?
            if ((cPtr = getCommandNode(cfgPtr->devPtr->cmdPtr, readPtr))) {
                memset(string, 0, sizeof(string));
                snprintf(string, sizeof(string), "%s: %s\n", cPtr->name, cPtr->send);
                Writen(socketfd, string, strlen(string));
                // Error String defined
                char buf[MAXBUF];
                memset(buf, 0, sizeof(buf));
                if (cPtr->errStr && char2hex(buf, sizeof(buf), cPtr->errStr, cPtr->len)) {
                    Writen(socketfd, "\tError at (Hex): ", 17);
                    Writen(socketfd, buf, strlen(buf));
                    Writen(socketfd, "\n", 1);
                }
                // recvTimeout?
                if (cPtr->recvTimeout) {
                    snprintf(string, sizeof(string), "\tRECV Timeout: %d ms\n", cPtr->recvTimeout);
                    Writen(socketfd, string, strlen(string));
                }
                // Retry defined?
                if (cPtr->retry) {
                    snprintf(string, sizeof(string), "\tRetry: %d\n", cPtr->retry);
                    Writen(socketfd, string, strlen(string));
                }
                // Is Bit defined?
                if (cPtr->bit > 0) {
                    snprintf(string, sizeof(string), "\tBit (BP): %d\n", cPtr->bit);
                    Writen(socketfd, string, strlen(string));
                }
                // Pre command defined?
                if (cPtr->precmd) {
                    snprintf(string, sizeof(string), "\tPre command (P0-P9): %s\n", cPtr->precmd);
                    Writen(socketfd, string, strlen(string));
                }

                // If a unit has been given, we also output it
                compilePtr cmpPtr;
                cmpPtr = cPtr->cmpPtr;
                while (cmpPtr) {
                    if (cmpPtr->uPtr) {
                        // Unit gefunden
                        char *gcalc;
                        char *scalc;
                        // We differentiate the calculation by get and setaddr
                        if (cmpPtr->uPtr->gCalc && *cmpPtr->uPtr->gCalc) {
                            gcalc = cmpPtr->uPtr->gCalc;
                        } else {
                            gcalc = cmpPtr->uPtr->gICalc;
                        }
                        if (cmpPtr->uPtr->sCalc && *cmpPtr->uPtr->sCalc) {
                            scalc = cmpPtr->uPtr->sCalc;
                        } else {
                            scalc = cmpPtr->uPtr->sICalc;
                        }

                        snprintf(string, sizeof(string),
                                 "\tUnit: %s (%s)\n\t  Type: %s\n\t  Get-Calc: %s\n\t  \
                                  Set-Calc: %s\n\t Einheit: %s\n",
                                 cmpPtr->uPtr->name, cmpPtr->uPtr->abbrev,
                                 cmpPtr->uPtr->type,
                                 gcalc,
                                 scalc,
                                 cmpPtr->uPtr->entity);
                        Writen(socketfd, string, strlen(string));
                        // If it's an enum, is the more?
                        if (cmpPtr->uPtr->ePtr) {
                            enumPtr ePtr;
                            ePtr = cmpPtr->uPtr->ePtr;
                            char dummy[MAXBUF];
                            while (ePtr) {
                                memset(dummy, 0, sizeof(dummy));
                                if (!ePtr->bytes) {
                                    strcpy(dummy, "<default>");
                                } else {
                                    char2hex(dummy, sizeof(dummy), ePtr->bytes, ePtr->len);
                                }
                                Writen(socketfd, "\t  Enum Bytes: ", 15);
                                Writen(socketfd, dummy, strlen(dummy));
                                Writen(socketfd, " Text: ", 7);
                                Writen(socketfd, ePtr->text,
                                       strlen(ePtr->text));
                                Writen(socketfd, "\n", 1);
                                ePtr = ePtr->next;
                            }
                        }
                    }
                    cmpPtr = cmpPtr->next;
                }
            } else {
                memset(string, 0, sizeof(string));
                Writen(socketfd, "ERR: command ", 13);
                Writen(socketfd, readPtr, strlen(readPtr));
                Writen(socketfd, " unknown\n", 9);
            }
        } else if (*readBuf) {
            if (!Writen(socketfd, UNKNOWN, strlen(UNKNOWN))) {
                sendErrMsg(socketfd);
                framer_closeDevice(fd);
                if (lockOwned) {
                    vcontrol_semrelease();
                }
                return 0;
            }
        }
        sendErrMsg(socketfd);
        if (!Writen(socketfd, PROMPT, strlen(PROMPT))) {
            sendErrMsg(socketfd);
            framer_closeDevice(fd);
            if (lockOwned) {
                vcontrol_semrelease();
            }
            return 0;
        }
        memset(readBuf, 0, sizeof(readBuf));
    }
    sendErrMsg(socketfd);
    framer_closeDevice(fd);
    if (lockOwned) {
        vcontrol_semrelease();
    }
    return 0;
}

static volatile sig_atomic_t reloadRequested = 0;
static volatile sig_atomic_t terminateRequested = 0;

static void sigHupHandler(int signo)
{
    (void)signo;
    reloadRequested = 1;
}

char *pidFile = NULL;

static void sigTermHandler(int signo)
{
    (void)signo;
    terminateRequested = 1;
}

static int parseTcpPort(const char *text, int *port)
{
    char *end = NULL;
    long parsed;
    if (!text || !*text || !port) return 0;
    errno = 0;
    parsed = strtol(text, &end, 10);
    if (errno || end == text || *end || parsed < 1 || parsed > 65535) return 0;
    *port = (int)parsed;
    return 1;
}

// Here we go
int main(int argc, char *argv[])
{
    // Parse the command line options
    char *device = NULL;
    char *cmdfile = NULL;
    char *logfile = NULL;
    char *username = NULL;
    char *groupname = NULL;
    static int useSyslog = 0;
    static int debug = 0;
    static int verbose = 0;
    int tcpport = 0;
    char *listenAddress = NULL;
    static int simuOut = 0;
    int opt;

    while (1) {
        static struct option long_options[] = {
            {"commandfile", required_argument, 0,            0 },
            {"device",      required_argument, 0,            0 },
            {"debug",       no_argument,       &debug,       1 },
            {"vsim",        no_argument,       &simuOut,     1 },
            {"logfile",     required_argument, 0,            0 },
            {"pidfile",     required_argument, 0,            0 },
            {"username",    required_argument, 0,            0 },
            {"groupname",   required_argument, 0,            0 },
            {"nodaemon",    no_argument,       &makeDaemon,  0 },
            {"port",        required_argument, 0,            0 },
            {"listen",      required_argument, 0,            0 },
            {"syslog",      no_argument,       &useSyslog,   1 },
            {"xmlfile",     required_argument, 0,            0 },
            {"verbose",     no_argument,       &verbose,     1 },
            {"Version",     no_argument,       0,            0 },
            {"inet4",       no_argument,       &inetversion, 4 },
            {"inet6",       no_argument,       &inetversion, 6 },
            {"help",        no_argument,       0,            0 },
            {0,             0,                 0,            0 }
        };

        // getopt_long stores the option index here.
        int option_index = 0;
        opt = getopt_long (argc, argv, "c:d:gil:P:U:G:np:L:sx:vV46h",
                           long_options, &option_index);

        // Detect the end of the options.
        if (opt == -1) {
            break;
        }

        switch (opt) {
        case 0:
            // If this option sets a flag, we do nothing for now
            if (long_options[option_index].flag != 0) {
                break;
            }
            if (verbose) {
                printf("option %s", long_options[option_index].name);
                if (optarg) {
                    printf(" with arg %s", optarg);
                }
                printf("\n");
            }
            if (strcmp("help", long_options[option_index].name) == 0) {
                usage();
            }
            break;
        case '4':
            inetversion = 4;
            break;
        case '6':
            inetversion = 6;
            break;
        case 'c':
            cmdfile = optarg;
            break;
        case 'd':
            device = optarg;
            break;
        case 'g':
            debug = 1;
            break;
        case 'i':
            simuOut = 1;
            break;
        case 'l':
            logfile = optarg;
            break;
        case 'P':
            pidFile = optarg;
            break;
        case 'U':
            username = optarg;
            break;
        case 'G':
            groupname = optarg;
            break;
        case 'n':
            makeDaemon = 0;
            break;
        case 'p':
            if (!parseTcpPort(optarg, &tcpport)) {
                fprintf(stderr, "Invalid TCP port: %s\n", optarg);
                return 2;
            }
            break;
        case 'L':
            listenAddress = optarg;
            break;
        case 's':
            useSyslog = 1;
            break;
        case 'v':
            puts ("option -v\n");
            verbose = 1;
            break;
        case 'V':
            printf("vcontrold version %s\n", VERSION);
            return 0;
            break;
        case 'x':
            xmlfile = optarg;
            break;
        case 'h':
        case '?':
            // getopt_long already printed an error message.
            usage();
            break;
        default:
            abort();
        }
    }

    initLog(useSyslog, logfile, debug);

    logIT(LOG_NOTICE, "started vcontrold version %s", VERSION);

    if (!parseXMLFile(xmlfile)) {
        fprintf(stderr, "Error loading %s, terminating\n", xmlfile);
        exit(1);
    }

    // The global variables cfgPtr and protoPtr have been filled
    // (parseXMLFile has already dereferenced cfgPtr, so it can't be NULL here)
    if (! tcpport) {
        tcpport = cfgPtr->port;
    }
    if (! listenAddress) {
        listenAddress = cfgPtr->listenAddress;
    }
    if (! device) {
        device = cfgPtr->tty;
    }
    if (! logfile) {
        logfile = cfgPtr->logfile;
    }
    if (! pidFile) {
        pidFile = cfgPtr->pidfile;
    }
    if (! username) {
        username = cfgPtr->username;
    }
    if (! groupname) {
        groupname = cfgPtr->groupname;
    }
    if (! useSyslog) {
        useSyslog = cfgPtr->syslog;
    }
    if (! debug) {
        debug = cfgPtr->debug;
    }

    if (!initLog(useSyslog, logfile, debug)) {
        exit(1);
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = sigHupHandler;
    if (sigaction(SIGHUP, &sa, NULL) < 0) {
        logIT1(LOG_ERR, "Error handling SIGHUP");
        exit(1);
    }

    sa.sa_handler = sigTermHandler;
    if (sigaction(SIGQUIT, &sa, NULL) < 0) {
        logIT(LOG_ERR, "Error handling SIGQUIT: %s", strerror(errno));
        exit(1);
    }
    if (sigaction(SIGINT, &sa, NULL) < 0) {
        logIT(LOG_ERR, "Error handling SIGINT: %s", strerror(errno));
        exit(1);
    }
    if (sigaction(SIGTERM, &sa, NULL) < 0) {
        logIT(LOG_ERR, "Error handling SIGTERM: %s", strerror(errno));
        exit(1);
    }

    // If -i has been given, we log the commands in Simulator INI format
    if (simuOut) {
        char file[100];
        snprintf(file, sizeof(file), INIOUTFILE, cfgPtr->devID);
        if (! (iniFD = fopen(file, "w"))) {
            logIT(LOG_ERR, "Could not create simulator INI file %s", file);
        }
        if (iniFD) {
            fprintf(iniFD, "[DATA]\n");
        }
    }

    int fd = 0;
    char result[MAXBUF];
    size_t resultLen = 0;
    int sid;
    int pidFD = -1;
    char str[32];

    if (tcpport) {
        if (makeDaemon) {
            int pid;
            // Some siganl handling
            if (signal(SIGCHLD, SIG_IGN) == SIG_ERR) {
                logIT1(LOG_ERR, "Error handling SIGCHLD");
            }

            pid = fork();
            if (pid < 0) {
                logIT(LOG_ERR, "fork failed (%d)", pid);
                exit(1);
            }
            if (pid > 0) {
                exit(0); // Parent is terminated, child runs on
            }

            // From here, only the child is running

            // Close FD
            close(STDIN_FILENO);
            close(STDOUT_FILENO);
            close(STDERR_FILENO);
            umask(0);

            sid = setsid();
            if (sid < 0) {
                logIT1(LOG_ERR, "setsid failed");
                exit(1);
            }
            if (chdir("/") < 0) {
                logIT1(LOG_ERR, "chdir / failed");
                exit(1);
            }

            if (pidFile) {
                pidFD = open(pidFile, O_RDWR | O_CREAT, 0600);
                if (pidFD == -1) {
                    logIT(LOG_ERR, "Could not open PID lock file %s, exiting", pidFile);
                    exit(1);
                }
                if (lockf(pidFD, F_TLOCK, 0) == -1) {
                    logIT(LOG_ERR, "Could not lock PID lock file %s, exiting", pidFile);
                    exit(1);
                }

                snprintf(str, sizeof(str), "%d\n", getpid());
                if (write(pidFD, str, strlen(str)) != (ssize_t)strlen(str)) {
                    logIT(LOG_ERR, "Could not write PID file %s", pidFile);
                    exit(1);
                }
            }
        }

        int sockfd = -1;
        int listenfd = openSocket2(tcpport, listenAddress);
        if (listenfd < 0) {
                logIT(LOG_ERR, "Could not start vcontrold on %s port %d", (listenAddress != NULL ? listenAddress : "localhost"), tcpport);
                exit(1);
        }

        // Drop privileges after binding
        if (0 == getuid()) {
            struct passwd *pw;
            struct group *grp;
            struct stat stb;

            if (username == NULL) {
                username = strdup("nobody");
                if (!username) {
                    logIT1(LOG_ERR, "Could not allocate default username");
                    exit(1);
                }
            }
            pw = getpwnam(username);
            if (pw == NULL) {
                int errsv = errno;
                logIT(LOG_ERR, "Error while dropping privileges: calling getpwnam(\"%s\") failed with errno %d", username, errsv);
                exit(1);
            }
            if (groupname == NULL) {
                groupname = strdup("dialout");
                if (!groupname) {
                    logIT1(LOG_ERR, "Could not allocate default groupname");
                    exit(1);
                }
            }
            grp = getgrnam(groupname);
            if (grp == NULL) {
                int errsv = errno;
                logIT(LOG_ERR, "Error while dropping privileges: calling getgrnam(\"%s\") failed with errno %d", groupname, errsv);
                exit(1);
            }

            int logfd = logfile
                ? open(logfile, O_CREAT | O_RDONLY, S_IRUSR | S_IWUSR)
                : -1;
            // Make logfile accessible to the anticipated user/group
            if ((logfd >= 0) && (fstat(logfd, &stb) == 0)) {
                fchmod(logfd, stb.st_mode | S_IRGRP | S_IWGRP);
                if (fchown(logfd, pw->pw_uid, grp->gr_gid) != 0) {
                    logIT(LOG_WARNING, "Could not change logfile ownership: %s",
                          strerror(errno));
                }
            }
            if (logfd >= 0) {
                close(logfd);
            }
            // Make lock file accessible to the anticipated user/group
            if (stat(tmpfilename, &stb) == 0) {
                chmod(tmpfilename, stb.st_mode | S_IRGRP | S_IWGRP);
                if (chown(tmpfilename, pw->pw_uid, grp->gr_gid) != 0) {
                    logIT(LOG_WARNING, "Could not change lockfile ownership: %s",
                          strerror(errno));
                }
            }

            if (setgroups(0, NULL) != 0) {
                int errsv = errno;
                logIT(LOG_ERR, "Error while dropping privileges: calling setgroups(0, NULL) failed with errno %d", errsv);
                exit(1);
            }

            if (setgid(grp->gr_gid) != 0) {
                int errsv = errno;
                logIT(LOG_ERR, "Error while dropping privileges: calling setgid(%d) failed with errno %d", grp->gr_gid, errsv);
                exit(1);
            }

            if (setuid(pw->pw_uid) != 0) {
                int errsv = errno;
                logIT(LOG_ERR, "Error while dropping privileges: calling setuid(%d) failed with errno %d", pw->pw_uid, errsv);
                exit(1);
            }

            logIT(LOG_INFO, "Dropped privileges, now running with effective user ID %d, group ID %d", geteuid(), getegid());
        } else {
            if (username || groupname) {
                logIT1(LOG_INFO, "Not started as root, username/groupname settings ignored");
            }
        }

        if (!vcontrol_seminit()) { logIT1(LOG_ERR, "Could not initialize process semaphore"); exit(1); }

        signal(SIGPIPE, SIG_IGN);
        while (!terminateRequested) {
            int configReloaded = 0;
            sockfd = listenToSocket(listenfd, makeDaemon);
            if (reloadRequested) {
                reloadRequested = 0;
                logIT1(LOG_NOTICE, "Reloading configuration after SIGHUP");
                configReloaded = reloadConfig();
            }
            if (sockfd >= 0) {
                // Socket returned fd, the rest is done interactively
                interactive(sockfd, cfgPtr->tty);
                closeSocket(sockfd);
                setDebugFD(-1);
                if (makeDaemon) {
                    logIT(LOG_INFO, "Child process with PID %d terminated", getpid());
                    exit(0); // The child bids boodbye
                }
            } else if (!terminateRequested && !configReloaded) {
                logIT1(LOG_ERR, "Error connecting");
            }
        }
        close(listenfd);
    } else {
        if (!vcontrol_seminit()) { logIT1(LOG_ERR, "Could not initialize process semaphore"); exit(1); }
    }

    if (cmdfile && *cmdfile) {
        readCmdFile(cmdfile, result, sizeof(result), &resultLen,
                    cfgPtr && cfgPtr->tty ? cfgPtr->tty : device);
    }

    if (!vcontrol_semfree()) logIT1(LOG_ERR, "Could not remove process semaphore cleanly");

    if (fd >= 0) {
        close(fd);
    }
    if (pidFile) {
        if (pidFD >= 0) {
            close(pidFD);
        }
        unlink(pidFile);
    };
    logIT1(LOG_LOCAL0, "vcontrold terminated");

    return 0;
}
