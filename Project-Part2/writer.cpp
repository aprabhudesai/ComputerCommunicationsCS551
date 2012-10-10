/*============================================================================================
#       Students Name:  Aniket Zamwar & Abhishek Prabhudesai
#       USC ID:         Aniket   - 1488-5616-98
                        Abhishek - 4722-5949-88
#       Nunki ID:       zamwar
#       Nunki ID:       prabhude
#       Email:          zamwar@usc.edu
#                       prabhude@usc.edu
#       Submitted:      20th April, 2012
#       Project:        WarmUp Project #3 PART#2 Final - CSCI 551 Spring 2012
#       Instructor:     Bill Cheng
============================================================================================*/

#include <iostream>
#include <string.h>
#include "systemDataStructures.h"
#include "iniparser.h"
#include "headers.h"

extern pthread_mutex_t checkThreadLock;
extern pthread_cond_t checkThreadCV;
extern int checkStatus;

extern void removeConnectedNeighborInfo(char *);
extern void printConnectedNeighborInforMap();
extern bool writeToLogFile(struct message *ptr,char *);
extern void writeCommentToLogFile(char *);
extern void writeErrorToLogFile(char *);
//extern void freeMessage(struct message *);

extern int checkNodeType();
extern void sigpipeHandler(int);

extern sigset_t signalSetPipe;
extern struct sigaction actPipe;

void sendFileOverNetwork(struct fileMetadata*,int);

void writer(void *param){

	static int msgCount = 0;
	char *portAndHostName;  // used as key to get structure pointer, this structure has queue reference and socket desc
	portAndHostName = (char *)param;
	string portAndHostNameKey = string(portAndHostName);

	int sockDesc;
	char connNodeId[300];
	struct connectionDetails *connectionDetailsObj;

	/*
	 * SIGPIPE handler details
	 */
	actPipe.sa_handler = sigpipeHandler;
	sigaction(SIGPIPE, &actPipe, NULL);
	pthread_sigmask(SIG_UNBLOCK, &signalSetPipe, NULL);

	pthread_mutex_lock(&connectedNeighborsMapLock);

	connectedNeighborsMapIter = connectedNeighborsMap.find(portAndHostNameKey);
	if(connectedNeighborsMapIter == connectedNeighborsMap.end()){
		pthread_mutex_unlock(&connectedNeighborsMapLock);
		writeErrorToLogFile((char *)"Connection Details Not Found in Writer");
		pthread_exit(NULL);
		//Handle Error - No Key-Value Found
	}
	connectionDetailsObj = connectedNeighborsMapIter -> second;
	sockDesc = connectionDetailsObj -> socketDesc;
	pthread_mutex_unlock(&connectedNeighborsMapLock);

	int nodetype = checkNodeType();
	/*
	* Other variables needed
	*/
	struct message* messageStruct;

	uint32_t i = 0;
	time_t mysystime;
	int bytes_sent;

	while(1){

		messageStruct = NULL;
		//Acquire Messaging Queue Lock
		pthread_mutex_lock(&connectionDetailsObj -> messagingQueueLock);
		// If Queue is empty, wait for signal from Event Dispatcher to get message to be sent

		if(connectionDetailsObj -> messagingQueue.empty()){
			writeCommentToLogFile((char *)"Waiting for Event Dispatcher to put message in queue and signal");
			pthread_cond_wait(&connectionDetailsObj -> messagingQueueCV, &connectionDetailsObj -> messagingQueueLock);
			//cout << "Signal Received from Event Dispatcher" << endl;

		}
		pthread_mutex_unlock(&connectionDetailsObj -> messagingQueueLock);

		pthread_mutex_lock(&connectionDetailsObj->connectionLock);
		pthread_mutex_lock(&connectionDetailsObj -> messagingQueueLock);
		if(connectionDetailsObj -> notOperational == 1 && connectionDetailsObj -> messagingQueue.empty()){
				// delete all the allocated memory to handled which will not be handled by timer or Cache

				connectionDetailsObj->threadsExitedCount++;

				pthread_mutex_unlock(&connectionDetailsObj -> messagingQueueLock);
				pthread_mutex_unlock(&connectionDetailsObj->connectionLock);
				//close(sockDesc);
				goto OUT;

		}
		// Pop the message from queue to be sent
		if(connectionDetailsObj -> messagingQueue.empty())
		{
			pthread_mutex_unlock(&connectionDetailsObj -> messagingQueueLock);
			pthread_mutex_unlock(&connectionDetailsObj->connectionLock);
			continue;
		}
		msgCount++;
		messageStruct = connectionDetailsObj -> messagingQueue.front();
		connectionDetailsObj -> messagingQueue.pop();

		//Release Messaging Queue Lock
		pthread_mutex_unlock(&connectionDetailsObj -> messagingQueueLock);
		pthread_mutex_unlock(&connectionDetailsObj->connectionLock);

		if(messageStruct == NULL){
			writeCommentToLogFile((char *)"Message expired and not available to writer");
			continue;
		}
		// Convert Message Header to character Stream
		messageStruct -> messageHeader = createHeader(messageStruct);
		if(messageStruct -> messageHeader == NULL){
			writeErrorToLogFile((char *)"Writer could not convert Message Header to Character Stream");

			if((messageStruct->messType == RCHECK) ||
						(messageStruct->messType == RSTATUS) ||
						(messageStruct->messType == KEEPALIVE) ||
						(messageStruct->messType == HELLO && connectionDetailsObj->helloStatus == 1)){
				freeMessage(messageStruct);
				messageStruct = NULL;
			}
			else{
				pthread_mutex_lock(&messageCacheMapLock);
				messageStruct->sendStatus = 1;
				pthread_mutex_unlock(&messageCacheMapLock);
			}
			continue;
		}


		// ===================== No need to go forward for some Messages ==================================

		//Convert Message body to Character Stream
		if(!createMessageBuffer(messageStruct)){
			writeErrorToLogFile((char *)"Writer could not convert Message Body to Character Stream");
			if(	(messageStruct->messType == RCHECK) ||
					(messageStruct->messType == RSTATUS) ||
					(messageStruct->messType == KEEPALIVE) ||
					(messageStruct->messType == HELLO && connectionDetailsObj->helloStatus == 1)){
				freeMessage(messageStruct);
				messageStruct = NULL;
			}
			else{
				pthread_mutex_lock(&messageCacheMapLock);
				messageStruct->sendStatus = 1;
				pthread_mutex_unlock(&messageCacheMapLock);
			}
			continue;
		}

		// Send Message Header
		i = 0;
		bytes_sent = -1;
		bytes_sent = send(sockDesc,messageStruct -> messageHeader,HEADER_LENGTH, 0);
		if(bytes_sent == -1 || bytes_sent == 0){
			writeErrorToLogFile((char *)"Error Sending Message to Server\n");
			//Handle Error
			pthread_mutex_lock(&connectionDetailsObj->connectionLock);
			connectionDetailsObj->threadsExitedCount++;
			if(nodetype == 0)  //for not beacon
			{
				if(connectionDetailsObj->threadsExitedCount == 1 && connectionDetailsObj -> isJoinConnection == 0 && checkSendFlag == 0){
					//connectionDetailsObj -> notOperational = 1;

					pthread_mutex_lock(&checkThreadLock);
					checkStatus = 1;
					pthread_cond_signal(&checkThreadCV);
					pthread_mutex_unlock(&checkThreadLock);

				}
			}
			close(connectionDetailsObj->socketDesc);
			pthread_mutex_unlock(&connectionDetailsObj->connectionLock);
			goto OUT;
		}
		i++;
			/*
			 * update the last write activity time
			 */

		mysystime = time(NULL);
		connectionDetailsObj->lastWriteActivity = mysystime;

		/*
		 * update the last write activity time
		 */
		mysystime = time(NULL);
		connectionDetailsObj->lastWriteActivity = mysystime;
		// if we have Message Body send message Body
		if(messageStruct -> dataLength > 0){
			i = 0;
			bytes_sent = -1;

			bytes_sent = -1;

			int sizeofDataTobeSent = 0;
			if(messageStruct->messType == STORE){
				struct storeMessage* ptr = (struct storeMessage*)messageStruct->messageStruct;
				sizeofDataTobeSent = ptr->nonFileLength;
			}
			else if(messageStruct->messType == RGET){
				struct getRespMessage* ptr = (struct getRespMessage*)messageStruct->messageStruct;
				sizeofDataTobeSent = ptr->nonFileLength;
			}
			else{
				sizeofDataTobeSent = messageStruct -> dataLength;
			}

			bytes_sent = send(sockDesc,messageStruct -> message,sizeofDataTobeSent, 0);
			if(bytes_sent == -1 || bytes_sent == 0){
				writeErrorToLogFile((char *)"Error Sending Message to Server\n");
				//Handle Error
				pthread_mutex_lock(&connectionDetailsObj->connectionLock);
				connectionDetailsObj->threadsExitedCount++;
				if(nodetype == 0)
				{
					if(connectionDetailsObj->threadsExitedCount == 1 && connectionDetailsObj -> isJoinConnection == 0 && checkSendFlag == 0){
						pthread_mutex_lock(&checkThreadLock);
						checkStatus = 1;
						pthread_cond_signal(&checkThreadCV);
						pthread_mutex_unlock(&checkThreadLock);

					}
				}
				pthread_mutex_unlock(&connectionDetailsObj->connectionLock);
				goto OUT;
			}
			i++;
			/*
			 * update the last write activity time
			 */
			mysystime = time(NULL);
			connectionDetailsObj->lastWriteActivity = mysystime;

			//--------------- PART 2 Messages where File needs to be sent over network ------------------------------
			// if STORE Message send file
			if(messageStruct->messType == STORE){
				struct storeMessage *bptr;
				bptr = (struct storeMessage *)messageStruct->messageStruct;
				sendFileOverNetwork(bptr->respMetadata,sockDesc);
			}
			else if(messageStruct->messType == RGET){
				struct getRespMessage *bptr;
				bptr = (struct getRespMessage *)messageStruct->messageStruct;
				sendFileOverNetwork(bptr->respMetadata,sockDesc);
			}
		}

		// Write to Log
		sprintf(connNodeId,"%s_%d",connectionDetailsObj->hostname,connectionDetailsObj->wellKnownPort);
		writeToLogFile(messageStruct,connNodeId);
		
		pthread_mutex_lock(&connectionDetailsObj->connectionLock);
		if(connectionDetailsObj -> notOperational == 1){
					// delete all the allocated memory to handled which will not be handled by timer or Cache
					connectionDetailsObj->threadsExitedCount++;
					pthread_mutex_unlock(&connectionDetailsObj->connectionLock);

					goto OUT;

		}
		pthread_mutex_unlock(&connectionDetailsObj->connectionLock);

		if((messageStruct->messType == RCHECK) ||
				(messageStruct->messType == RSTATUS) ||
				(messageStruct->messType == KEEPALIVE) ||
				(messageStruct->messType == HELLO && connectionDetailsObj->helloStatus == 1)){
			freeMessage(messageStruct);
			messageStruct = NULL;
		}
		else{
			pthread_mutex_lock(&messageCacheMapLock);
			messageStruct->sendStatus = 1;
			pthread_mutex_unlock(&messageCacheMapLock);
		}
	}
	OUT:
	close(connectionDetailsObj->socketDesc);
	if(messageStruct != NULL)
	{
		if(	(messageStruct->messType == RCHECK) ||
				(messageStruct->messType == RSTATUS) ||
				(messageStruct->messType == KEEPALIVE) ||
				(messageStruct->messType == HELLO && connectionDetailsObj->helloStatus == 1)){
			freeMessage(messageStruct);
			messageStruct = NULL;
		}
		else{
			pthread_mutex_lock(&messageCacheMapLock);
			messageStruct->sendStatus = 1;
			pthread_mutex_unlock(&messageCacheMapLock);
		}
	}
	close(connectionDetailsObj->socketDesc);
	pthread_exit(NULL);
}



void sendFileOverNetwork(struct fileMetadata *metaptr,int sockDesc){

	FILE *fp;
	unsigned char fileContent[1000]="\0";
	int readSize = 0;
	int bytes_sent;

	fp = fopen(metaptr->fileWithPath,"r");
	if(fp == NULL)
		return;

	memset(fileContent,0,1000);

	//READ AND SEND FILE
	while (!feof(fp)) {
		memset(fileContent,0,1000);
		readSize = 0;
		readSize = fread(fileContent,1,sizeof(fileContent),fp);
		if(readSize > 0){
			bytes_sent = -1;
			bytes_sent = send(sockDesc,fileContent,readSize, 0);
		}
		else{
			break; //Done
		}
	}

	fclose(fp);

    char *check = NULL;
    check = strstr(metaptr->fileWithPath,"temp");
    if(check != NULL)
    	remove(metaptr->fileWithPath);

}
