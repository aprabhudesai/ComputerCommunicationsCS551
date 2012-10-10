/*------------------------------------------------------------------------------
 *      Name: Aniket Zamwar and Abhishek Prabhudesai
 *      USC ID: 1488-5616-98
 *      Aludra/Nunki ID: zamwar
 *      Aludra/Nunki ID: prabhude
 *
 *      Project: Project #3 - CSCI 551 Spring 2012
 *      Instructor: Bill Cheng
 *      File Description: This section of project consists of all the implementation
 *      				of project logic.
------------------------------------------------------------------------------*/

/*
	Includes all the functions to create chraccter stream from struture.
 	And functions to create structures from incoming character stream data.

 */

#include "systemDataStructures.h"
#include <openssl/sha.h> /* please read this */
#include <openssl/md5.h>
#include <iostream>

using namespace std;
extern NodeInfo ndInfo;
void BroadCastMessage(struct message *);

/*
	OTHER FUNCTIONS USED
*/
void displayUOID(unsigned char *uoidTemp){
	int i = 0;
	while(i < 20){
   		printf("%02x ",*(uoidTemp + i));
    	i++;
	}
	printf("\n");
	return;
}

/*
 * Allocates memeory/ Creates object of structure for the structure of Message Header
 */
struct message* createMessageHeaderStruct(){
	struct message *ptr = NULL;
	ptr = (struct message*)malloc(sizeof(struct message));
	memset(ptr,0,sizeof(struct message));
	ptr->message = NULL;
	ptr->messageStruct = NULL;
	ptr->messageHeader = NULL;
	ptr->connectedNeighborsMapKey = NULL;
	return ptr;
}


void printHeader(struct message* mHeader){
	printf("Header Message Type: \'0x%02x\'\n",mHeader-> messType);
	printf("Header UOID: "); 
	//displayUOID(mHeader-> uoid);
	printf("Header ttl: \'%d\'\n",mHeader-> ttl);
	printf("Header Data Length: 0x\'%d\'\n",mHeader-> dataLength);
	return;
}

struct message* CreateMessageHeader(uint8_t temp_messType,uint8_t temp_ttl,uint32_t temp_dataLength,
			char *temp_messageHeader,char *temp_message,void *temp_messageStruct,char* temp_connectedNeighborsMapKey,
			int temp_myMessage, struct NodeInfo* ndInfoTemp){
			
			time_t systime;
			struct message *messageHeaderObj = createMessageHeaderStruct();
			systime = time(NULL);
			messageHeaderObj->timestamp = systime;

			messageHeaderObj -> messType = temp_messType;
			GetUOID(ndInfoTemp -> nodeInstanceId, (char *)"msg", messageHeaderObj -> uoid, sizeof(messageHeaderObj -> uoid) -1);
			messageHeaderObj -> uoid[21] = '\0';
			messageHeaderObj -> ttl = temp_ttl;  //temporary
			messageHeaderObj -> resv = 0;
			messageHeaderObj -> dataLength = temp_dataLength;
			messageHeaderObj -> myMessage = temp_myMessage;
			messageHeaderObj -> messageHeader = temp_messageHeader; //character Stream
			messageHeaderObj -> message = temp_message; // Body character Stream
			messageHeaderObj -> messageStruct = (void*)temp_messageStruct; // actual message body struct
			messageHeaderObj -> connectedNeighborsMapKey = temp_connectedNeighborsMapKey;
			messageHeaderObj -> howMessageFormed = 's';
			messageHeaderObj -> sendStatus = 0;
			return messageHeaderObj;
}

char* createHeader(struct message* mHeader){

	uint32_t netDataLength;
	char *mHeaderStream;
	char *mHeaderStreamPtr;
	
	mHeaderStream = NULL;
	mHeaderStream = (char *)malloc(27);
	memset(mHeaderStream,0,27);
	mHeaderStreamPtr = mHeaderStream; 
			
	memcpy(mHeaderStream,&mHeader-> messType,1);
	mHeaderStream+=1;

	memcpy(mHeaderStream,&mHeader-> uoid,20);
	mHeaderStream+=20;

	memcpy(mHeaderStream,&mHeader-> ttl,1);
	mHeaderStream+=1;

	memcpy(mHeaderStream,&mHeader-> resv,1);
	mHeaderStream+=1;
	
	netDataLength = (uint32_t)htonl((uint32_t)mHeader-> dataLength);
	memcpy (mHeaderStream,&netDataLength,4);

	return mHeaderStreamPtr;
}

struct message* parseHeader(char *mHeaderStream){

	struct message* mHeader;
	mHeader = createMessageHeaderStruct();

	memcpy(&mHeader-> messType,mHeaderStream,1);
	mHeaderStream+=1;

	memcpy(&mHeader-> uoid,mHeaderStream,20);
	mHeaderStream+=20;
	mHeader -> uoid[20]='\0';

	memcpy(&mHeader-> ttl,mHeaderStream,1);
	mHeaderStream+=1;

	memcpy(&mHeader-> resv,mHeaderStream,1);
	mHeaderStream+=1;

	memcpy(&mHeader-> dataLength,mHeaderStream,4);
	mHeader-> dataLength = (uint32_t)ntohl(mHeader-> dataLength);

	mHeader->myMessage = NOTMYMESSAGE;

	mHeader->sendStatus = 1;

	return mHeader;
	
}

/*---------------------------------------------------------

		JOIN MESSAGE - Create & Parse Structure
_________________________________________________________*/


void printJoinMessage(struct joinMessage* ptr){
		printf("Join Message Data Length \'%d\' \n",ptr -> dataLength);
		printf("Join Message HostLocation \'%d\' \n",ptr -> hostLocation);
		printf("Join Message HostPort \'%d\'\n",ptr -> hostPort);
		printf("JoinMessage Hostname \'%s\'\n",ptr-> hostName);
}

struct joinMessage* createjoinMessageStruct(){
	struct joinMessage *ptr = NULL;
	ptr = (struct joinMessage*)malloc(sizeof(struct joinMessage));
	memset(ptr,0,sizeof(struct joinMessage));
	return ptr;
}

struct joinMessage* createJoin(uint32_t temp_hostLocation,uint16_t temp_hostPort,char *temp_hostName){

		struct joinMessage* ptr = createjoinMessageStruct();
		ptr->hostLocation = temp_hostLocation;
		strcpy(ptr->hostName,temp_hostName);
		ptr->hostPort = temp_hostPort;
		ptr->dataLength = (uint32_t)(6 + strlen(ptr -> hostName));
		return ptr;
}

char *createJoinMessage(struct joinMessage *ptr){

	uint32_t net_hostLocation;
	uint16_t net_hostPort;
	char *buffer;
	char *bufferPtr;
	
	buffer = NULL;
	buffer = (char*)malloc(ptr -> dataLength);
	memset(buffer,0,ptr -> dataLength);
	bufferPtr = buffer;
	
	net_hostLocation = (uint32_t)htonl((uint32_t)ptr -> hostLocation);
	memcpy(buffer,&net_hostLocation,4);
	buffer+=4;
	
	net_hostPort = (uint16_t)htons((uint16_t)ptr -> hostPort);
	memcpy(buffer,&net_hostPort,2);
	buffer+=2;
	
	memcpy(buffer,ptr-> hostName,strlen(ptr -> hostName));
	
	return bufferPtr;
}

struct joinMessage* parseJoinMessage(char *buffer,uint32_t dataLength){
	uint32_t host_hostLocation;
	uint16_t host_hostPort;

	struct joinMessage* ptr;
	ptr = createjoinMessageStruct();
	ptr -> dataLength = dataLength;
	
	memcpy(&host_hostLocation, buffer,4);
	ptr -> hostLocation = (uint32_t)ntohl((uint32_t)host_hostLocation);
	buffer+=4;

	memcpy(&host_hostPort,buffer,2);
	ptr -> hostPort = (uint16_t)ntohs((uint16_t)host_hostPort);
	buffer+=2;

	memcpy(ptr-> hostName,buffer,ptr -> dataLength - 6);
	ptr-> hostName[ptr -> dataLength - 6] = '\0';
	
	return ptr;
}


/* ---------------------------------------------------

				JOIN RESPONSE - Create & Parse
 _________________________________________________________*/

struct joinRespMessage* createjoinRespMessageStruct(){
	struct joinRespMessage *ptr = NULL;
	ptr = (struct joinRespMessage*)malloc(sizeof(struct joinRespMessage));
	memset(ptr,0,sizeof(struct joinRespMessage));
	return ptr;
}

void printjoinRespMessage(struct joinRespMessage* ptr){
	printf("\nJoin Response Message Data Length \'%d\' \n",ptr -> dataLength);
	printf("Join Response Message Distance \'%d\' \n",ptr -> distance);
	printf("Join Response Message HostPort \'%d\'\n",ptr -> hostPort);
	printf("Join Response Message Hostname \'%s\'\n",ptr-> hostName);
	printf("Join Response Message UOID: ");
	displayUOID(ptr-> uoid);
	return;
}

struct joinRespMessage * createJoinRespMessage(struct joinMessage* joinMessageObj, struct message *ptr,struct NodeInfo *ndInfoTemp){

	struct joinRespMessage * joinRespMessageObj = createjoinRespMessageStruct();

	int i = 0;
	while(i<20)
	{
		joinRespMessageObj->uoid[i] = ptr -> uoid[i];
		i++;
	}
	joinRespMessageObj->uoid[i] = '\0';

	joinRespMessageObj -> distance = (uint32_t)abs((int)(joinMessageObj -> hostLocation - ndInfoTemp -> location));
	joinRespMessageObj -> hostPort = ndInfoTemp -> port;
	strcpy(joinRespMessageObj -> hostName,ndInfoTemp -> hostname);
	joinRespMessageObj -> dataLength = (uint32_t)(26 + strlen(joinRespMessageObj -> hostName));

	return joinRespMessageObj;
}

char *createJoinRespMessage(struct joinRespMessage *ptr){
	uint32_t net_distance;
	uint16_t net_hostPort;

	char* buffer;
	char *bufferPtr;

	buffer = NULL;
	buffer = (char*)malloc(ptr -> dataLength);
	memset(buffer,0,ptr->dataLength);
	bufferPtr = buffer;
	
	memcpy(buffer,ptr -> uoid,20);
	buffer+= 20;
	
	net_distance = (uint32_t)htonl((uint32_t)ptr -> distance);
	memcpy(buffer,&net_distance,4);
	buffer+=4;
	
	net_hostPort = (uint16_t)htons((uint16_t)ptr -> hostPort);
	memcpy(buffer,&net_hostPort,2);
	buffer+=2;
	
	memcpy(buffer,ptr -> hostName,strlen(ptr-> hostName));
	
	return bufferPtr;
}

struct joinRespMessage* parseJoinRespMessage(char *buffer,uint32_t dataLength){
	uint32_t host_distance;
	uint16_t host_hostPort;

	struct joinRespMessage* ptr;
	ptr = createjoinRespMessageStruct();
	ptr -> dataLength = dataLength;
	
	memcpy(ptr -> uoid,buffer,20);
	buffer+= 20;
	ptr -> uoid[20] = '\0';

	memcpy(&host_distance,buffer,4);
	ptr -> distance = (uint32_t)ntohl((uint32_t)host_distance);
	buffer+=4;

	memcpy(&host_hostPort,buffer,2);
	ptr -> hostPort = (uint16_t)ntohs((uint16_t)host_hostPort);
	buffer+=2;

	memcpy(ptr -> hostName,buffer,ptr -> dataLength - 26);
	ptr -> hostName[ptr -> dataLength - 26] = '\0';
	
	return ptr;
}

/* ---------------------------------------------------

				HELLO MESSAGE - Create & Parse
 _________________________________________________________*/

struct helloMessage* createhelloMessageStruct(){
	struct helloMessage *ptr = NULL;
	ptr = (struct helloMessage*)malloc(sizeof(struct helloMessage));
	memset(ptr,0,sizeof(struct helloMessage));
	return ptr;
}

void printHelloMessage(struct helloMessage *ptr)
{
	printf("Hello Message HostPort \'%d\'\n",ptr->hostPort);
	printf("HelloMessage Hostname \'%s\'\n",ptr->hostName);
	printf("HelloMessage DataLength \'%d\'\n",ptr->dataLength);
	return;
}

struct helloMessage* createHello(char *tempHostname,uint16_t tempPort){
	struct helloMessage* ptr = createhelloMessageStruct();

	ptr->hostPort = tempPort;
	strcpy(ptr->hostName,tempHostname);
	ptr->dataLength = 2 + strlen(ptr->hostName);
	return ptr;
}

char* createHelloMessage(struct helloMessage* ptr){
	uint16_t net_hostPort;
	char* buffer;
	char *bufferPtr;

	buffer = NULL;
	buffer = (char *)malloc(ptr -> dataLength);
	memset(buffer,0,ptr->dataLength);
	bufferPtr = buffer;	
	
	net_hostPort = (uint16_t)htons((uint16_t)ptr -> hostPort);
	memcpy(buffer,&net_hostPort,2);
	buffer+=2;
	
	memcpy(buffer,ptr -> hostName,strlen(ptr-> hostName));
	
	return bufferPtr;	
}

struct helloMessage* parseHelloMessage(char *buffer,uint32_t dataLength){
	uint16_t host_hostPort;

	struct helloMessage* ptr;
	ptr = createhelloMessageStruct();
	ptr -> dataLength = dataLength;
	
	memcpy(&host_hostPort,buffer,2);
	ptr -> hostPort = (uint16_t)ntohs((uint16_t)host_hostPort);
	buffer+=2;
	
	memcpy(ptr -> hostName,buffer,ptr -> dataLength - 2);
	ptr -> hostName[ptr -> dataLength - 2] = '\0';
	
	return ptr;	
}

/* ---------------------------------------------------

				KeepAlive MESSAGE - Create & Parse
 _________________________________________________________*/

char* createKeepAliveMessage(struct keepAliveMessage *ptr){
	//empty
	return NULL;
}

/* ---------------------------------------------------

				Notify MESSAGE - Create & Parse
 _________________________________________________________*/

struct notifyMessage* createnotifyMessageStruct(){
	struct notifyMessage *ptr = NULL;
	ptr = (struct notifyMessage*)malloc(sizeof(struct notifyMessage));
	memset(ptr,0,sizeof(struct notifyMessage));
	return ptr;
}

void printNotifyMessage(struct notifyMessage* ptr){
	printf("\nNotify Message Error Status: \'%d\'\n",ptr -> errorCode);
	printf("Notify Message  Message Data Length \'%d\' \n",ptr -> dataLength);
	return;
}

struct notifyMessage* createNotify(uint8_t err){
	struct notifyMessage* temp = createnotifyMessageStruct();
	temp -> errorCode = err;
	temp -> dataLength = 1;
	return temp;
}


char* createNotifyMessage(struct notifyMessage *ptr){
	//0: unknown
	//1: user shutdown
	//2: unexpected kill signal received
	//3: self-restart
	char *buffer;
	buffer = NULL;
	buffer = (char *)malloc(ptr -> dataLength);
	memset(buffer,0,ptr->dataLength);

	memcpy(buffer,&ptr -> errorCode,1);
	return buffer;
}

struct notifyMessage* parseNotifyMessage(char *buffer,uint32_t dataLength){
	//0: unknown
	//1: user shutdown
	//2: unexpected kill signal received
	//3: self-restart
	
	struct notifyMessage* ptr;
	ptr = createnotifyMessageStruct();
	ptr -> dataLength = dataLength;
	
	memcpy(&ptr -> errorCode,buffer,1);
	
	return ptr;
}

/* ---------------------------------------------------

				Check MESSAGE - Create & Parse
 _________________________________________________________*/

char * createCheckMessage(struct checkMessage *ptr){
	//emptyMessage
	return NULL;
}


/* ---------------------------------------------------

				CheckResponse MESSAGE - Create & Parse
 _________________________________________________________*/

struct checkRespMessage* createcheckRespMessageStruct(){
	struct checkRespMessage *ptr = NULL;
	ptr = (struct checkRespMessage*)malloc(sizeof(struct checkRespMessage));
	memset(ptr,0,sizeof(struct checkRespMessage));
	return ptr;
}

void printcheckRespMessage(struct checkRespMessage* ptr){
	
	printf("\n\nCheckResponse Message Data Length \'%d\' \n",ptr -> dataLength);
	printf("Check Response Message UOID: ");displayUOID(ptr-> checkMsgHeaderUOID);
	return;
}

struct checkRespMessage* createCheckResp(unsigned char * uoid){
	struct checkRespMessage* temp;
	temp = createcheckRespMessageStruct();
	int i = 0;
	while(i<20)
	{
		*(temp -> checkMsgHeaderUOID + i) = *(uoid + i);
		i++;
	}
	temp->checkMsgHeaderUOID[i] = '\0';
	temp -> dataLength = 20;
	return temp;
}

char* createCheckRespMessage(struct checkRespMessage *ptr){
	
	char *buffer;

	buffer = NULL;
	buffer = (char *)malloc(21);
	memset(buffer,0,21);
	memcpy(buffer,ptr -> checkMsgHeaderUOID,20);
	buffer[21] = '\0';
	
	return buffer;
}

struct checkRespMessage* parseCheckRespMessage(char *buffer,uint32_t dataLength){

	struct checkRespMessage* ptr;
	ptr = createcheckRespMessageStruct();
	ptr -> dataLength = dataLength;
	
	memcpy(ptr -> checkMsgHeaderUOID,buffer,20);
	ptr -> checkMsgHeaderUOID[21] = '\0';
	
	return ptr;
}

/* ---------------------------------------------------

				Status MESSAGE - Create & Parse
 _________________________________________________________*/

struct statusMessage* statusMessageStruct(){
	struct statusMessage* ptr = NULL;
	ptr = (struct statusMessage*)malloc(sizeof(struct statusMessage));
	memset(ptr,0,sizeof(struct statusMessage));
	return ptr;
}

void printstatusMessage(struct statusMessage* ptr){

	printf("\n\nStatus Message Data Length \'%d\' \n",ptr->dataLength);
	printf("Status Message status Type: %d\n\n",ptr->statusType);
	return;
}

struct statusMessage* createStatus(uint8_t temp_statusType){
	// 0x01 means the neighbors information should be sent in the reply
	// 0x02 means the files information should be sent in the reply

	struct statusMessage* temp;
	temp = statusMessageStruct();
	temp->statusType = temp_statusType;
	temp -> dataLength = 1;

	return temp;
}

char* createStatusMessage(struct statusMessage *ptr){

	char *buffer;
	buffer = NULL;
	buffer = (char *)malloc(ptr -> dataLength);
	memset(buffer,0,ptr->dataLength);

	memcpy(buffer,&ptr->statusType,1);

	return buffer;
}

struct statusMessage* parseStatusMessage(char *buffer,uint32_t dataLength){

	struct statusMessage* ptr;
	ptr = statusMessageStruct();
	ptr -> dataLength = dataLength;

	memcpy(&ptr->statusType,buffer,1);

	return ptr;
}

/* ---------------------------------------------------

				Status Response MESSAGE - Create & Parse
 _________________________________________________________*/

struct statusRespMessage* statusRespMessageStruct(){
	struct statusRespMessage* ptr;
	ptr = NULL;
	ptr = (struct statusRespMessage*)malloc(sizeof(struct statusRespMessage));
	memset(ptr,0,sizeof(struct statusRespMessage));
	return ptr;
}

void printstatusRespMessage(struct statusRespMessage* ptr){

	printf("\n\nStatus Response Message Data Length \'%d\' \n",ptr -> dataLength);
	printf("Status Response Message hostInfo Length: %d\n\n",ptr->hostInfoLength);
	printf("Status Response Message hostPort: %d\n\n",ptr->hostPort);
	printf("Status Response Message hostName Length: %s\n\n",ptr->hostName);

	int temp = 0;
	if(ptr->statusType == 0x01){
		while(temp < ptr->dataRecordCount){
			printf("Status Response Message Data Record Size: %d\n\n",ptr->dataRecordSize[temp]);
			printf("Status Response Message consists Neighbor hostPort: %d\n\n",ptr->neighHostPorts[temp]);
			printf("Status Response Message consists Neighbor hostName : %s\n\n",ptr->neighHostNames[temp]);
			temp++;
		}
	}
	else{
		// for File Status Response Message
	}

	return;
}

struct statusRespMessage* createStatusResp(struct NodeInfo* ndInfoTemp,struct message* messageObj){
	struct statusRespMessage *ptr;
	ptr = statusRespMessageStruct();

	struct statusMessage* statusMessageObj = (struct statusMessage *)messageObj->messageStruct;

	if(statusMessageObj->statusType == 0x01){

		ptr->statusType=0x01;
		ptr->dataRecordCount = 0;

		int i=0;
		while(i<20){
			ptr->statusMsgHeaderUOID[i] = messageObj -> uoid[i];
			i++;
		}
		ptr->statusMsgHeaderUOID[i] = '\0';

		ptr->hostInfoLength = 2 + strlen(ndInfoTemp->hostname);
		ptr->hostPort = ndInfoTemp->port;
		strcpy(ptr->hostName,ndInfoTemp->hostname);

		ptr->dataLength = 24 +strlen(ndInfoTemp->hostname);

		i=0;
		struct connectionDetails*connectionDetailsObj;
		pthread_mutex_lock(&connectedNeighborsMapLock);
		connectedNeighborsMapIter = connectedNeighborsMap.begin();
		if(connectedNeighborsMapIter != connectedNeighborsMap.end()){
			while(connectedNeighborsMapIter != connectedNeighborsMap.end()){
				connectionDetailsObj = connectedNeighborsMapIter -> second;
				pthread_mutex_lock(&connectionDetailsObj->connectionLock);
				if(connectionDetailsObj->notOperational == 0 &&
						connectionDetailsObj->isJoinConnection == 0
						&& connectionDetailsObj->threadsExitedCount == 0
						&& connectionDetailsObj->helloStatus == 1){
					ptr->dataRecordCount++;
					ptr->dataRecordSize[i]= 2 + strlen(connectionDetailsObj->hostname);
					ptr->neighHostPorts[i]= connectionDetailsObj -> wellKnownPort;
					strcpy(ptr->neighHostNames[i],connectionDetailsObj->hostname);
					ptr -> dataLength+= 4 + ptr->dataRecordSize[i];
					i++;
				}
				pthread_mutex_unlock(&connectionDetailsObj->connectionLock);
				connectedNeighborsMapIter++;
			}
			ptr->dataRecordSize[i-1] = 0;
		}
		else{
			ptr->dataRecordCount = 0;
		}

		pthread_mutex_unlock(&connectedNeighborsMapLock);
	}
	else{
		//Second Part
		ptr->statusType=0x02;
		ptr->dataRecordCount = GetMyFilesMetaData(ptr->fileMetadataObj);

		int i=0;
		while(i<20){
			ptr->statusMsgHeaderUOID[i] = messageObj -> uoid[i];
			i++;
		}
		ptr->statusMsgHeaderUOID[i] = '\0';

		ptr->hostInfoLength = 2 + strlen(ndInfoTemp->hostname);
		ptr->hostPort = ndInfoTemp->port;
		strcpy(ptr->hostName,ndInfoTemp->hostname);
		ptr->dataLength = 24 +strlen(ndInfoTemp->hostname);

		i=0;
		while(i < ptr->dataRecordCount){
			ptr->dataRecordSize[i] = ptr->fileMetadataObj[i]->metaSize - 20;
			ptr->dataLength = ptr->dataLength + ptr-> dataRecordSize[i] + 4;
			i++;
		}
		if(ptr->dataRecordCount != 0)
			ptr->dataRecordSize[i-1] = 0;
	}
	return ptr;
}



char* createStatusRespMessage(struct statusRespMessage *ptr){

	char *buffer;
	char *bufferPtr;

	uint16_t temp_short;
	uint32_t temp_long;

	buffer = NULL;
	buffer = (char *)malloc(ptr -> dataLength);
	memset(buffer,0,ptr->dataLength);
	bufferPtr = buffer;

	if(ptr->statusType == 0x01){
		memcpy(buffer,ptr->statusMsgHeaderUOID,20);
		buffer+=20;

		temp_short = (uint16_t)htons((uint16_t)ptr->hostInfoLength);
		memcpy(buffer,&temp_short,2);
		buffer+=2;

		temp_short = (uint16_t)htons((uint16_t)ptr->hostPort);
		memcpy(buffer,&temp_short,2);
		buffer+=2;

		memcpy(buffer,ptr->hostName,strlen(ptr->hostName));
		buffer+=strlen(ptr->hostName);

		int i=0;
		if(ptr->dataRecordCount != 0){
			while(i < (ptr->dataRecordCount)-1){

				temp_long=(uint32_t)htonl((uint32_t)ptr->dataRecordSize[i]);
				memcpy(buffer,&temp_long,4);
				buffer+=4;

				temp_short = (uint16_t)htons((uint16_t)ptr->neighHostPorts[i]);
				memcpy(buffer,&temp_short,2);
				buffer+=2;

				memcpy(buffer,ptr->neighHostNames[i],strlen(ptr->neighHostNames[i]));
				buffer+=strlen(ptr->neighHostNames[i]);

				i++;
			}
			temp_long=(uint32_t)htonl((uint32_t)ptr->dataRecordSize[i]);
			memcpy(buffer,&temp_long,4);
			buffer+=4;

			temp_short = (uint16_t)htons((uint16_t)ptr->neighHostPorts[i]);
			memcpy(buffer,&temp_short,2);
			buffer+=2;

			memcpy(buffer,ptr->neighHostNames[i],strlen(ptr->neighHostNames[i]));
		}
	}
	else{
		//Second Part of Project
		memcpy(buffer,ptr->statusMsgHeaderUOID,20);
		buffer+=20;

		temp_short = (uint16_t)htons((uint16_t)ptr->hostInfoLength);
		memcpy(buffer,&temp_short,2);
		buffer+=2;

		temp_short = (uint16_t)htons((uint16_t)ptr->hostPort);
		memcpy(buffer,&temp_short,2);
		buffer+=2;

		memcpy(buffer,ptr->hostName,strlen(ptr->hostName));
		buffer+=strlen(ptr->hostName);

		if(ptr->dataRecordCount != 0){

			int i=0;
			uint32_t templong;
			char CRLF[4] = "\r\n";

			while(i < ptr->dataRecordCount){

				// Copy Next Length
				templong= (uint32_t)htonl((uint32_t)ptr->dataRecordSize[i]);
				memcpy(buffer,&templong,4);
				buffer+=4;

				// Copy Meta Data contents
				//Metadata Format:
				//FileName<CR><LF>FileSize<CR><LF>SHA1<CR><LF>Nonce<CR><LF>keywords<CR><LF>
				//Bitvector<CR><LF>

				//-- FileName  ------------------------ //
				memcpy(buffer,ptr->fileMetadataObj[i]->fileName,strlen((char*)ptr->fileMetadataObj[i]->fileName));
				buffer+=strlen((char*)ptr->fileMetadataObj[i]->fileName);
				//-- <CR><LF>
				memcpy(buffer,CRLF,2);
				buffer+=2;

				//-- File Size   ------------------------ //
				templong = (uint32_t)htonl((uint32_t)ptr->fileMetadataObj[i]->fileSize);
				memcpy(buffer,&templong,4);
				buffer+=4;
				//-- <CR><LF>
				memcpy(buffer,CRLF,2);
				buffer+=2;

				//-- File SHA1   ------------------------ //
				memcpy(buffer,ptr->fileMetadataObj[i]->SHA1val,20);
				buffer+=20;
				//-- <CR><LF>
				memcpy(buffer,CRLF,2);
				buffer+=2;

				//-- Nonce   ------------------------ //
				memcpy(buffer,ptr->fileMetadataObj[i]->nonce,20);
				buffer+=20;
				//-- <CR><LF>
				memcpy(buffer,CRLF,2);
				buffer+=2;

				//-- keywords ------------------------ //
				memcpy(buffer,ptr->fileMetadataObj[i]->keywordsWithSpace,strlen((char*)ptr->fileMetadataObj[i]->keywordsWithSpace));
				buffer+=strlen((char*)ptr->fileMetadataObj[i]->keywordsWithSpace);
				//-- <CR><LF>
				memcpy(buffer,CRLF,2);
				buffer+=2;

				//-- bitVector ------------------------ //
				memcpy(buffer,ptr->fileMetadataObj[i]->bitVector,128);
				buffer+=128;
				//-- <CR><LF>
				memcpy(buffer,CRLF,2);
				buffer+=2;

				i++;
			}

		}

	}
	return bufferPtr;
}

struct statusRespMessage* parseStatusRespMessage(char *buffer,uint32_t dataLength, uint8_t temp_statusType){

	struct statusRespMessage* ptr;
	ptr = statusRespMessageStruct();
	ptr -> dataLength = dataLength;

	uint32_t remLength = dataLength;

	if(temp_statusType == 0x01){
		uint32_t dataRead = 0;
		ptr->statusType = 0x01;
		ptr->dataRecordCount = 0;

		memcpy(ptr->statusMsgHeaderUOID,buffer,20);
		buffer+=20;
		dataRead = 20;


		memcpy(&(ptr->hostInfoLength),buffer,2);
		ptr->hostInfoLength = (uint16_t)ntohs((uint16_t)ptr->hostInfoLength);
		buffer+=2; dataRead+=2;


		memcpy(&(ptr->hostPort),buffer,2);
		ptr->hostPort = (uint16_t)ntohs((uint16_t)ptr->hostPort);
		buffer+=2;dataRead+=2;

		memcpy(ptr->hostName,buffer,ptr->hostInfoLength - 2);
		ptr->hostName[ptr->hostInfoLength - 2] = '\0';
		buffer+=strlen(ptr->hostName);dataRead+=strlen(ptr->hostName);

		remLength = dataLength - dataRead;
		int i=0;
		while(dataRead < ptr->dataLength && remLength != 0){

			memcpy(&ptr->dataRecordSize[i],buffer,4);
			ptr->dataRecordSize[i] = (uint32_t)ntohl((uint32_t)ptr->dataRecordSize[i]);
			buffer+=4;dataRead+=4;
			remLength-=4;

			memcpy(&(ptr->neighHostPorts[i]),buffer,2);
			ptr->neighHostPorts[i] = (uint16_t)ntohs((uint16_t)ptr->neighHostPorts[i]);
			buffer+=2;dataRead+=2;
			remLength-=2;

			if(ptr->dataRecordSize[i] != 0){
				memcpy(ptr->neighHostNames[i],buffer,(int)(ptr->dataRecordSize[i] - 2));
				ptr->neighHostNames[i][ptr->dataRecordSize[i] - 2] = '\0';
				dataRead+=strlen(ptr->neighHostNames[i]);
				remLength-=(uint32_t)strlen(ptr->neighHostNames[i]);
				buffer+=strlen(ptr->neighHostNames[i]);
				i++;
			}
			else{
				memcpy(ptr->neighHostNames[i],buffer,remLength);
				ptr->neighHostNames[i][remLength] = '\0';
				i++;
				break;
			}
		}
		ptr->dataRecordCount = i;
	}
	else{
		//second part of project
		uint32_t dataRead = 0;
		ptr->statusType = 0x02;
		ptr->dataRecordCount = 0;

		memcpy(ptr->statusMsgHeaderUOID,buffer,20);
		buffer+=20;
		dataRead = 20;

		memcpy(&(ptr->hostInfoLength),buffer,2);
		ptr->hostInfoLength = (uint16_t)ntohs((uint16_t)ptr->hostInfoLength);
		buffer+=2; dataRead+=2;


		memcpy(&(ptr->hostPort),buffer,2);
		ptr->hostPort = (uint16_t)ntohs((uint16_t)ptr->hostPort);
		buffer+=2;dataRead+=2;

		memcpy(ptr->hostName,buffer,ptr->hostInfoLength - 2);
		ptr->hostName[ptr->hostInfoLength - 2] = '\0';
		buffer+=strlen(ptr->hostName);dataRead+=strlen(ptr->hostName);

		remLength = dataLength - dataRead;
		int i=0;
		uint32_t templong;
		unsigned char tempChar;
		while(dataRead < dataLength){

			int j = 0;
			// Copy Next Length
			memcpy(&ptr->dataRecordSize[i],buffer,4);
			templong = (uint32_t)ntohl((uint32_t)ptr->dataRecordSize[i]);
			ptr->dataRecordSize[i] = templong;
			buffer+=4;dataRead+=4;

			// Copy Meta Data contents
			//Metadata Format:
			//FileName<CR><LF>FileSize<CR><LF>SHA1<CR><LF>Nonce<CR><LF>keywords<CR><LF>
			//Bitvector<CR><LF>

			struct fileMetadata *recvMeta = createMetadataStruct();
			ptr->fileMetadataObj[i] = recvMeta;
			ptr->dataRecordCount = i + 1;
			recvMeta->metaSize = 20;

			do{
				memcpy(&tempChar,buffer,1);
				recvMeta->fileName[j] = tempChar;
				j++;
				buffer=buffer+1;dataRead+=1;
			}while(tempChar != '\r');
			recvMeta->fileName[j-1] = '\0';
			recvMeta->metaSize+=strlen((char*)recvMeta->fileName);

			// skip '\n'
			buffer = buffer + 1;dataRead+=1;

			memcpy(&recvMeta->fileSize,buffer,4);
			templong = (uint32_t)ntohl((uint32_t)recvMeta->fileSize);
			recvMeta->fileSize = templong;
			buffer+=4;dataRead+=4;
			recvMeta->metaSize+=4;

			//skip CR and LF
			buffer+=2;dataRead+=2;
			memcpy(recvMeta->SHA1val,buffer,20);
			buffer+=20;dataRead+=20;
			recvMeta->metaSize+=20;
			//skip CR and LF
			buffer+=2;dataRead+=2;

			memcpy(recvMeta->nonce,buffer,20);
			buffer+=20;dataRead+=20;
			recvMeta->metaSize+=20;

			//skip CR and LF
			buffer+=2;dataRead+=2;

			j=0;
			do{
				memcpy(&tempChar,buffer,1);
				recvMeta->keywordsWithSpace[j] = tempChar;
				j++;
				buffer=buffer+1;dataRead+=1;
			}while(tempChar != '\r');
			recvMeta->keywordsWithSpace[j-1] = '\0';
			recvMeta->metaSize+=strlen((char*)recvMeta->keywordsWithSpace);
			parseKeywords(recvMeta);

			// skip '\n'
			buffer = buffer + 1;dataRead+=1;

			//-- bitVector ------------------------ //
			memcpy(recvMeta->bitVector,buffer,128);
			buffer+=128;dataRead+=128;
			recvMeta->metaSize+=128;

			recvMeta->metaSize+=12;
			//skip CR and LF
			buffer+=2;dataRead+=2;

			i++;
		}
	}
	return ptr;
}

struct statusRespMessage * createSelfStatus(struct NodeInfo *ndInfoTemp,uint8_t statusType)
{
	struct statusRespMessage *ptr;
		ptr = statusRespMessageStruct();

		if(statusType == 0x01){

			ptr->statusType=0x01;
			ptr->dataRecordCount = 0;

			ptr->hostInfoLength = 2 + strlen(ndInfoTemp->hostname);
			ptr->hostPort = ndInfoTemp->port;
			strcpy(ptr->hostName,ndInfoTemp->hostname);

			ptr->dataLength = 24 +strlen(ndInfoTemp->hostname);

			int i=0;
			struct connectionDetails*connectionDetailsObj;
			pthread_mutex_lock(&connectedNeighborsMapLock);

			connectedNeighborsMapIter = connectedNeighborsMap.begin();
				while(connectedNeighborsMapIter != connectedNeighborsMap.end()){
					connectionDetailsObj = connectedNeighborsMapIter -> second;

					pthread_mutex_lock(&connectionDetailsObj->connectionLock);
					if(connectionDetailsObj->notOperational == 0 && connectionDetailsObj->isJoinConnection == 0
						&& connectionDetailsObj->helloStatus == 1 && connectionDetailsObj->threadsExitedCount == 0){
						ptr->dataRecordCount++;
						ptr->dataRecordSize[i]= 2 + strlen(connectionDetailsObj->hostname);
						ptr->neighHostPorts[i]= connectionDetailsObj -> wellKnownPort;
						strcpy(ptr->neighHostNames[i],connectionDetailsObj->hostname);
						ptr -> dataLength+= 4 + ptr->dataRecordSize[i];
						i++;
					}
					pthread_mutex_unlock(&connectionDetailsObj->connectionLock);
					connectedNeighborsMapIter++;
				}
				ptr->dataRecordSize[i-1] = 0;
			pthread_mutex_unlock(&connectedNeighborsMapLock);
		}
		else{
			//Second Part
			ptr->statusType=0x02;
			ptr->dataRecordCount = GetMyFilesMetaData(ptr->fileMetadataObj);

			ptr->hostInfoLength = 2 + strlen(ndInfoTemp->hostname);
			ptr->hostPort = ndInfoTemp->port;
			strcpy(ptr->hostName,ndInfoTemp->hostname);
			ptr->dataLength = 24 +strlen(ndInfoTemp->hostname);

			int i=0;
			while(i < ptr->dataRecordCount){
				ptr->dataRecordSize[i] = ptr->fileMetadataObj[i]->metaSize - 20;
				ptr->dataLength = ptr->dataLength + ptr-> dataRecordSize[i] + 4;
				i++;
			}
			if(ptr->dataRecordCount != 0)
				ptr->dataRecordSize[i-1] = 0;
		}
		return ptr;
}

//---------------------------- PART 2 STARTS HERE -------------------------------------------------------

struct fileMetadata* createMetadataStruct(){
	struct fileMetadata* ptr;
	ptr = NULL;
	ptr = (struct fileMetadata*)malloc(sizeof(struct fileMetadata));
	memset(ptr,0,sizeof(struct fileMetadata));

	ptr->SHA1val[20]='\0';
	ptr->password[20]='\0';
	ptr->fileId[20]='\0';
	ptr->fileName[0]='\0';
	ptr->nonce[20]='\0';
	ptr->keywordsWithSpace[0]='\0';
	ptr->fileWithPath[0]='\0';
	return ptr;
}


/*
 * Search Message Functions
 *
 */

struct searchMessage* searchMessageStruct(){
	struct searchMessage* ptr;
	ptr = NULL;

	ptr = (struct searchMessage*)malloc(sizeof(struct searchMessage));
	memset(ptr,0,sizeof(struct searchMessage));
	return ptr;
}

void printSearchMessage(struct searchMessage *ptr){

	printf("\nSearch Message Type: \'%d\'\n",ptr->searchType);
	printf("Search Message Query: \'%s\'",ptr->query);
	printf("Search Message dataLength: \'%d\'",ptr->dataLength);
}


/*
 *   Pass the full input query string to this function.
 *   This function traverses the string and creates the array.
 *
 */
struct searchMessage* createSearch(unsigned char *queryString, int searchType){

	struct searchMessage* temp = searchMessageStruct();

	temp->searchType = searchType;

	if(searchType == 2){
		 int i=0;
		 while (*queryString != '\0') {
		        if (*queryString > '9')
		        	temp->query[i] = ((tolower(*queryString) - 'a') + 10) << 4;
		        else
		        	temp->query[i] = (*queryString - '0') << 4;

		        queryString++;
		        if (*queryString > '9')
		        	temp->query[i] |= tolower(*queryString) - 'a' + 10;
		        else
		        	temp->query[i] |= *queryString - '0';
		        queryString++;
		        i++;
		    }

		temp->dataLength = 1 + 20;
	}
	else{
		int i=0;
		while(*(queryString + i) != '\0'){
			temp->query[i] = *(queryString + i);
			i++;
		}
		temp->query[i] = '\0';

		temp->dataLength = 1 + strlen((char*)temp->query);
	}
	return temp;
}


char * createSearchMessage(struct searchMessage *ptr){
	char *buffer;
	char *bufferPtr;
	buffer = NULL;
	buffer = (char *)malloc(ptr -> dataLength);
	memset(buffer,0,ptr->dataLength);
	bufferPtr = buffer;

	memcpy(buffer,&ptr->searchType,1);
	buffer+=1;

	memcpy(buffer,ptr->query,strlen((char*)ptr->query));

	return bufferPtr;
}


struct searchMessage *parseSearchMessage(char *buffer,uint32_t dataLength){
	struct searchMessage* temp = searchMessageStruct();

	temp->dataLength = dataLength;

	memcpy(&temp->searchType,buffer,1);
	buffer+=1;

	memcpy(temp->query,buffer,temp->dataLength - 1);
	temp->query[temp->dataLength-1] = '\0';

	return temp;
}

//--------------------- Search Message Ends here


/*
 *
 * SEARCH RESPONSE MESSAGE
 */

struct searchRespMessage * createSearchRespMessageStruct(){

	struct searchRespMessage* ptr;
	ptr = NULL;

	ptr = (struct searchRespMessage*)malloc(sizeof(struct searchRespMessage));
	memset(ptr,0,sizeof(struct searchRespMessage));
	return ptr;
}


struct searchRespMessage *createSearchResp(struct fileMetadata *metadataPtr[], int metaCount, struct message *obj){

	struct searchRespMessage *temp = createSearchRespMessageStruct();

	int i=0;
	while(i<20){
		temp->searchUOID[i] = obj->uoid[i];
		i++;
	}
	temp->searchUOID[i] = '\0';
	temp->dataLength = 20;

	temp->respCount = metaCount; // metaCount has to be more than or at the most equal to 1
	i = 0;

	while(i < metaCount){

		if((metaCount - 1) == i){ // last element or only one element
			temp->nextlength[i] = 0;
			temp->respMetadata[i] = metadataPtr[i];
			temp->dataLength+= 4 + metadataPtr[i]->metaSize; // nextLength 4 bytes + (FileID+Metadata)
			break;
		}
		else{ // other than last element
			temp->nextlength[i] = metadataPtr[i]->metaSize - 20; //subtract FileID Size from total
			temp->respMetadata[i] = metadataPtr[i];
			temp->dataLength+= 4 + metadataPtr[i]->metaSize; // nextLength 4 bytes + (FileID+Metadata)
		}
		i++;
	}

	return temp;
}


char * createSearchRespMessage(struct searchRespMessage *ptr){

	char *buffer = NULL;
	char *bufferPtr;

	uint32_t templong;
	unsigned char CRLF[4] = "\r\n";

	buffer = (char*)malloc(ptr->dataLength);
	memset(buffer,0,ptr->dataLength);

	bufferPtr = buffer;

	memcpy(buffer,ptr->searchUOID,20);
	buffer = buffer + 20;

	int i=0;

	while(i < ptr->respCount){

		// Copy Next Length
		templong= (uint32_t)htonl((uint32_t)ptr->nextlength[i]);
		memcpy(buffer,&templong,4);
		buffer+=4;

		// File Id
		memcpy(buffer, ptr->respMetadata[i]->fileId,20);
		buffer+=20;

		// Copy Meta Data contents
		//Metadata Format:
		//FileName<CR><LF>FileSize<CR><LF>SHA1<CR><LF>Nonce<CR><LF>keywords<CR><LF>
		//Bitvector<CR><LF>

		//-- FileName  ------------------------ //
		memcpy(buffer,ptr->respMetadata[i]->fileName,strlen((char*)ptr->respMetadata[i]->fileName));
		buffer+=strlen((char*)ptr->respMetadata[i]->fileName);
		//-- <CR><LF>
		memcpy(buffer,CRLF,2);
		buffer+=2;

		//-- File Size   ------------------------ //
		templong = (uint32_t)htonl((uint32_t)ptr->respMetadata[i]->fileSize);
		memcpy(buffer,&templong,4);
		buffer+=4;
		//-- <CR><LF>
		memcpy(buffer,CRLF,2);
		buffer+=2;

		//-- File SHA1   ------------------------ //
		memcpy(buffer,ptr->respMetadata[i]->SHA1val,20);
		buffer+=20;
		//-- <CR><LF>
		memcpy(buffer,CRLF,2);
		buffer+=2;

		//-- Nonce   ------------------------ //
		memcpy(buffer,ptr->respMetadata[i]->nonce,20);
		buffer+=20;
		//-- <CR><LF>
		memcpy(buffer,CRLF,2);
		buffer+=2;

		//-- keywords ------------------------ //
		memcpy(buffer,ptr->respMetadata[i]->keywordsWithSpace,strlen((char*)ptr->respMetadata[i]->keywordsWithSpace));
		buffer+=strlen((char*)ptr->respMetadata[i]->keywordsWithSpace);
		//-- <CR><LF>
		memcpy(buffer,CRLF,2);
		buffer+=2;


		//-- bitVector ------------------------ //
		memcpy(buffer,ptr->respMetadata[i]->bitVector,128);
		buffer+=128;
		//-- <CR><LF>
		memcpy(buffer,CRLF,2);
		buffer+=2;

		i++;
	}

	return bufferPtr;
}

void parseKeywords(struct fileMetadata* ptr){
	unsigned char *pch;

	int i = 0;
	ptr->keyWordCount = 0;

	pch = ptr->keywordsWithSpace;

	while(*pch != '\0'){

		if(*pch == ' '){
			ptr->keywords[ptr->keyWordCount][i++] = '\0';
			ptr->keyWordCount+=1;
			pch++;
			i=0;
		}

		ptr->keywords[ptr->keyWordCount][i++] = *pch;
		pch++;
	}
	ptr->keywords[ptr->keyWordCount][i++]='\0';
	ptr->keyWordCount+=1;
}

struct searchRespMessage* parseSearchRespMessage(char *buffer,uint32_t dataLength){

	struct searchRespMessage *ptr = createSearchRespMessageStruct();

	ptr->dataLength = dataLength;

	int i = 0;
	int j = 0;
	uint32_t dataRead = 0;
	uint32_t templong;

	unsigned char tempChar;

	memcpy(ptr->searchUOID,buffer,20);
	buffer = buffer + 20;
	dataRead = 20;

	while(dataRead < dataLength){

		j = 0;
		// Copy Next Length
		memcpy(&ptr->nextlength[i],buffer,4);
		templong = (uint32_t)ntohl((uint32_t)ptr->nextlength[i]);
		ptr->nextlength[i] = templong;
		buffer+=4;dataRead+=4;

		// Copy Meta Data contents
		//Metadata Format:
		//FileName<CR><LF>FileSize<CR><LF>SHA1<CR><LF>Nonce<CR><LF>keywords<CR><LF>
		//Bitvector<CR><LF>
		struct fileMetadata *recvMeta = createMetadataStruct();
		ptr->respMetadata[i] = recvMeta;
		ptr->respCount = i + 1;
		recvMeta->metaSize = 20;
		// File Id
		memcpy(recvMeta->fileId,buffer,20);
		buffer+=20;dataRead+=20;


		do{
			memcpy(&tempChar,buffer,1);
			recvMeta->fileName[j] = tempChar;
			j++;
			buffer=buffer+1;dataRead+=1;
		}while(tempChar != '\r');
		recvMeta->fileName[j-1] = '\0';
		recvMeta->metaSize+=strlen((char*)recvMeta->fileName);

		// skip '\n'
		buffer = buffer + 1;dataRead+=1;

		memcpy(&recvMeta->fileSize,buffer,4);
		templong = (uint32_t)ntohl((uint32_t)recvMeta->fileSize);
		recvMeta->fileSize = templong;
		buffer+=4;dataRead+=4;
		recvMeta->metaSize+=4;

		//skip CR and LF
		buffer+=2;dataRead+=2;
		memcpy(recvMeta->SHA1val,buffer,20);
		buffer+=20;dataRead+=20;
		recvMeta->metaSize+=20;
		//skip CR and LF
		buffer+=2;dataRead+=2;

		memcpy(recvMeta->nonce,buffer,20);
		buffer+=20;dataRead+=20;
		recvMeta->metaSize+=20;

		//skip CR and LF
		buffer+=2;dataRead+=2;

		j=0;
		do{
			memcpy(&tempChar,buffer,1);
			recvMeta->keywordsWithSpace[j] = tempChar;
			j++;
			buffer=buffer+1;dataRead+=1;
		}while(tempChar != '\r');
		recvMeta->keywordsWithSpace[j-1] = '\0';
		recvMeta->metaSize+=strlen((char*)recvMeta->keywordsWithSpace);
		parseKeywords(recvMeta);

		// skip '\n'
		buffer = buffer + 1;dataRead+=1;

		//-- bitVector ------------------------ //
		memcpy(recvMeta->bitVector,buffer,128);
		buffer+=128;dataRead+=128;
		recvMeta->metaSize+=128;

		recvMeta->metaSize+=12;
		//skip CR and LF
		buffer+=2;dataRead+=2;

		i++;
	}

	return ptr;
}
//----------------------- SEARCH RESPONSE MESSAGE ENDS HERE ----------------------------


/*
 *  GET MESSAGE
 */

struct getMessage * createGetMessageStruct(){
	struct getMessage* ptr;
	ptr = NULL;

	ptr = (struct getMessage*)malloc(sizeof(struct getMessage));
	memset(ptr,0,sizeof(struct getMessage));
	return ptr;
}

// Command
// get 2 <FileName>
// give this function the 2nd metadata from search Response i.e. @index 1
// if any filename mentioned in the command or if no File Name in command send NULL
struct getMessage *createGet(struct fileMetadata *getFileMeta,unsigned char *fileName){

	struct getMessage *ptr =  createGetMessageStruct();
	int i=0;
	if(*fileName == '\0')
		fileName = getFileMeta->fileName;

	while(*fileName != '\0'){
		 ptr -> fileName[i] = *fileName;
		fileName++;i++;
	}
	ptr -> fileName[i] = '\0';

	ptr->dataLength = 40;

	memcpy(ptr->fileUOID,getFileMeta->fileId,20);
	memcpy(ptr->fileSHA,getFileMeta->SHA1val,20);

	return ptr;
}

char* createGetMessage(struct getMessage *temp){

 	char* buffer = NULL;
 	char* bufferPtr = NULL;

 	buffer = (char *)malloc(temp->dataLength);
 	memset(buffer,0,temp->dataLength);
	bufferPtr = buffer;

 	memcpy(buffer,temp->fileUOID,20);
 	buffer+=20;
	memcpy(buffer,temp->fileSHA,20);

 	return bufferPtr;
 }

struct getMessage *parseGetMessage(char *buffer, uint32_t dataLength){
	struct getMessage *ptr;
	ptr = createGetMessageStruct();

	ptr->dataLength = dataLength;

	memcpy(ptr->fileUOID,buffer,20);
 	buffer+=20;
	memcpy(ptr->fileSHA,buffer,20);

	return ptr;
}

//-------------------------------------------- GET Message ENDS here

/*
 *   GET Response Message
 */
struct getRespMessage* createGetRespMessageStruct(){
	struct getRespMessage* ptr = NULL;
	ptr = (struct getRespMessage*) malloc(sizeof(struct getRespMessage));
	memset(ptr,0,sizeof(struct getRespMessage));

	return ptr;
}

struct getRespMessage* createGetResp(struct fileMetadata *fileMeta,struct message *messObj){

	struct stat fileBuf;
	int exists;
	exists = stat((char*)fileMeta->fileWithPath, &fileBuf);

	if(exists < 0){
		//write error to file
		return NULL;
	}

	struct getRespMessage* ptr = createGetRespMessageStruct();

	memcpy(ptr->getUOID,messObj->uoid,20);
	ptr->metaLength = fileMeta->metaSize - 20;
	ptr->respMetadata = fileMeta;

	ptr->dataLength = 20 + 4 + ptr->metaLength + fileMeta->fileSize;
	ptr->nonFileLength = 20 + 4 + ptr->metaLength;

	return ptr;
}

char * createGetRespMessage(struct getRespMessage * ptr){

	char *buffer;
	char * bufferPtr;
	char CRLF[4]="\r\n";
	buffer = (char *)malloc(ptr->nonFileLength);
	memset(buffer,0,ptr->nonFileLength);
	bufferPtr = buffer;

	memcpy(buffer,ptr->getUOID,20);
	buffer+=20;

	uint32_t templong;
	templong = (uint32_t)htonl((uint32_t)ptr->metaLength);
	memcpy(buffer,&templong,4);
	buffer+=4;


	// Copy Meta Data contents
	//Metadata Format:
	//FileName<CR><LF>FileSize<CR><LF>SHA1<CR><LF>Nonce<CR><LF>keywords<CR><LF>
	//Bitvector<CR><LF>

	//-- FileName  ------------------------ //
			memcpy(buffer,ptr->respMetadata->fileName,strlen((char*)ptr->respMetadata->fileName));
			buffer+=strlen((char*)ptr->respMetadata->fileName);
			//-- <CR><LF>
			memcpy(buffer,CRLF,2);
			buffer+=2;

			//-- File Size   ------------------------ //
			templong = (uint32_t)htonl((uint32_t)ptr->respMetadata->fileSize);
			memcpy(buffer,&templong,4);
			buffer+=4;
			//-- <CR><LF>
			memcpy(buffer,CRLF,2);
			buffer+=2;

			//-- File SHA1   ------------------------ //
			memcpy(buffer,ptr->respMetadata->SHA1val,20);
			buffer+=20;
			//-- <CR><LF>
			memcpy(buffer,CRLF,2);
			buffer+=2;

			//-- Nonce   ------------------------ //
			memcpy(buffer,ptr->respMetadata->nonce,20);
			buffer+=20;
			//-- <CR><LF>
			memcpy(buffer,CRLF,2);
			buffer+=2;

			//-- keywords ------------------------ //
			memcpy(buffer,ptr->respMetadata->keywordsWithSpace,strlen((char*)ptr->respMetadata->keywordsWithSpace));
			buffer+=strlen((char*)ptr->respMetadata->keywordsWithSpace);
			//-- <CR><LF>
			memcpy(buffer,CRLF,2);
			buffer+=2;

			//-- bitVector ------------------------ //
			memcpy(buffer,ptr->respMetadata->bitVector,128);
			buffer+=128;
			//-- <CR><LF>
			memcpy(buffer,CRLF,2);

	return bufferPtr;
}

struct getRespMessage * parseGetRespMessage(char *buffer,uint32_t dataLength){

	struct getRespMessage *ptr = createGetRespMessageStruct();
	int j;
	unsigned char tempChar;
	uint32_t templong;

	memcpy(ptr->getUOID,buffer,20);
	buffer+=20;

	memcpy(&templong,buffer,4);
	ptr->metaLength = (uint32_t)htonl((uint32_t)templong);
	buffer+=4;

	// Copy Meta Data contents
	//Metadata Format:
	//FileName<CR><LF>FileSize<CR><LF>SHA1<CR><LF>Nonce<CR><LF>keywords<CR><LF>
	//Bitvector<CR><LF>
	struct fileMetadata *recvMeta = createMetadataStruct();

	recvMeta->metaSize = 20;
	//File Id -- Dont get in message so create one for it
	GetUOID(ndInfo.nodeInstanceId, (char *)"fileid",recvMeta->fileId,20);

	j=0;
	do{
		memcpy(&tempChar,buffer,1);
		recvMeta->fileName[j] = tempChar;
		j++;
		buffer=buffer+1;
	}while(tempChar != '\r');
	recvMeta->fileName[j-1] = '\0';
	recvMeta->metaSize+=strlen((char*)recvMeta->fileName);

	// skip '\n'
	buffer = buffer + 1;

	memcpy(&recvMeta->fileSize,buffer,4);
	templong = (uint32_t)ntohl((uint32_t)recvMeta->fileSize);
	recvMeta->fileSize = templong;
	buffer+=4;
	recvMeta->metaSize+=4;

	//skip CR and LF
	buffer+=2;
	memcpy(recvMeta->SHA1val,buffer,20);
	buffer+=20;
	recvMeta->metaSize+=20;
	//skip CR and LF
	buffer+=2;

	memcpy(recvMeta->nonce,buffer,20);
	buffer+=20;
	recvMeta->metaSize+=20;

	//skip CR and LF
	buffer+=2;

	j=0;
	do{
		memcpy(&tempChar,buffer,1);
		recvMeta->keywordsWithSpace[j] = tempChar;
		j++;
		buffer=buffer+1;
	}while(tempChar != '\r');
	recvMeta->keywordsWithSpace[j-1] = '\0';
	recvMeta->metaSize+=strlen((char*)recvMeta->keywordsWithSpace);
	parseKeywords(recvMeta);

	// skip '\n'
	buffer = buffer + 1;

	//-- bitVector ------------------------ //
	memcpy(recvMeta->bitVector,buffer,128);
	buffer+=128;
	recvMeta->metaSize+=128;

	recvMeta->metaSize+=12;
	//skip CR and LF
	buffer+=2;

	//recvMeta->fileSize = dataLength - 4 - 20 - ptr->metaLength;
	ptr->respMetadata = recvMeta;

	ptr->nonFileLength = 20 + 4 + ptr->metaLength;

	return ptr;
}


//--------------------------------------------- GET RESPONSE ENDS here -----------------------

/*
 *
 *   STORE Message
 */

struct storeMessage * createStoreMessageStruct(){
	struct storeMessage *ptr;
	ptr = NULL;
	ptr = (struct storeMessage *)malloc(sizeof(struct storeMessage));
	memset(ptr,0,sizeof(struct storeMessage));
	return ptr;
}

struct storeMessage *createStore(struct fileMetadata *fileMeta){

	struct stat fileBuf;
	int exists;
	exists = stat((char*)fileMeta->fileWithPath, &fileBuf);

	if(exists < 0){
		//write error to file
		return NULL;
	}

	struct storeMessage* ptr = createStoreMessageStruct();

	ptr->metaLength = fileMeta->metaSize - 20;
	ptr->nonFileLength = 4 + ptr->metaLength;
	ptr->dataLength = 4 + ptr->metaLength + fileMeta->fileSize;

	ptr->respMetadata = fileMeta;

	return ptr;
}

char * createStoreMessage(struct storeMessage *ptr){


	char *buffer;
	char * bufferPtr;
	char CRLF[4]="\r\n";
	buffer = (char *)malloc(ptr->nonFileLength);
	memset(buffer,0,ptr->nonFileLength);
	bufferPtr = buffer;

	int templong;
	templong = (uint32_t)htonl((uint32_t)ptr->metaLength);
	memcpy(buffer,&templong,4);
	buffer+=4;


	// Copy Meta Data contents
	//Metadata Format:
	//FileName<CR><LF>FileSize<CR><LF>SHA1<CR><LF>Nonce<CR><LF>keywords<CR><LF>
	//Bitvector<CR><LF>

	//-- FileName  ------------------------ //
	memcpy(buffer,ptr->respMetadata->fileName,strlen((char*)ptr->respMetadata->fileName));
	buffer+=strlen((char*)ptr->respMetadata->fileName);
	//-- <CR><LF>
	memcpy(buffer,CRLF,2);
	buffer+=2;

	//-- File Size   ------------------------ //
	templong = (uint32_t)htonl((uint32_t)ptr->respMetadata->fileSize);
	memcpy(buffer,&templong,4);
	buffer+=4;
	//-- <CR><LF>
	memcpy(buffer,CRLF,2);
	buffer+=2;

	//-- File SHA1   ------------------------ //
	memcpy(buffer,ptr->respMetadata->SHA1val,20);
	buffer+=20;
	//-- <CR><LF>
	memcpy(buffer,CRLF,2);
	buffer+=2;

	//-- Nonce   ------------------------ //
	memcpy(buffer,ptr->respMetadata->nonce,20);
	buffer+=20;
	//-- <CR><LF>
	memcpy(buffer,CRLF,2);
	buffer+=2;

	//-- keywords ------------------------ //
	memcpy(buffer,ptr->respMetadata->keywordsWithSpace,strlen((char*)ptr->respMetadata->keywordsWithSpace));
	buffer+=strlen((char*)ptr->respMetadata->keywordsWithSpace);
	//-- <CR><LF>
	memcpy(buffer,CRLF,2);
	buffer+=2;


	//-- bitVector ------------------------ //
	memcpy(buffer,ptr->respMetadata->bitVector,128);
	buffer+=128;
	//-- <CR><LF>
	memcpy(buffer,CRLF,2);

	return bufferPtr;
}

struct storeMessage * parseStoreMessage(char *buffer,uint32_t dataLength){

	struct storeMessage *ptr = createStoreMessageStruct();
	int j;
	unsigned char tempChar;
	uint32_t templong;
	memcpy(&templong,buffer,4);
	ptr->metaLength = (uint32_t)ntohl((uint32_t)templong);
	buffer+=4;

	// Copy Meta Data contents
	//Metadata Format:
	//FileName<CR><LF>FileSize<CR><LF>SHA1<CR><LF>Nonce<CR><LF>keywords<CR><LF>
	//Bitvector<CR><LF>

	struct fileMetadata *recvMeta = createMetadataStruct();

	recvMeta->metaSize = 20;
	// File Id -- Dont get in message so create one for it
	GetUOID(ndInfo.nodeInstanceId, (char *)"fileid",recvMeta->fileId,20);

	j=0;
	do{
		memcpy(&tempChar,buffer,1);
		recvMeta->fileName[j] = tempChar;
		j++;
		buffer=buffer+1;
	}while(tempChar != '\r');
	recvMeta->fileName[j-1] = '\0';
	recvMeta->metaSize+=strlen((char*)recvMeta->fileName);

	// skip '\n'
	buffer = buffer + 1;

	memcpy(&recvMeta->fileSize,buffer,4);
	templong = (uint32_t)ntohl((uint32_t)recvMeta->fileSize);
	recvMeta->fileSize = templong;
	buffer+=4;
	recvMeta->metaSize+=4;

	//skip CR and LF
	buffer+=2;
	memcpy(recvMeta->SHA1val,buffer,20);
	buffer+=20;
	recvMeta->metaSize+=20;
	//skip CR and LF
	buffer+=2;

	memcpy(recvMeta->nonce,buffer,20);
	buffer+=20;
	recvMeta->metaSize+=20;

	//skip CR and LF
	buffer+=2;

	j=0;
	do{
		memcpy(&tempChar,buffer,1);
		recvMeta->keywordsWithSpace[j] = tempChar;
		j++;
		buffer=buffer+1;
	}while(tempChar != '\r');
	recvMeta->keywordsWithSpace[j-1] = '\0';
	recvMeta->metaSize+=strlen((char*)recvMeta->keywordsWithSpace);
	parseKeywords(recvMeta);

	// skip '\n'
	buffer = buffer + 1;

	//-- bitVector ------------------------ //
	memcpy(recvMeta->bitVector,buffer,128);
	buffer+=128;
	recvMeta->metaSize+=128;

	recvMeta->metaSize+=12;
	//skip CR and LF
	buffer+=2;

	ptr->respMetadata = recvMeta;

	ptr->metaLength = recvMeta->metaSize - 20;
	ptr->nonFileLength = 4 + ptr->metaLength;
	ptr->dataLength = dataLength;


	return ptr;
}

// DELETE MESSAGE ---------------------------------------------------------------------------------

struct deleteMessage * createDeleteMessageStruct(){
	struct deleteMessage* ptr = NULL;
	ptr = (struct deleteMessage*) malloc(sizeof(struct deleteMessage));
	memset(ptr,0,sizeof(struct deleteMessage));

	return ptr;
}

//buffer = "FileName=foo SHA1=6b6c... Nonce=fe18...\0"  // null teminated Schar array
// Lines Separated by <CR><LF>
struct deleteMessage *createDelete(unsigned char *buffer){
	struct deleteMessage *ptr = createDeleteMessageStruct();

	unsigned char *pch;
	int i=0;

	pch = (unsigned char *)strchr((char *)buffer,'=');
	pch = pch + 1;

	while(*pch != ' '){
		ptr->fileName[i++]=*pch;
		pch++;
	}
	ptr->fileName[i]='\0';

	i=0;
	pch = (unsigned char *)strchr((char *)pch,'=');
	pch = pch + 1;
	while(*pch != ' '){
		if (*pch > '9')
			ptr->fileSHA[i] = ((tolower(*pch) - 'a') + 10) << 4;
		else
			ptr->fileSHA[i] = (*pch - '0') << 4;

		pch++;
		if (*pch > '9')
			ptr->fileSHA[i] |= tolower(*pch) - 'a' + 10;
		else
			ptr->fileSHA[i] |= *pch - '0';
		pch++;
		i++;
	}
	ptr->fileSHA[i]='\0';

	i=0;
	pch = (unsigned char *)strchr((char *)pch,'=');
	pch = pch + 1;
	while(*pch != '\0'){
		if (*pch > '9')
			ptr->nonce[i] = ((tolower(*pch) - 'a') + 10) << 4;
		else
			ptr->nonce[i] = (*pch - '0') << 4;
		pch++;
		if (*pch > '9')
			ptr->nonce[i] |= tolower(*pch) - 'a' + 10;
		else
			ptr->nonce[i] |= *pch - '0';
		pch++;
		i++;
	}
	ptr->nonce[i]='\0';

	ptr->password[0]='\0';
	ptr->dataLength = strlen((char *)ptr->fileName) + 60 + 8;

	return ptr;
}

/*
 * 	FileName=foo
  	SHA1=63de...
  	Nonce=fcca...
  	Password=bac9...
	The lines are separated by <CR><LF> (i.e., "\r\n").
 */
char * createDeleteMessage(struct deleteMessage *ptr){

	char *buffer;
	char *bufferPtr;
	unsigned char CRLF[4] = "\r\n";

	buffer = (char *)malloc(ptr->dataLength);
	memset(buffer,0,ptr->dataLength);
	bufferPtr = buffer;

	//File Name
	memcpy(buffer,ptr->fileName,strlen((char*)ptr->fileName));
	buffer+=strlen((char*)ptr->fileName);

	//CRLF
	memcpy(buffer,CRLF,2);
	buffer+=2;

	//SHA1
	memcpy(buffer,ptr->fileSHA,20);
	buffer+=20;

	//CRLF
	memcpy(buffer,CRLF,2);
	buffer+=2;

	// Nonce
	memcpy(buffer,ptr->nonce,20);
	buffer+=20;

	//CRLF
	memcpy(buffer,CRLF,2);
	buffer+=2;

	// Password
	memcpy(buffer,ptr->password,20);
	buffer+=20;

	//CRLF
	memcpy(buffer,CRLF,2);
	buffer+=2;

	return bufferPtr;
}


struct deleteMessage * parseDeleteMessage(char *buffer, uint32_t dataLength){
	struct deleteMessage *ptr = createDeleteMessageStruct();

	ptr->dataLength = dataLength;

	unsigned char tempChar;
	//File Name
	int j=0;
	do{
		memcpy(&tempChar,buffer,1);
		ptr->fileName[j] = tempChar;
		j++;
		buffer=buffer+1;
	}while(tempChar != '\r');
	ptr->fileName[j-1] = '\0';

	//Ignore LF
	buffer+=1;

	//SHA1
	memcpy(ptr->fileSHA,buffer,20);
	ptr->fileSHA[20] = '\0';
	buffer+=20;

	//CRLF
	buffer+=2;

	// Nonce
	memcpy(ptr->nonce,buffer,20);
	ptr->nonce[20] = '\0';
	buffer+=20;

	//CRLF
	buffer+=2;

	// Password
	memcpy(ptr->password,buffer,20);
	ptr->password[20] = '\0';
	buffer+=20;


	return ptr;
}

// Parse the received character buffer stream to respective structure

bool parseMessage(struct message *ptr){


	if(ptr -> message == NULL)
		return false;


	switch(ptr -> messType){
		
		case JOIN:
					{
						ptr -> messageStruct = (void *) parseJoinMessage(ptr -> message,ptr -> dataLength);
						if(ptr -> messageStruct == NULL)
							return false;
							
					}
					break;
		case RJOIN:
					{
						ptr -> messageStruct = (void *) parseJoinRespMessage(ptr -> message,ptr -> dataLength);
						if(ptr -> messageStruct == NULL)
							return false;
					}
					break;
			
		case HELLO:
					{
						ptr -> messageStruct = (void *) parseHelloMessage(ptr -> message,ptr -> dataLength);
						if(ptr -> messageStruct == NULL)
							return false;
					}
					break;
			
		case KEEPALIVE:
					{
						ptr -> messageStruct = NULL;
						//it does not has any Message body, only has message header.
						return true;
					}
					break;
			
		case NOTIFY:
					{
						ptr -> messageStruct = (void *) parseNotifyMessage(ptr -> message,ptr -> dataLength);
						if(ptr -> messageStruct == NULL)
							return false;
					}
					break;
			
		case CHECK:
					{
						ptr -> messageStruct = NULL;
						//it does not has any Message body, only has message header.
						return true;
					}
					break;
			
		case RCHECK:
					{
						ptr -> messageStruct = (void *) parseCheckRespMessage(ptr -> message,ptr -> dataLength);
						if(ptr -> messageStruct == NULL)
							return false;
					}
					break;
			
		case STATUS:
					{
							// not implemented yet.
						ptr -> messageStruct = (void *) parseStatusMessage(ptr -> message,ptr -> dataLength);
						if(ptr -> messageStruct == NULL)
							return false;
					}
					break;
			
		case RSTATUS:
					{
							// not implemented yet.
						char statusUOID[21]="\0";
						uint8_t statusType;
						memcpy(statusUOID,(char*)ptr -> message,20);
						string uoidKey = string(statusUOID);

						pthread_mutex_lock(&messageCacheMapLock);
						messageCacheMapIter = messageCacheMap.find(uoidKey);
						if(messageCacheMapIter == messageCacheMap.end())
						{
							ptr -> messageStruct = NULL;
							return false;
						}
						struct statusMessage* sentStatusMessage = (struct statusMessage*)messageCacheMapIter->second->messageStruct;
						statusType = sentStatusMessage->statusType;
						pthread_mutex_unlock(&messageCacheMapLock);


						ptr -> messageStruct = (void *) parseStatusRespMessage(ptr -> message,ptr -> dataLength,statusType);
						if(ptr -> messageStruct == NULL)
							return false;
					}
					break;
		// Project Part 2

		case SEARCH:
					{
						ptr -> messageStruct = (void *) parseSearchMessage(ptr -> message,ptr -> dataLength);
						if(ptr -> messageStruct == NULL)
							return false;
					}
					break;
			
		case RSEARCH:
					{
						ptr -> messageStruct = (void *) parseSearchRespMessage(ptr -> message,ptr -> dataLength);
						if(ptr -> messageStruct == NULL)
							return false;
					}
					break;
			
		case DELETE:
					{
						ptr -> messageStruct = (void *) parseDeleteMessage(ptr -> message,ptr -> dataLength);
						if(ptr -> messageStruct == NULL)
							return false;
					}
					break;
			
		case STORE:
					{
						// not implemented yet.
						ptr -> messageStruct = (void *) parseStoreMessage(ptr -> message,ptr -> dataLength);
						if(ptr -> messageStruct == NULL)
							return false;
					}
					break;
			
		case GET:
					{
						ptr -> messageStruct = (void *) parseGetMessage(ptr -> message,ptr -> dataLength);
						if(ptr -> messageStruct == NULL)
							return false;
					}
					break;	

		case RGET:
					{
						ptr -> messageStruct = (void *) parseGetRespMessage(ptr -> message,ptr -> dataLength);
						if(ptr -> messageStruct == NULL)
							return false;
					}
					break;
			
		default: return false;
			
	}

	return true;

}


bool createMessageBuffer(struct message *ptr){
	
	switch(ptr -> messType){
		
		case JOIN:
					{
						ptr -> message = createJoinMessage((struct joinMessage *)ptr -> messageStruct);
						if(ptr -> message == NULL)
							return false;
							
					}
					break;
		case RJOIN:
					{
						ptr -> message = createJoinRespMessage((struct joinRespMessage *)ptr -> messageStruct);
						if(ptr -> message == NULL)
							return false;
					}
					break;
			
		case HELLO:
					{
						ptr -> message = createHelloMessage((struct helloMessage *)ptr -> messageStruct);
						if(ptr -> message == NULL)
							return false;
					}
					break;
			
		case KEEPALIVE:
					{
						ptr -> message = NULL;
						return true;
						//it does not has any Message body, only has message header.
					}
					break;
			
		case NOTIFY:
					{
						ptr -> message = createNotifyMessage((struct notifyMessage *)ptr -> messageStruct);
						if(ptr -> message == NULL)
							return false;
					}
					break;
			
		case CHECK:
					{
						ptr -> message = NULL;
						return true;
						//it does not has any Message body, only has message header.
					}
					break;
			
		case RCHECK:
					{
						ptr -> message = createCheckRespMessage((struct checkRespMessage *)ptr -> messageStruct);
						if(ptr -> message == NULL)
							return false;
					}
					break;
			
		case STATUS:
					{
						ptr -> message = createStatusMessage((struct statusMessage *)ptr -> messageStruct);
						if(ptr -> message == NULL)
							return false;
					}
					break;
			
		case RSTATUS:
					{
						ptr -> message = createStatusRespMessage((struct statusRespMessage *)ptr -> messageStruct);
						if(ptr -> message == NULL)
							return false;
					}
					break;
		// Project Part 2

		case SEARCH:
					{
						ptr -> message = createSearchMessage((struct searchMessage *)ptr -> messageStruct);
						if(ptr -> message == NULL)
							return false;
					}
					break;
			
		case RSEARCH:
					{
						ptr -> message = createSearchRespMessage((struct searchRespMessage *)ptr -> messageStruct);
						if(ptr -> message == NULL)
							return false;
					}
					break;
			
		case DELETE:
					{
						ptr -> message = createDeleteMessage((struct deleteMessage *)ptr -> messageStruct);
						if(ptr -> message == NULL)
							return false;
					}
					break;
			
		case STORE:
					{
						ptr -> message = createStoreMessage((struct storeMessage *)ptr -> messageStruct);
						if(ptr -> message == NULL)
						return false;
					}
					break;
			
		case GET:
					{
						ptr -> message = createGetMessage((struct getMessage *)ptr -> messageStruct);
						if(ptr -> message == NULL)
							return false;
					}
					break;	

		case RGET:
					{
						ptr -> message = createGetRespMessage((struct getRespMessage *)ptr -> messageStruct);
						if(ptr -> message == NULL)
							return false;
					}
					break;
						
		default: return false;
			
	}

	return true;		
}

//================================================================================================
//============================ New Functions for Part #2 =========================================
//================================================================================================

void CreateBitVector(struct fileMetadata *fileMeta){

	int keywordCount = 0;

	memset(fileMeta->bitVector,0,128);

	while(keywordCount < fileMeta->keyWordCount){

		unsigned char keyword[60] = "\0";

		memcpy(keyword,fileMeta->keywords[keywordCount],strlen((char*)fileMeta->keywords[keywordCount]));
		keywordCount++;
		unsigned char md5out[16]="\0";
		unsigned char sha1out[20]="\0";

		unsigned char sha1char[3] = "\0";
		unsigned char md5char[3] = "\0";
		unsigned char temp = 0x00001;

		SHA1(keyword,strlen((char *)keyword),sha1out);
		MD5(keyword,strlen((char *)keyword),md5out);

		sha1char[1] = sha1out[19];
		sha1char[0] = sha1out[18] & temp;

		md5char[1] = md5out[15];
		md5char[0] = md5out[14] & temp;

		uint16_t md5val = 0,sha1val = 0;

		md5val = md5val | md5char[0];
		md5val = (md5val << 8);
		md5val = md5val | md5char[1];

		sha1val = sha1val | sha1char[0];
		sha1val = (sha1val << 8);
		sha1val = sha1val | sha1char[1];

		sha1val = sha1val + 512;
		unsigned char tempo;

		int rem, index;
		rem = sha1val % 8;
		index = sha1val / 8;

		tempo = 0x01;
		tempo = tempo << rem;
		fileMeta->bitVector[index] = fileMeta->bitVector[index] | tempo;

		rem = md5val % 8;
		index = md5val / 8;

		tempo = 0x01;
		tempo = tempo << rem;
		fileMeta->bitVector[index] = fileMeta->bitVector[index] | tempo;
	}
}


struct fileMetadata* createMetadataAndFiles(unsigned char *fileName,unsigned char *keywordsWithSpace,
								int currentCount,unsigned char *sha1out, uint32_t fileSize){

	struct fileMetadata* fileMeta;

	// Create .meta, .pass, .fileid "File Names"
	char metaFile[256]="\0";
	char passFile[256]="\0";
	char fileidFile[256]="\0";
    char metaInit[]="[metadata]\n";

	int i = 0;
	unsigned char tempChar[2048]="\0";
	char CRLF[3]="\r\n";
	string insertKey;
	strcpy(metaFile,ndInfo.homeDir);
		sprintf((char *)tempChar,"/files/%d.meta",currentCount);
		strcat(metaFile,(char *)tempChar);

	strcpy(passFile,ndInfo.homeDir);
		sprintf((char *)tempChar,"/files/%d.pass",currentCount);
		strcat(passFile,(char *)tempChar);

		strcpy(fileidFile,ndInfo.homeDir);
			sprintf((char *)tempChar,"/files/%d.fileid",currentCount);
			strcat(fileidFile,(char *)tempChar);

	FILE *metaFP;	FILE *passFP; 	FILE *fileidFP;

	fileMeta = createMetadataStruct();

	// Create File ID
	GetUOID(ndInfo.nodeInstanceId, (char *)"fileid",fileMeta->fileId,20);

	// Create Password
	GetUOID(ndInfo.nodeInstanceId, (char *)"pass",fileMeta->password,20);

	SHA1(fileMeta->password, 20 , fileMeta->nonce);

	//convert keywords to lower and copy keywordwithspace
	while(*(keywordsWithSpace + i) != '\0'){
		*(keywordsWithSpace + i) = tolower(*(keywordsWithSpace + i));
		i++;
	}

	memcpy(fileMeta->keywordsWithSpace,keywordsWithSpace,strlen((char*)keywordsWithSpace));
	fileMeta->keywordsWithSpace[strlen((char*)keywordsWithSpace)]='\0';

	//copy File's SHA1
	memcpy(fileMeta->SHA1val,sha1out,20);

	//copy Filename
	memcpy(fileMeta->fileName,fileName,strlen((char*)fileName));
	fileMeta->fileName[strlen((char*)fileName)]='\0';

	//copy FileSize
	fileMeta->fileSize = fileSize;

	//parse Keywords into an array
	parseKeywords(fileMeta);

	CreateBitVector(fileMeta);

	//Write Contents to File

	metaFP = fopen(metaFile,"w+");

	if(metaFP == NULL){
		printf("\nError Creating %s Meta File\n",metaFile);
		return NULL;
	}
	// Write [metadata]
	fwrite(metaInit,1,11,metaFP);

	//Write FileName + CRLF
	sprintf((char *)tempChar,"FileName=%s",fileMeta->fileName);
	fwrite(tempChar,1,strlen((char *)tempChar),metaFP);
	fwrite(CRLF,1,2,metaFP);

	//Write File Size + CRLF
	sprintf((char *)tempChar,"FileSize=%d",fileMeta->fileSize);
	fwrite(tempChar,1,strlen((char *)tempChar),metaFP);
	fwrite(CRLF,1,2,metaFP);

	//Write SHA of File + CRLF
	fprintf(metaFP,"%s","SHA1=");
	i=0;
	while(i<20){
		fprintf(metaFP,"%02x",fileMeta->SHA1val[i]);
		i++;
	}
	fwrite(CRLF,1,2,metaFP);

	//Write Nonce + CRLF
	fprintf(metaFP,"%s","Nonce=");
	i=0;
	while(i<20){
		fprintf(metaFP,"%02x",fileMeta->nonce[i]);
		i++;
	}
	fwrite(CRLF,1,2,metaFP);

	//Write keywords + CRLF
	fprintf(metaFP,"Keywords=%s",fileMeta->keywordsWithSpace);
	fwrite(CRLF,1,2,metaFP);

	//Write Bit Vector and CRLF
	fprintf(metaFP,"%s","Bit-vector=");
	i = 127;
	while(i>=0){
		fprintf(metaFP,"%02x",fileMeta->bitVector[i]);
		i--;
	}
	fwrite(CRLF,1,2,metaFP);

	fclose(metaFP);

	passFP = fopen(passFile,"w+");

	if(passFP == NULL){
			printf("\nError Creating %s Meta File\n",passFile);
			return NULL;
		}

	i=0;
	while(i<20){
		fprintf(passFP,"%02x",fileMeta->password[i]);
		i++;
	}
	fclose(passFP);

	fileidFP = fopen(fileidFile,"w+");
	if(fileidFP == NULL){
				printf("\nError Creating %s Meta File\n",fileidFile);
				return NULL;
			}

	i=0;
	while(i<20){
		fprintf(fileidFP,"%02x",fileMeta->fileId[i]);
		i++;
	}
	fclose(fileidFP);

	// = File Name + 4 of file Size + 20 SHA1 + 20 nonce +
	// keywords length including space + 128 bit Vector + file ID 20 + 12 bytes for all <CR> and <LF>
	// DO NOT Include keywordCount, fileType size in this size
	//*** Include <CR><LF> for all entries in the meta Size (there are 6 <CR><LF> in total = 12 bytes)
	fileMeta->metaSize = strlen((char*)fileName) + 4 + 20 +20 + strlen((char*)keywordsWithSpace) + 128 + 20 + 12;

	return fileMeta;
}

int handleDeleteCommand(struct deleteMessage *myDeleteMsg)
{
	int status = 2;
	int i=0,fileMatch = 0,fileNo = 0;
	char filekey[256] = "\0";
	while(myDeleteMsg->fileName[i] != '\0')
	{
		filekey[i] = myDeleteMsg->fileName[i];
		i++;
	}
	filekey[i] = '\0';

	string fileKeyForMap = string(filekey);
	pthread_mutex_lock(&fileNameIndexMapLock);
	fileNameIndexMapIter = fileNameIndexMap.find(fileKeyForMap);
	if(fileNameIndexMapIter == fileNameIndexMap.end())
	{
		//I don't have the file
		pthread_mutex_unlock(&fileNameIndexMapLock);
		return 0;
	}
	else
	{
		fileNameIndexIt = fileNameIndexMap.equal_range(fileNameIndexMapIter->first.c_str());
		for(fileNameIndexITIter = fileNameIndexIt.first; fileNameIndexITIter != fileNameIndexIt.second ; fileNameIndexITIter++)
		{
			fileMatch = checkForFileWithNonceAndSHA1(myDeleteMsg,fileNameIndexITIter->second,0);
			if(fileMatch == 1 || fileMatch == 2)
			{
				fileNo = fileNameIndexITIter->second;
				break;
			}
		}
		pthread_mutex_unlock(&fileNameIndexMapLock);
		if(fileMatch == 1)
		{
			//I have the file with its password

			removeFileEntryFromIndexes(fileNo);

			pthread_mutex_lock(&permFileListLock);
			for(PermFileListIter = PermFileList.begin(); PermFileListIter != PermFileList.end() ; PermFileListIter++)
			{
				if(*PermFileListIter == fileNo)
				{
					PermFileList.erase(PermFileListIter);
					break;
				}
			}
			pthread_mutex_unlock(&permFileListLock);

			char permFileName[256] = "\0";
			memset(permFileName,0,256);
			sprintf(permFileName,"%s/files/%d.meta",ndInfo.homeDir,fileNo);
			remove(permFileName);

			memset(permFileName,0,256);
			sprintf(permFileName,"%s/files/%d.data",ndInfo.homeDir,fileNo);
			remove(permFileName);

			memset(permFileName,0,256);
			sprintf(permFileName,"%s/files/%d.fileid",ndInfo.homeDir,fileNo);
			remove(permFileName);

			memset(permFileName,0,256);
			sprintf(permFileName,"%s/files/%d.pass",ndInfo.homeDir,fileNo);
			remove(permFileName);
			return 2;
		}
		else if(fileMatch == 2)
		{
			//File Present but no password i.e. I am not the owner of the file
			pthread_mutex_unlock(&fileNameIndexMapLock);
			return 1;
		}
		else if(fileMatch == 0)
		{
			pthread_mutex_unlock(&fileNameIndexMapLock);
			return 0;
		}

	}
	pthread_mutex_unlock(&fileNameIndexMapLock);
	return status;
}

bool handleStoreCommand(unsigned char *filename, uint8_t ttl, unsigned char *keywordsWithSpace){

	char toBeStoredDataFile[256]="\0";
	char tempChar[100]="\0";

	struct NodeInfo * ndInfoTemp;
	ndInfoTemp = &ndInfo;
	strcpy(toBeStoredDataFile,ndInfo.homeDir);
	int currentCount = getNextFileIndex();

	sprintf(tempChar,"/files/%d",currentCount);
	strcat(tempChar,".data");
	strcat(toBeStoredDataFile,tempChar);

	FILE *originalFP;
	FILE *dataFP;

	originalFP = fopen((char*)filename,"r");
	dataFP = fopen(toBeStoredDataFile,"w+");

	if(originalFP == NULL){
		printf("\nFile %s not found: STORE Command\n",filename);
		return false;
	}
	if(dataFP == NULL){
			printf("\nFile %s not found: STORE Command\n",toBeStoredDataFile);
			return false;
	}
	/*
	 *  SHA1 Declarations
	 */
	unsigned char sha1out[SHA_DIGEST_LENGTH];
	unsigned char *sha1val;
	SHA_CTX sha1obj;

	// SHA1 Initialization
	sha1val = &sha1out[0];
	SHA1_Init(&sha1obj);

	// Copy File Contents from Original to new File.
	// Calculate SHA1 for the file.
	unsigned char fileContent[1000] = "\0";

	while (!feof(originalFP)) {

		memset(fileContent,0,1000);
		int readSize = 0;
		readSize = fread(fileContent,1,sizeof(fileContent),originalFP);
		if(readSize > 0){
			fwrite (fileContent, 1,readSize,dataFP);
			SHA1_Update(&sha1obj,fileContent,readSize);
		}
		else{
			break; //Done
		}
	}
	SHA1_Final(sha1val, &sha1obj);
	fclose(originalFP);
	fclose(dataFP);

	uint32_t fileSize;
	struct stat fileBuf;
	int exists;
	exists = stat(toBeStoredDataFile, &fileBuf);

	if(exists < 0){
		//write error to Log file
		printf("\nFile Does not exist\n");
		return false;
	}

	fileSize = fileBuf.st_size;

	struct fileMetadata* fileMeta;
	fileMeta = createMetadataAndFiles(filename,keywordsWithSpace,currentCount,sha1out,fileSize);
	strcpy(fileMeta->fileWithPath,toBeStoredDataFile);


	/*Add File to Permanent File List*/
	pthread_mutex_lock(&permFileListLock);
	PermFileListIter = PermFileList.end();
	PermFileList.insert(PermFileListIter,currentCount);
	pthread_mutex_unlock(&permFileListLock);

	insertFileEntryIntoIndex(fileMeta,currentCount);

	if(ttl > 0){
		struct storeMessage *myStoreMsg = createStore(fileMeta);

		struct message *mymessage;

		mymessage = CreateMessageHeader(STORE, /*Mess Type*/
										ttl, /* TTL */
										myStoreMsg -> dataLength, /*DataLength Message Body */
										NULL, /*char *messageHeader*/
										NULL, /*char *message*/
										(void*)myStoreMsg, /*void *messageStruct*/
										NULL,/*char* temp_connectedNeighborsMapKey*/
										MYMESSAGE, /* MyMessage or NotMyMessage*/
										ndInfoTemp /*Node Info Pointer*/);

		// BroadCast To all the Connection Details.
		// Put to queue of all connection Details.

		/*printConnectedNeighborInforMap();*/
		BroadCastMessage(mymessage);

	}
	return true;
}


void BroadCastMessage(struct message *ptr){


	struct connectionDetails* connectionDetailsObj;
	pthread_mutex_lock(&connectedNeighborsMapLock);
	connectedNeighborsMapIter = connectedNeighborsMap.begin();
	if(connectedNeighborsMapIter == connectedNeighborsMap.end()){
		pthread_mutex_unlock(&connectedNeighborsMapLock);
		//Handle Error - No Key-Value Found
	}
	else{

		while(connectedNeighborsMapIter != connectedNeighborsMap.end()){
			connectionDetailsObj = connectedNeighborsMapIter -> second;
			if(connectionDetailsObj->notOperational == 0 &&
					connectionDetailsObj->helloStatus == 1 &&
					connectionDetailsObj->threadsExitedCount == 0){
						// need to forward to these, Key of Sender is not Equal to ConnectionDetails Map Object Key
					if(connectionDetailsObj->isJoinConnection != 1)
					{
						pthread_mutex_lock(&connectionDetailsObj -> messagingQueueLock);
						ptr->sendStatus = 0;
						connectionDetailsObj -> messagingQueue.push(ptr);
						pthread_cond_signal(&connectionDetailsObj -> messagingQueueCV);
						pthread_mutex_unlock(&connectionDetailsObj -> messagingQueueLock);
					}
			}
			connectedNeighborsMapIter++;
		}
		pthread_mutex_unlock(&connectedNeighborsMapLock);
	}

}

