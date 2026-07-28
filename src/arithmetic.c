// SPDX-FileCopyrightText: 2007-2016 The original vcontrold authors (cf. doc/original_authors.txt)
//
// SPDX-License-Identifier: GPL-3.0-or-later

// Calculation of arithmetic expressions

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

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
#define BYTE_FIRST 200
#define BYTE_LAST 299
#define PBYTE_FIRST 300
#define PBYTE_LAST 399
#define BITPOS 400
#define VALUE 410
#define NICHT 500
#define UND 501
#define ODER 502
#define XOR 503
#define SHL 504
#define SHR 505

int nextToken(char **str, char **c, int *count);
void  pushBack(char **str, int n);
double execExpression(char **str, unsigned char *bPtr, int bufferLen,
                      double floatV, char *err);
double execTerm(char **str, unsigned char *bPtr, int bufferLen,
                double floatV, char *err);
double execFactor(char **str, unsigned char *bPtr, int bufferLen,
                  double floatV, char *err);
int execIExpression(char **str, unsigned char *bPtr, int bufferLen,
                    char bitpos, char *pPtr, char *err);
int execITerm(char **str, unsigned char *bPtr, int bufferLen,
              char bitpos, char *pPtr, char *err);
int execIFactor(char **str, unsigned char *bPtr, int bufferLen,
                char bitpos, char *pPtr, char *err);

double execExpression(char **str, unsigned char *bPtr, int bufferLen,
                      double floatV, char *err)
{
    int f = 1;
    double term1, term2;
    //float exp1,exp2;
    char *item;
    int n;

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

    term1 = execTerm(str, bPtr, bufferLen, floatV, err) * f;
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

        term2 = execTerm(str, bPtr, bufferLen, floatV, err);
        if (*err) {
            return 0;
        }
        //printf(" T2=%f\n",term2);
        term1 += term2 * f;
    }

    //printf(" Exp=%f\n",term1);
    return term1;
}

double execTerm(char **str, unsigned char *bPtr, int bufferLen,
                double floatV, char *err)
{
    double factor1, factor2;
    int op;
    char *item;
    int n;

    //printf("execTerm: %s\n",*str);

    factor1 = execFactor(str, bPtr, bufferLen, floatV, err);
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
        factor2 = execFactor(str, bPtr, bufferLen, floatV, err);
        //printf(" F2=%f\n",factor2);
        if (*err) {
            return 0;
        }
        if (op == MAL) {
            factor1 *= factor2;
        } else {
            if (factor2 == 0.0) {
                sprintf(err, "division by zero");
                return 0;
            }
            factor1 /= factor2;
        }
    }
}

double execFactor(char **str, unsigned char *bPtr, int bufferLen,
                  double floatV, char *err)
{
    char nstring[100];
    double expression;
    double factor;
    char *nPtr;
    char *item;
    char token;
    int n;

    //printf("execFactor: %s\n",*str);

    int parsedToken = nextToken(str, &item, &n);
    if (parsedToken >= BYTE_FIRST && parsedToken <= BYTE_LAST) {
        int offset = parsedToken - BYTE_FIRST;
        if (bPtr && offset < bufferLen) return bPtr[offset];
        snprintf(err, 128, "factor B%d index out of bounds (size=%d)",
                 offset, bufferLen);
        return 0;
    }
    switch (parsedToken) {
    case VALUE:
        return floatV;
    case HEX:
        nPtr = nstring;
        memset(nstring, 0, sizeof(nstring));
        strcpy(nstring, "0x");
        nPtr += 2;
        token = nextToken(str, &item, &n);
        while ((token == DIGIT) || (token == HEXDIGIT)) {
            if (nPtr >= nstring + sizeof(nstring) - 1) {
                sprintf(err, "numeric literal too long");
                return 0;
            }
            *nPtr++ = *item;
            token = nextToken(str, &item, &n);
        }
        pushBack(str, n);
        factor = strtod(nstring, NULL);
        return factor;
    case DIGIT:
        nPtr = nstring;
        do {
            if (nPtr >= nstring + sizeof(nstring) - 1) {
                sprintf(err, "numeric literal too long");
                return 0;
            }
            *nPtr++ = *item;
        } while ((token = nextToken(str, &item, &n)) == DIGIT);
        // If a . follows, we have a decimal number
        if (token == PUNKT) {
            do {
                if (nPtr >= nstring + sizeof(nstring) - 1) {
                    sprintf(err, "numeric literal too long");
                    return 0;
                }
                *nPtr++ = *item;
            } while ((token = nextToken(str, &item, &n)) == DIGIT);
        }
        pushBack(str, n);
        *nPtr = '\0';
        factor = atof(nstring);
        //printf("  Zahl: %s (f:%f)\n",nstring,factor);
        return factor;
    case KAUF:
        expression = execExpression(str, bPtr, bufferLen, floatV, err);
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

int execIExpression(char **str, unsigned char *bPtr, int bufferLen,
                    char bitpos, char *pPtr, char *err)
{
    int term1, term2;
    //int exp1, exp2;
    int op;
    char *item;
    int n;

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
        term1 = execITerm(str, bPtr, bufferLen, bitpos, pPtr, err) * -1;
    } else if (op == NICHT) {
        term1 = ~(execITerm(str, bPtr, bufferLen, bitpos, pPtr, err));
    } else {
        term1 = execITerm(str, bPtr, bufferLen, bitpos, pPtr, err);
    }

    if (*err) {
        return 0;
    }
    //printf(" T1=%d\n",term1);

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
            //printf(" Exp=%d\n",term1);
            pushBack(str, n);
            return term1;
        }

        if (op == MINUS) {
            term2 = execITerm(str, bPtr, bufferLen, bitpos, pPtr, err) * -1;
        } else if (op == NICHT) {
            term2 = ~(execITerm(str, bPtr, bufferLen, bitpos, pPtr, err));
        } else if (op == PLUS) {
            term2 = execITerm(str, bPtr, bufferLen, bitpos, pPtr, err);
        } if (*err) {
            return 0;
        }
        //printf(" T2=%d\n",term2);
        term1 += term2;
    }

    //printf(" Exp=%d\n",term1);
    return term1;
}

int execITerm(char **str, unsigned char *bPtr, int bufferLen,
              char bitpos, char *pPtr, char *err)
{
    int factor1, factor2;
    int op;
    char *item;
    int n;

    //printf("execTerm: %s\n",*str);

    factor1 = execIFactor(str, bPtr, bufferLen, bitpos, pPtr, err);
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
            //printf("  ret(%d)\n",factor1);
            return factor1;
        }

        factor2 = execIFactor(str, bPtr, bufferLen, bitpos, pPtr, err);

        if (*err) {
            return 0;
        }

        if (op == MAL) {
            factor1 *= factor2;
        } else if (op == GETEILT) {
            if (factor2 == 0) {
                sprintf(err, "division by zero");
                return 0;
            }
            factor1 /= factor2;
        } else if (op == MODULO) {
            if (factor2 == 0) {
                sprintf(err, "modulo by zero");
                return 0;
            }
            factor1 %= factor2;
        } else if (op == UND) {
            factor1 &= factor2;
        } else if (op == ODER) {
            factor1 |= factor2;
        } else if (op == XOR) {
            factor1 ^= factor2;
        } else if (op == SHL) {
            if (factor2 < 0 || factor2 >= (int)(sizeof(int) * 8)) {
                sprintf(err, "invalid shift count");
                return 0;
            }
            factor1 <<= factor2;
        } else if (op == SHR) {
            if (factor2 < 0 || factor2 >= (int)(sizeof(int) * 8)) {
                sprintf(err, "invalid shift count");
                return 0;
            }
            factor1 >>= factor2;
        } else {
            sprintf(err, "Error exec ITerm: Unknown token %d", op);
            return 0;
        }
    }
}

int execIFactor(char **str, unsigned char *bPtr, int bufferLen,
                char bitpos, char *pPtr, char *err)
{
    char nstring[100];
    int expression;
    int factor;
    char *nPtr;
    char *item;
    char token;
    int n;

    //printf("execFactor: %s\n",*str);

    int parsedToken = nextToken(str, &item, &n);
    if (parsedToken >= BYTE_FIRST && parsedToken <= BYTE_LAST) {
        int offset = parsedToken - BYTE_FIRST;
        if (bPtr && offset < bufferLen) return ((int)bPtr[offset]) & 0xff;
        snprintf(err, 128, "factor B%d index out of bounds (size=%d)",
                 offset, bufferLen);
        return 0;
    }
    if (parsedToken >= PBYTE_FIRST && parsedToken <= PBYTE_LAST) {
        int offset = parsedToken - PBYTE_FIRST;
        if (pPtr && offset < bufferLen) return ((int)pPtr[offset]) & 0xff;
        snprintf(err, 128, "factor P%d index out of bounds (size=%d)",
                 offset, bufferLen);
        return 0;
    }
    switch (parsedToken) {
    case BITPOS:
        return ((int)bitpos) & 0xff;
    case HEX:
        nPtr = nstring;
        memset(nstring, 0, sizeof(nstring));
        strcpy(nstring, "0x");
        nPtr += 2;
        token = nextToken(str, &item, &n);
        while ((token == DIGIT) || (token == HEXDIGIT)) {
            if (nPtr >= nstring + sizeof(nstring) - 1) {
                sprintf(err, "numeric literal too long");
                return 0;
            }
            *nPtr++ = *item;
            token = nextToken(str, &item, &n);
        }
        pushBack(str, n);
        sscanf(nstring, "%i", &factor);
        return factor;
    case DIGIT:
        nPtr = nstring;
        do {
            if (nPtr >= nstring + sizeof(nstring) - 1) {
                sprintf(err, "numeric literal too long");
                return 0;
            }
            *nPtr++ = *item;
        } while ((token = nextToken(str, &item, &n)) == DIGIT);
        // If a . follows, we have a decimal number
        if (token == PUNKT) {
            do {
                if (nPtr >= nstring + sizeof(nstring) - 1) {
                    sprintf(err, "numeric literal too long");
                    return 0;
                }
                *nPtr++ = *item;
            } while ((token = nextToken(str, &item, &n)) == DIGIT);
        }
        pushBack(str, n);
        *nPtr = '\0';
        factor = atof(nstring);
        return factor;
    case KAUF:
        expression = execIExpression(str, bPtr, bufferLen, bitpos, pPtr, err);
        if (*err) {
            return 0;
        }
        if (nextToken(str, &item, &n) != KZU) {
            sprintf(err, "expected factor:) [%c]\n", *item);
            return 0;
        }
        return expression;
    case NICHT:
        return ~execIFactor(str, bPtr, bufferLen, bitpos, pPtr, err);
    default:
        sprintf(err, "expected factor: B0..B99 P0..P99 BP number ( ) [%c]\n", *item);
        return 0;
    }
}

int nextToken(char **str, char **c, int *count)
{
    char item;
    char first;

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
        return ERROR;
    case '>':
        *count = 2;
        switch (*(*str)++) {
        case '>':
            return SHR;
        }
        return ERROR;
    case 'B':
        first = *(*str)++;
        (*count)++;
        if (first == 'P') return BITPOS;
        if (first >= 'A' && first <= 'F') {
            return BYTE_FIRST + 10 + first - 'A';
        }
        if (isdigit((unsigned char)first)) {
            int offset = first - '0';
            if (isdigit((unsigned char)**str)) {
                offset = offset * 10 + *(*str)++ - '0';
                (*count)++;
            }
            return BYTE_FIRST + offset;
        }
        return ERROR;
    case 'P':
        first = *(*str)++;
        (*count)++;
        if (first >= 'A' && first <= 'F') {
            return PBYTE_FIRST + 10 + first - 'A';
        }
        if (isdigit((unsigned char)first)) {
            int offset = first - '0';
            if (isdigit((unsigned char)**str)) {
                offset = offset * 10 + *(*str)++ - '0';
                (*count)++;
            }
            return PBYTE_FIRST + offset;
        }
        return ERROR;
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
