/* Routinen zum lesen von XML Dateien */
/* $Id: xmlconfig.c 34 2008-04-06 19:39:29Z marcust $ */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <libxml/parser.h>
#include <libxml/xinclude.h>
#include <arpa/inet.h>

#include "xmlconfig.h"
#include "common.h"
#include "parser.h"

#if defined(__FreeBSD__)
#include <netinet/in.h>
#endif

/* Deklarationen */
protocolPtr newProtocolNode(protocolPtr ptr);
char *getPropertyNode(xmlAttrPtr cur,xmlChar *name);
enumPtr newEnumNode(enumPtr ptr);
void removeProtocolList(protocolPtr ptr);
void removeUnitList(unitPtr ptr);
void removeMacroList(macroPtr ptr);
void removeCommandList(commandPtr ptr);
void removeDeviceList(devicePtr ptr);
void removeIcmdList(icmdPtr ptr);
void removeAllowList(allowPtr ptr);
void removeEnumList(enumPtr ptr);
void freeAllLists();




/* globale Variablen */
protocolPtr protoPtr=NULL;
unitPtr uPtr=NULL;
devicePtr devPtr=NULL;
configPtr cfgPtr=NULL;
commandPtr cmdPtr=NULL;



protocolPtr newProtocolNode(protocolPtr ptr) {
	protocolPtr nptr;
	if (ptr && ptr->next) {
		return(newProtocolNode(ptr->next));
	}
	nptr=calloc(1,sizeof(Protocol));
	if (!nptr) {
		fprintf(stderr,"malloc gescheitert\n");
		exit(1);
	}
	if (ptr) 
		ptr->next=nptr;
	nptr->next=NULL;
	nptr->icPtr=NULL;
	nptr->id = 0;
	return(nptr);
}

protocolPtr getProtocolNode(protocolPtr ptr,const char *name) {
	if (!ptr) 
		return(NULL);
	if (strcmp(ptr->name,name)!=0) 
		return(getProtocolNode(ptr->next,name));
	return(ptr);
}

void removeProtocolList(protocolPtr ptr) {
	if (ptr && ptr->next)
		removeProtocolList(ptr->next);
	if (ptr) {
		removeMacroList(ptr->mPtr);
		removeIcmdList(ptr->icPtr);
		free(ptr->name);
		free(ptr);
	}
}
	
	

unitPtr newUnitNode(unitPtr ptr) {
	unitPtr nptr;
	if (ptr && ptr->next) {
		return(newUnitNode(ptr->next));
	}
	nptr=calloc(1,sizeof(Unit));
	if (!nptr) {
		fprintf(stderr,"malloc gescheitert\n");
		exit(1);
	}
	if (ptr) 
		ptr->next=nptr;
	nptr->next=NULL;
	return(nptr);
}

unitPtr getUnitNode(unitPtr ptr,const char *name) {
	if (!ptr) 
		return(NULL);
	if (ptr->abbrev && (strcmp(ptr->abbrev,name)!=0)) 
		return(getUnitNode(ptr->next,name));
	return(ptr);
}

void removeUnitList(unitPtr ptr) {
	if (ptr && ptr->next)
		removeUnitList(ptr->next);
	if (ptr) {
		removeEnumList(ptr->ePtr);
		free(ptr->name);
		free(ptr->abbrev);
		free(ptr->gCalc);
		free(ptr->sCalc);
		free(ptr->gICalc);
		free(ptr->sICalc);
		free(ptr->entity);
		free(ptr->type);
		free(ptr);
	}
}

macroPtr newMacroNode(macroPtr ptr) {
	macroPtr nptr;
	if (ptr && ptr->next) {
		return(newMacroNode(ptr->next));
	}
	nptr=calloc(1,sizeof(Macro));
	if (!nptr) {
		fprintf(stderr,"malloc gescheitert\n");
		exit(1);
	}
	if (ptr) 
		ptr->next=nptr;
	nptr->next=NULL;
	return(nptr);
}

macroPtr getMacroNode(macroPtr ptr,const char *name) {
	if (!ptr) 
		return(NULL);
	if (ptr->name && strcmp(ptr->name,name)!=0) 
	/* if (*name && !(strstr(ptr->name,name)==ptr->name))  */
		return(getMacroNode(ptr->next,name));
	return(ptr);
}

void removeMacroList(macroPtr ptr) {
	if (ptr && ptr->next)
		removeMacroList(ptr->next);
	if (ptr) {
		free(ptr->name);
		free(ptr->command);
		free(ptr);
	}
}

commandPtr newCommandNode(commandPtr ptr) {
	commandPtr nptr;
	if (ptr && ptr->next) {
		return(newCommandNode(ptr->next));
	}
	nptr=calloc(1,sizeof(Command));
	if (!nptr) {
		fprintf(stderr,"malloc gescheitert\n");
		exit(1);
	}
	if (ptr) 
		ptr->next=nptr;
	nptr->next=NULL;
	nptr->cmpPtr=NULL;
	nptr->bit=-1;
	return(nptr);
}

void addCommandNode(commandPtr ptr, commandPtr nptr) {
	if (!ptr) 
		return;
	else if (ptr->next)
		addCommandNode(ptr->next,nptr);
	else {
		ptr->next=nptr;
	}
}

commandPtr getCommandNode(commandPtr ptr,const char *name) {
	if (!ptr) 
		return(NULL);
	if (ptr->name && name && (strcmp(ptr->name,name)!=0)) 
		return(getCommandNode(ptr->next,name));
	return(ptr);
}

void removeCommandList(commandPtr ptr) {
	if (ptr && ptr->next)
		removeCommandList(ptr->next);
	if (ptr) {
		removeCompileList(ptr->cmpPtr);
		free(ptr->send);
		if (ptr->nodeType >0) { /* wurde aus der xml Datei gelesen */
			if (ptr->addr)
				free(ptr->addr);
			if (ptr->precmd)
				free(ptr->precmd);
			if (ptr->unit)
				free(ptr->unit);
			if (ptr->pcmd)
				free(ptr->pcmd);
			if (ptr->errStr)
				free(ptr->errStr);
			if (ptr->nodeType==1) { /* Originalkonoten */
				if (ptr->name)
					free(ptr->name);
				if (ptr->description)
					free(ptr->description);
			}
		}
		free(ptr);
	}
}

devicePtr newDeviceNode(devicePtr ptr) {
	devicePtr nptr;
	if (ptr && ptr->next) {
		return(newDeviceNode(ptr->next));
	}
	nptr=calloc(1,sizeof(Device));
	if (!nptr) {
		fprintf(stderr,"malloc gescheitert\n");
		exit(1);
	}
	if (ptr) 
		ptr->next=nptr;
	nptr->next=NULL;
	return(nptr);
}

devicePtr getDeviceNode(devicePtr ptr,char *id) {
	if (!ptr) 
		return(NULL);
	if (strcmp(ptr->id,id)!=0) 
		return(getDeviceNode(ptr->next,id));
	return(ptr);
}

void removeDeviceList(devicePtr ptr) {
	if (ptr && ptr->next)
		removeDeviceList(ptr->next);
	if (ptr) {
		removeCommandList(ptr->cmdPtr);
		free(ptr->name);
		free(ptr->id);
		free(ptr);
	}
}



icmdPtr newIcmdNode(icmdPtr ptr) {
	icmdPtr nptr;
	if (ptr && ptr->next) {
		return(newIcmdNode(ptr->next));
	}
	nptr=calloc(1,sizeof(iCmd));
	if (!nptr) {
		fprintf(stderr,"malloc gescheitert\n");
		exit(1);
	}
	if (ptr) 
		ptr->next=nptr;
	nptr->next=NULL;
	return(nptr);
}

icmdPtr getIcmdNode(icmdPtr ptr,const char *name) {
	if (!ptr) 
		return(NULL);
	if (ptr->name && (strcmp(ptr->name,name)!=0)) 
		return(getIcmdNode(ptr->next,name));
	return(ptr);
}


void removeIcmdList(icmdPtr ptr) {
	if (ptr && ptr->next)
		removeIcmdList(ptr->next);
	if (ptr) {
		free(ptr->name);
		free(ptr->send);
		free(ptr);
	}
}


allowPtr getAllowNode(allowPtr ptr,const in_addr_t testIP) {
	if (!ptr) 
		return(NULL);
	if ((ntohl(ptr->ip) & ptr->mask) == (ntohl(testIP) & ptr->mask)) 
		return(ptr);
	return(getAllowNode(ptr->next,testIP));
}

allowPtr newAllowNode(allowPtr ptr) {
	allowPtr nptr;
	if (ptr && ptr->next) {
		return(newAllowNode(ptr->next));
	}
	nptr=calloc(1,sizeof(Allow));
	if (!nptr) {
		fprintf(stderr,"malloc gescheitert\n");
		exit(1);
	}
	if (ptr) 
		ptr->next=nptr;
	return(nptr);
}

void removeAllowList(allowPtr ptr) {
	if (ptr && ptr->next)
		removeAllowList(ptr->next);
	if (ptr) {
		free(ptr->text);
		free(ptr);
	}
}



enumPtr newEnumNode(enumPtr ptr) {
	enumPtr nptr;
	if (ptr && ptr->next) {
		return(newEnumNode(ptr->next));
	}
	nptr=calloc(1,sizeof(Enumerate));
	if (!nptr) {
		fprintf(stderr,"malloc gescheitert\n");
		exit(1);
	}
	if (ptr) 
		ptr->next=nptr;
	return(nptr);
}


enumPtr getEnumNode(enumPtr ptr,char *search, int len) {
	if (!ptr) 
		return(NULL);
	/* len angegeben, wir suchen nach Bytes */
	if ((len>0) && ptr->bytes && (memcmp(ptr->bytes,search,len)==0))
		return(ptr);
	/* len==0 -> Stringvergelich */
	else if (!len && ptr->text && (strcmp(ptr->text,search)==0))
		return(ptr);
	/* len == -1 -> default Wert */
	else if ((len<0) && (ptr->bytes==NULL))
		return(ptr);
	else 
		return(getEnumNode(ptr->next,search,len));
}

void removeEnumList(enumPtr ptr) {
	if (ptr && ptr->next)
		removeEnumList(ptr->next);
	if (ptr) {
		free(ptr->text);
		free(ptr->bytes);
		free(ptr);
	}
}



void printNode(xmlNodePtr ptr) {
        static int blanks=0;
	int n;
	if (!ptr)
		return;
	for (n=0;n<=blanks;n++)
		printf(" ");
	if ((ptr->type==XML_ELEMENT_NODE) || (ptr->type==XML_TEXT_NODE)) {
		printf("(%d) Node::Name=%s Type:%d Content=%s\n",ptr->line,ptr->name,ptr->type,ptr->content);
	}
	else {
		printf("Node::Name=%s\n",ptr->name);
	}
	if ((ptr->type==XML_ELEMENT_NODE) && ptr->properties) {
		blanks++;
		printNode((xmlNodePtr ) ptr->properties);
		blanks--;
	}
	if (ptr->children) {
		blanks++;
		printNode(ptr->children);
		blanks--;
	}
	if ( ptr->next) {
	   printNode(ptr->next);
	}
}

char *getTextNode(xmlNodePtr cur) {
	if ((cur->children) && (cur->children->type == XML_TEXT_NODE) ) 
		return((char *)cur->children->content);
	else 
		return(NULL);
}

char *getPropertyNode(xmlAttrPtr cur,xmlChar *name) {
	if ((cur) && 
	    (cur->type == XML_ATTRIBUTE_NODE) &&
	     strstr((char *)cur->name,(char *)name))
		return((char *)getTextNode((xmlNodePtr) cur));
	else if (cur && cur->next) 
		return((char *)getPropertyNode(cur->next,name));
	else
		return(NULL);
}

void nullIT(char **ptr) {
	*ptr=calloc(1,sizeof(char));
	**ptr='\0';
}

configPtr parseConfig(xmlNodePtr cur) {
	int serialFound=0;
	int netFound=0;
	int logFound=0;
	configPtr cfgPtr;
	char *chrPtr;
	xmlNodePtr prevPtr;
	char string[1000];
	allowPtr aPtr;
	char ip[16];

	cfgPtr=calloc(1,sizeof(Config));
	cfgPtr->port=0;
	cfgPtr->syslog=0;
	cfgPtr->debug=0;
	cfgPtr->aPtr=NULL;
	while(cur) {
		snprintf(string, sizeof(string),"CONFIG:(%d) Node::Name=%s Type:%d Content=%s",cur->line,cur->name,cur->type,cur->content);
		logIT(LOG_INFO,string);
		if (strstr((char *)cur->name,"serial"))  {
			serialFound=1;
			prevPtr=cur;
			cur=cur->children;
		}
		else if (strstr((char *)cur->name,"net"))  {
			netFound=1;
			prevPtr=cur;
			cur=cur->children;
		}
		else if (strstr((char *)cur->name,"logging"))  {
			logFound=1;
			prevPtr=cur;
			cur=cur->children;
		}
		else if (strstr((char *)cur->name,"device"))  {
			chrPtr=getPropertyNode(cur->properties,(xmlChar *)"ID");
			snprintf(string, sizeof(string),"     Device ID=%s",cfgPtr->devID);
			logIT(LOG_INFO,string);
			if (chrPtr) {
				cfgPtr->devID=calloc(strlen(chrPtr)+1,sizeof(char));
				strcpy(cfgPtr->devID,chrPtr);
			}
			else
				nullIT(&cfgPtr->devID);
			cur=cur->next;
		}
			
		else if (serialFound && strstr((char *)cur->name,"tty"))  {
		 	chrPtr=getTextNode(cur);	
			snprintf(string, sizeof(string),"   (%d) Node::Name=%s Type:%d Content=%s",cur->line,cur->name,cur->type,chrPtr);
			logIT(LOG_INFO,string);
			if (chrPtr) {
				cfgPtr->tty=calloc(strlen(chrPtr)+1,sizeof(char));
				strcpy(cfgPtr->tty,chrPtr);
			}
			else
				nullIT(&cfgPtr->devID);

			(cur->next && 
			 (!(cur->next->type==XML_TEXT_NODE)|| cur->next->next)) ? (cur=cur->next) : (cur=prevPtr->next);
		}
		else if (netFound && strstr((char *)cur->name,"port"))  {
		 	chrPtr=getTextNode(cur);	
			snprintf(string, sizeof(string),"   (%d) Node::Name=%s Type:%d Content=%s",cur->line,cur->name,cur->type,chrPtr);
			logIT(LOG_INFO,string);
			if (chrPtr) {
				cfgPtr->port=atoi(chrPtr);
			}
			(cur->next && 
			 (!(cur->next->type==XML_TEXT_NODE)|| cur->next->next)) ? (cur=cur->next) : (cur=prevPtr->next);
		}
		else if (netFound && strstr((char *)cur->name,"allow"))  {
			chrPtr=getPropertyNode(cur->properties,(xmlChar *)"ip");
			snprintf(string, sizeof(string),"   (%d) Node::Name=%s Type:%d Content=%s",cur->line,cur->name,cur->type,chrPtr);
			logIT(LOG_INFO,string);

			/* wir zerlegen nun chrPtr in ip/size
			ist keine Maske angegeben dann nehmen wir /32 an
			danach wird eine inverser Maske gebaut und in mask gespeichert
			ip==text Inhalt IP-Adresse mask==Bitmaske */

			char *ptr;
			short count;
			short size;
			in_addr_t mask;

			bzero(ip,sizeof(ip));
			bzero(string,sizeof(string));
			if ((ptr=strchr(chrPtr,'/'))) {
				strncpy(string,ptr+1,sizeof(string)-1);
				size=atoi(string);
				strncpy(ip,chrPtr,ptr-chrPtr);
			}
			else {	
				strncpy(ip,chrPtr,sizeof(ip)-1);
				size=32;
			}
			if(inet_addr(ip) != INADDR_NONE) {
				aPtr=newAllowNode(cfgPtr->aPtr);
				aPtr->text=calloc(strlen(chrPtr)+1,sizeof(char));
				strcpy(aPtr->text,chrPtr);
				mask=0;
				/* wir basteln die Bitmaske */
				if(size) {
				 	mask=0x80000000; 
					for(count=0;count<size-1;count++) {
						mask>>=1;
						mask|=0x80000000; 
					}
				}
				aPtr->mask=mask;
				aPtr->ip=inet_addr(ip);
				if(!cfgPtr->aPtr) 
					cfgPtr->aPtr=aPtr;
				snprintf(string, sizeof(string),"     Allow IP:%s Size:/%d",ip,size);
				logIT(LOG_INFO,string);
			}

			(cur->next && 
			 (!(cur->next->type==XML_TEXT_NODE)|| cur->next->next)) ? (cur=cur->next) : (cur=prevPtr->next);
		}
		else if (logFound && strstr((char *)cur->name,"file"))  {
		 	chrPtr=getTextNode(cur);	
			snprintf(string, sizeof(string),"   (%d) Node::Name=%s Type:%d Content=%s",cur->line,cur->name,cur->type,chrPtr);
			logIT(LOG_INFO,string);
			if (chrPtr) {
				cfgPtr->logfile=calloc(strlen(chrPtr)+1,sizeof(char));
				strcpy(cfgPtr->logfile,chrPtr);
			}
			else
				nullIT(&cfgPtr->devID);
			(cur->next && 
			 (!(cur->next->type==XML_TEXT_NODE)|| cur->next->next)) ? (cur=cur->next) : (cur=prevPtr->next);
		}
		else if (logFound && strstr((char *)cur->name,"syslog"))  {
		 	chrPtr=getTextNode(cur);	
			snprintf(string, sizeof(string),"   (%d) Node::Name=%s Type:%d Content=%s",cur->line,cur->name,cur->type,chrPtr);
			logIT(LOG_INFO,string);
			((*chrPtr=='y')||(*chrPtr=='1')) ? (cfgPtr->syslog=1) : (cfgPtr->syslog=0);
			(cur->next && 
			 (!(cur->next->type==XML_TEXT_NODE)|| cur->next->next)) ? (cur=cur->next) : (cur=prevPtr->next);
		}
		else if (logFound && strstr((char *)cur->name,"debug"))  {
		 	chrPtr=getTextNode(cur);	
			((*chrPtr=='y')||(*chrPtr=='1')) ? (cfgPtr->debug=1) : (cfgPtr->debug=0);
			(cur->next && 
			 (!(cur->next->type==XML_TEXT_NODE)|| cur->next->next)) ? (cur=cur->next) : (cur=prevPtr->next);
		}
		else 
			(cur->next && 
			 (!(cur->next->type==XML_TEXT_NODE)|| cur->next->next)) ? (cur=cur->next) : (cur=prevPtr->next);
	}
	return(cfgPtr);
		
}

unitPtr parseUnit(xmlNodePtr cur) {
	unitPtr uPtr;
	unitPtr uStartPtr=NULL;
	char *unit;
	char *chrPtr;
	int unitFound=0;
	xmlNodePtr prevPtr;
	char string[1000];
	enumPtr ePtr;

	while(cur) {
		snprintf(string, sizeof(string),"UNIT: (%d) Node::Name=%s Type:%d Content=%s",cur->line,cur->name,cur->type,cur->content);
		logIT(LOG_INFO,string);
		if (cur->type == XML_TEXT_NODE) {
			cur=cur->next;
			continue;
		}
		if (strstr((char *)cur->name,"unit"))  {
			unit=getPropertyNode(cur->properties,(xmlChar *)"name");
			if (unit) { /* neue Unit gelesen */
				snprintf(string, sizeof(string),"Neue Unit: %s",unit);
				logIT(LOG_INFO,string);
				uPtr=newUnitNode(uStartPtr);
				if(!uStartPtr) 
					uStartPtr=uPtr;
				uPtr->name=calloc(strlen(unit)+1,sizeof(char));
				strcpy(uPtr->name,unit);
				unitFound=1;
				prevPtr=cur;
				cur=cur->children;
				continue;
			}
		}
		else if (unitFound && strstr((char *)cur->name,"enum"))  {
			chrPtr=getPropertyNode(cur->properties,(xmlChar *)"text");
			snprintf(string, sizeof(string),"   (%d) Node::Name=%s Type:%d Content=%s (text)",cur->line,cur->name,cur->type,chrPtr);
			logIT(LOG_INFO,string);
			if (chrPtr) {
				ePtr=newEnumNode(uPtr->ePtr);
				if(!uPtr->ePtr) 
					uPtr->ePtr=ePtr;
				ePtr->text=calloc(strlen(chrPtr)+1,sizeof(char));
				strncpy(ePtr->text,chrPtr,strlen(chrPtr));
				chrPtr=getPropertyNode(cur->properties,(xmlChar *)"bytes");
				if (chrPtr) {
					snprintf(string, sizeof(string),"          (%d) Node::Name=%s Type:%d Content=%s (bytes)",cur->line,cur->name,cur->type,chrPtr);
					logIT(LOG_INFO,string);
					bzero(string,sizeof(string));
					ePtr->len=string2chr(chrPtr,string,sizeof(string));
					ePtr->bytes=calloc(ePtr->len,sizeof(char));
					memcpy(ePtr->bytes,string,ePtr->len);
				}
			}
			else  {
				logIT(LOG_ERR,"Property Node ohne text=");
				return(NULL);
			}

			(cur->next && 
			(!(cur->next->type==XML_TEXT_NODE)|| cur->next->next)) ? (cur=cur->next) : (cur=prevPtr->next);
		}
		else if (unitFound && strstr((char *)cur->name,"abbrev"))  {
			chrPtr=getTextNode(cur);	
			snprintf(string, sizeof(string),"   (%d) Node::Name=%s Type:%d Content=%s",cur->line,cur->name,cur->type,chrPtr);
			logIT(LOG_INFO,string);
			if (chrPtr) {
				uPtr->abbrev=calloc(strlen(chrPtr)+1,sizeof(char));
				strcpy(uPtr->abbrev,chrPtr);
			}
			else 
				nullIT(&uPtr->abbrev);
			(cur->next && 
			(!(cur->next->type==XML_TEXT_NODE)|| cur->next->next)) ? (cur=cur->next) : (cur=prevPtr->next);
		}
		else if (unitFound && (strcmp((char *)cur->name,"calc")==0))  {
			chrPtr=getPropertyNode(cur->properties,(xmlChar *)"get");
			snprintf(string, sizeof(string),"   (%d) Node::Name=%s Type:%d Content=%s (get)",cur->line,cur->name,cur->type,chrPtr);
			logIT(LOG_INFO,string);
			if (chrPtr) {
				uPtr->gCalc=calloc(strlen(chrPtr)+1,sizeof(char));
				strcpy(uPtr->gCalc,chrPtr);
			}
			else 
				nullIT(&uPtr->gCalc);
			chrPtr=getPropertyNode(cur->properties,(xmlChar *)"set");
			snprintf(string, sizeof(string),"   (%d) Node::Name=%s Type:%d Content=%s (set)",cur->line,cur->name,cur->type,chrPtr);
			logIT(LOG_INFO,string);
			if (chrPtr) {
				uPtr->sCalc=calloc(strlen(chrPtr)+1,sizeof(char));
				strcpy(uPtr->sCalc,chrPtr);
			}
			else 
				nullIT(&uPtr->sCalc);
			(cur->next && 
			(!(cur->next->type==XML_TEXT_NODE)|| cur->next->next)) ? (cur=cur->next) : (cur=prevPtr->next);
		}
		else if (unitFound && (strcmp((char *)cur->name,"icalc")==0))  {
			chrPtr=getPropertyNode(cur->properties,(xmlChar *)"get");
			snprintf(string, sizeof(string),"   (%d) Node::Name=%s Type:%d Content=%s (get)",cur->line,cur->name,cur->type,chrPtr);
			logIT(LOG_INFO,string);
			if (chrPtr) {
				uPtr->gICalc=calloc(strlen(chrPtr)+1,sizeof(char));
				strcpy(uPtr->gICalc,chrPtr);
			}
			else 
				nullIT(&uPtr->gICalc);
			chrPtr=getPropertyNode(cur->properties,(xmlChar *)"set");
			snprintf(string, sizeof(string),"   (%d) Node::Name=%s Type:%d Content=%s (set)",cur->line,cur->name,cur->type,chrPtr);
			logIT(LOG_INFO,string);
			if (chrPtr) {
				uPtr->sICalc=calloc(strlen(chrPtr)+1,sizeof(char));
				strcpy(uPtr->sICalc,chrPtr);
			}
			else 
				nullIT(&uPtr->sICalc);
			(cur->next && 
			(!(cur->next->type==XML_TEXT_NODE)|| cur->next->next)) ? (cur=cur->next) : (cur=prevPtr->next);
		}
		else if (unitFound && strstr((char *)cur->name,"type"))  {
			chrPtr=getTextNode(cur);	
			snprintf(string, sizeof(string),"   (%d) Node::Name=%s Type:%d Content=%s",cur->line,cur->name,cur->type,chrPtr);
			logIT(LOG_INFO,string);
			if (chrPtr) {
				uPtr->type=calloc(strlen(chrPtr)+1,sizeof(char));
				strcpy(uPtr->type,chrPtr);
			}
			else 
				nullIT(&uPtr->type);
			(cur->next && 
			(!(cur->next->type==XML_TEXT_NODE)|| cur->next->next)) ? (cur=cur->next) : (cur=prevPtr->next);
		}
		else if (unitFound && strstr((char *)cur->name,"entity"))  {
			chrPtr=getTextNode(cur);	
			snprintf(string, sizeof(string),"   (%d) Node::Name=%s Type:%d Content=%s",cur->line,cur->name,cur->type,chrPtr);
			logIT(LOG_INFO,string);
			if(chrPtr) {
				uPtr->entity=calloc(strlen(chrPtr)+1,sizeof(char));
				strcpy(uPtr->entity,chrPtr);
			}
			else 
				nullIT(&uPtr->entity);
			/* (cur->next && cur->next->next) ? (cur=cur->next) : (cur=prevPtr->next); */
			(cur->next && 
			 (!(cur->next->type==XML_TEXT_NODE)|| cur->next->next)) ? (cur=cur->next) : (cur=prevPtr->next);
		}
		else {
			logIT(LOG_ERR,"Fehler beim parsen unit");
			return(NULL);
		}
	}
	return(uStartPtr);
}

macroPtr parseMacro(xmlNodePtr cur) {
	macroPtr mPtr;
	macroPtr mStartPtr=NULL;
	char *macro;
	char *chrPtr;
	int macroFound=0;
	xmlNodePtr prevPtr;
	char string[1000];

	while(cur) {
		snprintf(string, sizeof(string),"MACRO: (%d) Node::Name=%s Type:%d Content=%s",cur->line,cur->name,cur->type,cur->content);
		logIT(LOG_INFO,string);
		if (cur->type == XML_TEXT_NODE) {
			cur=cur->next;
			continue;
		}
		if (strstr((char *)cur->name,"macro"))  {
			macro=getPropertyNode(cur->properties,(xmlChar *)"name");
			if (macro) { /* neues Macro gelesen */
				snprintf(string, sizeof(string),"Neues Macro: %s",macro);
				logIT(LOG_INFO,string);
				mPtr=newMacroNode(mStartPtr);
				if(!mStartPtr) 
					mStartPtr=mPtr;
				mPtr->name=calloc(strlen(macro)+1,sizeof(char));
				strcpy(mPtr->name,macro);
				macroFound=1;
				prevPtr=cur;
				cur=cur->children;
				continue;
			}
		}
		else if (macroFound && strstr((char *)cur->name,"command"))  {
			chrPtr=getTextNode(cur);	
			snprintf(string, sizeof(string),"   (%d) Node::Name=%s Type:%d Content=%s",cur->line,cur->name,cur->type,chrPtr);
			logIT(LOG_INFO,string);
			if (chrPtr) { 
				mPtr->command=calloc(strlen(chrPtr)+1,sizeof(char));
				strcpy(mPtr->command,chrPtr);
			}
			else 
				nullIT(&mPtr->command);
			(cur->next && 
			(!(cur->next->type==XML_TEXT_NODE)|| cur->next->next)) ? (cur=cur->next) : (cur=prevPtr->next);
		}
		else {
			snprintf(string, sizeof(string),"Fehler beim parsen macro");
			logIT(LOG_INFO,string);
			return(NULL);
		}
	}
	return(mStartPtr);
}

commandPtr parseCommand(xmlNodePtr cur,commandPtr cPtr,devicePtr dePtr) {
	commandPtr cStartPtr=NULL;
	devicePtr dPtr;
	char *command;
	char *protocmd;
	char *chrPtr;
	xmlNodePtr prevPtr;
	char string[1000];
	char *id;
	int commandFound;
	commandPtr ncPtr;
	short count;

	/* wir unterscheiden ob der Aufruf rekursiv erfolgte,
           dann ist cPtr gesetzt */
	if(!cPtr)
		commandFound=0;
	else {
		commandFound=1;
		prevPtr=NULL;
	}

	while(cur) {
		snprintf(string, sizeof(string),"COMMAND: (%d) Node::Name=%s Type:%d Content=%s",cur->line,cur->name,cur->type,cur->content);
		logIT(LOG_INFO,string);
		if (xmlIsBlankNode(cur))  {
			cur=cur->next;
			continue;
		}
		if (strcmp((char *)cur->name,"command")==0)  {
			command=getPropertyNode(cur->properties,(xmlChar *)"name");
			protocmd=getPropertyNode(cur->properties,(xmlChar *)"protocmd");
			if (command) { /* neues Command gelesen */
				snprintf(string, sizeof(string),"Neues Command: %s",command);
				logIT(LOG_INFO,string);
				cPtr=newCommandNode(cStartPtr);
				if(!cStartPtr) 
					cStartPtr=cPtr;
				cPtr->nodeType=1; /* keine Kopie, brauchen wir beim loeschen */
				if (command) {
					cPtr->name=calloc(strlen(command)+1,sizeof(char));
					strcpy(cPtr->name,command);
				}
				else
					nullIT(&cPtr->name);
				if(protocmd) {
					cPtr->pcmd=calloc(strlen(protocmd)+1,sizeof(char));
					strcpy(cPtr->pcmd,protocmd);
				}
				else
					nullIT(&cPtr->pcmd);
				commandFound=1;
				prevPtr=cur;
				cur=cur->children;
				continue;
			}
		}
		else if (commandFound && strstr((char *)cur->name,"device"))  {
			id=getPropertyNode(cur->properties,(xmlChar *)"ID");
			protocmd=getPropertyNode(cur->properties,(xmlChar *)"protocmd");
			if (id) { /* neues Device unterhalb von Command  gelesen */
				snprintf(string, sizeof(string),"    Neues Device-Command: %s",id);
				logIT(LOG_INFO,string);
				/* suche device aus der Liste */
				if (!(dPtr=getDeviceNode(dePtr,id))) {
					snprintf(string, sizeof(string),"Device %s nicht definiert (%d)",id,cur->line);
					logIT(LOG_ERR,string);
					return(NULL);
				}
				/* description uebernehmen wir vom command Eintrag */
				strncpy(string,cPtr->description,sizeof(string));
				/* nun rekursiv noch mal parseCommand fuer die Kinder */
				ncPtr=newCommandNode(NULL);
				parseCommand(cur->children,ncPtr,dePtr);
				if (!dPtr->cmdPtr) 
					dPtr->cmdPtr=ncPtr;
				else
					addCommandNode(dPtr->cmdPtr,ncPtr);
				/* decription in neuen Konten referenzieren */
				ncPtr->description=cPtr->description;
				ncPtr->name=cPtr->name;
				/* falls keine Unit angegeben war, kopieren wir sie */
				if (!ncPtr->unit && cPtr->unit) {
					ncPtr->unit=calloc(strlen(cPtr->unit)+1,sizeof(char));
					strcpy(ncPtr->unit,cPtr->unit);
				}
				/* dito mit dem Protokoll Kommando */
				if(protocmd) {
					ncPtr->pcmd=calloc(strlen(protocmd)+1,sizeof(char));
					strcpy(ncPtr->pcmd,protocmd);
				}
				else { 
					ncPtr->pcmd=calloc(strlen(cPtr->pcmd)+1,sizeof(char));
					strcpy(ncPtr->pcmd,cPtr->pcmd);
				}
				ncPtr->nodeType=2; /* 2== Decription, name  wurden kopiert */
				if (cur->next && 
				(!(cur->next->type==XML_TEXT_NODE)|| cur->next->next)) 
					 cur=cur->next;
				else if (prevPtr) 
					cur=prevPtr->next;
				else
					cur=NULL;

			}
		}
		else if (commandFound && strstr((char *)cur->name,"addr"))  {
			chrPtr=getTextNode(cur);	
			snprintf(string, sizeof(string),"   (%d) Node::Name=%s Type:%d Content=%s",cur->line,cur->name,cur->type,chrPtr);
			logIT(LOG_INFO,string);
			if (chrPtr) {
				cPtr->addr=calloc(strlen(chrPtr)+1,sizeof(char));
				strcpy(cPtr->addr,chrPtr);
			}
			else
				nullIT(&cPtr->addr);
			if (cur->next && 
			(!(cur->next->type==XML_TEXT_NODE)|| cur->next->next)) 
				 cur=cur->next;
			else if (prevPtr) 
				cur=prevPtr->next;
			else
				cur=NULL;
		}
		else if (commandFound && strstr((char *)cur->name,"error"))  {
			chrPtr=getTextNode(cur);	
			snprintf(string, sizeof(string),"   (%d) Node::Name=%s Type:%d Content=%s",cur->line,cur->name,cur->type,chrPtr);
			logIT(LOG_INFO,string);
			if (chrPtr) {
				bzero(string,sizeof(string));
				if ((count=string2chr(chrPtr,string,sizeof(string)))){
					cPtr->errStr=calloc(count,sizeof(char));
					memcpy(cPtr->errStr,string,count);
				}
			}
			else
				nullIT(&cPtr->errStr);
			if (cur->next && 
			(!(cur->next->type==XML_TEXT_NODE)|| cur->next->next)) 
				 cur=cur->next;
			else if (prevPtr) 
				cur=prevPtr->next;
			else
				cur=NULL;
		}
		else if (commandFound && strstr((char *)cur->name,"unit"))  {
			chrPtr=getTextNode(cur);	
			snprintf(string, sizeof(string),"   (%d) Node::Name=%s Type:%d Content=%s",cur->line,cur->name,cur->type,chrPtr);
			logIT(LOG_INFO,string);
			if (chrPtr) {
				cPtr->unit=calloc(strlen(chrPtr)+1,sizeof(char));
				strcpy(cPtr->unit,chrPtr);
			}
			else
				nullIT(&cPtr->unit);
			if (cur->next && 
			(!(cur->next->type==XML_TEXT_NODE)|| cur->next->next)) 
				 cur=cur->next;
			else if (prevPtr) 
				cur=prevPtr->next;
			else
				cur=NULL;
		}
		else if (commandFound && (strcmp((char *)cur->name,"precommand")==0))  {
			chrPtr=getTextNode(cur);	
			snprintf(string, sizeof(string),"   (%d) Node::Name=%s Type:%d Content=%s",cur->line,cur->name,cur->type,chrPtr);
			logIT(LOG_INFO,string);
			if (chrPtr) {
				cPtr->precmd=calloc(strlen(chrPtr)+1,sizeof(char));
				strcpy(cPtr->precmd,chrPtr);
			}
			else
				nullIT(&cPtr->precmd);
			if (cur->next && 
			(!(cur->next->type==XML_TEXT_NODE)|| cur->next->next)) 
				 cur=cur->next;
			else if (prevPtr) 
				cur=prevPtr->next;
			else
				cur=NULL;
		}
		else if (commandFound && strstr((char *)cur->name,"description"))  {
			chrPtr=getTextNode(cur);	
			snprintf(string, sizeof(string),"   (%d) Node::Name=%s Type:%d Content=%s",cur->line,cur->name,cur->type,chrPtr);
			logIT(LOG_INFO,string);
			if (chrPtr) {
				cPtr->description=calloc(strlen(chrPtr)+1,sizeof(char));
				strcpy(cPtr->description,chrPtr);
			}
			else
				nullIT(&cPtr->description);
			if (cur->next && 
			(!(cur->next->type==XML_TEXT_NODE)|| cur->next->next)) 
				 cur=cur->next;
			else if (prevPtr) 
				cur=prevPtr->next;
			else
				cur=NULL;
		}
		else if (commandFound && strstr((char *)cur->name,"len"))  {
			chrPtr=getTextNode(cur);	
			snprintf(string, sizeof(string),"   (%d) Node::Name=%s Type:%d Content=%s",cur->line,cur->name,cur->type,chrPtr);
			logIT(LOG_INFO,string);
			if (chrPtr) {
				cPtr->len=atoi(chrPtr);
			}
			if (cur->next && 
			(!(cur->next->type==XML_TEXT_NODE)|| cur->next->next)) 
				 cur=cur->next;
			else if (prevPtr) 
				cur=prevPtr->next;
			else
				cur=NULL;
		}
		else if (commandFound && strstr((char *)cur->name,"bit"))  {
			chrPtr=getTextNode(cur);	
			snprintf(string, sizeof(string),"   (%d) Node::Name=%s Type:%d Content=%s",cur->line,cur->name,cur->type,chrPtr);
			logIT(LOG_INFO,string);
			if (chrPtr) {
				cPtr->bit=atoi(chrPtr);
			}
			if (cur->next && 
			(!(cur->next->type==XML_TEXT_NODE)|| cur->next->next)) 
				 cur=cur->next;
			else if (prevPtr) 
				cur=prevPtr->next;
			else
				cur=NULL;
		}
		else {
			logIT(LOG_ERR,"Fehler beim parsen command");
			return(NULL);
		}
	}
	return(cStartPtr);
}

icmdPtr parseICmd(xmlNodePtr cur) {
	icmdPtr icPtr;
	icmdPtr icStartPtr=NULL;
	char *command;
	char *chrPtr;
	int commandFound=0;
	xmlNodePtr prevPtr;
	char string[1000];

	while(cur) {
		snprintf(string, sizeof(string),"ICMD: (%d) Node::Name=%s Type:%d Content=%s",cur->line,cur->name,cur->type,cur->content);
		logIT(LOG_INFO,string);
		if (xmlIsBlankNode(cur))  {
		/* if (cur->type == XML_TEXT_NODE) { */
			cur=cur->next;
			continue;
		}
		if (strstr((char *)cur->name,"command"))  {
			command=getPropertyNode(cur->properties,(xmlChar *)"name");
			if (command) { /* neues Command gelesen */
				snprintf(string, sizeof(string),"Neues iCommand: %s",command);
				logIT(LOG_INFO,string);
				icPtr=newIcmdNode(icStartPtr);
				if(!icStartPtr) 
					icStartPtr=icPtr;
				icPtr->name=calloc(strlen(command)+1,sizeof(char));
				strcpy(icPtr->name,command);
				commandFound=1;
				prevPtr=cur;
				cur=cur->children;
				continue;
			}
		}
		else if (commandFound && strstr((char *)cur->name,"send"))  {
			chrPtr=getTextNode(cur);	
			snprintf(string, sizeof(string),"   (%d) Node::Name=%s Type:%d Content=%s",cur->line,cur->name,cur->type,chrPtr);
			logIT(LOG_INFO,string);
			if (chrPtr) { 
				icPtr->send=calloc(strlen(chrPtr)+2,sizeof(char));
				strcpy(icPtr->send,chrPtr);
			}
			else
				nullIT(&icPtr->send);
			(cur->next && 
			(!(cur->next->type==XML_TEXT_NODE)|| cur->next->next)) ? (cur=cur->next) : (cur=prevPtr->next);
		}
		else if (commandFound && strstr((char *)cur->name,"retry"))  {
			chrPtr=getTextNode(cur);	
			snprintf(string, sizeof(string),"   (%d) Node::Name=%s Type:%d Content=%s",cur->line,cur->name,cur->type,chrPtr);
			logIT(LOG_INFO,string);
			if (chrPtr) { 
				icPtr->retry=atoi(chrPtr);
			}
			(cur->next && 
			(!(cur->next->type==XML_TEXT_NODE)|| cur->next->next)) ? (cur=cur->next) : (cur=prevPtr->next);
		}
		else if (commandFound && strstr((char *)cur->name,"recvTimeout"))  {
			chrPtr=getTextNode(cur);	
			snprintf(string, sizeof(string),"   (%d) Node::Name=%s Type:%d Content=%s",cur->line,cur->name,cur->type,chrPtr);
			logIT(LOG_INFO,string);
			if (chrPtr) { 
				icPtr->recvTimeout=atoi(chrPtr);
			}
			(cur->next && 
			(!(cur->next->type==XML_TEXT_NODE)|| cur->next->next)) ? (cur=cur->next) : (cur=prevPtr->next);
		}
		else {
			logIT(LOG_ERR,"Fehler beim parsen command");
			return(NULL);
		}
	}
	return(icStartPtr);
}

devicePtr parseDevice(xmlNodePtr cur,protocolPtr pPtr) {
	devicePtr dPtr;
	devicePtr dStartPtr=NULL;
	char *proto;
	char *name;
	char *id;
	xmlNodePtr prevPtr;
	char string[1000];

	while(cur) {
		snprintf(string, sizeof(string),"DEVICE: (%d) Node::Name=%s Type:%d Content=%s",cur->line,cur->name,cur->type,cur->content);
		logIT(LOG_INFO,string);
		if (cur->type == XML_TEXT_NODE) {
			cur=cur->next;
			continue;
		}
		if (strstr((char *)cur->name,"device"))  {
			name=getPropertyNode(cur->properties,(xmlChar *)"name");
			id=getPropertyNode(cur->properties,(xmlChar *)"ID");
			proto=getPropertyNode(cur->properties,(xmlChar *)"protocol");
			if (proto) { /* neues Protocol gelesen */
				snprintf(string, sizeof(string),"    Neues Device: name=%s ID=%s proto=%s",name,id,proto);
				logIT(LOG_INFO,string);
				dPtr=newDeviceNode(dStartPtr);
				if (!dStartPtr) 
					 dStartPtr=dPtr; /* Anker merken */
				if(name) {
					dPtr->name=calloc(strlen(name)+1,sizeof(char));
					strcpy(dPtr->name,name);
				}
				else
					nullIT(&dPtr->name);
				if (id) {
					dPtr->id=calloc(strlen(id)+1,sizeof(char));
					strcpy(dPtr->id,id);
				}
				else
					nullIT(&dPtr->id);
				if (!(dPtr->protoPtr=getProtocolNode(pPtr,proto))) {
					snprintf(string, sizeof(string),"Protokoll %s nicht definiert",proto);
					logIT(LOG_ERR,string);
					return(NULL);
				}
				
			}
			else {
				logIT(LOG_ERR,"Fehler beim parsen device");
				return(NULL);
			}
			prevPtr=cur;
			cur=cur->next;
		}
		else 
			(cur->next && 
			(!(cur->next->type==XML_TEXT_NODE)|| cur->next->next)) ? (cur=cur->next) : (cur=prevPtr->next);
	}

	return(dStartPtr);

}

protocolPtr parseProtocol(xmlNodePtr cur) {
	int protoFound=0;
	protocolPtr protoPtr;
	protocolPtr protoStartPtr=NULL;
	macroPtr mPtr;
	icmdPtr icPtr;
	char *proto;
	char *chrPtr;
	xmlNodePtr prevPtr;
	char string[1000];

	while(cur) {
		snprintf(string, sizeof(string),"PROT: (%d) Node::Name=%s Type:%d Content=%s",cur->line,cur->name,cur->type,cur->content);
		logIT(LOG_INFO,string);
		if (cur->type == XML_TEXT_NODE) {
			cur=cur->next;
			continue;
		}
		if (strstr((char *)cur->name,"protocol"))  {
			proto=getPropertyNode(cur->properties,(xmlChar *)"name");
			if (proto) { /* neues Protocol gelesen */
				snprintf(string, sizeof(string),"Neues Protokoll: %s",proto);
				logIT(LOG_INFO,string);
				protoPtr=newProtocolNode(protoStartPtr);
				if (!protoStartPtr) 
					 protoStartPtr=protoPtr; /* Anker merken */
				if (proto) {
					protoPtr->name=calloc(strlen(proto)+1,sizeof(char));
					strcpy(protoPtr->name,proto);
				}
				else
					nullIT(&protoPtr->name);
			}
			else {
				logIT(LOG_ERR,"Fehler beim parsen proto");
				return(NULL);
			}
			protoFound=1;
			prevPtr=cur;
			cur=cur->children;
		}
		else if (protoFound && strstr((char *)cur->name,"pid"))  {
			chrPtr=getTextNode(cur);
			protoPtr->id = hex2chr(chrPtr);
			(cur->next &&
			(!(cur->next->type==XML_TEXT_NODE)|| cur->next->next)) ? (cur=cur->next) : (cur=prevPtr->next);
		}
		else if (protoFound && strstr(cur->name,"macros"))  {
			mPtr=parseMacro(cur->children);
			if (mPtr) 
				protoPtr->mPtr=mPtr;
			(cur->next && 
			(!(cur->next->type==XML_TEXT_NODE)|| cur->next->next)) ? (cur=cur->next) : (cur=prevPtr->next);
		}
		else if (protoFound && strstr((char *)cur->name,"commands"))  {
			icPtr=parseICmd(cur->children);
			if (icPtr) 
				protoPtr->icPtr=icPtr;
			(cur->next && 
			(!(cur->next->type==XML_TEXT_NODE)|| cur->next->next)) ? (cur=cur->next) : (cur=prevPtr->next);
		}
		else 
			(cur->next && 
			(!(cur->next->type==XML_TEXT_NODE)|| cur->next->next)) ? (cur=cur->next) : (cur=prevPtr->next);
	}
	return(protoStartPtr);
		
}
	

void removeComments(xmlNodePtr node){
	while(node){
		//printf("type:%d name=%s\n",node->type, node->name);
		if(node->children){						// if the node has children process the children
			removeComments(node->children);										
		}
		if(node->type == XML_COMMENT_NODE){		// if the node is a comment?
			//printf("found comment\n");
			if(node->next){						 
				removeComments(node->next);		
			}
			xmlUnlinkNode(node);				//unlink 
			xmlFreeNode(node);					//and free the node
		}
		node=node->next;
	}	
}
	
int parseXMLFile(char *filename) {
	xmlDocPtr doc;
	xmlNodePtr cur,curStart;
	xmlNodePtr prevPtr;
	xmlNsPtr ns;
	char string[1000];
	devicePtr dPtr;
	commandPtr cPtr,ncPtr;
	protocolPtr TprotoPtr=NULL;
	unitPtr TuPtr=NULL;
	devicePtr TdevPtr=NULL;
	commandPtr TcmdPtr=NULL;
	configPtr TcfgPtr=NULL;
	


	xmlKeepBlanksDefault(0); 
	doc=xmlParseFile(filename);
	if (doc == NULL) 
		return(0);
	curStart=xmlDocGetRootElement(doc);
	cur=curStart;
	if (cur == NULL) {
	        snprintf(string, sizeof(string),"empty document\n");
		logIT(LOG_ERR,string);
       		xmlFreeDoc(doc);
        	return(0);
    	}
	ns=xmlSearchNsByHref(doc,cur,(const xmlChar *) "http://www.openv.de/vcontrol");
	if (ns == NULL) {
       		snprintf(string, sizeof(string),"document of the wrong type, vcontrol Namespace not found");
		logIT(LOG_ERR,string);
        	xmlFreeDoc(doc);
        	return(0);
	}
	if (xmlStrcmp(cur->name, (const xmlChar *) "V-Control")) {
		snprintf(string, sizeof(string),"document of the wrong type, root node != V-Control");
		logIT(LOG_ERR,string);
		xmlFreeDoc(doc);
		return(0);
	}
	/* Xinlcude durchfuehren */
	short xc=0;
	if ((xc=xmlXIncludeProcessFlags(doc,XML_PARSE_XINCLUDE|XML_PARSE_NOXINCNODE))==0) {
		snprintf(string, sizeof(string),"Kein XInclude durchgefuehrt");
		logIT(LOG_WARNING,string);
	}
	else if (xc<0) {
		snprintf(string, sizeof(string),"Fehler bei XInclude");
		logIT(LOG_ERR,string);
		return(0);
	}
	else {
		snprintf(string, sizeof(string),"%d XInclude durchgefuehrt",xc);
		logIT(LOG_INFO,string);
	}
		
	removeComments(cur);		// now the xml tree is complete -> remove all comments
	
	int unixFound=0;
	int protocolsFound=0;
	cur=cur->children;
	while (cur) {
		snprintf(string, sizeof(string),"XML: (%d) Node::Name=%s Type:%d Content=%s",cur->line,cur->name,cur->type,cur->content);
		logIT(LOG_INFO,string);
		if (xmlIsBlankNode(cur))  {
			cur=cur->next;
			continue;
		}
		if (strstr((char *)cur->name,"unix"))  {
			if(unixFound) { /* hier duerfen wir nie hinkommen, 2. Durchlauf */
				logIT(LOG_ERR,"Fehler in der XML Konfig");
				return(0);
			}
			prevPtr=cur;
			unixFound=1;
			cur=cur->children; /* unter Unix folgt config , also children */
			continue;
		}
		if (strstr((char *)cur->name,"extern"))  {
			cur=cur->children;
			if (cur && strstr((char *)cur->name,"vito"))  {
				prevPtr=cur;
				cur=cur->children; /* der xinclude Kram steht unter <extern><vito> */
				continue;
			}
			else {
				cur=prevPtr->next;
			}
		}
		else if (strstr((char *)cur->name,"protocols"))  {
			if(protocolsFound) { /* hier duerfen wir nie hinkommen, 2. Durchlauf */
				logIT(LOG_ERR,"Fehler in der XML Konfig");
				return(0);
			}
			protocolsFound=1;
			if (!(TprotoPtr=parseProtocol(cur->children))) 
				return(0);
			cur=cur->next; /* gleiche Ebene wie unix, also next */
			continue;
		}
		else if (strstr((char *)cur->name,"units"))  {
			if (!(TuPtr=parseUnit(cur->children)))
				return(0);
			cur=cur->next; /* gleiche Ebene wie unix, also next */
			continue;
		}
		else if (strstr((char *)cur->name,"commands"))  {
			if (!(TcmdPtr=parseCommand(cur->children,NULL,TdevPtr)))
				return(0);
			/* (cur->next && cur->next->next) ? (cur=cur->next) : (cur=prevPtr->next); */
			(cur->next) ? (cur=cur->next) : (cur=prevPtr->next); 
			continue;
		}
		else if (strstr((char *)cur->name,"devices"))  {
			if (!(TdevPtr=parseDevice(cur->children,TprotoPtr)))
				return(0);
			(cur->next) ? (cur=cur->next) : (cur=prevPtr->next); 
			continue;
		}
		else if (strstr((char *)cur->name,"config")) {
			if(!(TcfgPtr=parseConfig(cur->children)))
				return(0);
			(cur->next && cur->next->next) ? (cur=cur->next) : (cur=prevPtr->next);
		}
		else 
			cur=cur->next;
	}

	/* fuer alle Kommandos, die Default Definitionen haben, grasen wir alle Devices ab
	   und ergaenzen dort die einzelnen Befehle */
	cPtr=TcmdPtr;
	while(cPtr) {
		dPtr=TdevPtr;
		while(dPtr) {
			if (!getCommandNode(dPtr->cmdPtr,cPtr->name)) {
			/* den kennen wir nicht und kopieren die Daten */
				snprintf(string, sizeof(string),"Kopiere Kommando %s nach Device %s",cPtr->name,dPtr->id);
				logIT(LOG_INFO,string);
				ncPtr=newCommandNode(dPtr->cmdPtr);
				if (!dPtr->cmdPtr) 
					dPtr->cmdPtr=ncPtr;
				ncPtr->name=cPtr->name;
				ncPtr->pcmd=cPtr->pcmd;
				ncPtr->addr=cPtr->addr;
				ncPtr->unit=cPtr->unit;
				ncPtr->bit=cPtr->bit;
				ncPtr->errStr=cPtr->errStr;
				ncPtr->precmd=cPtr->precmd;
				ncPtr->description=cPtr->description;
				ncPtr->len=cPtr->len;
			}
			dPtr=dPtr->next;
		}
		cPtr=cPtr->next;
	}
	/* wir suchen das default Devive  */

	if (!(TcfgPtr->devPtr=getDeviceNode(TdevPtr,TcfgPtr->devID))) {
			snprintf(string, sizeof(string),"Device %s nicht definiert\n",TcfgPtr->devID);
			logIT(LOG_ERR,string);
			return(0);
	}
	/* wenn wir hier angekommen sind, war das Laden erfolgreich */
	/* nun die alten Liste freigeben und die neuen zuweisen */

	/* falls wir wiederholt augferufen werden (sighup) sollte alles frei gegeben werden */
	freeAllLists();

	protoPtr=TprotoPtr;
	uPtr=TuPtr;
	devPtr=TdevPtr;
	cmdPtr=TcmdPtr;
	cfgPtr=TcfgPtr;
	
	xmlFreeDoc(doc); 
	return(1);


}


void freeAllLists() {
	removeProtocolList(protoPtr);
	removeUnitList(uPtr);
	removeDeviceList(devPtr);
	removeCommandList(cmdPtr);
	protoPtr=NULL;
	uPtr=NULL;
	devPtr=NULL;
	cmdPtr=NULL;
	if (cfgPtr) {
		free(cfgPtr->tty);
		free(cfgPtr->logfile);
		free(cfgPtr->devID);
		removeAllowList(cfgPtr->aPtr);
		free(cfgPtr);
		cfgPtr=NULL;
	}
}

