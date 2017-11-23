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

// Calculation of arithmetic expressions

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "arithmetic.h"

// Definition of parsed tokens
#define HEX 8           // '0x'
#define HEXDIGIT 10     // 'a'..'f'
#define DIGIT 11        // '0'..'9'
#define PUNKT 12        // '.'
#define PLUS 100        // '+'
#define MINUS 101       // '-'
#define MAL 102         // '*'
#define GETEILT 103     // '/'
#define MODULO 104      // '%'
#define KAUF 110        // '('
#define KZU 111         // ')'
#define NICHT 112       // '~'
#define UND 113         // '&'
#define ODER 114        // '|'
#define XOR 115         // '^'
#define SHL 116         // '<<'
#define SHR 117         // '>>'
// BYTE0 bis BYTE99 (BYTE00..BYTE09 ist auch gueltig)
#define BYTE_FIRST 200
#define BYTE_LAST 299
// PBYTE0 bis PBYTE99 (PBYTE00..PBYTE09 ist auch gueltig)
#define PBYTE_FIRST 300
#define PBYTE_LAST 399
#define BITPOS 400      // 'P'
#define VALUE 410       // 'V'
#define END 0
#define ERROR -100


// Declaration of internal functions
static int nextToken(char **str, char **c, int *count);
static void pushBack(char **str, int n);
static float execTerm(char **str, unsigned char *bufferPtr, int bufferLen, float floatV,char *err);
static float execFactor(char **str, unsigned char *bufferPtr, int bufferLen, float floatV, char *err);
static int execITerm(char **str, unsigned char *bufferPtr, int bufferLen, char bitpos, char *pPtr, char *err);
static int execIFactor(char **str, unsigned char *bufferPtr, int bufferLen, char bitpos,char *pPtr, char *err);

// -----------------------------------------------------------------------------
// Evaluate floatingpoint expressions
// -----------------------------------------------------------------------------
float execExpression(char **str, unsigned char *bufferPtr, int bufferLen, float floatV, char *err)
{
    int f = 1;
    float term1;
    float term2;
    char *item;
    int n;

    //printf("execExpression: %s\n", *str);

    switch (nextToken(str, &item, &n)) {
    case PLUS:
        f = 1;
        break;
    case MINUS:
        f = -1;
        break;
    default:
        pushBack(str, n);
        break;
    }

    term1 = execTerm(str, bufferPtr, bufferLen, floatV, err) * f;
    if (*err) {
        return 0;
    }
    //printf(" T1=%f\n", term1);

    int t;
    f = 1;
    while ((t = nextToken(str, &item, &n)) != END) {
        switch (t) {
        case PLUS:
            f = 1;
            break;
        case MINUS:
            f = -1;
            break;
        default:
            //printf(" Exp=%f\n", term1);
            pushBack(str, n);
            return term1;
        }

        term2 = execTerm(str, bufferPtr, bufferLen, floatV, err);
        if (*err) {
            return 0;
        }
        //printf(" T2=%f\n", term2);
        term1 += term2 * f;
    }

    //printf(" Exp=%f\n", term1);
    return term1;
}

// -----------------------------------------------------------------------------
// Evaluate integer expressions
// -----------------------------------------------------------------------------
int execIExpression(char **str, unsigned char *bufferPtr, int bufferLen, char bitpos, char *pPtr, char *err)
{
    int term1;
    int term2;
    int op;
    char *item;
    int n;

    //printf("execExpression: %s\n", *str);

    op = ERROR;
    switch (nextToken(str, &item, &n)) {
    case PLUS:
        op = PLUS;
        break;
    case MINUS:
        op = MINUS;
        break;
    case NICHT:
        op = NICHT;
        break;
    default:
        pushBack(str, n);
        break;
    }

    if (op == MINUS) {
        term1 = -execITerm(str, bufferPtr, bufferLen, bitpos, pPtr, err);
    } else if (op == NICHT) {
        term1 = ~execITerm(str, bufferPtr, bufferLen, bitpos, pPtr, err);
    } else {
        term1 = execITerm(str, bufferPtr, bufferLen, bitpos, pPtr, err);
    }

    if (*err) {
        return 0;
    }

    int t;
    op = ERROR;
    while ((t = nextToken(str, &item, &n)) != END) {
        switch (t) {
        case PLUS:
            op = PLUS;
            break;
        case MINUS:
            op = MINUS;
            break;
        case NICHT:
            op = NICHT;
            break;
        default:
            pushBack(str, n);
            return term1;
        }

        if (op == MINUS) {
            term2 = -execITerm(str, bufferPtr, bufferLen, bitpos, pPtr, err);
        } else if (op == NICHT) {
            term2 = ~execITerm(str, bufferPtr, bufferLen, bitpos, pPtr, err);
        } else if (op == PLUS) {
            term2 = execITerm(str, bufferPtr, bufferLen, bitpos, pPtr, err);
        } if (*err) {
            return 0;
        }
        term1 += term2;
    }

    return term1;
}

static float execTerm(char **str, unsigned char *bufferPtr, int bufferLen, float floatV,char *err)
{
    float factor1;
    float factor2;
    int op;
    char *item;
    int n;

    //printf("execTerm: %s\n", *str);

    factor1 = execFactor(str, bufferPtr, bufferLen, floatV, err);
    if (*err) {
        return 0;
    }

    //printf(" F1=%f\n", factor1);
    while (1) {
        switch (nextToken(str, &item, &n)) {
        case MAL:
            op = MAL;
            break;
        case GETEILT:
            op = GETEILT;
            break;
        default:
            pushBack(str, n);
            //printf("  ret(%f)\n", factor1);
            return factor1;
        }
        factor2 = execFactor(str, bufferPtr, bufferLen, floatV, err);
        //printf(" F2=%f\n", factor2);
        if (*err) {
            return 0;
        }
        if (op == MAL) {
            factor1 *= factor2;
        } else {
            factor1 /= factor2;
        }
    }
}

static float execFactor(char **str, unsigned char *bufferPtr, int bufferLen, float floatV, char *err)
{
    char nstring[100];
    float expression;
    float factor;
    char *nPtr;
    char *item;
    int token;
    int n;

    //printf("execFactor: %s\n", *str);

    token = nextToken(str, &item, &n);
    if (token >= BYTE_FIRST && token <= BYTE_LAST) {
        // BYTE0 .. BYTE99
        int offset = token - BYTE_FIRST;
        if (offset < bufferLen)
        {
            return bufferPtr[offset];
        }
        sprintf(err,"factor B%d index out of bounds (size=%d)\n", offset, bufferLen);
        return 0;
    }
    switch(token) {
    case VALUE:
        return floatV;
    case HEX:
        nPtr = nstring;
        bzero(nstring, sizeof(nstring));
        strcpy(nstring, "0x");
        nPtr += 2;
        token = nextToken(str, &item, &n);
        while ((token == DIGIT) || (token == HEXDIGIT)) {
            *nPtr++ = *item;
            token = nextToken(str, &item, &n);
        }
        pushBack(str, n);
        sscanf(nstring, "%f", &factor);
        return factor;
    case DIGIT:
        nPtr = nstring;
        do {
            *nPtr++ = *item;
        } while ((token = nextToken(str, &item, &n)) == DIGIT);
        // If a . follows, we have a decimal number
        if (token == PUNKT) {
            do {
                *nPtr++ = *item;
            } while ((token = nextToken(str, &item, &n)) == DIGIT);
        }
        pushBack(str, n);
        *nPtr = '\0';
        factor = atof(nstring);
        //printf("  Zahl: %s (f:%f)\n", nstring, factor);
        return factor;
    case KAUF:
        expression = execExpression(str, bufferPtr, bufferLen, floatV, err);
        if (*err) {
            return 0;
        }
        if (nextToken(str, &item, &n) != KZU) {
            sprintf(err, "expected factor:) [%c]\n", *item);
            return 0;
        }
        return expression;
    default:
        sprintf(err, "expected factor: B0..B99 number ( ) [%c]\n", *item);
        return 0;
    }
}

static int execITerm(char **str, unsigned char *bufferPtr, int bufferLen, char bitpos, char *pPtr, char *err)
{
    int factor1, factor2;
    int op;
    char *item;
    int n;

    //printf("execTerm: %s\n", *str);

    factor1 = execIFactor(str, bufferPtr, bufferLen, bitpos, pPtr, err);
    if (*err) {
        return 0;
    }

    while (1) {
        switch (nextToken(str, &item, &n)) {
        case MAL:
            op = MAL;
            break;
        case GETEILT:
            op = GETEILT;
            break;
        case MODULO:
            op = MODULO;
            break;
        case UND:
            op = UND;
            break;
        case ODER:
            op = ODER;
            break;
        case XOR:
            op = XOR;
            break;
        case SHL:
            op = SHL;
            break;
        case SHR:
            op = SHR;
            break;
        default:
            pushBack(str, n);
            //printf("  ret(%f)\n", factor1);
            return factor1;
        }

        factor2 = execIFactor(str, bufferPtr, bufferLen, bitpos, pPtr, err);

        if (*err) {
            return 0;
        }

        if (op == MAL) {
            factor1 *= factor2;
        } else if (op == GETEILT) {
            factor1 /= factor2;
        } else if (op == MODULO) {
            factor1 %= factor2;
        } else if (op == UND) {
            factor1 &= factor2;
        } else if (op == ODER) {
            factor1 |= factor2;
        } else if (op == XOR) {
            factor1 ^= factor2;
        } else if (op == SHL) {
            factor1 <<= factor2;
        } else if (op == SHR) {
            factor1 >>= factor2;
        } else {
            sprintf(err, "Error exec ITerm: Unknown token %d", op);
            return 0;
        }
    }
}

static int execIFactor(char **str, unsigned char *bufferPtr, int bufferLen, char bitpos,char *pPtr, char *err)
{
    char nstring[100];
    int expression;
    int factor;
    char *nPtr;
    char *item;
    int token;
    int n;

    //printf("execFactor: %s\n", *str);

    token = nextToken(str, &item, &n);
    if (token >= BYTE_FIRST && token <= BYTE_LAST) {
        int offset = token - BYTE_FIRST;
        if (offset < bufferLen) {
            return ((int)bufferPtr[offset]) & 0xff;
        }
        sprintf(err,"factor B%d index out of bounds (size=%d)\n", offset, bufferLen);
        return 0;
    }
    if (token >= PBYTE_FIRST && token <= PBYTE_LAST) {
        int offset = token - PBYTE_FIRST;
        if (offset < bufferLen) {
            return ((int)bufferPtr[offset]) & 0xff;
        }
        sprintf(err,"factor P%d index out of bounds (size=%d)\n", offset, bufferLen);
        return 0;
    }

    switch(token) {
    case BITPOS:
        return ((int)bitpos) & 0xff;
    case HEX:
        nPtr = nstring;
        bzero(nstring, sizeof(nstring));
        strcpy(nstring, "0x");
        nPtr += 2;
        token = nextToken(str, &item, &n);
        while ((token == DIGIT) || (token == HEXDIGIT)) {
            *nPtr++ = *item;
            token = nextToken(str, &item, &n);
        }
        pushBack(str, n);
        sscanf(nstring, "%i", &factor);
        return factor;
    case DIGIT:
        nPtr = nstring;
        do {
            *nPtr++ = *item;
        } while ((token = nextToken(str, &item, &n)) == DIGIT);
        // If a . follows, we have a decimal number
        if (token == PUNKT) {
            do {
                *nPtr++ = *item;
            } while ((token = nextToken(str, &item, &n)) == DIGIT);
        }
        pushBack(str, n);
        *nPtr = '\0';
        factor = atof(nstring);
        return factor;
    case KAUF:
        expression = execIExpression(str, bufferPtr, bufferLen, bitpos, pPtr, err);
        if (*err) {
            return 0;
        }
        if (nextToken(str, &item, &n) != KZU) {
            sprintf(err, "expected factor:) [%c]\n", *item);
            return 0;
        }
        return expression;
    case NICHT:
        return ~execIFactor(str, bufferPtr, bufferLen, bitpos, pPtr, err);
    default:
        sprintf(err, "expected factor: B0..B99 P0..P99 BP number ( ) [%c]\n", *item);
        return 0;
    }
}

static int nextToken(char **str, char **c, int *count)
{
    char item;
    char c1;

    //printf("\tInput String:%s\n", *str);
    item = **str;
    while (isblank(item)) {
        item = *(++*str);
    }

    *c = *str;
    (*str)++;
    //printf("\t  Token: %c   [ %s ] \n",**c,*str);
    *count = 1;

    switch (**c) {
    case '+':
        return PLUS;
    case '-':
        return MINUS;
    case '*':
        return MAL;
    case '/':
        return GETEILT;
    case '%':
        return MODULO;
    case '(':
        return KAUF;
    case ')':
        return KZU;
    case 'V':
        return VALUE;
    case '^':
        return XOR;
    case '&':
        return UND;
    case '|':
        return ODER;
    case '~':
        return NICHT;
    case '<':
        (*count)++;
        switch (*(*str)++) {
        case '<' :
            return SHL;
        default:
            // '<' is not supported
            return ERROR;
        }
    case '>':
        (*count)++;
        switch (*(*str)++) {
        case '>':
            return SHR;
        default:
            // '>' is not supported
            return ERROR;
        }
    case 'B':
        (*count)++;
        c1 = *(*str)++;
        if (c1 == 'P') {
            return(BITPOS);
        } else if (isdigit(c1)) {
            int offset = c1 - '0';
            char c2 = *(*str);
            if (isdigit(c2)) {
                (*str)++;
                (*count)++;
                offset *= 10;
                offset += c2 - '0';
            }
            return BYTE_FIRST + offset;
        }
        return ERROR;
    case 'P':
        (*count)++;
        c1 = *(*str)++;
        if (isdigit(c1))
        {
            int offset=c1-'0';
            char c2 = *(*str);
            if (isdigit(c2))
            {
                (*str)++;
                (*count)++;
                offset *= 10;
                offset += c2 - '0';
            }
            return PBYTE_FIRST + offset;
        }
        return ERROR;
    case '0':
        if (*(*str) == 'x') {
            (*count)++;
            (*str)++;
            return HEX;
        }
        return DIGIT;
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
        return DIGIT;
    case 'a':
    case 'b':
    case 'c':
    case 'd':
    case 'e' :
    case 'f':
        return HEXDIGIT;
    case '.':
        return PUNKT;
    case '\0':
        return END;
    default:
        return ERROR;
    }
}

static void pushBack(char **str, int count)
{
    (*str) -= count;
    //printf("\t<<::%s\n", *str);
}
