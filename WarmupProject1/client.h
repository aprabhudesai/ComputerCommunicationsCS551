/*
*	client.h: This is the header file for the client.
*/

int nSocket=0;
int clntdisplayFlag = 0;
char hostName[50];
int portNum;
int reqType;
unsigned char servRespData[11];
uint16_t respType;
uint32_t respoffset;
uint8_t respdelay;
uint32_t respdataLength;
unsigned char respData[512];
unsigned char md5CheckSum[100];
struct hostent *server = NULL;
struct in_addr addr_of_server;
char dataField[512];
unsigned char sendMsg[512] = {'\0'};
uint32_t dataLength;
uint8_t delay;
uint32_t offset;
int dataRecvd=0;
unsigned char serveraddress[16] = {'\0'};
