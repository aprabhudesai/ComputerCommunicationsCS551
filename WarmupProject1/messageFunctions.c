/*
*	messageFunctions.c: This file provides some functions that are used commonly by the client and the server
*/

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <stdint.h>
#include "messageFunctions.h"

/*
*	Function: prepareMsgHeader
*	This function prepares the header of the request/ reply that is to be sent to the server/ client
*/

void prepareMsgHeader(uint16_t rType,uint32_t dataLength,unsigned char *msgHeader,uint32_t offset,uint8_t delay,int clntsrvflag)
{
	rType = htons(rType);
	memcpy(msgHeader,&rType,2);	
	offset = htonl(offset);
	memcpy((msgHeader+2),&offset,4);
	delay = htons(delay);
	memcpy((msgHeader+6),&delay,1);
	//dataLength = strlen(reqData);
	dataLength = htonl(dataLength);
	memcpy((msgHeader+7),&dataLength,4);
	if(clntsrvflag == 0)
		memcpy((msgHeader+11),reqData,dataLength);
}

/*
*	Function: displayToScreen
*	This function is used by the client and the server to display the data revceived on the screen
*	when the "-m" flag is specified
*/

void displayToScreen(uint32_t dataLength,unsigned char *address,uint16_t type,uint32_t offset,uint8_t delay)
{
	printf("\n\tReceived %d bytes from %s",(dataLength+11),address);
	printf("\n\t  MessageType: 0x%04x",type);
	printf("\n\t       Offset: 0x%08x",offset);
	printf("\n\t  ServerDelay: 0x%02x",delay);
	printf("\n\t   DataLength: 0x%08x",dataLength);
}
