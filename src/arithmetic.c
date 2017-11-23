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

#define HEX 8
#define HEXDIGIT 10
#define DIGIT 11
#define PUNKT 12
#define END 0
#define ERROR -100
#define PLUS 100
#define MINUS 101
#define MAL 102
#define GETEILT 103
#define MODULO 104
#define KAUF 110
#define KZU 111
#define BYTE0 200
#define BYTE1 201
#define BYTE2 202
#define BYTE3 203
#define BYTE4 204
#define BYTE5 205
#define BYTE6 206
#define BYTE7 207
#define BYTE8 208
#define BYTE9 209
#define PBYTE0 210
#define PBYTE1 211
#define PBYTE2 212
#define PBYTE3 213
#define PBYTE4 214
#define PBYTE5 215
#define PBYTE6 216
#define PBYTE7 217
#define PBYTE8 218
#define PBYTE9 219
#define BITPOS 220
#define VALUE 300
#define NICHT 400
#define UND 401
#define ODER 402
#define XOR 403
#define SHL 404
#define SHR 405

int nextToken(char **str, char **c, int *count);
void  pushBack(char **str, int n);
float execTerm(char **str, unsigned char *bPtr, float floatV, char *err);
float execFactor(char **str, unsigned  char *bPtr, float floatV, char *err);
int execITerm(char **str, unsigned char *bPtr, char bitpos, char *pPtr, char *err);
int execIFactor(char **str, unsigned char *bPtr, char bitpos, char *pPtr, char *err);

float execExpression(char **str, unsigned char *bInPtr, float floatV, char *err)
{
    int f = 1;
    float term1, term2;
    //float exp1,exp2;
    char *item;
    //unsigned char bPtr[10];
    unsigned char bPtr[10];
    int n;

    //printf("execExpression: %s\n",*str);

    // Tweak bPtr Bytes 0..9 and copy them to nPtr
    // We did not receive characters
    for (n = 0; n <= 9; n++) {
        //bPtr[n]=*bInPtr++ & 255;
        bPtr[n] = *bInPtr++;
    }

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

    term1 = execTerm(str, bPtr, floatV, err) * f;
    if (*err) {
        return 0;
    }
    //printf(" T1=%f\n",term1);

    int t;
    while ((t = nextToken(str, &item, &n)) != END) {
        f = 1;
        switch (t) {
        case PLUS:
            f = 1;
            break;
        case MINUS:
            f = -1;
            break;
        default:
            //printf(" Exp=%f\n",term1);
            pushBack(str, n);
            return term1;
        }

        term2 = execTerm(str, bPtr, floatV, err);
        if (*err) {
            return 0;
        }
        //printf(" T2=%f\n",term2);
        term1 += term2 * f;
    }

    //printf(" Exp=%f\n",term1);
    return term1;
}

float execTerm(char **str, unsigned char *bPtr, float floatV, char *err)
{
    float factor1, factor2;
    int op;
    char *item;
    int n;

    //printf("execTerm: %s\n",*str);

    factor1 = execFactor(str, bPtr, floatV, err);
    if (*err) {
        return 0;
    }

    //printf(" F1=%f\n",factor1);
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
            //printf("  ret(%f)\n",factor1);
            return factor1;
        }
        factor2 = execFactor(str, bPtr, floatV, err);
        //printf(" F2=%f\n",factor2);
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

float execFactor(char **str, unsigned char *bPtr, float floatV, char *err)
{
    char nstring[100];
    float expression;
    float factor;
    char *nPtr;
    char *item;
    char token;
    int n;

    //printf("execFactor: %s\n",*str);

    switch (nextToken(str, &item, &n)) {
    case BYTE0:
        return bPtr[0];
    case BYTE1:
        return bPtr[1];
    case BYTE2:
        return bPtr[2];
    case BYTE3:
        return bPtr[3];
    case BYTE4:
        return bPtr[4];
    case BYTE5:
        return bPtr[5];
    case BYTE6:
        return bPtr[6];
    case BYTE7:
        return bPtr[7];
    case BYTE8:
        return bPtr[8];
    case BYTE9:
        return bPtr[9];
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
        //printf("  Zahl: %s (f:%f)\n",nstring,factor);
        return factor;
    case KAUF:
        expression = execExpression(str, bPtr, floatV, err);
        if (*err) {
            return 0;
        }
        if (nextToken(str, &item, &n) != KZU) {
            sprintf(err, "expected factor:) [%c]\n", *item);
            return 0;
        }
        return expression;
    default:
        sprintf(err, "expected factor: B0..B9 number ( ) [%c]\n", *item);
        return 0;
    }
}

int execIExpression(char **str, unsigned char *bInPtr, char bitpos, char *pPtr, char *err)
{
    int f = 1;
    int term1, term2;
    //int exp1, exp2;
    int op;
    char *item;
    unsigned char bPtr[10];
    int n;

    //printf("execExpression: %s\n", *str);

    // Tweak bPtr bytes 0..9 and copy them to nPtr
    // We have received characters
    for (n = 0; n <= 9; n++) {
        //bPtr[n]=*bInPtr++ & 255;
        bPtr[n] = *bInPtr++;
    }

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
        term1 = execITerm(str, bPtr, bitpos, pPtr, err) * -1;
    } else if (op == NICHT) {
        term1 = ~(execITerm(str, bPtr, bitpos, pPtr, err));
    } else {
        term1 = execITerm(str, bPtr, bitpos, pPtr, err);
    }

    if (*err) {
        return 0;
    }

    int t;
    op = ERROR;
    while ((t = nextToken(str, &item, &n)) != END) {
        f = 1;
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
            term2 = execITerm(str, bPtr, bitpos, pPtr, err) * -1;
        } else if (op == NICHT) {
            term2 = ~(execITerm(str, bPtr, bitpos, pPtr, err));
        } else if (op == PLUS) {
            term2 = execITerm(str, bPtr, bitpos, pPtr, err);
        } if (*err) {
            return 0;
        }
        term1 += term2;
    }

    return term1;
}

int execITerm(char **str, unsigned char *bPtr, char bitpos, char *pPtr, char *err)
{
    int factor1, factor2;
    int op;
    char *item;
    int n;

    //printf("execTerm: %s\n",*str);

    factor1 = execIFactor(str, bPtr, bitpos, pPtr, err);
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
            //printf("  ret(%f)\n",factor1);
            return factor1;
        }

        factor2 = execIFactor(str, bPtr, bitpos, pPtr, err);

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

int execIFactor(char **str, unsigned char *bPtr, char bitpos, char *pPtr, char *err)
{
    char nstring[100];
    int expression;
    int factor;
    char *nPtr;
    char *item;
    char token;
    int n;

    //printf("execFactor: %s\n",*str);

    switch (nextToken(str, &item, &n)) {
    case BYTE0:
        return ((int)bPtr[0]) & 0xff;
    case BYTE1:
        return ((int)bPtr[1]) & 0xff;
    case BYTE2:
        return ((int)bPtr[2]) & 0xff;
    case BYTE3:
        return ((int)bPtr[3]) & 0xff;
    case BYTE4:
        return ((int)bPtr[4]) & 0xff;
    case BYTE5:
        return ((int)bPtr[5]) & 0xff;
    case BYTE6:
        return ((int)bPtr[6]) & 0xff;
    case BYTE7:
        return ((int)bPtr[7]) & 0xff;
    case BYTE8:
        return ((int)bPtr[8]) & 0xff;
    case BYTE9:
        return ((int)bPtr[9]) & 0xff;
    case BITPOS:
        return ((int)bitpos) & 0xff;
    case PBYTE0:
        return ((int)pPtr[0]) & 0xff;
    case PBYTE1:
        return ((int)pPtr[1]) & 0xff;
    case PBYTE2:
        return ((int)pPtr[2]) & 0xff;
    case PBYTE3:
        return ((int)pPtr[3]) & 0xff;
    case PBYTE4:
        return ((int)pPtr[4]) & 0xff;
    case PBYTE5:
        return ((int)pPtr[5]) & 0xff;
    case PBYTE6:
        return ((int)pPtr[6]) & 0xff;
    case PBYTE7:
        return ((int)pPtr[7]) & 0xff;
    case PBYTE8:
        return ((int)pPtr[8]) & 0xff;
    case PBYTE9:
        return ((int)pPtr[9]) & 0xff;
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
        expression = execIExpression(str, bPtr, bitpos, pPtr, err);
        if (*err) {
            return 0;
        }
        if (nextToken(str, &item, &n) != KZU) {
            sprintf(err, "expected factor:) [%c]\n", *item);
            return 0;
        }
        return expression;
    case NICHT:
        return ~execIFactor(str, bPtr, bitpos, pPtr, err);
    default:
        sprintf(err, "expected factor: B0..B9 P0..P9 BP number ( ) [%c]\n", *item);
        return 0;
    }
}

int nextToken(char **str, char **c, int *count)
{
    char item;

    //printf("\tInput String:%s\n",*str);

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
    case '0':
        if (*(*str) == 'x') {
            (*str)++;
            *count = 2;
            return HEX;
        }
        return DIGIT;
    case '<':
        *count = 2;
        switch (*(*str)++) {
        case '<' :
            return SHL;
        }
    case '>':
        *count = 2;
        switch (*(*str)++) {
        case '>':
            return SHR;
        }
    case 'B':
        *count = 2;
        switch (*(*str)++) {
        case '0':
            return BYTE0;
        case '1':
            return BYTE1;
        case '2':
            return BYTE2;
        case '3':
            return BYTE3;
        case '4':
            return BYTE4;
        case '5':
            return BYTE5;
        case '6':
            return BYTE6;
        case '7':
            return BYTE7;
        case '8':
            return BYTE8;
        case '9':
            return BYTE9;
        case 'P':
            return BITPOS;
        }
    case 'P':
        *count = 2;
        switch (*(*str)++) {
        case '0':
            return PBYTE0;
        case '1':
            return PBYTE1;
        case '2':
            return PBYTE2;
        case '3':
            return PBYTE3;
        case '4':
            return PBYTE4;
        case '5':
            return PBYTE5;
        case '6':
            return PBYTE6;
        case '7':
            return PBYTE7;
        case '8':
            return PBYTE8;
        case '9':
            return PBYTE9;
        }
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

void  pushBack(char **str, int count)
{
    (*str) -= count;
    //printf("\t<<::%s\n",*str);
}
