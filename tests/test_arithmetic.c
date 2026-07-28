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
    result = execExpression(&cursor, bytes, input, error);
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

int main(void)
{
    int ok = 1;
    ok &= check_expression("V/3600", 14709586.0,
                           4085.996111111111, 0);
    ok &= check_expression("V+1", 4294967295.0, 4294967296.0, 0);
    ok &= check_expression("1/0", 0.0, 0.0, 1);
    ok &= check_expression("(V*2)+5", 12.5, 30.0, 0);
    return ok ? 0 : 1;
}
