// SPDX-FileCopyrightText: 2007-2016 The original vcontrold authors (cf. doc/original_authors.txt)
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef UNIT_H
#define UNIT_H

int procGetUnit(unitPtr uPtr, char *recvBuf, int len, char *result, char bitpos, char *pRecvPtr);
int procSetUnit(unitPtr uPtr, char *sendBuf, short *sendLen, char bitpos, char *pRecvPtr);

#endif // UNIT_H
