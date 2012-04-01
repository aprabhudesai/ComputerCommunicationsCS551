#include <iostream>
#include "systemDataStructures.h"
#include "iniparser.h"
#include "headers.h"
//#include "messageStructures.h"

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

void writer(void *param){

	static int msgCount = 0;
	//signal(SIGPIPE,SIG_IGN);
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
		//printf("\n\n\t------------> Sending Message HEADER Type : %02x, To Port : %d\n\n",messageStruct->messType,connectionDetailsObj->port);
		while(i < HEADER_LENGTH){
			bytes_sent = -1;
			bytes_sent = send(sockDesc,messageStruct -> messageHeader + i,1, 0);
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
				//pthread_exit(NULL);
			}
			i++;
			/*
			 * update the last write activity time
			 */

			mysystime = time(NULL);
			connectionDetailsObj->lastWriteActivity = mysystime;
		}
		/*
		 * update the last write activity time
		 */
		mysystime = time(NULL);
		connectionDetailsObj->lastWriteActivity = mysystime;
		// if we have Message Body send message Body
	//	printf("\n\n\t------------> Sending Message HEADER Type : %02x, To Port : %d Message SIze %d\n\n",messageStruct->messType,connectionDetailsObj->port,messageStruct -> dataLength);
		if(messageStruct -> dataLength > 0){
			i = 0;
			bytes_sent = -1;
			while(i < messageStruct -> dataLength){
				bytes_sent = -1;
				bytes_sent = send(sockDesc,messageStruct -> message + i,1, 0);
				if(bytes_sent == -1 || bytes_sent == 0){
					writeErrorToLogFile((char *)"Error Sending Message to Server\n");
					//Handle Error
					pthread_mutex_lock(&connectionDetailsObj->connectionLock);
						connectionDetailsObj->threadsExitedCount++;
						if(nodetype == 0)
						{
							if(connectionDetailsObj->threadsExitedCount == 1 && connectionDetailsObj -> isJoinConnection == 0 && checkSendFlag == 0){
								//connectionDetailsObj -> notOperational = 1;

								pthread_mutex_lock(&checkThreadLock);
									checkStatus = 1;
								pthread_cond_signal(&checkThreadCV);
								pthread_mutex_unlock(&checkThreadLock);

							}
						}
						//close(connectionDetailsObj->socketDesc);
					pthread_mutex_unlock(&connectionDetailsObj->connectionLock);
					//printConnectedNeighborInforMap();
					goto OUT;
					//pthread_exit(NULL);
				}
				i++;
				/*
				 * update the last write activity time
				 */
				mysystime = time(NULL);
				connectionDetailsObj->lastWriteActivity = mysystime;
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
					//close(sockDesc);
					goto OUT;

				// To be Handled
				// No Data on socket
				// Need to either Drop Connection or drop packet and wait for new packet
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
		//printConnectedNeighborInforMap();
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
		//removeConnectedNeighborInfo(portAndHostName);
	close(connectionDetailsObj->socketDesc);


	pthread_exit(NULL);
}
