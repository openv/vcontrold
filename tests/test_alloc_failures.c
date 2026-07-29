#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "client.h"
#include "parser.h"
#include "xmlconfig.h"

void *__real_calloc(size_t count, size_t size);
void *__real_realloc(void *pointer, size_t size);

static int fail_calloc;
static int fail_realloc;

void *__wrap_calloc(size_t count, size_t size)
{
    if (fail_calloc) {
        fail_calloc = 0;
        return NULL;
    }
    return __real_calloc(count, size);
}

void *__wrap_realloc(void *pointer, size_t size)
{
    if (fail_realloc) {
        fail_realloc = 0;
        return NULL;
    }
    return __real_realloc(pointer, size);
}

extern protocolPtr newProtocolNode(protocolPtr);
extern trPtr newTrNode(trPtr);
extern compilePtr newCompileNode(compilePtr);
extern unitPtr newUnitNode(unitPtr);
extern macroPtr newMacroNode(macroPtr);
extern commandPtr newCommandNode(commandPtr);
extern devicePtr newDeviceNode(devicePtr);
extern icmdPtr newIcmdNode(icmdPtr);
extern enumPtr newEnumNode(enumPtr);

int inetversion = 0;
FILE *iniFD = NULL;

int main(void)
{
    char *received = NULL;

    fail_calloc = 1;
    if (newTrNode(NULL) != NULL) return 1;
    fail_calloc = 1;
    if (newCompileNode(NULL) != NULL) return 1;
    fail_calloc = 1;
    if (newProtocolNode(NULL) != NULL) return 1;
    fail_calloc = 1;
    if (newUnitNode(NULL) != NULL) return 1;
    fail_calloc = 1;
    if (newMacroNode(NULL) != NULL) return 1;
    fail_calloc = 1;
    if (newCommandNode(NULL) != NULL) return 1;
    fail_calloc = 1;
    if (newDeviceNode(NULL) != NULL) return 1;
    fail_calloc = 1;
    if (newIcmdNode(NULL) != NULL) return 1;
    fail_calloc = 1;
    if (newEnumNode(NULL) != NULL) return 1;
    fail_calloc = 1;
    if (recvSync(-1, "prompt", &received) != -1 || received != NULL) return 1;

    /*
     * A failed shrink is non-fatal: recvSync keeps the original allocation.
     * EOF supplies the path without waiting for a timeout.
     */
    int descriptors[2];
    if (pipe(descriptors) != 0) return 1;
    close(descriptors[1]);
    fail_realloc = 1;
    int result = recvSync(descriptors[0], "prompt", &received);
    close(descriptors[0]);
    free(received);
    return result == 0 ? 0 : 1;
}
