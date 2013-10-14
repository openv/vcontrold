/* arithemetic..h */
/* Berechnung arithmetischer Ausdruecke */
/* $Id: arithmetic.h 34 2008-04-06 19:39:29Z marcust $ */

#ifndef _ARITHMETIC_H_
#define _ARITHMETIC_H_

float execExpression(char **str,char *bPtr,float floatV, char *err); 
int execIExpression(char **str,char *bPtr,char bitpos,char *pPtr,char *err); 

#endif // _ARITHMETIC_H_
