/* arithemetic..h */
/* Berechnung arithmetischer Ausdruecke */
/* $Id: arithmetic.h 34 2008-04-06 19:39:29Z marcust $ */

float execExpression(char **str,char *bPtr,float floatV, char *err);
int execIExpression(char **str,char *bPtr,char bitpos,char *pPtr,char *err);

