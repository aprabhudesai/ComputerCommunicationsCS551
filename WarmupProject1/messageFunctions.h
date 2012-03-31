/*
*	messageFunctions.h: This is the header file for the common
*	functions that are used by the client and the server
*/

char *reqData;
void prepareMsgHeader(uint16_t rType,uint32_t dataLength,unsigned char *msgHeader,uint32_t offset,uint8_t delay,int clntsrvflag);
void displayToScreen(uint32_t dataLength, unsigned char *address,uint16_t type,uint32_t offset,uint8_t delay);
