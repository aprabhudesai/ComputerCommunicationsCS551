/*------------------------------------------------------------------------------
 *      Name: Aniket Zamwar and Abhishek Prabhudesai
 *      USC ID: 1488-5616-98
 *      Aludra/Nunki ID: zamwar
 *      Aludra/Nunki ID: prabhude
 *
 *      Project: WarmUp Project #3 - CSCI 551 Spring 2012
 *      Instructor: Bill Cheng
 *      File Description: This section of project consists of all the implementation
 *      				of project logic.
------------------------------------------------------------------------------*/

/*
	Includes all the functions to create chraccter stream from struture.
 	And functions to create structures from incoming character stream data.

 */

#include "systemDataStructures.h"
#include <iostream>

using namespace std;

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


/*void setMessageTime(struct message *ptr){

	gettimeofday(&ptr->logTime, NULL);
	return;
}*/


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
			//systime *= 1000000;
			messageHeaderObj->timestamp = systime;

			messageHeaderObj -> messType = temp_messType;
			GetUOID(ndInfoTemp -> nodeInstanceId, (char *)"msg", messageHeaderObj -> uoid, sizeof(messageHeaderObj -> uoid) -1);
			messageHeaderObj -> uoid[21] = '\0';
			//messageHeaderObj -> uoidKey = string(messageHeaderObj -> uoid);
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
	//printHeader(mHeader);

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
	//mHeader -> uoidKey = string("abcd");

	memcpy(&mHeader-> ttl,mHeaderStream,1);
	mHeaderStream+=1;

	memcpy(&mHeader-> resv,mHeaderStream,1);
	mHeaderStream+=1;

	memcpy(&mHeader-> dataLength,mHeaderStream,4);
	mHeader-> dataLength = (uint32_t)ntohl(mHeader-> dataLength);

	mHeader->myMessage = NOTMYMESSAGE;

	mHeader->sendStatus = 1;
	//printHeader(mHeader);

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
		//uint32_t hostLocation;
		//uint16_t hostPort;
		//char hostName[256];
		//uint32_t dataLength;

		struct joinMessage* ptr = createjoinMessageStruct();
		ptr->hostLocation = temp_hostLocation;
		strcpy(ptr->hostName,temp_hostName);
		ptr->hostPort = temp_hostPort;
		ptr->dataLength = (uint32_t)(6 + strlen(ptr -> hostName));
		return ptr;
}

char *createJoinMessage(struct joinMessage *ptr){

	//printJoinMessage(ptr);

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
	ptr -> hostPort = (uint16_t)htons((uint16_t)host_hostPort);
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
	//printjoinRespMessage(ptr);
	uint32_t net_distance;
	uint16_t net_hostPort;
	
//	printjoinRespMessage(ptr);

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
	//ptr -> uoidKey = string(ptr -> uoid);

	memcpy(&host_distance,buffer,4);
	ptr -> distance = (uint32_t)htonl((uint32_t)host_distance);
	buffer+=4;

	memcpy(&host_hostPort,buffer,2);
	ptr -> hostPort = (uint16_t)htons((uint16_t)host_hostPort);
	buffer+=2;

	memcpy(ptr -> hostName,buffer,ptr -> dataLength - 26);
	ptr -> hostName[ptr -> dataLength - 26] = '\0';
	
	//printjoinRespMessage(ptr);
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
	//printHelloMessage(ptr);

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
	ptr -> hostPort = (uint16_t)htons((uint16_t)host_hostPort);
	buffer+=2;
	
	memcpy(ptr -> hostName,buffer,ptr -> dataLength - 2);
	ptr -> hostName[ptr -> dataLength - 2] = '\0';
	
	//printHelloMessage(ptr);

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
	//strcpy(reinterpret_cast<char*>(temp -> checkMsgHeaderUOID),reinterpret_cast<char*>(uoid));
	temp -> dataLength = 20;
	return temp;
}

char* createCheckRespMessage(struct checkRespMessage *ptr){
	// char checkMsgHeaderUOID[21];
	
	char *buffer;

	buffer = NULL;
	buffer = (char *)malloc(21);
	memset(buffer,0,21);
	memcpy(buffer,ptr -> checkMsgHeaderUOID,20);
	buffer[21] = '\0';
	
	return buffer;
}

struct checkRespMessage* parseCheckRespMessage(char *buffer,uint32_t dataLength){
	// char checkMsgHeaderUOID[21];

	struct checkRespMessage* ptr;
	ptr = createcheckRespMessageStruct();
	ptr -> dataLength = dataLength;
	
	memcpy(ptr -> checkMsgHeaderUOID,buffer,20);
	ptr -> checkMsgHeaderUOID[21] = '\0';
	
	//ptr -> uoidKey = string(ptr -> checkMsgHeaderUOID);

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

	printf("\n\nStatus Message Data Length \'%d\' \n",ptr -> dataLength);
	printf("Status Message status Type: %d\n\n",ptr->statusType);
	return;
}

struct statusMessage* createStatus(uint8_t temp_statusType){
	//0x01 means the neighbors information should be sent in the reply
	// 0x02 means the files information should be sent in the reply

	struct statusMessage* temp;
	temp = statusMessageStruct();
	temp->statusType = temp_statusType;
	temp -> dataLength = 1;
	//printstatusMessage(temp);

	return temp;
}

char* createStatusMessage(struct statusMessage *ptr){

	char *buffer;
	buffer = NULL;
	buffer = (char *)malloc(ptr -> dataLength);
	memset(buffer,0,ptr->dataLength);

	memcpy(buffer,&ptr->statusType,1);

	//printstatusMessage(ptr);
	return buffer;
}

struct statusMessage* parseStatusMessage(char *buffer,uint32_t dataLength){

	struct statusMessage* ptr;
	ptr = statusMessageStruct();
	ptr -> dataLength = dataLength;

	memcpy(&ptr->statusType,buffer,1);
	//printstatusMessage(ptr);
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

	//printf("Status Response Message UOID: ");displayUOID(ptr->statusMsgHeaderUOID);

	return;
}

struct statusRespMessage* createStatusResp(struct NodeInfo* ndInfoTemp,struct message* messageObj){
	struct statusRespMessage *ptr;
	ptr = statusRespMessageStruct();

	struct statusMessage* statusMessageObj = (struct statusMessage *)messageObj->messageStruct;

	if(statusMessageObj->statusType == 0x01){

		ptr->statusType=0x01;
		ptr->dataRecordCount = 0;

		//strcpy(reinterpret_cast<char*>(ptr->statusMsgHeaderUOID),reinterpret_cast<char*>(messageObj -> uoid));
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
		//printf("\n\n\t\t\t-----------------------------< Response Status Message Created which is as under: <-------------------> ");
		//printstatusRespMessage(ptr);
	}
	else{
		//Second Part
	}
	return ptr;
}



char* createStatusRespMessage(struct statusRespMessage *ptr){

	char *buffer;
	char *bufferPtr;
	buffer = NULL;
	buffer = (char *)malloc(ptr -> dataLength);
	memset(buffer,0,ptr->dataLength);
	bufferPtr = buffer;

	if(ptr->statusType == 0x01){
		memcpy(buffer,ptr->statusMsgHeaderUOID,20);
		buffer+=20;

		memcpy(buffer,&(ptr->hostInfoLength),2);
		buffer+=2;

		memcpy(buffer,&(ptr->hostPort),2);
		buffer+=2;

		memcpy(buffer,ptr->hostName,strlen(ptr->hostName));
		buffer+=strlen(ptr->hostName);

		int i=0;
		if(ptr->dataRecordCount != 0){
			//uint32_t temp=0;
			while(i < (ptr->dataRecordCount)-1){

				memcpy(buffer,&ptr->dataRecordSize[i],4);
				buffer+=4;

				memcpy(buffer,&(ptr->neighHostPorts[i]),2);
				buffer+=2;

				memcpy(buffer,ptr->neighHostNames[i],strlen(ptr->neighHostNames[i]));
				buffer+=strlen(ptr->neighHostNames[i]);

				i++;
			}
			memcpy(buffer,&ptr->dataRecordSize[i],4);
			buffer+=4;

			memcpy(buffer,&(ptr->neighHostPorts[i]),2);
			buffer+=2;

			memcpy(buffer,ptr->neighHostNames[i],strlen(ptr->neighHostNames[i]));
		}
		//struct statusRespMessage *trial = parseStatusRespMessage(bufferPtr,ptr->dataLength, 0x01);
		//printf("\n\n\t\t\t-----------------------------< CHECKKKKKK Response Status Mesage from Character Stream is as Under: <-------------------> ");
		//printstatusRespMessage(trial);
	}
	else{
		//Second Part of Project
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
		buffer+=2; dataRead+=2;

		memcpy(&(ptr->hostPort),buffer,2);
		buffer+=2;dataRead+=2;

		memcpy(ptr->hostName,buffer,ptr->hostInfoLength - 2);
		ptr->hostName[ptr->hostInfoLength - 2] = '\0';
		buffer+=strlen(ptr->hostName);dataRead+=strlen(ptr->hostName);

		remLength = dataLength - dataRead;
		int i=0;
		while(dataRead < ptr->dataLength && remLength != 0){

			memcpy(&ptr->dataRecordSize[i],buffer,4);
			buffer+=4;dataRead+=4;
			remLength-=4;

			memcpy(&(ptr->neighHostPorts[i]),buffer,2);
			buffer+=2;dataRead+=2;
			remLength-=2;

			if(ptr->dataRecordSize[i] != 0){
				memcpy(ptr->neighHostNames[i],buffer,ptr->dataRecordSize[i] - 2);
				ptr->neighHostNames[i][ptr->dataRecordSize[i] - 2] = '\0';
				dataRead+=strlen(ptr->neighHostNames[i]);
				remLength-=(uint32_t)strlen(ptr->neighHostNames[i]);
				buffer+=strlen(ptr->neighHostNames[i]);
				i++;
			}
			else{
				//printf("\n\n\t\t^^^^^^^^^^^^^^^^^^^^^^^^^^^Data Record Size ZERO --- LAST RECORD\n");
				memcpy(ptr->neighHostNames[i],buffer,remLength);
				ptr->neighHostNames[i][remLength] = '\0';
				i++;
				break;
			}
		}
		ptr->dataRecordCount = i;
		//printf("\n\n\t\t\t-----------------------------< Response Status Mesage from Character Stream is as Under: <-------------------> ");
		//printstatusRespMessage(ptr);
	}
	else{
		//second part of project
	}
	return ptr;
}

struct statusRespMessage * createSelfStatus(struct NodeInfo *ndInfoTemp,uint8_t statusType)
{
	struct statusRespMessage *ptr;
		ptr = statusRespMessageStruct();

		//struct statusMessage* statusMessageObj = (struct statusMessage *)messageObj->messageStruct;

		if(statusType == 0x01){

			ptr->statusType=0x01;
			ptr->dataRecordCount = 0;

			//strcpy(reinterpret_cast<char*>(ptr->statusMsgHeaderUOID),reinterpret_cast<char*>(messageObj -> uoid));
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
			//printf("\n\n\t\t\t-----------------------------< Response Status Message Created which is as under: <-------------------> ");
			//printstatusRespMessage(ptr);
		}
		else{
			//Second Part
		}
		return ptr;
}

//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------

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
						ptr -> messageStruct = (void *) parseStatusRespMessage(ptr -> message,ptr -> dataLength,0x01);
						if(ptr -> messageStruct == NULL)
							return false;
					}
					break;
		// Project Part 2
		/*
		case SEARCH:
					{
			
					}
					break;
			
		case RSEARCH:
					{
			
					}
					break;
			
		case DELETE:
					{
			
					}
					break;
			
		case STORE:
					{
			
					}
					break;
			
		case GET:
					{
			
					}
					break;	

		case RGET:
					{
						
					}
					break;
			*/
			
		default: return false;
			
	}

	return true;

}


bool createMessageBuffer(struct message *ptr){
	
	//if(ptr -> messageStruct == NULL)
	//	return false;


	switch(ptr -> messType){
		
		case JOIN:
					{
						//cout << "JOIN: Convert struct to character Stream\n";
						ptr -> message = createJoinMessage((struct joinMessage *)ptr -> messageStruct);
						if(ptr -> message == NULL)
							return false;
							
					}
					break;
		case RJOIN:
					{
						//cout << "RJOIN: Convert struct to character Stream\n";
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
							// not implemented yet.
						ptr -> message = createStatusMessage((struct statusMessage *)ptr -> messageStruct);
						if(ptr -> message == NULL)
							return false;
					}
					break;
			
		case RSTATUS:
					{
							// not implemented yet.
						ptr -> message = createStatusRespMessage((struct statusRespMessage *)ptr -> messageStruct);
						if(ptr -> message == NULL)
							return false;
					}
					break;
		// Project Part 2
		/*
		case SEARCH:
					{
			
					}
					break;
			
		case RSEARCH:
					{
			
					}
					break;
			
		case DELETE:
					{
			
					}
					break;
			
		case STORE:
					{
			
					}
					break;
			
		case GET:
					{

								}
					break;	

		case RGET:
					{
						
					}
					break;
			*/
			
		default: return false;
			
	}

	return true;		
}



/* Project Part 2

char* createSearchMessage(struct searchMessage){
	 If the search type is 1, the next field contains an exact file name. If the search type is 2, the next field contains an exact SHA1 hash value (not hexstring-encoded). If the search type is 3, the next field contains a list of keywords (separated by space characters).
	uint8_t searchType;
	char *query;
}

struct searchRespMessage{
		
};

struct getMessage{

};

struct getRespMessage{

};

struct storeMessage{

};

struct deleteMessage{

};

struct statusMessage{

};

struct statusRespMessage{

};

*/
