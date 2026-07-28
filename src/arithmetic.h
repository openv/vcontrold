// SPDX-FileCopyrightText: 2007-2016 The original vcontrold authors (cf. doc/original_authors.txt)
//
// SPDX-License-Identifier: GPL-3.0-or-later

// Calculation of arithmetic expressions

#ifndef ARITHMETIC_H
#define ARITHMETIC_H

double execExpression(char **str, unsigned char *buffer, int bufferLen,
                      double numericValue, char *err);
int execIExpression(char **str, unsigned char *buffer, int bufferLen,
                    char bitpos, char *previousBuffer, char *err);

#endif // ARITHMETIC_H
