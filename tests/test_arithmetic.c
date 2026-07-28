#include <math.h>
#include <stdio.h>
#include <string.h>

#include "arithmetic.h"

static int check_expression(const char *source, double input,
                            double expected, int expect_error)
{
    char expression[512];
    char bytes[16] = {0};
    char error[256] = {0};
    char *cursor;
    double result;

    snprintf(expression, sizeof(expression), "%s", source);
    cursor = expression;
    result = execExpression(&cursor, (unsigned char *)bytes, sizeof(bytes),
                            input, error);
    if (!!*error != !!expect_error) {
        fprintf(stderr, "%s: unexpected error state: %s\n", source, error);
        return 0;
    }
    if (!expect_error && fabs(result - expected) > 1e-9) {
        fprintf(stderr, "%s: got %.12f, expected %.12f\n",
                source, result, expected);
        return 0;
    }
    return 1;
}

static int check_byte_expression(const char *source, int bufferLen,
                                 double expected, int expect_error)
{
    char expression[64];
    unsigned char bytes[100];
    char error[256] = {0};
    char *cursor;
    double result;

    for (int i = 0; i < (int)sizeof(bytes); i++) bytes[i] = (unsigned char)i;
    snprintf(expression, sizeof(expression), "%s", source);
    cursor = expression;
    result = execExpression(&cursor, bytes, bufferLen, 0.0, error);
    if (!!*error != !!expect_error) {
        fprintf(stderr, "%s: unexpected error state: %s\n", source, error);
        return 0;
    }
    if (!expect_error && fabs(result - expected) > 1e-9) {
        fprintf(stderr, "%s: got %.12f, expected %.12f\n",
                source, result, expected);
        return 0;
    }
    return 1;
}

static int check_integer_expression(const char *source, int expected,
                                    int expect_error)
{
    char expression[128];
    unsigned char bytes[4] = {1, 2, 3, 4};
    char previous[4] = {5, 6, 7, 8};
    char error[256] = {0};
    char *cursor;
    int result;

    snprintf(expression, sizeof(expression), "%s", source);
    cursor = expression;
    result = execIExpression(&cursor, bytes, sizeof(bytes), 3,
                             previous, error);
    if (!!*error != !!expect_error) return 0;
    return expect_error || result == expected;
}

int main(void)
{
    int ok = 1;
    ok &= check_expression("V/3600", 14709586.0,
                           4085.996111111111, 0);
    ok &= check_expression("V+1", 4294967295.0, 4294967296.0, 0);
    ok &= check_expression("1/0", 0.0, 0.0, 1);
    ok &= check_expression("(V*2)+5", 12.5, 30.0, 0);
    ok &= check_byte_expression("B0+B15+B99", 100, 114.0, 0);
    ok &= check_byte_expression("BA+BF", 16, 25.0, 0);
    ok &= check_byte_expression("B16", 16, 0.0, 1);
    ok &= check_integer_expression("42+0x10", 58, 0);
    ok &= check_integer_expression("1.5", 0, 1);
    ok &= check_integer_expression("999999999999999999999", 0, 1);
    return ok ? 0 : 1;
}
