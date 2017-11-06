/* unit.h */
/* $Id$ */

int procGetUnit(unitPtr uPtr,char *recvBuf,int len,char *result,char bitpos,char *pRecvPtr); 
int procSetUnit(unitPtr uPtr,char *sendBuf,short *sendLen,char bitpos, char *pRecvPtr);

