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

#include "headers.h"
#include "systemDataStructures.h"
#include "ntwknode.h"
#include "keyboardThread.h"
#include <iostream>

extern MESSCACHEMAPTYPE messageCacheMap;
extern MESSCACHEMAPTYPE::iterator messageCacheMapIter;
extern pthread_mutex_t messageCacheMapLock;
extern pthread_cond_t messageCacheMapCV;  // not needed as of now


extern CONNMAPTYPE connectedNeighborsMap;
extern CONNMAPTYPE::iterator connectedNeighborsMapIter;
extern pthread_mutex_t connectedNeighborsMapLock;


extern CHECKMSGMAPTYPE checkMsgMap;
extern CHECKMSGMAPTYPE::iterator checkMsgMapIter;
extern pthread_mutex_t checkMsgMapLock;


pthread_mutex_t checkThreadLock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t checkThreadCV = PTHREAD_COND_INITIALIZER;
int checkStatus = 0;
extern NodeInfo *ndInfoTemp;

string prevsent;

void checkThread(void * ptr){

	struct NodeInfo *ndInfoTemp;
	ndInfoTemp = (struct NodeInfo *)ptr;

	prevsent = string("Invalid");
	while(1){

		pthread_mutex_lock(&systemWideLock);
		if(autoshutdownFlag == 1)
		{
			pthread_mutex_unlock(&systemWideLock);
			pthread_exit(NULL);
		}
		pthread_mutex_unlock(&systemWideLock);

		pthread_mutex_lock(&checkThreadLock);

		if(checkStatus == 0){
			pthread_cond_wait(&checkThreadCV, &checkThreadLock);
		}

		if(autoshutdownFlag == 1)
		{
			pthread_mutex_unlock(&checkThreadLock);
			pthread_exit(NULL);
		}

		printf("\n\nNode Lost a connection hence sending CHECK message\n");
		printf("\nservant:%d> ",ndInfoTemp->port);
		//Got Signal, Create Check Message
		struct message* messageHeaderObj;
		uint32_t msglen = 0;
		messageHeaderObj = CreateMessageHeader(CHECK, /*message Type*/
										ndInfoTemp -> ttl, /*TTL*/
										msglen, /*datalenght of body*/
										NULL, /*char *messageHeader*/
										NULL, /*char *message*/
										(void*)NULL, /*void *messageStruct*/
										NULL,/*char* temp_connectedNeighborsMapKey*/
										MYMESSAGE, /*MyMessage or notMyMessage */
										ndInfoTemp /*Node Info Pointer*/);


		char keytype[256] = "\0";
		int i = 0;
		while(i<20)
		{
			keytype[i] = *(messageHeaderObj->uoid + i);
			i++;
		}
		keytype[i] = '\0';
		string uoid = string(keytype);

		pthread_mutex_lock(&messageCacheMapLock);
		messageCacheMapIter = messageCacheMap.find(prevsent);
		if(messageCacheMapIter != messageCacheMap.end())
		{
			messageCacheMap.erase(prevsent);
		}
		prevsent = uoid;
		messageCacheMap.insert(MESSCACHEMAPTYPE::value_type(uoid,messageHeaderObj));
		pthread_mutex_unlock(&messageCacheMapLock);

		// Send Check to connected Nodes:
			struct connectionDetails *connectionDetailsObj;
			pthread_mutex_lock(&connectedNeighborsMapLock);
				connectedNeighborsMapIter = connectedNeighborsMap.begin();

				while(connectedNeighborsMapIter != connectedNeighborsMap.end()){

					connectionDetailsObj = connectedNeighborsMapIter -> second;
					pthread_mutex_lock(&connectionDetailsObj->connectionLock);
					if(connectionDetailsObj -> notOperational == 0 && // operational connection
						connectionDetailsObj -> isJoinConnection == 0 && // not connected to node for join request
						connectionDetailsObj -> threadsExitedCount == 0 &&
						connectionDetailsObj ->helloStatus == 1){ // reader/writer both are working

								pthread_mutex_lock(&connectionDetailsObj -> messagingQueueLock);
								//cout << "\nPut Check Message to queue for forward\n";
								connectionDetailsObj -> messagingQueue.push(messageHeaderObj);
								pthread_cond_signal(&connectionDetailsObj -> messagingQueueCV);
								pthread_mutex_unlock(&connectionDetailsObj -> messagingQueueLock);


						}
					pthread_mutex_unlock(&connectionDetailsObj->connectionLock);
					connectedNeighborsMapIter++;
				}
			pthread_mutex_unlock(&connectedNeighborsMapLock);
			checkStatus = 0;
		pthread_mutex_unlock(&checkThreadLock);
	}
}
