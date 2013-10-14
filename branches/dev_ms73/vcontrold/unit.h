/* unit.h */
/* $Id$ */

#ifndef _UNIT_H_
#define _UNIT_H_

int procGetUnit(unitPtr uPtr,char *recvBuf,int len,char *result,char bitpos,char *pRecvPtr); 
int procSetUnit(unitPtr uPtr,char *sendBuf,short *sendLen,char bitpos, char *pRecvPtr);

#endif // _UNIT_H_
