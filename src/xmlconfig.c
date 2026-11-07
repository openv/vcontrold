// SPDX-FileCopyrightText: 2007-2016 The original vcontrold authors (cf. doc/original_authors.txt)
//
// SPDX-License-Identifier: GPL-3.0-or-later

// Routines for reading XML files

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <syslog.h>
#include <libxml/parser.h>
#include <libxml/xinclude.h>
#include <arpa/inet.h>
#include <errno.h>
#include <limits.h>

#include "xmlconfig.h"
#include "common.h"
#include "parser.h"

#if defined(__FreeBSD__)
#include <netinet/in.h>
#endif

// Declarations
protocolPtr newProtocolNode(protocolPtr ptr);
char *getPropertyNode(xmlAttrPtr cur, xmlChar *name);
enumPtr newEnumNode(enumPtr ptr);
void removeProtocolList(protocolPtr ptr);
void removeUnitList(unitPtr ptr);
void removeMacroList(macroPtr ptr);
void removeCommandList(commandPtr ptr);
void removeDeviceList(devicePtr ptr);
void removeIcmdList(icmdPtr ptr);
void removeEnumList(enumPtr ptr);
void freeAllLists(void);

// Globale variables
protocolPtr protoPtr = NULL;
unitPtr uPtr = NULL;
devicePtr devPtr = NULL;
configPtr cfgPtr = NULL;
commandPtr cmdPtr = NULL;

static int parse_long_range(const char *text, long min, long max,
                            long *value, const char *field)
{
    char *end = NULL;
    long parsed;
    if (!text || !*text || !value) {
        logIT(LOG_ERR, "Missing numeric XML value for %s", field);
        return 0;
    }
    errno = 0;
    parsed = strtol(text, &end, 10);
    if (errno || end == text || *end != '\0' || parsed < min || parsed > max) {
        logIT(LOG_ERR, "Invalid XML value for %s: %s (allowed %ld..%ld)",
              field, text, min, max);
        return 0;
    }
    *value = parsed;
    return 1;
}

static int parse_boolean(const char *text, int *value, const char *field)
{
    if (!text || !*text || !value) {
        logIT(LOG_ERR, "Missing boolean XML value for %s", field);
        return 0;
    }
    if (!strcasecmp(text, "y") || !strcasecmp(text, "yes") ||
            !strcasecmp(text, "true") || !strcmp(text, "1")) {
        *value = 1;
        return 1;
    }
    if (!strcasecmp(text, "n") || !strcasecmp(text, "no") ||
            !strcasecmp(text, "false") || !strcmp(text, "0")) {
        *value = 0;
        return 1;
    }
    logIT(LOG_ERR, "Invalid boolean XML value for %s: %s", field, text);
    return 0;
}

static void removeConfigNode(configPtr ptr)
{
    if (!ptr) return;
    free(ptr->tty);
    free(ptr->logfile);
    free(ptr->devID);
    free(ptr->listenAddress);
    free(ptr->pidfile);
    free(ptr->username);
    free(ptr->groupname);
    free(ptr);
}

protocolPtr newProtocolNode(protocolPtr ptr)
{
    protocolPtr nptr;
    if (ptr && ptr->next) {
        return newProtocolNode(ptr->next);
    }

    nptr = calloc(1, sizeof(Protocol));
    if (! nptr) {
        logIT1(LOG_ERR, "calloc protocol failed");
        return NULL;
    }

    if (ptr) {
        ptr->next = nptr;
    }
    nptr->next = NULL;
    nptr->icPtr = NULL;
    nptr->id = 0;

    return nptr;
}

protocolPtr getProtocolNode(protocolPtr ptr, const char *name)
{
    if (! ptr) {
        return NULL;
    }

    if (strcmp(ptr->name, name) != 0) {
        return getProtocolNode(ptr->next, name);
    }

    return ptr;
}

void removeProtocolList(protocolPtr ptr)
{
    if (ptr && ptr->next) {
        removeProtocolList(ptr->next);
    }
    if (ptr) {
        removeMacroList(ptr->mPtr);
        removeIcmdList(ptr->icPtr);
        free(ptr->name);
        free(ptr);
    }
}

unitPtr newUnitNode(unitPtr ptr)
{
    unitPtr nptr;
    if (ptr && ptr->next) {
        return newUnitNode(ptr->next);
    }

    nptr = calloc(1, sizeof(Unit));
    if (! nptr) {
        logIT1(LOG_ERR, "calloc unit failed");
        return NULL;
    }

    if (ptr) {
        ptr->next = nptr;
    }

    nptr->next = NULL;
    return nptr;
}

unitPtr getUnitNode(unitPtr ptr, const char *name)
{
    if (! ptr) {
        return NULL;
    }

    if (ptr->abbrev && (strcmp(ptr->abbrev, name) != 0)) {
        return getUnitNode(ptr->next, name);
    }

    return ptr;
}

void removeUnitList(unitPtr ptr)
{
    if (ptr && ptr->next) {
        removeUnitList(ptr->next);
    }

    if (ptr) {
        removeEnumList(ptr->ePtr);
        free(ptr->name);
        free(ptr->abbrev);
        free(ptr->gCalc);
        free(ptr->sCalc);
        free(ptr->gICalc);
        free(ptr->sICalc);
        free(ptr->entity);
        free(ptr->type);
        free(ptr);
    }
}

macroPtr newMacroNode(macroPtr ptr)
{
    macroPtr nptr;
    if (ptr && ptr->next) {
        return newMacroNode(ptr->next);
    }

    nptr = calloc(1, sizeof(Macro));
    if (! nptr) {
        logIT1(LOG_ERR, "calloc macro failed");
        return NULL;
    }

    if (ptr) {
        ptr->next = nptr;
    }

    nptr->next = NULL;
    return nptr;
}

macroPtr getMacroNode(macroPtr ptr, const char *name)
{
    if (! ptr) {
        return NULL;
    }

    if (ptr->name && strcmp(ptr->name, name) != 0) {
        //if (*name && !(strstr(ptr->name,name)==ptr->name))
        return getMacroNode(ptr->next, name);
    }

    return ptr;
}

void removeMacroList(macroPtr ptr)
{
    if (ptr && ptr->next) {
        removeMacroList(ptr->next);
    }

    if (ptr) {
        free(ptr->name);
        free(ptr->command);
        free(ptr);
    }
}

commandPtr newCommandNode(commandPtr ptr)
{
    commandPtr nptr;
    if (ptr && ptr->next) {
        return newCommandNode(ptr->next);
    }

    nptr = calloc(1, sizeof(Command));
    if (! nptr) {
        logIT1(LOG_ERR, "calloc command failed");
        return NULL;
    }

    if (ptr) {
        ptr->next = nptr;
    }

    nptr->next = NULL;
    nptr->cmpPtr = NULL;
    nptr->bit = -1;

    return nptr;
}

void addCommandNode(commandPtr ptr, commandPtr nptr)
{
    if (! ptr) {
        return;
    } else if (ptr->next) {
        addCommandNode(ptr->next, nptr);
    } else {
        ptr->next = nptr;
    }
}

commandPtr getCommandNode(commandPtr ptr, const char *name)
{
    if (! ptr) {
        return NULL;
    }

    if (ptr->name && name && (strcmp(ptr->name, name) != 0)) {
        return getCommandNode(ptr->next, name);
    }

    return ptr;
}

void removeCommandList(commandPtr ptr)
{
    if (ptr && ptr->next) {
        removeCommandList(ptr->next);
    }

    if (ptr) {
        removeCompileList(ptr->cmpPtr);
        free(ptr->send);
        if (ptr->nodeType > 0) {
            // Has been read from the XML file
            if (ptr->addr) {
                free(ptr->addr);
            }
            if (ptr->precmd) {
                free(ptr->precmd);
            }
            if (ptr->unit) {
                free(ptr->unit);
            }
            if (ptr->pcmd) {
                free(ptr->pcmd);
            }
            if (ptr->errStr) {
                free(ptr->errStr);
            }
            if (ptr->nodeType == 1) {
                // Originalknoten
                if (ptr->name) {
                    free(ptr->name);
                }
                if (ptr->description) {
                    free(ptr->description);
                }
            }
        }
        free(ptr);
    }
}

devicePtr newDeviceNode(devicePtr ptr)
{
    devicePtr nptr;
    if (ptr && ptr->next) {
        return newDeviceNode(ptr->next);
    }

    nptr = calloc(1, sizeof(Device));
    if (! nptr) {
        logIT1(LOG_ERR, "calloc device failed");
        return NULL;
    }

    if (ptr) {
        ptr->next = nptr;
    }

    nptr->next = NULL;
    return nptr;
}

devicePtr getDeviceNode(devicePtr ptr, const char *id)
{
    if (! ptr) {
        return NULL;
    }

    if (strcmp(ptr->id, id) != 0) {
        return getDeviceNode(ptr->next, id);
    }

    return ptr;
}

void removeDeviceList(devicePtr ptr)
{
    if (ptr && ptr->next) {
        removeDeviceList(ptr->next);
    }
    if (ptr) {
        removeCommandList(ptr->cmdPtr);
        free(ptr->name);
        free(ptr->id);
        free(ptr);
    }
}

icmdPtr newIcmdNode(icmdPtr ptr)
{
    icmdPtr nptr;
    if (ptr && ptr->next) {
        return newIcmdNode(ptr->next);
    }

    nptr = calloc(1, sizeof(iCmd));
    if (! nptr) {
        logIT1(LOG_ERR, "calloc protocol command failed");
        return NULL;
    }

    if (ptr) {
        ptr->next = nptr;
    }

    nptr->next = NULL;
    return nptr;
}

icmdPtr getIcmdNode(icmdPtr ptr, const char *name)
{
    if (! ptr) {
        return NULL;
    }

    if (ptr->name && (strcmp(ptr->name, name) != 0)) {
        return getIcmdNode(ptr->next, name);
    }

    return ptr;
}

void removeIcmdList(icmdPtr ptr)
{
    if (ptr && ptr->next) {
        removeIcmdList(ptr->next);
    }
    if (ptr) {
        free(ptr->name);
        free(ptr->send);
        free(ptr);
    }
}

enumPtr newEnumNode(enumPtr ptr)
{
    enumPtr nptr;
    if (ptr && ptr->next) {
        return newEnumNode(ptr->next);
    }

    nptr = calloc(1, sizeof(Enumerate));
    if (! nptr) {
        logIT1(LOG_ERR, "calloc enumeration failed");
        return NULL;
    }

    if (ptr) {
        ptr->next = nptr;
    }

    return nptr;
}

enumPtr getEnumNode(enumPtr ptr, const char *search, int len)
{
    if (! ptr) {
        return NULL;
    }

    if ((len > 0) && ptr->bytes && (memcmp(ptr->bytes, search, len) == 0)) {
        // len given, we search for bytes
        return ptr;
    } else if (!len && ptr->text && (strcmp(ptr->text, search) == 0)) {
        // len == 0 --> string comparison
        return ptr;
    } else if ((len < 0) && (ptr->bytes == NULL)) {
        // len == -1 --> default value
        return ptr;
    } else {
        return getEnumNode(ptr->next, search, len);
    }
}

void removeEnumList(enumPtr ptr)
{
    if (ptr && ptr->next) {
        removeEnumList(ptr->next);
    }

    if (ptr) {
        free(ptr->text);
        free(ptr->bytes);
        free(ptr);
    }
}

void printNode(xmlNodePtr ptr)
{
    static int blanks = 0;
    int n;

    if (! ptr) {
        return;
    }

    for (n = 0; n <= blanks; n++) {
        printf(" ");
    }

    if ((ptr->type == XML_ELEMENT_NODE) || (ptr->type == XML_TEXT_NODE)) {
        printf("(%d) Node::Name=%s Type:%d Content=%s\n",
               ptr->line, ptr->name, ptr->type, ptr->content);
    } else {
        printf("Node::Name=%s\n", ptr->name);
    }

    if ((ptr->type == XML_ELEMENT_NODE) && ptr->properties) {
        blanks++;
        printNode((xmlNodePtr) ptr->properties);
        blanks--;
    }

    if (ptr->children) {
        blanks++;
        printNode(ptr->children);
        blanks--;
    }

    if ( ptr->next) {
        printNode(ptr->next);
    }
}

char *getTextNode(xmlNodePtr cur)
{
    if ((cur->children) && (cur->children->type == XML_TEXT_NODE)) {
        return (char *)cur->children->content;
    } else {
        return NULL;
    }
}

char *getPropertyNode(xmlAttrPtr cur, xmlChar *name)
{
    if ((cur) && (cur->type == XML_ATTRIBUTE_NODE) && xmlStrEqual(cur->name, name)) {
        return (char *)getTextNode((xmlNodePtr) cur);
    } else if (cur && cur->next) {
        return (char *)getPropertyNode(cur->next, name);
    } else {
        return NULL;
    }
}

static int duplicateString(char **destination, const char *source)
{
    size_t length;
    char *copy;

    if (!destination || !source) return 0;
    length = strlen(source);
    copy = calloc(length + 1, sizeof(char));
    if (!copy) {
        logIT1(LOG_ERR, "Could not allocate XML string");
        return 0;
    }
    memcpy(copy, source, length + 1);
    *destination = copy;
    return 1;
}

configPtr parseConfig(xmlNodePtr cur)
{
    int serialFound = 0;
    int netFound = 0;
    int logFound = 0;
    configPtr cfgPtr;
    char *chrPtr;
    xmlNodePtr prevPtr = cur;

    cfgPtr = calloc(1, sizeof(Config));
    if (!cfgPtr) {
        logIT1(LOG_ERR, "calloc failed");
        return NULL;
    }
    cfgPtr->port = 0;
    cfgPtr->syslog = 0;
    cfgPtr->debug = 0;
    cfgPtr->readonly = 0;

    while (cur) {
        logIT(LOG_INFO, "CONFIG:(%d) Node::Name=%s Type:%d Content=%s",
              cur->line, cur->name, cur->type, cur->content);
        if (xmlStrEqual(cur->name, (const xmlChar *)"serial"))  {
            serialFound = 1;
            prevPtr = cur;
            cur = cur->children;
        } else if (xmlStrEqual(cur->name, (const xmlChar *)"net")) {
            netFound = 1;
            prevPtr = cur;
            cur = cur->children;
        } else if (xmlStrEqual(cur->name, (const xmlChar *)"logging"))  {
            logFound = 1;
            prevPtr = cur;
            cur = cur->children;
        } else if (xmlStrEqual(cur->name, (const xmlChar *)"pidfile")) {
            chrPtr = getTextNode(cur);
            logIT(LOG_INFO, "   (%d) Node::Name=%s Type:%d Content=%s",
                  cur->line, cur->name, cur->type, chrPtr);
            if (chrPtr) {
                if (!duplicateString(&cfgPtr->pidfile, chrPtr)) goto fail;
            } else {
                if (!duplicateString(&cfgPtr->pidfile, "")) goto fail;
            }

            (cur->next && (! (cur->next->type == XML_TEXT_NODE) || cur->next->next))
                ? (cur = cur->next) : (cur = prevPtr->next);
        } else if (xmlStrEqual(cur->name, (const xmlChar *)"username")) {
            chrPtr = getTextNode(cur);
            logIT(LOG_INFO, "   (%d) Node::Name=%s Type:%d Content=%s",
                  cur->line, cur->name, cur->type, chrPtr);
            if (chrPtr) {
                if (!duplicateString(&cfgPtr->username, chrPtr)) goto fail;
            } else {
                if (!duplicateString(&cfgPtr->username, "")) goto fail;
            }

            (cur->next && (! (cur->next->type == XML_TEXT_NODE) || cur->next->next))
                ? (cur = cur->next) : (cur = prevPtr->next);
        } else if (xmlStrEqual(cur->name, (const xmlChar *)"groupname")) {
            chrPtr = getTextNode(cur);
            logIT(LOG_INFO, "   (%d) Node::Name=%s Type:%d Content=%s",
                  cur->line, cur->name, cur->type, chrPtr);
            if (chrPtr) {
                if (!duplicateString(&cfgPtr->groupname, chrPtr)) goto fail;
            } else {
                if (!duplicateString(&cfgPtr->groupname, "")) goto fail;
            }

            (cur->next && (! (cur->next->type == XML_TEXT_NODE) || cur->next->next))
                ? (cur = cur->next) : (cur = prevPtr->next);
        } else if (xmlStrEqual(cur->name, (const xmlChar *)"device"))  {
            chrPtr = getPropertyNode(cur->properties, (xmlChar *)"ID");
            logIT(LOG_INFO, "     Device ID=%s", chrPtr);
            if (chrPtr) {
                if (!duplicateString(&cfgPtr->devID, chrPtr)) goto fail;
            } else {
                if (!duplicateString(&cfgPtr->devID, "")) goto fail;
            }
            cur = cur->next;
        } else if (serialFound && xmlStrEqual(cur->name, (const xmlChar *)"tty")) {
            chrPtr = getTextNode(cur);
            logIT(LOG_INFO, "   (%d) Node::Name=%s Type:%d Content=%s",
                  cur->line, cur->name, cur->type, chrPtr);
            if (chrPtr) {
                if (!duplicateString(&cfgPtr->tty, chrPtr)) goto fail;
            } else {
                if (!duplicateString(&cfgPtr->tty, "")) goto fail;
            }

            (cur->next && (! (cur->next->type == XML_TEXT_NODE) || cur->next->next))
                ? (cur = cur->next) : (cur = prevPtr->next);
        } else if (netFound && xmlStrEqual(cur->name, (const xmlChar *)"port"))  {
            chrPtr = getTextNode(cur);
            logIT(LOG_INFO, "   (%d) Node::Name=%s Type:%d Content=%s",
                  cur->line, cur->name, cur->type, chrPtr);
            if (chrPtr) {
                long value;
                if (!parse_long_range(chrPtr, 1, 65535, &value, "port")) {
                    goto fail;
                }
                cfgPtr->port = (int)value;
            }
            (cur->next && (! (cur->next->type == XML_TEXT_NODE) || cur->next->next))
                ? (cur = cur->next) : (cur = prevPtr->next);
        } else if (netFound && xmlStrEqual(cur->name, (const xmlChar *)"listen")) {
            chrPtr = getTextNode(cur);
            logIT(LOG_INFO, "   (%d) Node::Name=%s Type:%d Content=%s",
                  cur->line, cur->name, cur->type, chrPtr);
            if (chrPtr) {
                if (!duplicateString(&cfgPtr->listenAddress, chrPtr)) goto fail;
            } else {
                if (!duplicateString(&cfgPtr->listenAddress, "")) goto fail;
            }
            (cur->next && (! (cur->next->type == XML_TEXT_NODE) || cur->next->next))
                ? (cur = cur->next) : (cur = prevPtr->next);
        } else if (logFound && xmlStrEqual(cur->name, (const xmlChar *)"file")) {
            chrPtr = getTextNode(cur);
            logIT(LOG_INFO, "   (%d) Node::Name=%s Type:%d Content=%s",
                  cur->line, cur->name, cur->type, chrPtr);
            if (chrPtr) {
                if (!duplicateString(&cfgPtr->logfile, chrPtr)) goto fail;
            } else {
                if (!duplicateString(&cfgPtr->logfile, "")) goto fail;
            }
            (cur->next && (! (cur->next->type == XML_TEXT_NODE) || cur->next->next))
                ? (cur = cur->next) : (cur = prevPtr->next);
        } else if (logFound && xmlStrEqual(cur->name, (const xmlChar *)"syslog")) {
            chrPtr = getTextNode(cur);
            logIT(LOG_INFO, "   (%d) Node::Name=%s Type:%d Content=%s",
                  cur->line, cur->name, cur->type, chrPtr);
            if (!parse_boolean(chrPtr, &cfgPtr->syslog, "syslog")) goto fail;
            (cur->next &&
            (! (cur->next->type == XML_TEXT_NODE) || cur->next->next))
                ? (cur = cur->next) : (cur = prevPtr->next);
        } else if (logFound && xmlStrEqual(cur->name, (const xmlChar *)"debug")) {
            chrPtr = getTextNode(cur);
            if (!parse_boolean(chrPtr, &cfgPtr->debug, "debug")) goto fail;
            (cur->next && (! (cur->next->type == XML_TEXT_NODE) || cur->next->next))
                ? (cur = cur->next) : (cur = prevPtr->next);
        } else if (xmlStrEqual(cur->name, (const xmlChar *)"readonly")) {
            chrPtr = getTextNode(cur);
            if (!parse_boolean(chrPtr, &cfgPtr->readonly, "readonly")) goto fail;
            (cur->next && (! (cur->next->type == XML_TEXT_NODE) || cur->next->next))
                ? (cur = cur->next) : (cur = prevPtr->next);
        } else {
            (cur->next && (! (cur->next->type == XML_TEXT_NODE) || cur->next->next))
                ? (cur = cur->next) : (cur = prevPtr->next);
        }
    }

    return cfgPtr;

fail:
    removeConfigNode(cfgPtr);
    return NULL;
}

unitPtr parseUnit(xmlNodePtr cur)
{
    unitPtr uPtr = NULL;
    unitPtr uStartPtr = NULL;
    char *unit;
    char *chrPtr;
    int unitFound = 0;
    xmlNodePtr prevPtr = cur;
    char string[256];
    enumPtr ePtr;

    while (cur) {
        logIT(LOG_INFO, "UNIT: (%d) Node::Name=%s Type:%d Content=%s",
              cur->line, cur->name, cur->type, cur->content);

        if (cur->type == XML_TEXT_NODE) {
            cur = cur->next;
            continue;
        }

        if (xmlStrEqual(cur->name, (const xmlChar *)"unit")) {
            unit = getPropertyNode(cur->properties, (xmlChar *)"name");
            if (unit) {
                // read new unit
                logIT(LOG_INFO, "New unit: %s", unit);
                uPtr = newUnitNode(uStartPtr);
                if (!uPtr) {
                    removeUnitList(uStartPtr);
                    return NULL;
                }
                if (! uStartPtr) {
                    uStartPtr = uPtr;
                }
                if (!duplicateString(&uPtr->name, unit)) goto fail;
                unitFound = 1;
                prevPtr = cur;
                cur = cur->children;
                continue;
            }
        } else if (unitFound && xmlStrEqual(cur->name, (const xmlChar *)"enum")) {
            chrPtr = getPropertyNode(cur->properties, (xmlChar *)"text");
            logIT(LOG_INFO, "   (%d) Node::Name=%s Type:%d Content=%s (text)",
                  cur->line, cur->name, cur->type, chrPtr);
            if (chrPtr) {
                ePtr = newEnumNode(uPtr->ePtr);
                if (!ePtr) {
                    removeUnitList(uStartPtr);
                    return NULL;
                }
                if (! uPtr->ePtr) {
                    uPtr->ePtr = ePtr;
                }
                if (!duplicateString(&ePtr->text, chrPtr)) goto fail;
                chrPtr = getPropertyNode(cur->properties, (xmlChar *)"bytes");
                if (chrPtr) {
                    logIT(LOG_INFO, "          (%d) Node::Name=%s Type:%d Content=%s (bytes)",
                          cur->line, cur->name, cur->type, chrPtr);
                    memset(string, 0, sizeof(string));
                    ePtr->len = string2chr(chrPtr, string, sizeof(string));
                    if (ePtr->len < 0) {
                        logIT(LOG_ERR, "Invalid enum bytes: %s", chrPtr);
                        goto fail;
                    }
                    ePtr->bytes = calloc(ePtr->len, sizeof(char));
                    if (ePtr->len > 0 && !ePtr->bytes) goto fail;
                    memcpy(ePtr->bytes, string, (size_t)ePtr->len);
                }
            } else {
                logIT(LOG_ERR, "Property node without text=");
                goto fail;
            }

            (cur->next && (! (cur->next->type == XML_TEXT_NODE) || cur->next->next))
                ? (cur = cur->next) : (cur = prevPtr->next);

        } else if (unitFound && xmlStrEqual(cur->name, (const xmlChar *)"abbrev")) {
            chrPtr = getTextNode(cur);
            logIT(LOG_INFO, "   (%d) Node::Name=%s Type:%d Content=%s",
                  cur->line, cur->name, cur->type, chrPtr);
            if (!duplicateString(&uPtr->abbrev, chrPtr ? chrPtr : ""))
                goto fail;
            (cur->next && (! (cur->next->type == XML_TEXT_NODE) || cur->next->next))
                ? (cur = cur->next) : (cur = prevPtr->next);
        } else if (unitFound && (strcmp((char *)cur->name, "calc") == 0)) {
            chrPtr = getPropertyNode(cur->properties, (xmlChar *)"get");
            logIT(LOG_INFO, "   (%d) Node::Name=%s Type:%d Content=%s (get)",
                  cur->line, cur->name, cur->type, chrPtr);
            if (!duplicateString(&uPtr->gCalc, chrPtr ? chrPtr : ""))
                goto fail;
            chrPtr = getPropertyNode(cur->properties, (xmlChar *)"set");
            logIT(LOG_INFO, "   (%d) Node::Name=%s Type:%d Content=%s (set)",
                  cur->line, cur->name, cur->type, chrPtr);
            if (!duplicateString(&uPtr->sCalc, chrPtr ? chrPtr : ""))
                goto fail;
            (cur->next && (! (cur->next->type == XML_TEXT_NODE) || cur->next->next))
                ? (cur = cur->next) : (cur = prevPtr->next);
        } else if (unitFound && (strcmp((char *)cur->name, "icalc") == 0)) {
            chrPtr = getPropertyNode(cur->properties, (xmlChar *)"get");
            logIT(LOG_INFO, "   (%d) Node::Name=%s Type:%d Content=%s (get)",
                  cur->line, cur->name, cur->type, chrPtr);
            if (!duplicateString(&uPtr->gICalc, chrPtr ? chrPtr : ""))
                goto fail;
            chrPtr = getPropertyNode(cur->properties, (xmlChar *)"set");
            logIT(LOG_INFO, "   (%d) Node::Name=%s Type:%d Content=%s (set)",
                  cur->line, cur->name, cur->type, chrPtr);
            if (!duplicateString(&uPtr->sICalc, chrPtr ? chrPtr : ""))
                goto fail;
            (cur->next && (! (cur->next->type == XML_TEXT_NODE) || cur->next->next))
                ? (cur = cur->next) : (cur = prevPtr->next);
        } else if (unitFound && xmlStrEqual(cur->name, (const xmlChar *)"type")) {
            chrPtr = getTextNode(cur);
            logIT(LOG_INFO, "   (%d) Node::Name=%s Type:%d Content=%s",
                  cur->line, cur->name, cur->type, chrPtr);
            if (!duplicateString(&uPtr->type, chrPtr ? chrPtr : ""))
                goto fail;
            (cur->next && (! (cur->next->type == XML_TEXT_NODE) || cur->next->next))
                ? (cur = cur->next) : (cur = prevPtr->next);
        } else if (unitFound && xmlStrEqual(cur->name, (const xmlChar *)"entity")) {
            chrPtr = getTextNode(cur);
            logIT(LOG_INFO, "   (%d) Node::Name=%s Type:%d Content=%s",
                  cur->line, cur->name, cur->type, chrPtr);
            if (!duplicateString(&uPtr->entity, chrPtr ? chrPtr : ""))
                goto fail;
            //(cur->next && cur->next->next) ? (cur=cur->next) : (cur=prevPtr->next);
            (cur->next && (! (cur->next->type == XML_TEXT_NODE) || cur->next->next))
                ? (cur = cur->next) : (cur = prevPtr->next);
        } else {
            logIT(LOG_ERR, "Error parsing unit");
            goto fail;
        }
    }

    return uStartPtr;

fail:
    removeUnitList(uStartPtr);
    return NULL;
}

macroPtr parseMacro(xmlNodePtr cur)
{
    macroPtr mPtr;
    macroPtr mStartPtr = NULL;
    char *macro;
    char *chrPtr;
    int macroFound = 0;
    xmlNodePtr prevPtr = cur;

    while (cur) {
        logIT(LOG_INFO, "MACRO: (%d) Node::Name=%s Type:%d Content=%s",
              cur->line, cur->name, cur->type, cur->content);

        if (cur->type == XML_TEXT_NODE) {
            cur = cur->next;
            continue;
        }

        if (xmlStrEqual(cur->name, (const xmlChar *)"macro")) {
            macro = getPropertyNode(cur->properties, (xmlChar *)"name");
            if (macro) {
                // Read new macro
                logIT(LOG_INFO, "New macro: %s", macro);
                mPtr = newMacroNode(mStartPtr);
                if (!mPtr) {
                    removeMacroList(mStartPtr);
                    return NULL;
                }
                if (! mStartPtr) {
                    mStartPtr = mPtr;
                }
                if (!duplicateString(&mPtr->name, macro)) goto fail;
                macroFound = 1;
                prevPtr = cur;
                cur = cur->children;
                continue;
            }
        } else if (macroFound && xmlStrEqual(cur->name, (const xmlChar *)"command")) {
            chrPtr = getTextNode(cur);
            logIT(LOG_INFO, "   (%d) Node::Name=%s Type:%d Content=%s",
                  cur->line, cur->name, cur->type, chrPtr);
            if (!duplicateString(&mPtr->command, chrPtr ? chrPtr : ""))
                goto fail;
            (cur->next && (! (cur->next->type == XML_TEXT_NODE) || cur->next->next))
                ? (cur = cur->next) : (cur = prevPtr->next);
        } else {
            logIT(LOG_INFO, "Error parsing macro");
            goto fail;
        }
    }

    return mStartPtr;

fail:
    removeMacroList(mStartPtr);
    return NULL;
}

commandPtr parseCommand(xmlNodePtr cur, commandPtr cPtr, devicePtr dePtr)
{
    commandPtr cStartPtr = NULL;
    devicePtr dPtr;
    char *command;
    char *protocmd;
    char *chrPtr;
    xmlNodePtr prevPtr = cur;
    char string[256];    // TODO: get rid of that one
    char *id;
    int commandFound;
    commandPtr ncPtr;
    short count;
    int deviceOverrideHasContent;

    // We look for a recursive call (then, cPtr is set)
    if (! cPtr) {
        commandFound = 0;
    } else {
        cStartPtr = cPtr;
        commandFound = 1;
        prevPtr = NULL;
    }

    while (cur) {
        logIT(LOG_INFO, "COMMAND: (%d) Node::Name=%s Type:%d Content=%s",
              cur->line, cur->name, cur->type, cur->content);

        if (xmlIsBlankNode(cur)) {
            cur = cur->next;
            continue;
        }

        if (strcmp((char *)cur->name, "command") == 0) {
            command = getPropertyNode(cur->properties, (xmlChar *)"name");
            protocmd = getPropertyNode(cur->properties, (xmlChar *)"protocmd");
            if (command) {
                // Read new command
                logIT(LOG_INFO, "New command: %s", command);
                cPtr = newCommandNode(cStartPtr);
                if (!cPtr) {
                    goto fail;
                }
                if (! cStartPtr) {
                    cStartPtr = cPtr;
                }
                cPtr->nodeType = 1; // No copy, we need this for deleting
                if (!duplicateString(&cPtr->name, command) ||
                        !duplicateString(&cPtr->pcmd,
                                         protocmd ? protocmd : ""))
                    goto fail;
                commandFound = 1;
                prevPtr = cur;
                cur = cur->children;
                continue;
            }
        } else if (commandFound && xmlStrEqual(cur->name, (const xmlChar *)"device")) {
            id = getPropertyNode(cur->properties, (xmlChar *)"ID");
            protocmd = getPropertyNode(cur->properties, (xmlChar *)"protocmd");
            if (id) {
                // Read new device below command
                logIT(LOG_INFO, "    New device command: %s", id);
                // Search device from the list
                if (! (dPtr = getDeviceNode(dePtr, id))) {
                    logIT(LOG_ERR, "Device %s is not defined (%d)", id, cur->line);
                    goto fail;
                }
                // We take the description from the command entry
                snprintf(string, sizeof(string), "%s",
                         cPtr->description ? cPtr->description : "");
                deviceOverrideHasContent = protocmd != NULL;
                for (xmlNodePtr child = cur->children; child; child = child->next) {
                    if (child->type == XML_ELEMENT_NODE) {
                        deviceOverrideHasContent = 1;
                        break;
                    }
                }
                // Now again recursively parseCommand for all children
                ncPtr = newCommandNode(NULL);
                if (!ncPtr) {
                    goto fail;
                }
                /*
                 * The recursive parser already owns strings it adds to this
                 * override. Mark that ownership before parsing so its failure
                 * cleanup releases partially populated nodes as well.
                 */
                ncPtr->nodeType = 2;
                if (!parseCommand(cur->children, ncPtr, dePtr)) {
                    goto fail;
                }
                if (! dPtr->cmdPtr) {
                    dPtr->cmdPtr = ncPtr;
                } else {
                    addCommandNode(dPtr->cmdPtr, ncPtr);
                }
                // Refer to description in new accounts
                ncPtr->description = cPtr->description;
                ncPtr->name = cPtr->name;
                /*
                 * An empty device element is an explicit exclusion marker.
                 * Keep its address empty so the default command is not copied
                 * to this device later.  Partial overrides still inherit any
                 * omitted values from the default command.
                 */
                if (deviceOverrideHasContent && !ncPtr->addr && cPtr->addr) {
                    if (!duplicateString(&ncPtr->addr, cPtr->addr))
                        goto fail;
                }
                if (deviceOverrideHasContent && !ncPtr->precmd && cPtr->precmd) {
                    if (!duplicateString(&ncPtr->precmd, cPtr->precmd))
                        goto fail;
                }
                if (deviceOverrideHasContent && !ncPtr->errStr &&
                        cPtr->errStr && cPtr->len > 0) {
                    ncPtr->errStr = calloc((size_t)cPtr->len, sizeof(char));
                    if (!ncPtr->errStr) goto fail;
                    memcpy(ncPtr->errStr, cPtr->errStr, (size_t)cPtr->len);
                }
                if (deviceOverrideHasContent && !ncPtr->len) ncPtr->len = cPtr->len;
                if (deviceOverrideHasContent && !ncPtr->bit) ncPtr->bit = cPtr->bit;
                // If no unit has been given, we copy it
                if (deviceOverrideHasContent && !ncPtr->unit && cPtr->unit) {
                    if (!duplicateString(&ncPtr->unit, cPtr->unit))
                        goto fail;
                }
                // Same for the protocol command
                if (protocmd) {
                    if (!duplicateString(&ncPtr->pcmd, protocmd))
                        goto fail;
                } else if (deviceOverrideHasContent) {
                    if (!cPtr->pcmd ||
                            !duplicateString(&ncPtr->pcmd, cPtr->pcmd))
                        goto fail;
                }
                ncPtr->nodeType = 2; // 2 == decription, name has been copied
                if (cur->next && (! (cur->next->type == XML_TEXT_NODE) || cur->next->next)) {
                    cur = cur->next;
                } else if (prevPtr) {
                    cur = prevPtr->next;
                } else {
                    cur = NULL;
                }
            }
        } else if (commandFound && xmlStrEqual(cur->name, (const xmlChar *)"addr")) {
            chrPtr = getTextNode(cur);
            logIT(LOG_INFO, "   (%d) Node::Name=%s Type:%d Content=%s",
                  cur->line, cur->name, cur->type, chrPtr);
            if (!duplicateString(&cPtr->addr, chrPtr ? chrPtr : ""))
                goto fail;
            if (cur->next && (! (cur->next->type == XML_TEXT_NODE) || cur->next->next)) {
                cur = cur->next;
            } else if (prevPtr) {
                cur = prevPtr->next;
            } else {
                cur = NULL;
            }
        } else if (commandFound && xmlStrEqual(cur->name, (const xmlChar *)"error")) {
            chrPtr = getTextNode(cur);
            logIT(LOG_INFO, "   (%d) Node::Name=%s Type:%d Content=%s",
                  cur->line, cur->name, cur->type, chrPtr);
            if (chrPtr) {
                memset(string, 0, sizeof(string));
                if ((count = string2chr(chrPtr, string, sizeof(string)))) {
                    cPtr->errStr = calloc(count, sizeof(char));
                    if (!cPtr->errStr) goto fail;
                    memcpy(cPtr->errStr, string, count);
                }
            } else {
                free(cPtr->errStr);
                cPtr->errStr = NULL;
            }
            if (cur->next && (! (cur->next->type == XML_TEXT_NODE) || cur->next->next)) {
                cur = cur->next;
            } else if (prevPtr) {
                cur = prevPtr->next;
            } else {
                cur = NULL;
            }
        } else if (commandFound && xmlStrEqual(cur->name, (const xmlChar *)"unit")) {
            chrPtr = getTextNode(cur);
            logIT(LOG_INFO, "   (%d) Node::Name=%s Type:%d Content=%s",
                  cur->line, cur->name, cur->type, chrPtr);
            if (!duplicateString(&cPtr->unit, chrPtr ? chrPtr : ""))
                goto fail;
            if (cur->next && (! (cur->next->type == XML_TEXT_NODE) || cur->next->next)) {
                cur = cur->next;
            } else if (prevPtr) {
                cur = prevPtr->next;
            } else {
                cur = NULL;
            }
        } else if (commandFound && (strcmp((char *)cur->name, "precommand") == 0)) {
            chrPtr = getTextNode(cur);
            logIT(LOG_INFO, "   (%d) Node::Name=%s Type:%d Content=%s",
                  cur->line, cur->name, cur->type, chrPtr);
            if (!duplicateString(&cPtr->precmd, chrPtr ? chrPtr : ""))
                goto fail;
            if (cur->next && (! (cur->next->type == XML_TEXT_NODE) || cur->next->next)) {
                cur = cur->next;
            } else if (prevPtr) {
                cur = prevPtr->next;
            } else {
                cur = NULL;
            }
        } else if (commandFound && xmlStrEqual(cur->name, (const xmlChar *)"description")) {
            chrPtr = getTextNode(cur);
            logIT(LOG_INFO, "   (%d) Node::Name=%s Type:%d Content=%s",
                  cur->line, cur->name, cur->type, chrPtr);
            if (!duplicateString(&cPtr->description, chrPtr ? chrPtr : ""))
                goto fail;
            if (cur->next && (! (cur->next->type == XML_TEXT_NODE) || cur->next->next)) {
                cur = cur->next;
            } else if (prevPtr) {
                cur = prevPtr->next;
            } else {
                cur = NULL;
            }
        } else if (commandFound && xmlStrEqual(cur->name, (const xmlChar *)"len")) {
            chrPtr = getTextNode(cur);
            logIT(LOG_INFO, "   (%d) Node::Name=%s Type:%d Content=%s",
                  cur->line, cur->name, cur->type, chrPtr);
            if (chrPtr) {
                long value;
                if (!parse_long_range(chrPtr, 1, 253, &value, "command len")) {
                    goto fail;
                }
                cPtr->len = (unsigned char)value;
            }
            if (cur->next && (! (cur->next->type == XML_TEXT_NODE) || cur->next->next)) {
                cur = cur->next;
            } else if (prevPtr) {
                cur = prevPtr->next;
            } else {
                cur = NULL;
            }
        } else if (commandFound && xmlStrEqual(cur->name, (const xmlChar *)"bit")) {
            chrPtr = getTextNode(cur);
            logIT(LOG_INFO, "   (%d) Node::Name=%s Type:%d Content=%s",
                  cur->line, cur->name, cur->type, chrPtr);
            if (chrPtr) {
                long value;
                if (!parse_long_range(chrPtr, 0, 7, &value, "command bit")) {
                    goto fail;
                }
                cPtr->bit = (char)value;
            }
            if (cur->next && (! (cur->next->type == XML_TEXT_NODE) || cur->next->next)) {
                cur = cur->next;
            } else if (prevPtr) {
                cur = prevPtr->next;
            } else {
                cur = NULL;
            }
        } else {
            logIT(LOG_ERR, "Error parsing command");
            goto fail;
        }
    }

    return cStartPtr;

fail:
    removeCommandList(cStartPtr);
    return NULL;
}

icmdPtr parseICmd(xmlNodePtr cur)
{
    icmdPtr icPtr;
    icmdPtr icStartPtr = NULL;
    char *command;
    char *chrPtr;
    int commandFound = 0;
    xmlNodePtr prevPtr = cur;

    while (cur) {
        logIT(LOG_INFO, "ICMD: (%d) Node::Name=%s Type:%d Content=%s",
              cur->line, cur->name, cur->type, cur->content);
        if (xmlIsBlankNode(cur))  {
            //if (cur->type == XML_TEXT_NODE) {
            cur = cur->next;
            continue;
        }
        if (xmlStrEqual(cur->name, (const xmlChar *)"command")) {
            command = getPropertyNode(cur->properties, (xmlChar *)"name");
            if (command) {
                // Read new command
                logIT(LOG_INFO, "New iCommand: %s", command);
                icPtr = newIcmdNode(icStartPtr);
                if (!icPtr) {
                    removeIcmdList(icStartPtr);
                    return NULL;
                }
                if (! icStartPtr) {
                    icStartPtr = icPtr;
                }
                if (!duplicateString(&icPtr->name, command)) goto fail;
                commandFound = 1;
                prevPtr = cur;
                cur = cur->children;
                continue;
            }
        } else if (commandFound && xmlStrEqual(cur->name, (const xmlChar *)"send")) {
            chrPtr = getTextNode(cur);
            logIT(LOG_INFO, "   (%d) Node::Name=%s Type:%d Content=%s",
                  cur->line, cur->name, cur->type, chrPtr);
            if (!duplicateString(&icPtr->send, chrPtr ? chrPtr : ""))
                goto fail;
            (cur->next && (! (cur->next->type == XML_TEXT_NODE) || cur->next->next))
                ? (cur = cur->next) : (cur = prevPtr->next);
        } else if (commandFound && xmlStrEqual(cur->name, (const xmlChar *)"retry")) {
            chrPtr = getTextNode(cur);
            logIT(LOG_INFO, "   (%d) Node::Name=%s Type:%d Content=%s",
                  cur->line, cur->name, cur->type, chrPtr);
            if (chrPtr) {
                long value;
                if (!parse_long_range(chrPtr, 0, 20, &value, "retry")) {
                    return NULL;
                }
                icPtr->retry = (unsigned char)value;
            }
            (cur->next && (! (cur->next->type == XML_TEXT_NODE) || cur->next->next))
                ? (cur = cur->next) : (cur = prevPtr->next);
        } else if (commandFound && xmlStrEqual(cur->name, (const xmlChar *)"recvTimeout")) {
            chrPtr = getTextNode(cur);
            logIT(LOG_INFO, "   (%d) Node::Name=%s Type:%d Content=%s",
                  cur->line, cur->name, cur->type, chrPtr);
            if (chrPtr) {
                long value;
                if (!parse_long_range(chrPtr, 0, 60000, &value,
                                      "recvTimeout")) {
                    return NULL;
                }
                icPtr->recvTimeout = (unsigned short)value;
            }
            (cur->next && (! (cur->next->type == XML_TEXT_NODE) || cur->next->next))
                ? (cur = cur->next) : (cur = prevPtr->next);
        } else {
            logIT(LOG_ERR, "Error parsing command");
            goto fail;
        }
    }

    return icStartPtr;

fail:
    removeIcmdList(icStartPtr);
    return NULL;
}

devicePtr parseDevice(xmlNodePtr cur, protocolPtr pPtr)
{
    devicePtr dPtr;
    devicePtr dStartPtr = NULL;
    char *proto;
    char *name;
    char *id;
    xmlNodePtr prevPtr = cur;

    while (cur) {
        logIT(LOG_INFO, "DEVICE: (%d) Node::Name=%s Type:%d Content=%s",
              cur->line, cur->name, cur->type, cur->content);

        if (cur->type == XML_TEXT_NODE) {
            cur = cur->next;
            continue;
        }

        if (xmlStrEqual(cur->name, (const xmlChar *)"device")) {
            name = getPropertyNode(cur->properties, (xmlChar *)"name");
            id = getPropertyNode(cur->properties, (xmlChar *)"ID");
            proto = getPropertyNode(cur->properties, (xmlChar *)"protocol");
            if (proto) {
                // Read new protocol
                logIT(LOG_INFO, "    Neues Device: name=%s ID=%s proto=%s", name, id, proto);
                dPtr = newDeviceNode(dStartPtr);
                if (!dPtr) {
                    removeDeviceList(dStartPtr);
                    return NULL;
                }
                if (! dStartPtr) {
                    dStartPtr = dPtr;
                } // Remember anchor
                if (!duplicateString(&dPtr->name, name ? name : "") ||
                        !duplicateString(&dPtr->id, id ? id : ""))
                    goto fail;

                if (! (dPtr->protoPtr = getProtocolNode(pPtr, proto))) {
                    logIT(LOG_ERR, "Protocol %s not defined", proto);
                    goto fail;
                }
            } else {
                logIT(LOG_ERR, "Error parsing device");
                goto fail;
            }

            prevPtr = cur;
            cur = cur->next;

        } else {
            (cur->next && (! (cur->next->type == XML_TEXT_NODE) || cur->next->next))
                ? (cur = cur->next) : (cur = prevPtr->next);
        }
    }

    return dStartPtr;

fail:
    removeDeviceList(dStartPtr);
    return NULL;
}

protocolPtr parseProtocol(xmlNodePtr cur)
{
    int protoFound = 0;
    protocolPtr protoPtr;
    protocolPtr protoStartPtr = NULL;
    macroPtr mPtr;
    icmdPtr icPtr;
    char *proto;
    char *chrPtr;
    xmlNodePtr prevPtr = cur;

    while (cur) {
        logIT(LOG_INFO, "PROT: (%d) Node::Name=%s Type:%d Content=%s",
              cur->line, cur->name, cur->type, cur->content);

        if (cur->type == XML_TEXT_NODE) {
            cur = cur->next;
            continue;
        }

        if (xmlStrEqual(cur->name, (const xmlChar *)"protocol")) {
            proto = getPropertyNode(cur->properties, (xmlChar *)"name");
            if (proto) {
                // Read new protocol
                logIT(LOG_INFO, "New protocol %s", proto);
                protoPtr = newProtocolNode(protoStartPtr);
                if (!protoPtr) {
                    removeProtocolList(protoStartPtr);
                    return NULL;
                }
                if (! protoStartPtr) {
                    protoStartPtr = protoPtr;
                } // Remember anchor
                if (!duplicateString(&protoPtr->name, proto)) goto fail;
            } else {
                logIT(LOG_ERR, "Error parsing protocol");
                goto fail;
            }
            protoFound = 1;
            prevPtr = cur;
            cur = cur->children;
        } else if (protoFound && xmlStrEqual(cur->name, (const xmlChar *)"pid")) {
            chrPtr = getTextNode(cur);
            protoPtr->id = hex2chr(chrPtr);
            (cur->next && (! (cur->next->type == XML_TEXT_NODE) || cur->next->next))
                ? (cur = cur->next) : (cur = prevPtr->next);
        } else if (protoFound && xmlStrEqual(cur->name, (const xmlChar *)"macros")) {
            mPtr = parseMacro(cur->children);
            if (!mPtr) goto fail;
            protoPtr->mPtr = mPtr;
            (cur->next && (! (cur->next->type == XML_TEXT_NODE) || cur->next->next))
                ? (cur = cur->next) : (cur = prevPtr->next);
        } else if (protoFound && xmlStrEqual(cur->name, (const xmlChar *)"commands")) {
            icPtr = parseICmd(cur->children);
            if (!icPtr) goto fail;
            protoPtr->icPtr = icPtr;
            (cur->next && (! (cur->next->type == XML_TEXT_NODE) || cur->next->next))
                ? (cur = cur->next) : (cur = prevPtr->next);
        } else {
            (cur->next && (! (cur->next->type == XML_TEXT_NODE) || cur->next->next))
                ? (cur = cur->next) : (cur = prevPtr->next);
        }
    }

    return protoStartPtr;

fail:
    removeProtocolList(protoStartPtr);
    return NULL;
}

void removeComments(xmlNodePtr node)
{
    while (node) {
        //printf("type:%d name=%s\n",node->type, node->name);
        if (node->children) {
            // if the node has children, process the children
            removeComments(node->children);
        }
        // We save the pointer to the next sibling, as the node might be freed below
        xmlNodePtr nextNode = node->next;
        if (node->type == XML_COMMENT_NODE) {
            // if the node is a comment, we unlink ...
            //printf("found comment\n");
            xmlUnlinkNode(node); // unlink
            xmlFreeNode(node);   // and free the node
        }
        node = nextNode;
    }
}

int parseXMLFile(char *filename)
{
    xmlDocPtr doc = NULL;
    xmlNodePtr cur, curStart;
    xmlNodePtr prevPtr = NULL;
    xmlNsPtr ns;
    //char string[256];
    devicePtr dPtr;
    commandPtr cPtr, ncPtr;
    protocolPtr TprotoPtr = NULL;
    unitPtr TuPtr = NULL;
    devicePtr TdevPtr = NULL;
    commandPtr TcmdPtr = NULL;
    configPtr TcfgPtr = NULL;

    xmlKeepBlanksDefault(0);
    doc = xmlReadFile(filename, NULL, XML_PARSE_NONET);
    if (doc == NULL) {
        goto fail;
    }
    curStart = xmlDocGetRootElement(doc);
    cur = curStart;
    prevPtr = curStart;
    if (cur == NULL) {
        logIT(LOG_ERR, "empty document\n");
        goto fail;
    }
    ns = xmlSearchNsByHref(doc, cur, (const xmlChar *) "http://www.openv.de/vcontrol");
    if (ns == NULL) {
        logIT(LOG_ERR, "document of the wrong type, vcontrol Namespace not found");
        goto fail;
    }
    if (xmlStrcmp(cur->name, (const xmlChar *) "V-Control")) {
        logIT(LOG_ERR, "document of the wrong type, root node != V-Control");
        goto fail;
    }
    // Run XInclude
    short xc = 0;
    if ((xc = xmlXIncludeProcessFlags(doc, XML_PARSE_XINCLUDE | XML_PARSE_NOXINCNODE)) == 0) {
        logIT(LOG_WARNING, "Didn't perform XInclude");
    } else if (xc < 0) {
        logIT(LOG_ERR, "Error during XInclude");
        goto fail;
    } else {
        logIT(LOG_INFO, "%d XInclude performed", xc);
    }

    removeComments(cur); // now the xml tree is complete --> remove all comments

    int unixFound = 0;
    int protocolsFound = 0;
    cur = cur->children;

    while (cur) {
        logIT(LOG_INFO, "XML: (%d) Node::Name=%s Type:%d Content=%s",
              cur->line, cur->name, cur->type, cur->content);

        if (xmlIsBlankNode(cur)) {
            cur = cur->next;
            continue;
        }

        if (xmlStrEqual(cur->name, (const xmlChar *)"unix"))  {
            if (unixFound) {
                // We must not reach here, second pass
                logIT(LOG_ERR, "Error in XML config");
                goto fail;
            }
            prevPtr = cur;
            unixFound = 1;
            cur = cur->children; // On Unix, config follows. Thus children.
            continue;
        }

        if (xmlStrEqual(cur->name, (const xmlChar *)"extern")) {
            cur = cur->children;
            if (cur && xmlStrEqual(cur->name, (const xmlChar *)"vito"))  {
                prevPtr = cur;
                cur = cur->children; // The xinclude stuff can be found at <extern><vito>
                continue;
            } else {
                cur = prevPtr->next;
            }
        } else if (xmlStrEqual(cur->name, (const xmlChar *)"protocols")) {
            if (protocolsFound) {
                // We must not reach here, second pass
                logIT(LOG_ERR, "Error in XML config");
                goto fail;
            }
            protocolsFound = 1;
            if (! (TprotoPtr = parseProtocol(cur->children))) {
                goto fail;
            }
            cur = cur->next; // Same level as Unix, proceed
            continue;
        } else if (xmlStrEqual(cur->name, (const xmlChar *)"units")) {
            if (! (TuPtr = parseUnit(cur->children))) {
                goto fail;
            }
            cur = cur->next; // Same level as Unix, proceed
            continue;
        } else if (xmlStrEqual(cur->name, (const xmlChar *)"commands")) {
            if (! (TcmdPtr = parseCommand(cur->children, NULL, TdevPtr))) {
                goto fail;
            }
            //(cur->next && cur->next->next) ? (cur=cur->next) : (cur=prevPtr->next);
            (cur->next) ? (cur = cur->next) : (cur = prevPtr->next);
            continue;
        } else if (xmlStrEqual(cur->name, (const xmlChar *)"devices")) {
            if (! (TdevPtr = parseDevice(cur->children, TprotoPtr))) {
                goto fail;
            }
            (cur->next) ? (cur = cur->next) : (cur = prevPtr->next);
            continue;
        } else if (xmlStrEqual(cur->name, (const xmlChar *)"config")) {
            if (! (TcfgPtr = parseConfig(cur->children))) {
                goto fail;
            }
            (cur->next && cur->next->next) ? (cur = cur->next) : (cur = prevPtr->next);
        } else {
            cur = cur->next;
        }
    }

    // For all commands that have default definitions,
    // we roam all devices and add the particular commands.
    cPtr = TcmdPtr;
    while (cPtr) {
        dPtr = TdevPtr;
        while (dPtr) {
            if (cPtr->addr && !getCommandNode(dPtr->cmdPtr, cPtr->name)) {
                // We don't know this one and copy the commands
                logIT(LOG_INFO, "Copying command %s to device %s", cPtr->name, dPtr->id);
                ncPtr = newCommandNode(dPtr->cmdPtr);
                if (!ncPtr) {
                    goto fail;
                }
                if (! dPtr->cmdPtr) {
                    dPtr->cmdPtr = ncPtr;
                }
                ncPtr->name = cPtr->name;
                ncPtr->pcmd = cPtr->pcmd;
                ncPtr->addr = cPtr->addr;
                ncPtr->unit = cPtr->unit;
                ncPtr->bit = cPtr->bit;
                ncPtr->errStr = cPtr->errStr;
                ncPtr->precmd = cPtr->precmd;
                ncPtr->description = cPtr->description;
                ncPtr->len = cPtr->len;
            }
            dPtr = dPtr->next;
        }
        cPtr = cPtr->next;
    }

    // We search the default device
    if (!TcfgPtr || !TcfgPtr->devID || !TdevPtr) {
        logIT1(LOG_ERR, "XML configuration lacks config/device definitions");
        goto fail;
    }
    if (! (TcfgPtr->devPtr = getDeviceNode(TdevPtr, TcfgPtr->devID))) {
        logIT(LOG_ERR, "Device %s is not defined\n", TcfgPtr->devID);
        goto fail;
    }

    /*
     * Compile the complete candidate configuration before replacing any
     * active global state. A semantic error therefore leaves the old
     * configuration usable.
     */
    if (!compileCommand(TdevPtr, TuPtr)) {
        logIT1(LOG_ERR, "Candidate XML configuration did not compile");
        goto fail;
    }

    // If we reach here, the loading has been successful.
    // Now we free the old lists and allocate the new ones.

    // If we're called repetitive (SIGHUP), everything should be freed.
    freeAllLists();

    protoPtr = TprotoPtr;
    uPtr = TuPtr;
    devPtr = TdevPtr;
    cmdPtr = TcmdPtr;
    cfgPtr = TcfgPtr;

    xmlFreeDoc(doc);
    return 1;

fail:
    removeDeviceList(TdevPtr);
    removeCommandList(TcmdPtr);
    removeUnitList(TuPtr);
    removeProtocolList(TprotoPtr);
    removeConfigNode(TcfgPtr);
    if (doc) {
        xmlFreeDoc(doc);
    }
    return 0;
}

void freeAllLists(void)
{
    removeProtocolList(protoPtr);
    removeUnitList(uPtr);
    removeDeviceList(devPtr);
    removeCommandList(cmdPtr);
    protoPtr = NULL;
    uPtr = NULL;
    devPtr = NULL;
    cmdPtr = NULL;
    if (cfgPtr) {
        removeConfigNode(cfgPtr);
        cfgPtr = NULL;
    }
}
