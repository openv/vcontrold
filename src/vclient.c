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

// Vcontrold client

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <syslog.h>
#include <unistd.h>
#include <termios.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <getopt.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/ioctl.h>

#include "common.h"
#include "socket.h"
#include "io.h"
#include "client.h"
#include "vclient.h"
#include "version.h"

// global variables
int inetversion = 0;

void usage()
{
    //      1       10        20        30        40        50        60        70        80

    printf("usage:\n");
    printf("    vclient [-h <ip:port>] [-c <command1,command2,..>] [-f <commandfile>]\n");
    printf("            [-s <csv file>] [-t <template file>] [-o <output file>]\n");
    printf("            [-x <exec file>] [-k] [-m] [-v]\n\n");

    printf("or:\n");
    printf("    vclient [--host <ip>] [--port <port>] [--command <command1,command2,..>]\n");
    printf("            [--commandfile <command file>] [--csvfile <csv file>]\n");
    printf("            [--template <template file>] [--output <output file>]\n");
    printf("            [--execute <exec file>] [--cacti] [--munin] [--verbose]\n");
    printf("            [command3 [command4] ...]\n\n");

    printf("    -h|--host         <IPv4>:<Port> or <IPv6> of vcontrold\n");
    printf("    -p|--port         <port> of vcontrold when using IPv6\n");
    printf("    -c|--command      List of commands to be executed, sparated by commas\n");
    printf("    -f|--commandfile  Optional command file, one command per line\n");
    printf("    -s|--csvfile      Format output in CSV for further processing\n");
    printf("    -t|--template     Template, variables are substituted with acquired values\n");
    printf("    -o|--output       Write to given file instead of STDOUT\n");
    printf("    -x|--execute      The converted template (cf. -t) is written to the given\n");
    printf("                      file and executed subsequently\n");
    printf("    -m|--munin        Output a Munin data logger compatible format (units and\n");
    printf("                      error details are discarded)\n");
    printf("    -k|--cacti        Output a Cacti data logger compatible format (units and\n");
    printf("                      error details are discarded)\n");
    printf("    -v|--verbose      Be verbose (for testing purposes)\n");
    printf("    -V|--Version      Print version and exit\n");
    printf("    -4|--inet4        IPv4 is preferred\n");
    printf("    -6|--inet6        IPv6 is preferred\n");
    printf("                      (if none of the two above is set, the system default will\n");
    printf("                      be used)\n");
    printf("    --help            Display this help message\n\n");

    exit(1);
}

int main(int argc, char *argv[])
{
    // Get the command line options
    char *host = NULL;
    int port = 0;
    char commands[512] = "";
    const char *cmdfile = NULL;
    const char *csvfile = NULL;
    const char *tmplfile = NULL;
    const char *outfile = NULL;
    char string[1024] = "";
    char result[1024] = "";
    int sockfd;
    char dummylog[] = "\0";
    int opt;
    static int verbose = 0;
    static int munin = 0;
    static int cacti = 0;
    short execMe = 0;
    trPtr resPtr;
    FILE *filePtr;
    FILE *ofilePtr;

    while (1) {
        static struct option long_options[] = {
            {"host",        required_argument, 0,            'h'},
            {"port",        required_argument, 0,            'p'},
            {"command",     required_argument, 0,            'c'},
            {"commandfile", required_argument, 0,            'f'},
            {"csvfile",     required_argument, 0,            's'},
            {"template",    required_argument, 0,            't'},
            {"output",      required_argument, 0,            'o'},
            {"execute",     required_argument, 0,            'x'},
            {"verbose",     no_argument,       &verbose,     1  },
            {"Version",     no_argument,       0,            0  },
            {"munin",       no_argument,       &munin,       1  },
            {"cacti",       no_argument,       &cacti,       1  },
            {"inet4",       no_argument,       &inetversion, 4  },
            {"inet6",       no_argument,       &inetversion, 6  },
            {"help",        no_argument,       0,            0  },
            {0,             0,                 0,            0  }
        };
        // getopt_long stores the option index here.
        int option_index = 0;
        opt = getopt_long(argc, argv, "h:p:c:f:s:t:o:x:vVmk46", long_options, &option_index);

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
        case 'v':
            puts("option -v\n");
            verbose = 1;
            break;
        case 'V':
            printf("vclient version %s\n", VERSION);
            exit(1);
            break;
        case 'm':
            if (verbose) {
                puts("option -m\n");
            }
            munin = 1;
            break;
        case 'k':
            if (verbose) {
                puts("option -k\n");
            }
            cacti = 1;
            break;
        case 'h':
            if (verbose) {
                printf("option -h with value `%s'\n", optarg);
            }
            host = optarg;
            break;
        case 'p':
            if (verbose) {
                printf("option -p with value `%s'\n", optarg);
            }
            port = atoi(optarg);
            if (port == 0) {
                fprintf(stderr, "Invalid value for option --port: %s\n", optarg);
                usage(); // and exit
            }
            break;
        case 'c':
            if (verbose) {
                printf("option -c with value `%s'\n", optarg);
                printf("sizeof optarg:%zu, strlen:%zu, sizeof commands:%zu, strlen:%zu,  [%s]\n",
                       sizeof(optarg), strlen(optarg), sizeof(commands), strlen(commands),
                       commands);
            }
            if (strlen(commands) == 0) {
                strncpy(commands, optarg, sizeof(commands));
            } else {
                if (strlen(optarg) + 2 > sizeof(commands) - strlen(commands)) {
                    fprintf(stderr, "too many commands\n");
                    break;
                }
                strcat(commands, ",");
                strncat(commands, optarg, sizeof(commands) - strlen(commands) - 2);
            }
            break;
        case 'f':
            if (verbose) {
                printf("option -f with value `%s'\n", optarg);
            }
            cmdfile = optarg;
            break;
        case 's':
            if (verbose) {
                printf("option -s with value `%s'\n", optarg);
            }
            csvfile = optarg;
            break;
        case 't':
            if (verbose) {
                printf("option -t with value `%s'\n", optarg);
            }
            tmplfile = optarg;
            break;

        case 'o':
        case 'x':
            if (verbose) {
                printf("option -%c with value `%s'\n", opt, optarg);
            }
            outfile = optarg;
            if (opt == 'x') {
                execMe = 1;
            }
            break;
        case '4':
            if (verbose) {
                printf("option -%c with value `%s'\n", opt, optarg);
            }
            inetversion = 4;
            break;
        case '6':
            if (verbose) {
                printf("option -%c with value `%s'\n", opt, optarg);
            }
            inetversion = 6;
            break;
        case '?':
            // getopt_long already printed an error message.
            usage();
            break;
        default:
            abort();
        }
    }

    if (host == NULL) {
      host = "localhost";
    }

    // Collect any remaining command line arguments (not options).
    // and use the as commands like for the -c option.
    if (optind < argc) {
        if (verbose) {
            printf("non-option ARGV-elements: ");
        }
        while (optind < argc) {
            if (verbose) {
                printf("%s ", argv[optind]);
            }
            if (strlen(commands) == 0) {
                strncpy(commands, argv[optind], sizeof(commands));
            } else {
                if (strlen(argv[optind]) + 2 > sizeof(commands) - strlen(commands)) {
                    fprintf(stderr, "Command list too long\n");
                    optind++;
                    break;
                }
                strcat(commands, ",");
                strncat(commands, argv[optind], sizeof(commands) - strlen(commands) - 2);
            }
            optind++;
        }
        if (verbose) {
            putchar('\n');
        }
    }

   /* Check for :<port> if port==0
       then separate the port number from the host name
       or the IP adsress.
       The IP address could be a plain old IPv4 or a IPv6 one,
       which could contain more than one ':', so that makes a bad
       separator in the IPv6 case and you better use --port
       -h 192.168.2.1:3002 vs --host 2003:abcd:ff::1 --port 3002
       or --host 2003:abcd:ff::1:3002, assume the last :3002 be the port
       This is just for backwards compatibility. */
    if (port == 0) {
        // check for last ':' in host
        char *last_colon = NULL;

        last_colon = strrchr(host, ':');
        if (last_colon != NULL) {
            port = atoi(last_colon + 1);
            *last_colon = '\0';
        }
    }
    if (port == 0) {
        port = DEFAULT_PORT;
    }

    if (verbose) {
      printf("Host: %s Port: %d\n",host,port);
    }

    initLog(0, dummylog, verbose);
    if (! *commands && !cmdfile) {
        usage();
    }

    sockfd = connectServer(host, port);
    if (sockfd < 0) {
        logIT(LOG_ERR, "No connection to host %s on port %d", host, port);
        exit(1);
    }

    // Give commands directly
    resPtr = NULL;
    if (*commands) {
        resPtr = sendCmds(sockfd, commands);
    } else if (cmdfile) {
        resPtr = sendCmdFile(sockfd, cmdfile);
    }
    if (! resPtr) {
        logIT(LOG_ERR, "Error communicating with the server");
        exit(1);
    }
    disconnectServer(sockfd);

    if (outfile) {
        if (! (ofilePtr = fopen(outfile, "w"))) {
            logIT(LOG_ERR, "Could not create file %s", outfile);
            exit(1);
        }
        memset(string, 0, sizeof(string));
        logIT(LOG_INFO, "Output file %s", outfile);
    } else {
        ofilePtr = fdopen(fileno(stdout), "w");
    }

    // The result is in the resPtr list, now we differentiate the output
    if (csvfile) {
        // Kompakt Format mit Semikolon getrennt
        if (! (filePtr = fopen(csvfile, "a"))) {
            logIT(LOG_ERR, "Could not create file %s", csvfile);
            exit(1);
        }
        memset(string, 0, sizeof(string));
        memset(result, 0, sizeof(result));
        while (resPtr) {
            if (resPtr->err) {
                //fprintf(stderr,"%s:%s\n",resPtr->cmd,resPtr->err);
                fprintf(stderr, "%s: server error\n", resPtr->cmd);
                strcat(result, ";");
                resPtr = resPtr->next;
                continue;
            }
            memset(string, 0, sizeof(string));
            sprintf(string, "%f;", resPtr->result);
            strncat(result, string, sizeof(result) - strlen(result) - 1);
            resPtr = resPtr->next;
        }
        // Remove the last semicolon and add \n
        if (*result) {
            *(result + strlen(result) - 1) = '\n';
            fputs(result, filePtr);
        }
        fclose(filePtr);
    } else if (tmplfile) {
        // Template given
        char line[1000];
        char *lptr;
        char *lSptr;
        char *lEptr;
        char varname[20];
        unsigned short count;
        unsigned short idx;
        unsigned short maxIdx;
        trPtr tPtr = resPtr;
        trPtr *idxPtr;
        short varReplaced;

        // In array idxPtr, the particular results are referenced via the index
        for (count = 0; tPtr; tPtr = tPtr->next) {
            count++;
        }

        // We reserve an array with a suitable size
        idxPtr = calloc(count, sizeof(tPtr));

        maxIdx = count; // Biggest index in the variables

        count = 0;
        tPtr = resPtr;
        while (tPtr) {
            idxPtr[count++] = tPtr;
            tPtr = tPtr->next;
        }

        if (! (filePtr = fopen(tmplfile, "r"))) {
            logIT(LOG_ERR, "Could not open template file %s", tmplfile);
            exit(1);
        }
        // The following variables are to replace:
        // $Rn: Result (trPtr->raw)
        // $n: Float (trPtr->result)
        while ((fgets(line, sizeof(line) - 1, filePtr))) {
            logIT(LOG_INFO, "Tmpl line: %s", line);
            lSptr = line;
            while ((lptr = strchr(lSptr, '$'))) {
                varReplaced = 0;
                if ((lptr > line) && (*(lptr - 1) == '\\')) { // $ is masked by a backslash
                    memset(string, 0, sizeof(string));
                    strncpy(string, lSptr, lptr - lSptr - 1);
                    fprintf(ofilePtr, "%s%c", string, *lptr);
                    lSptr = lptr + 1;
                    continue;
                }
                lEptr = lptr + 1;
                // Noew, we search the end of the variables.
                while (isalpha(*lEptr) || isdigit(*lEptr)) {
                    lEptr++;
                }
                memset(varname, 0, sizeof(varname));
                strncpy(varname, lptr + 1, lEptr - lptr - 1);
                logIT(LOG_INFO, "    Recognized variable: %s", varname);

                // We output everything up to this
                memset(string, 0, sizeof(string));
                strncpy(string, lSptr, lptr - lSptr);
                fprintf(ofilePtr, "%s", string);

                // We differentiate the different variables

                // $Rn
                if ((strlen(varname) > 1) && (*varname == 'R') && (idx = atoi(varname + 1))) {
                    // Variable R index then in idx
                    if ((idx - 1) < maxIdx) {
                        tPtr = idxPtr[idx - 1];
                        logIT(LOG_INFO, "%s:%s", tPtr->cmd, tPtr->raw);
                        if (tPtr->raw) {
                            fprintf(ofilePtr, "%s", tPtr->raw);
                        }
                    } else {
                        logIT(LOG_ERR, "Index of variable $%s > %d", varname, maxIdx - 1);
                    }
                }
                // $Cn
                else if ((strlen(varname) > 1) && (*varname == 'C') && (idx = atoi(varname + 1))) {
                    // Variable R index then in idx
                    if ((idx - 1) < maxIdx) {
                        tPtr = idxPtr[idx - 1];
                        logIT(LOG_INFO, "Command: %s", tPtr->cmd);
                        if (tPtr->cmd) {
                            fprintf(ofilePtr, "%s", tPtr->cmd);
                        }
                    } else {
                        logIT(LOG_ERR, "Index of Variable $%s > %d", varname, maxIdx - 1);
                    }
                }
                // $En
                else if ((strlen(varname) > 1) && (*varname == 'E') && (idx = atoi(varname + 1))) {
                    // Variable R index then in idx
                    if ((idx - 1) < maxIdx) {
                        tPtr = idxPtr[idx - 1];
                        logIT(LOG_INFO, "Error: %s: %s", tPtr->cmd, tPtr->err);
                        if (tPtr->err) {
                            fprintf(ofilePtr, "%s", tPtr->err);
                        } else {
                            fprintf(ofilePtr, "OK");
                        }
                    } else {
                        logIT(LOG_ERR, "Index of variable $%s > %d", varname, maxIdx - 1);
                    }
                }
                // $n
                else if (isdigit(*varname) && (idx = atoi(varname)))  {
                    if ((idx - 1) < maxIdx) {
                        tPtr = idxPtr[idx - 1];
                        logIT(LOG_INFO, "%s:%f", tPtr->cmd, tPtr->result);
                        //if (tPtr->result)
                        fprintf(ofilePtr, "%f", tPtr->result);
                    } else {
                        logIT(LOG_ERR, "Index of variable $%s > %d", varname, maxIdx - 1);
                    }
                } else {
                    memset(string, 0, sizeof(string));
                    strncpy(string, lptr, lEptr - lptr);
                    fprintf(ofilePtr, "%s", string);
                }
                lSptr = lEptr;
            }
            fprintf(ofilePtr, "%s", lSptr);
        }

        fclose(filePtr);

        if (outfile && *outfile && execMe) {
            // Make the file executable and start
            fclose(ofilePtr);
            memset(string, 0, sizeof(string));
            logIT(LOG_INFO, "Executing file %s", outfile);
            if (chmod(outfile, S_IXUSR | S_IRUSR | S_IWUSR) != 0) {
                logIT(LOG_ERR, "Error chmod +x %s", outfile);
                exit(1);
            }

            short ret;
            if ((ret = system(outfile)) == -1) {
                logIT(LOG_ERR, "Error system(%s)", outfile);
                exit(1);
            }

            logIT(LOG_INFO, "Ret Code: %d", ret);
            exit(ret);
        }

    } else if (munin) {
        // Output Munin format
        while (resPtr) {
            fprintf(ofilePtr, "%s.value ", resPtr->cmd);
            if (resPtr->err) {
                fprintf(ofilePtr, "U\n");
                resPtr = resPtr->next;
                continue;
            }
            if (resPtr->raw) {
                fprintf(ofilePtr, "%f\n", resPtr->result);
            } else {
                fprintf(ofilePtr, "U\n");
            }
            resPtr = resPtr->next;
        }

    } else if (cacti) {
        // Output Cacti format
        int index = 1;
        while (resPtr) {
            fprintf(ofilePtr, "v%d:", index);
            if (resPtr->err) {
                fprintf(ofilePtr, "U ");
                resPtr = resPtr->next;
                index++;
                continue;
            }
            if (resPtr->raw) {
                fprintf(ofilePtr, "%f ", resPtr->result);
            } else {
                fprintf(ofilePtr, "U ");
            }
            index++;
            resPtr = resPtr->next;
        }
        fprintf(ofilePtr, "\n");

    } else {
        while (resPtr) {
            fprintf(ofilePtr, "%s:\n", resPtr->cmd);
            if (resPtr->err) {
                //fprintf(stderr,"%s",resPtr->err);
                fprintf(stderr, "server error\n");
                resPtr = resPtr->next;
                continue;
            }
            if (resPtr->raw) {
                fprintf(ofilePtr, "%s\n", resPtr->raw);
            }
            resPtr = resPtr->next;
        }
    }

    return 0;
}
