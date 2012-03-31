
#include <iostream>
#include <stdlib.h>
#include "systemDataStructures.h"
#include "iniparser.h"
#include "ntwknode.h"
#include "headers.h"
#include "message.h"
#include <time.h>

extern FILE *logFilePtr;
extern pthread_mutex_t logFileLock;
extern FILE *extFilePtr;
extern char extfilecmd[256];
extern pthread_mutex_t extFileLock;
extern struct NodeInfo ndInfo;
extern pthread_mutex_t checkThreadLock;
extern pthread_cond_t checkThreadCV;
extern int checkStatus;

extern STATUSMSGMAPTYPE statusMsgMap;
extern STATUSMSGMAPTYPE::iterator statusMsgMapIter;
extern pthread_mutex_t statusMsgMapLock;

extern pthread_t listenerThread;
extern pthread_mutex_t eventDispatcherQueueLock;
extern pthread_cond_t eventDispatcherQueueCV;
extern pthread_t keyboardThread;
extern NodeInfo ndInfo;


void ReConnectToBeaconNode(void *);
void freeMessage(struct message *);
void writeCommentToLogFile(char *);

void sigpipeHandler(int signum)
{

}

void sigUSR1Handler(int signum)
{
	//printf("\n\n----;;;-----Got a signal to Quit ---;;;-----\n\n");
	pthread_exit(NULL);
}

void sigUSR2Handler(int signum)
{
	//printf("\n\n----;;;-----Got a signal to Quit ---;;;-----\n\n");
	pthread_exit(NULL);
}

void resetBeaconNode()
{

}

void resetNonBeaconNode()
{

}


void scanAndEraseMsgCache()
{
	struct message *mapmessage;
	time_t mysystime;
	uint16_t msgLifeTime = ndInfo.msgLifetime;
	pthread_mutex_lock(&messageCacheMapLock);
	for(messageCacheMapIter = messageCacheMap.begin(); messageCacheMapIter != messageCacheMap.end(); messageCacheMapIter++)
	{
		mapmessage = messageCacheMapIter->second;
		mysystime = time(NULL);
		if((mysystime - mapmessage->timestamp) >= msgLifeTime)
		{
			if(mapmessage->messType == HELLO){
				pthread_mutex_lock(&connectedNeighborsMapLock);
				string connectionMapKey = string(mapmessage->connectedNeighborsMapKey);
				connectedNeighborsMapIter = connectedNeighborsMap.find(connectionMapKey);
				if(connectedNeighborsMapIter != connectedNeighborsMap.end())
				{
					/*
					 * Tell reader and writer to exit as this connection is no longer
					 * useful
					 */
					struct connectionDetails *cd =connectedNeighborsMapIter->second;
					pthread_mutex_lock(&(cd->connectionLock));
					pthread_mutex_lock(&(cd->messagingQueueLock));
					cd->notOperational = 1;
					pthread_cond_signal(&(cd->messagingQueueCV));
					pthread_mutex_unlock(&(cd->messagingQueueLock));
					close(cd->socketDesc);
					pthread_mutex_unlock(&(cd->connectionLock));
					//pthread_mutex_unlock(&connectedNeighborsMapLock);
				}
				pthread_mutex_unlock(&connectedNeighborsMapLock);
			}
			// Clear Message Cache
			//messageCacheMap.erase(messageCacheMapIter->first);
			// Free Memory
			else if(mapmessage->sendStatus == 0){
				time_t temptime;
				char timechar[100] = "\0";
				time(&temptime);
				strcpy(timechar,ctime(&temptime));

				string tempUOID = string(timechar);
				messageCacheMap.erase(messageCacheMapIter->first);
				messageCacheMap.insert(MESSCACHEMAPTYPE::value_type(tempUOID,mapmessage));
			}
			else{
				// Clear Message Cache
				struct message *tempmsg = messageCacheMapIter->second;
				messageCacheMap.erase(messageCacheMapIter->first);
				freeMessage(tempmsg);
				tempmsg = NULL;
				// Free Memory
			}
		}
	}
	pthread_mutex_unlock(&messageCacheMapLock);
}

void scanAndEraseConnectedNeighborsMap(int nodetype)
{
	/*
	 * Check Connected neighbors map to delete any unused connections
	 * unused connections are: notOperational = 1 & threadsExitedCount = 2
	 */
	struct connectionDetails *cd;
	time_t mysystime;

	pthread_mutex_lock(&messageCacheMapLock);
		pthread_mutex_lock(&connectedNeighborsMapLock);
		for(connectedNeighborsMapIter = connectedNeighborsMap.begin(); connectedNeighborsMapIter != connectedNeighborsMap.end(); connectedNeighborsMapIter++)
		{
			mysystime = time(NULL);

			cd = connectedNeighborsMapIter->second;
			pthread_mutex_lock(&cd->connectionLock);

			//if node is Beacon and disconnected i.e not operational, we need to reconnect to it.
			if(nodetype == 1)
			{
				/*
				 * BEACON
				 */
				if(checkNodeType(cd->wellKnownPort) && (cd->tiebreak == 0) && (cd->helloStatus==1) &&
						((cd->notOperational == 0 && (cd->threadsExitedCount == 2 || cd->threadsExitedCount == 1)))){
					printf("\n\nLost Connection with a BEACON node hence reconnecting\n\n");
					writeCommentToLogFile((char *)"Lost Connection with a BEACON node hence reconnecting\n");
					printf("\nservant:%d> ",ndInfo.port);
					struct connectionDetails *cdTemp = connectedNeighborsMapIter->second;

					connectedNeighborsMap.erase(connectedNeighborsMapIter->first);
					close(connectedNeighborsMapIter->second->socketDesc);
					pthread_mutex_unlock(&cd->connectionLock);
					pthread_t ReconnectThread;
					pthread_create(&ReconnectThread,NULL,(void* (*)(void*))ReConnectToBeaconNode,(void *)cdTemp);
					//ReConnectToBeaconNode(cdTemp,connectedNeighborsMapIter->first);

				}
				else if((cd->notOperational == 1 && cd->threadsExitedCount >= 1) ||
						(cd->isJoinConnection == 1 && cd->threadsExitedCount >= 1) ||
						(cd->notOperational == 1 && cd->threadsExitedCount >= 1 &&
								!checkNodeType(cd->wellKnownPort)))
				{
					struct connectionDetails * tempcd = connectedNeighborsMapIter->second;
					close(cd->socketDesc);
					connectedNeighborsMap.erase(connectedNeighborsMapIter->first);
					// Free Memory
					freeConnectionDetails(tempcd);
				}
			}
			else
			{
				/*
				 * NON BEACON
				 */
				if((cd->notOperational == 1 && cd->threadsExitedCount == 2) && !checkNodeType(cd->wellKnownPort))
				{
					//no_of_notoper++;
					struct connectionDetails * tempcd = connectedNeighborsMapIter->second;
					close(cd->socketDesc);
					connectedNeighborsMap.erase(connectedNeighborsMapIter->first);
					freeConnectionDetails(tempcd);
					// Free Memory
					//printConnectedNeighborInforMap();
				}
				else if((cd->notOperational == 1 && cd->threadsExitedCount == 2 && cd->isJoinConnection == 1))
				{
					struct connectionDetails * tempcd = connectedNeighborsMapIter->second;
					close(cd->socketDesc);
					connectedNeighborsMap.erase(connectedNeighborsMapIter->first);
					// Free Memory
					freeConnectionDetails(tempcd);
					//printConnectedNeighborInforMap();
				}

			}

			pthread_mutex_unlock(&cd->connectionLock);
		}
		pthread_mutex_unlock(&connectedNeighborsMapLock);
	pthread_mutex_unlock(&messageCacheMapLock);
	//printConnectedNeighborInforMap();
}


void checkLastReadActivityOfConnection(int nodetype)
{
	/*
	 * Check the last Read Activity for the connection If last read activity exceeds the
	 * keepalive timeout then this connection is probably dead.
	 * 1. make this connection not operational
	 * 2. inform the reader and writer threads to exit
	 */
	struct connectionDetails *cd;
	time_t mysystime;
	uint16_t keepAliveTimeout = ndInfo.keepAliveTimeout;
	pthread_mutex_lock(&connectedNeighborsMapLock);
	for(connectedNeighborsMapIter = connectedNeighborsMap.begin(); connectedNeighborsMapIter != connectedNeighborsMap.end(); connectedNeighborsMapIter++)
	{
		mysystime = time(NULL);
		//mysystime *= 1000000;
		cd = connectedNeighborsMapIter->second;
		pthread_mutex_lock(&cd->connectionLock);
		//printf("\n\nComparing : MyTime: %ld vs ReadActivity : %ld",mysystime,cd->lastReadActivity);
		if((mysystime - cd->lastReadActivity) >= keepAliveTimeout &&(cd->helloStatus == 1) &&
				(cd->notOperational == 0) && (cd->threadsExitedCount == 0))
		{
			//printf("\n\n-----------<> <> TIMER : No Read Activity from This connection: %s:%d Hence marking it notOperational <><>-----------\n\n",cd->hostname,cd->port);

			if(!checkNodeType(cd->wellKnownPort))
				cd->notOperational = 1;
			pthread_mutex_lock(&cd->messagingQueueLock);
			pthread_cond_signal(&cd->messagingQueueCV);
			pthread_mutex_unlock(&cd->messagingQueueLock);
			close(cd->socketDesc);
			pthread_mutex_unlock(&cd->connectionLock);

			if(nodetype == 0 && checkSendFlag == 0)
			{
				/*
				 * I am a nonBeacon; Send a check message over all the remaining operational connections
				 * and wait for the response to check if I am connected to SERVANT network
				 */
				pthread_mutex_lock(&checkThreadLock);
				checkStatus = 1;
				pthread_cond_signal(&checkThreadCV);
				pthread_mutex_unlock(&checkThreadLock);
			}
		}
		else
		{
			pthread_mutex_unlock(&cd->connectionLock);
		}
	}
	pthread_mutex_unlock(&connectedNeighborsMapLock);
}

void checkLastWriteActivityOfConnection()
{
	/*
	 * Check the last Write Activity for the connection. If the last write activity exceeds the
	 * keepalive timeout then I have to send a KEEPALIVE message over this connection
	 * to inform the neighbor that the connection is still active
	 */
	struct connectionDetails *cd;
	time_t mysystime;
	struct message *mymessage;
	uint32_t msglenforkeelalive = 0;
	struct NodeInfo *ndInfoCommon = &ndInfo;
	uint16_t keepAliveTimeout = (ndInfo.keepAliveTimeout/2);
	pthread_mutex_lock(&connectedNeighborsMapLock);
	for(connectedNeighborsMapIter = connectedNeighborsMap.begin(); connectedNeighborsMapIter != connectedNeighborsMap.end(); connectedNeighborsMapIter++)
	{
		mysystime = time(NULL);
		//mysystime *= 1000000;
		cd = connectedNeighborsMapIter->second;
		pthread_mutex_lock(&cd->connectionLock);
		//printf("\n\nComparing : MyTime: %ld vs ReadActivity : %ld",mysystime,cd->lastWriteActivity);
		if((mysystime - cd->lastWriteActivity) >= keepAliveTimeout &&(cd->helloStatus == 1) &&
				(cd->notOperational == 0) && (cd->threadsExitedCount == 0))
		{
			//printf("\n\n-----------<> <> TIMER : No Write Activity from This connection: %s:%d Hence Send a KEEPALIVE MSG <><>-----------\n\n",cd->hostname,cd->port);
			mymessage = CreateMessageHeader(KEEPALIVE, /*Mess Type*/
					1, /* TTL */
					msglenforkeelalive, /*DataLength Message Body */
					NULL, /*char *messageHeader*/
					NULL, /*char *message*/
					(void*)NULL, /*void *messageStruct*/
					NULL,/*char* temp_connectedNeighborsMapKey*/
					MYMESSAGE, /* MyMessage or NotMyMessage*/
					ndInfoCommon /*Node Info Pointer*/);
			cd->lastWriteActivity = mysystime;

			pthread_mutex_lock(&cd->messagingQueueLock);
			cd->messagingQueue.push(mymessage);
			pthread_cond_signal(&cd->messagingQueueCV);
			pthread_mutex_unlock(&cd->messagingQueueLock);
		}
		pthread_mutex_unlock(&cd->connectionLock);
	}
	pthread_mutex_unlock(&connectedNeighborsMapLock);
}

void clearStatusMsgMap()
{
	pthread_mutex_lock(&statusMsgMapLock);
	for(statusMsgMapIter = statusMsgMap.begin(); statusMsgMapIter != statusMsgMap.end();statusMsgMapIter++)
	{
		statusMsgMap.erase(statusMsgMapIter->first);
	}
	pthread_mutex_unlock(&statusMsgMapLock);
}


bool writeInfoToStatusFile(statusRespMessage *statusRespMsgObj)
{
	bool retStatus = true;
	int i=0;
	int no_of_records = statusRespMsgObj->dataRecordCount;
	STATUSMSGMAPTYPE::iterator statusMsgMapIterTemp;
	pthread_mutex_lock(&extFileLock);

	extFilePtr = fopen(extfilecmd,"a+");

	pthread_mutex_lock(&statusMsgMapLock);
	statusMsgMapIter = statusMsgMap.find(statusRespMsgObj->hostPort);
	if(statusMsgMapIter == statusMsgMap.end())
	{
		statusMsgMap.insert(STATUSMSGMAPTYPE::value_type(statusRespMsgObj->hostPort,1));
		fprintf(extFilePtr,"n -t * -s %d -c red -i black\n",statusRespMsgObj->hostPort);
	}
	/*
	 * write the nodes in the file
	 */
	for(i = 0; i < no_of_records; i++)
	{
		statusMsgMapIter = statusMsgMap.find(statusRespMsgObj->neighHostPorts[i]);
		if(statusMsgMapIter == statusMsgMap.end())
		{
			statusMsgMap.insert(STATUSMSGMAPTYPE::value_type(statusRespMsgObj->neighHostPorts[i],1));
			fprintf(extFilePtr,"n -t * -s %d -c red -i black\n",statusRespMsgObj->neighHostPorts[i]);
		}
	}

	/*
	 * write the links in the file
	 */
	for(i = 0; i < no_of_records; i++)
	{
		fprintf(extFilePtr,"l -t * -s %d -d %d -c blue\n",statusRespMsgObj->hostPort,statusRespMsgObj->neighHostPorts[i]);
	}
	pthread_mutex_unlock(&statusMsgMapLock);

	fclose(extFilePtr);
	pthread_mutex_unlock(&extFileLock);
	return retStatus;
}

void writeErrorToLogFile(char *info){
	pthread_mutex_lock(&logFileLock);
		fprintf(logFilePtr,"**%s\n",info);
	pthread_mutex_unlock(&logFileLock);
	return;
}

void writeCommentToLogFile(char *info){
	pthread_mutex_lock(&logFileLock);
		fprintf(logFilePtr,"//%s\n",info);
	pthread_mutex_unlock(&logFileLock);
	return;
}


bool writeToLogFile(struct message *ptr, char *nodeId){

	if(logFilePtr == NULL)
		return false;

	gettimeofday(&ptr->logTime, NULL);
	pthread_mutex_lock(&logFileLock);
	switch(ptr -> messType){

		case JOIN:
					{
						struct joinMessage *joinMessageObj = (struct joinMessage *)ptr->messageStruct;
						if(ptr->howMessageFormed == 'r'){
							  // r <time> <from> <msgtype> <size> <ttl> <msgid> <data>
							fprintf(logFilePtr,"%c %10ld.%03d %s JNRQ %d %d %02x%02x%02x%02x %d %s\n",
									ptr->howMessageFormed,
									ptr->logTime.tv_sec,(int)(ptr->logTime.tv_usec/1000),nodeId,
									HEADER_LENGTH + ptr->dataLength,
									ptr->ttl,
									ptr->uoid[16],ptr->uoid[17],ptr->uoid[18],ptr->uoid[19],
									joinMessageObj->hostPort,joinMessageObj->hostName);
						}
						else{
							  // f/s <time> <from> <msgtype> <size> <ttl> <msgid> <data>
							fprintf(logFilePtr,"%c %10ld.%03d %s JNRQ %d %d %02x%02x%02x%02x %d %s\n",
									ptr->howMessageFormed,
									ptr->logTime.tv_sec,(int)(ptr->logTime.tv_usec/1000),nodeId,
									HEADER_LENGTH + ptr->dataLength,
									ptr->ttl,
									ptr->uoid[16],ptr->uoid[17],ptr->uoid[18],ptr->uoid[19],
									joinMessageObj->hostPort,joinMessageObj->hostName);
						}

					}
					break;
		case RJOIN:
					{
						struct joinRespMessage *joinRespMessageObj = (struct joinRespMessage *)ptr->messageStruct;
						if(ptr->howMessageFormed == 'r'){
							fprintf(logFilePtr,"%c %10ld.%03d %s JNRS %d %d %02x%02x%02x%02x %02x%02x%02x%02x %d %d %s\n",
							ptr->howMessageFormed,
							ptr->logTime.tv_sec,(int)(ptr->logTime.tv_usec/1000),nodeId,
							HEADER_LENGTH + ptr->dataLength,
							ptr->ttl,
							ptr->uoid[16],ptr->uoid[17],ptr->uoid[18],ptr->uoid[19],
							joinRespMessageObj->uoid[16],joinRespMessageObj->uoid[17],joinRespMessageObj->uoid[18],joinRespMessageObj->uoid[19],
							joinRespMessageObj->distance,joinRespMessageObj->hostPort,joinRespMessageObj->hostName);
						}
						else{
							fprintf(logFilePtr,"%c %10ld.%03d %s JNRS %d %d %02x%02x%02x%02x %02x%02x%02x%02x %d %d %s\n",
							ptr->howMessageFormed,
							ptr->logTime.tv_sec,(int)(ptr->logTime.tv_usec/1000),nodeId,
							HEADER_LENGTH + ptr->dataLength,
							ptr->ttl,
							ptr->uoid[16],ptr->uoid[17],ptr->uoid[18],ptr->uoid[19],
							joinRespMessageObj->uoid[16],joinRespMessageObj->uoid[17],joinRespMessageObj->uoid[18],joinRespMessageObj->uoid[19],
							joinRespMessageObj->distance,joinRespMessageObj->hostPort,joinRespMessageObj->hostName);
						}

					}
					break;

		case HELLO:
					{
						struct helloMessage *helloMessageObj = (struct helloMessage *)ptr->messageStruct;
						if(ptr->howMessageFormed == 'r'){
							fprintf(logFilePtr,"%c %10ld.%03d %s HLLO %d %d %02x%02x%02x%02x %d %s\n",
							ptr->howMessageFormed,
							ptr->logTime.tv_sec,(int)(ptr->logTime.tv_usec/1000),nodeId,
							HEADER_LENGTH + ptr->dataLength,
							ptr->ttl,
							ptr->uoid[16],ptr->uoid[17],ptr->uoid[18],ptr->uoid[19],
							helloMessageObj->hostPort, helloMessageObj->hostName);
						}
						else{
							fprintf(logFilePtr,"%c %10ld.%03d %s HLLO %d %d %02x%02x%02x%02x %d %s\n",
							ptr->howMessageFormed,
							ptr->logTime.tv_sec,(int)(ptr->logTime.tv_usec/1000),nodeId,
							HEADER_LENGTH + ptr->dataLength,
							ptr->ttl,
							ptr->uoid[16],ptr->uoid[17],ptr->uoid[18],ptr->uoid[19],
							helloMessageObj->hostPort, helloMessageObj->hostName);
						}
					}
					break;

		case KEEPALIVE:
					{
						if(ptr->howMessageFormed == 'r'){
							fprintf(logFilePtr,"%c %10ld.%03d %s KPAV %d %d %02x%02x%02x%02x\n",
							ptr->howMessageFormed,
							ptr->logTime.tv_sec,(int)(ptr->logTime.tv_usec/1000),nodeId,
							HEADER_LENGTH + ptr->dataLength,
							ptr->ttl,
							ptr->uoid[16],ptr->uoid[17],ptr->uoid[18],ptr->uoid[19]);
						}
						else{
							fprintf(logFilePtr,"%c %10ld.%03d %s KPAV %d %d %02x%02x%02x%02x\n",
							ptr->howMessageFormed,
							ptr->logTime.tv_sec,(int)(ptr->logTime.tv_usec/1000),nodeId,
							HEADER_LENGTH + ptr->dataLength,
							ptr->ttl,
							ptr->uoid[16],ptr->uoid[17],ptr->uoid[18],ptr->uoid[19]);
						}
					}
					break;

		case NOTIFY:
					{
						struct notifyMessage* notifyMessageObj = (struct notifyMessage*)ptr->messageStruct;
						if(ptr->howMessageFormed == 'r'){
							fprintf(logFilePtr,"%c %10ld.%03d %s NTFY %d %d %02x%02x%02x%02x %d\n",
							ptr->howMessageFormed,
							ptr->logTime.tv_sec,(int)(ptr->logTime.tv_usec/1000),nodeId,
							HEADER_LENGTH + ptr->dataLength,
							ptr->ttl,
							ptr->uoid[16],ptr->uoid[17],ptr->uoid[18],ptr->uoid[19],
							notifyMessageObj->errorCode);
						}
						else{
							fprintf(logFilePtr,"%c %10ld.%03d %s NTFY %d %d %02x%02x%02x%02x %d\n",
							ptr->howMessageFormed,
							ptr->logTime.tv_sec,(int)(ptr->logTime.tv_usec/1000),nodeId,
							HEADER_LENGTH + ptr->dataLength,
							ptr->ttl,
							ptr->uoid[16],ptr->uoid[17],ptr->uoid[18],ptr->uoid[19],
							notifyMessageObj->errorCode);
						}
					}

					break;

		case CHECK:
					{
						if(ptr->howMessageFormed == 'r'){
							fprintf(logFilePtr,"%c %10ld.%03d %s CKRQ %d %d %02x%02x%02x%02x\n",
							ptr->howMessageFormed,
							ptr->logTime.tv_sec,(int)(ptr->logTime.tv_usec/1000),nodeId,
							HEADER_LENGTH + ptr->dataLength,
							ptr->ttl,
							ptr->uoid[16],ptr->uoid[17],ptr->uoid[18],ptr->uoid[19]);
						}
						else{
							fprintf(logFilePtr,"%c %10ld.%03d %s CKRQ %d %d %02x%02x%02x%02x\n",
							ptr->howMessageFormed,
							ptr->logTime.tv_sec,(int)(ptr->logTime.tv_usec/1000),nodeId,
							HEADER_LENGTH + ptr->dataLength,
							ptr->ttl,
							ptr->uoid[16],ptr->uoid[17],ptr->uoid[18],ptr->uoid[19]);
						}
					}
					break;

		case RCHECK:
					{
						struct checkRespMessage* checkResponseMessageObj = (struct checkRespMessage*)ptr->messageStruct;
						if(ptr->howMessageFormed == 'r'){
							fprintf(logFilePtr,"%c %10ld.%03d %s CKRS %d %d %02x%02x%02x%02x %02x%02x%02x%02x\n",
							ptr->howMessageFormed,
							ptr->logTime.tv_sec,(int)(ptr->logTime.tv_usec/1000),nodeId,
							HEADER_LENGTH + ptr->dataLength,
							ptr->ttl,
							ptr->uoid[16],ptr->uoid[17],ptr->uoid[18],ptr->uoid[19],
							checkResponseMessageObj->checkMsgHeaderUOID[16],
							checkResponseMessageObj->checkMsgHeaderUOID[17],
							checkResponseMessageObj->checkMsgHeaderUOID[18],
							checkResponseMessageObj->checkMsgHeaderUOID[19]);
						}
						else{
							fprintf(logFilePtr,"%c %10ld.%03d %s CKRS %d %d %02x%02x%02x%02x %02x%02x%02x%02x\n",
							ptr->howMessageFormed,
							ptr->logTime.tv_sec,(int)(ptr->logTime.tv_usec/1000),nodeId,
							HEADER_LENGTH + ptr->dataLength,
							ptr->ttl,
							ptr->uoid[16],ptr->uoid[17],ptr->uoid[18],ptr->uoid[19],
							checkResponseMessageObj->checkMsgHeaderUOID[16],
							checkResponseMessageObj->checkMsgHeaderUOID[17],
							checkResponseMessageObj->checkMsgHeaderUOID[18],
							checkResponseMessageObj->checkMsgHeaderUOID[19]);
						}
					}

					break;

		case STATUS:
					{
						struct statusMessage* statusMessageObj = (struct statusMessage*)ptr->messageStruct;
						if(ptr -> howMessageFormed == 'r'){
							fprintf(logFilePtr,"%c %10ld.%03d %s STRQ %d %d %02x%02x%02x%02x %02x\n",
							ptr->howMessageFormed,
							ptr->logTime.tv_sec,(int)(ptr->logTime.tv_usec/1000),nodeId,
							HEADER_LENGTH + ptr->dataLength,
							ptr->ttl,
							ptr->uoid[16],ptr->uoid[17],ptr->uoid[18],ptr->uoid[19],
							statusMessageObj->statusType);
						}
						else{
							fprintf(logFilePtr,"%c %10ld.%03d %s STRQ %d %d %02x%02x%02x%02x %02x\n",
							ptr->howMessageFormed,
							ptr->logTime.tv_sec,(int)(ptr->logTime.tv_usec/1000),nodeId,
							HEADER_LENGTH + ptr->dataLength,
							ptr->ttl,
							ptr->uoid[16],ptr->uoid[17],ptr->uoid[18],ptr->uoid[19],
							statusMessageObj->statusType);
						}
					}
					break;

		case RSTATUS:
					{

						struct statusRespMessage* statusResponseMessageObj = (struct statusRespMessage*)ptr->messageStruct;
						if(ptr->howMessageFormed == 'r'){
							fprintf(logFilePtr,"%c %10ld.%03d %s STRS %d %d %02x%02x%02x%02x %02x%02x%02x%02x\n",
							ptr->howMessageFormed,
							ptr->logTime.tv_sec,(int)(ptr->logTime.tv_usec/1000),nodeId,
							HEADER_LENGTH + ptr->dataLength,
							ptr->ttl,
							ptr->uoid[16],ptr->uoid[17],ptr->uoid[18],ptr->uoid[19],
							statusResponseMessageObj->statusMsgHeaderUOID[16],
							statusResponseMessageObj->statusMsgHeaderUOID[17],
							statusResponseMessageObj->statusMsgHeaderUOID[18],
							statusResponseMessageObj->statusMsgHeaderUOID[19]);
						}
						else{
							fprintf(logFilePtr,"%c %10ld.%03d %s STRS %d %d %02x%02x%02x%02x %02x%02x%02x%02x\n",
							ptr->howMessageFormed,
							ptr->logTime.tv_sec,(int)(ptr->logTime.tv_usec/1000),nodeId,
							HEADER_LENGTH + ptr->dataLength,
							ptr->ttl,
							ptr->uoid[16],ptr->uoid[17],ptr->uoid[18],ptr->uoid[19],
							statusResponseMessageObj->statusMsgHeaderUOID[16],
							statusResponseMessageObj->statusMsgHeaderUOID[17],
							statusResponseMessageObj->statusMsgHeaderUOID[18],
							statusResponseMessageObj->statusMsgHeaderUOID[19]);
						}
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

		default:
				pthread_mutex_unlock(&logFileLock);
				return false;

	}
	fflush(logFilePtr);
	pthread_mutex_unlock(&logFileLock);
	return true;


}

bool handleSelfMessages(struct message *myMessage){
	struct connectionDetails *connectionDetailsObj;
	
	pthread_mutex_lock(&connectedNeighborsMapLock);

	connectedNeighborsMapIter = connectedNeighborsMap.begin();
	if(connectedNeighborsMapIter == connectedNeighborsMap.end()){

		pthread_mutex_unlock(&connectedNeighborsMapLock);
		//Handle Error - No Key-Value Found
		return false;
	}
	else{
		while(connectedNeighborsMapIter != connectedNeighborsMap.end()){
			connectionDetailsObj = connectedNeighborsMapIter -> second;
			
			pthread_mutex_lock(&connectionDetailsObj -> messagingQueueLock);
			connectionDetailsObj -> messagingQueue.push(myMessage);
			pthread_cond_signal(&connectionDetailsObj -> messagingQueueCV);
			pthread_mutex_unlock(&connectionDetailsObj -> messagingQueueLock);
			connectedNeighborsMapIter++;
		}
		pthread_mutex_unlock(&connectedNeighborsMapLock);
		return true;
	}
}

bool sendMessageToNode(struct message *myMessage){
	struct connectionDetails *connectionDetailsObj;

	string connectedNeighborsMapKeyTemp = string(myMessage -> connectedNeighborsMapKey);
	pthread_mutex_lock(&connectedNeighborsMapLock);

	connectedNeighborsMapIter = connectedNeighborsMap.find(connectedNeighborsMapKeyTemp);
	if(connectedNeighborsMapIter == connectedNeighborsMap.end()){

		pthread_mutex_unlock(&connectedNeighborsMapLock);
		//Handle Error - No Key-Value Found
		return false;
	}
	else{
		//while(connectedNeighborsMapIter != connectedNeighborsMap.end()){
			connectionDetailsObj = connectedNeighborsMapIter -> second;
			//pthread_mutex_lock(&connectionDetailsObj->connectionLock);
			pthread_mutex_lock(&connectionDetailsObj -> messagingQueueLock);
			connectionDetailsObj -> messagingQueue.push(myMessage);
			pthread_cond_signal(&connectionDetailsObj -> messagingQueueCV);
			pthread_mutex_unlock(&connectionDetailsObj -> messagingQueueLock);
			//pthread_mutex_unlock(&connectionDetailsObj->connectionLock);
		//}
		pthread_mutex_unlock(&connectedNeighborsMapLock);
		return true;
	}
}

void removeConnectedNeighborInfo(char *keyToMap)
{
	string key = string(keyToMap);
	pthread_mutex_lock(&connectedNeighborsMapLock);
	connectedNeighborsMapIter = connectedNeighborsMap.find(key);
	if(connectedNeighborsMapIter != connectedNeighborsMap.end())
	{
		//The connection details are present in the map so delete the entry
		//connectionDetails *cd = connectedNeighborsMapIter->second;
		//if(cd->toBeDeleted == 1)
		{

			//If reader/ Writer has marked the entry to be deleted I will delete it
			//close(cd->socketDesc);
			//free(cd);
			//connectedNeighborsMap.erase(key);
		}
		//else
		{

			//If not then I will indicate that the entry can be deleted as I am Exiting
			//cd->toBeDeleted = 1;
			//connectedNeighborsMap.insert(CONNMAPTYPE::value_type(key,cd));
		}
	}
	else
	{

	}
	pthread_mutex_unlock(&connectedNeighborsMapLock);
}


void ReConnectToBeaconNode(void *param){

	int status = 0;
	time_t mysystime;

	struct NodeInfo *nodeInfoCommon = &ndInfo;
	struct hostent *beacon;

	struct connectionDetails* cdTemp = (struct connectionDetails *)param;

	uint16_t retryTimer = 0;

	writeCommentToLogFile((char *)"Trying To Reconnect To Beacon");
	while(1){

			sleep(retryTimer);
			retryTimer = nodeInfoCommon->retry;
			int neighborbeaconSock=0;
			if((neighborbeaconSock = socket(AF_INET,SOCK_STREAM,0)) == -1)
			{
				printf("\nError creating socket\n");
				continue;
			}
			struct sockaddr_in serv_addr;
			char beaconip[16];
			serv_addr.sin_family = AF_INET;

			char char_key[100] = "\0";
			sprintf(char_key,"%s%d",cdTemp->hostname,cdTemp->wellKnownPort);
			string key = string(char_key);

			//Get the Peer information
			beacon = gethostbyname(cdTemp->hostname);
			memset(beaconip,0,16);
			strcpy(beaconip,inet_ntoa(*((struct in_addr *)beacon->h_addr_list[0])));
			free(beacon);

			serv_addr.sin_addr.s_addr = inet_addr(beaconip);
			serv_addr.sin_port = htons(cdTemp->wellKnownPort);

			//Send connection request
			status = connect(neighborbeaconSock,(struct sockaddr *)&serv_addr,sizeof(serv_addr));
			// connect to the host and port specified in
			if (status < 0)
			{
				free(beacon);
				continue;
			}
			else
			{
				connectionDetails *cd = new connectionDetails;

				//Get Peer information
				socklen_t len;
				struct sockaddr_storage addr;
				char ipstr[16];
				int port;
				len = sizeof(addr);
				getpeername(neighborbeaconSock, (struct sockaddr*)&addr, &len);
				struct sockaddr_in *s = (struct sockaddr_in *)&addr;
				port = ntohs(s->sin_port);
				inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof ipstr);

				//Prepare the connection details
				strcpy(cd->hostname,cdTemp->hostname);
				cd->port = cdTemp->wellKnownPort;
				cd->socketDesc = neighborbeaconSock;
				cd->notOperational=0;
				cd->isJoinConnection = 0;
				cd->threadsExitedCount = 0;
				cd->wellKnownPort = cdTemp->wellKnownPort;
				mysystime = time(NULL);
				cd->lastReadActivity = mysystime;
				cd->lastWriteActivity = mysystime;
				cd->helloStatus = 0;
				cd->tiebreak = 0;
				pthread_mutex_init(&cd->connectionLock,NULL);
				pthread_cond_init(&cd->messagingQueueCV, NULL);
				pthread_mutex_init(&cd->messagingQueueLock,NULL);

				//connectedNeighborsMap.erase(keyTemp);

				pthread_mutex_lock(&connectedNeighborsMapLock);
				freeConnectionDetails(cdTemp);
				cdTemp = NULL;
				connectedNeighborsMap.insert(CONNMAPTYPE::value_type(key,cd));
				pthread_mutex_unlock(&connectedNeighborsMapLock);

				char *mapkey = (char *)malloc(100);
				memset(mapkey,0,100);
				strcpy(mapkey,char_key);

				//Create reader and writer threads and give them the connection key
				pthread_t childThread1;
				pthread_create(&childThread1,NULL,(void* (*)(void*))reader,mapkey);


				pthread_t childThread2;
				pthread_create(&childThread2,NULL,(void* (*)(void*))writer,mapkey);

				//Create a hello message to be sent to all the beacons
				struct message *mymessage;
				struct helloMessage *myhelloMsg;

				myhelloMsg = createHello(nodeInfoCommon->hostname,nodeInfoCommon->port);

				mymessage = CreateMessageHeader(HELLO, /*Message Type */
												1, /* TTL */
												myhelloMsg -> dataLength, /*data body length*/
												NULL, /*char *messageHeader*/
												NULL, /*char *message*/
												(void*)myhelloMsg, /*void *messageStruct*/
												mapkey,/*char* temp_connectedNeighborsMapKey*/
												MYMESSAGE, /*My Message or notmymessage*/
												nodeInfoCommon /*Node Info Pointer*/);


				char keytype[21] = "\0";
				int i = 0;
				while(i<20)
				{
					keytype[i] = *(mymessage->uoid + i);
					*(cd->hellomsguoid + i) = *(mymessage->uoid + i);
					i++;
				}
				string uoidKey = string(keytype);
				cd->hellomsguoid[i] = '\0';
				//Add Hello Message to Cache
				pthread_mutex_lock(&messageCacheMapLock);
				messageCacheMap.insert(MESSCACHEMAPTYPE::value_type(uoidKey,mymessage));
				pthread_mutex_unlock(&messageCacheMapLock);

				//Create messsage header and add msg body created above
				pthread_mutex_lock(&helloMsgMapLock);
				helloMsgMap.insert(HELLOMSGMAPTYPE::value_type(mapkey,1));
				pthread_mutex_unlock(&helloMsgMapLock);
				//Call the function that sends the message to all the connections
				sendMessageToNode(mymessage);

				pthread_exit(NULL);
			}
	}
}

/*
 *  Free Memory Data Structures
 */

void freeMessage(struct message *ptr){
		if(ptr==NULL)
			return;

		if(ptr!= NULL){

			switch(ptr->messType){
				case JOIN:
				{
					struct joinMessage* generalPtr = (struct joinMessage*)ptr ->messageStruct;
					if(generalPtr != NULL)
						free(generalPtr);
					ptr ->messageStruct = NULL;
					generalPtr = NULL;
				}
				break;
				case RJOIN:
				{
					struct joinRespMessage* generalPtr = (struct joinRespMessage*) ptr ->messageStruct;
					if(generalPtr != NULL)
						free(generalPtr);
					ptr ->messageStruct = NULL;
					generalPtr = NULL;
				}
				break;

				case HELLO:
				{
					struct helloMessage* generalPtr = (struct helloMessage*) ptr ->messageStruct;
					if(generalPtr != NULL)
						free(generalPtr);
					ptr ->messageStruct = NULL;
					generalPtr = NULL;


				}
				break;
				case RCHECK:
				{
					struct checkRespMessage* generalPtr = (struct checkRespMessage*) ptr ->messageStruct;
					if(generalPtr != NULL)
						free(generalPtr);
					ptr ->messageStruct = NULL;
					generalPtr = NULL;
				}
				break;

				case STATUS:
				{
					// not implemented yet.
					struct statusMessage* generalPtr = (struct statusMessage*) ptr ->messageStruct;
					if(generalPtr != NULL)
						free(generalPtr);
					ptr ->messageStruct = NULL;
					generalPtr = NULL;
				}
				break;

				case RSTATUS:
				{
					// not implemented yet.
					struct statusRespMessage* generalPtr = (struct statusRespMessage*) ptr ->messageStruct;
					if(generalPtr != NULL)
						free(generalPtr);
					ptr ->messageStruct = NULL;
					generalPtr = NULL;
				}
				break;

					default: break;
			}
			/*if(ptr->message != NULL)
			{
				char *tempMsg = (char *)ptr->message;
				free(tempMsg);
				ptr->message = NULL;
				tempMsg = NULL;
			}
			if(ptr->messageHeader != NULL)
			{
				char *tempMsg = (char *)ptr->messageHeader;
				free(tempMsg);
				ptr->messageHeader = NULL;
				tempMsg = NULL;
			}*/
			free(ptr);
		}

		ptr = NULL;
		return;
}

void freeConnectionDetails(struct connectionDetails *ptr){

	if(ptr == NULL)
		return;

	struct message *messageStruct;

	while(!ptr->messagingQueue.empty()){
		messageStruct = ptr -> messagingQueue.front();
		ptr -> messagingQueue.pop();
		freeMessage(messageStruct);
		messageStruct = NULL;
	}
	close(ptr->socketDesc);
	free(ptr);
	ptr = NULL;
	return;
}

void freeConnectionDetailsMap(){

	connectedNeighborsMapIter = connectedNeighborsMap.begin();

	while(connectedNeighborsMapIter != connectedNeighborsMap.end()){
		struct connectionDetails *cd;
		cd = connectedNeighborsMapIter->second;
		connectedNeighborsMap.erase(connectedNeighborsMapIter->first);
		//pthread_mutex_lock(&cd->connectionLock);
		freeConnectionDetails(cd);
		//pthread_mutex_unlock(&cd->connectionLock);
		cd = NULL;
		connectedNeighborsMapIter++;
	}
	return;
}

void freeMessageCache(){
	messageCacheMapIter = messageCacheMap.begin();

	while(messageCacheMapIter != messageCacheMap.end()){
		struct message *tempmsg;
		tempmsg = messageCacheMapIter->second;
		freeMessage(tempmsg);
		tempmsg = NULL;
		messageCacheMapIter++;
	}
	return;
}

/*
 * ----------------------------------------------------
 * ----------------------------------------------------
 */
void printConnectedNeighborInforMap()
{
	pthread_mutex_lock(&connectedNeighborsMapLock);
	printf("\n\n---------- Connected Peer Map -------------------------------\n");
	for(connectedNeighborsMapIter=connectedNeighborsMap.begin();connectedNeighborsMapIter != connectedNeighborsMap.end();connectedNeighborsMapIter++)
	{
		cout<<"\nHost : "<<connectedNeighborsMapIter->first<< " Operational State: " << connectedNeighborsMapIter->second->notOperational <<" IsJoin : "<<connectedNeighborsMapIter->second->isJoinConnection<<" ThreadsExitedCount = "<<connectedNeighborsMapIter->second->threadsExitedCount<<" HelloState : "<<connectedNeighborsMapIter->second->helloStatus<<" Tie : "<<connectedNeighborsMapIter->second->tiebreak<<"\n";
	}
	printf("\n---------------------------------------------------------------\n");
	pthread_mutex_unlock(&connectedNeighborsMapLock);
}

void printJoinMessageInfoMap()
{
	struct joinResponseDetails *joinRespDet;
	pthread_mutex_lock(&joinMsgMapLock);
	printf("\n\n---------- Join Responses Map ---------\n");
	for(joinMsgMapIter=joinMsgMap.begin();joinMsgMapIter != joinMsgMap.end();joinMsgMapIter++)
	{
		joinRespDet = joinMsgMapIter->second;
		cout<<"\nKey : "<<joinMsgMapIter->first<< " HostName: " << joinRespDet->hostName <<" Port: "<<joinRespDet->portNum<<" Distance: "<<joinRespDet->distToNode<<"\n";
	}
	printf("\n----------------------------------------\n");
	pthread_mutex_unlock(&joinMsgMapLock);
}
