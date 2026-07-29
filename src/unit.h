// SPDX-FileCopyrightText: 2007-2016 The original vcontrold authors (cf. doc/original_authors.txt)
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef UNIT_H
#define UNIT_H

int procGetUnit(unitPtr uPtr, const char *recvBuf, int len,
                char *result, size_t resultCapacity,
                char bitpos, const char *pRecvPtr);
int procSetUnit(unitPtr uPtr, char *sendBuf, size_t sendCapacity,
                short *sendLen, char bitpos, const char *pRecvPtr);

#endif // UNIT_H
