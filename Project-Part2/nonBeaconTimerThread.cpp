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

#include "systemDataStructures.h"
#include "headers.h"
#include "nonBeaconNode.h"
#include <iostream>

extern int no_of_join_responses;

struct NodeInfo *ndInfoTimer;
time_t mysystime = 0;
int no_of_hello_expired=0;
extern int participating;

extern int restartState;

extern pthread_t keyboardThreadNonbeacon;
extern pthread_t nonBeaconListenerThread;

extern pthread_mutex_t checkThreadLock;
extern pthread_cond_t checkThreadCV;

extern void scanAndEraseConnectedNeighborsMap(int);
extern void checkLastReadActivityOfConnection(int);
extern void checkLastWriteActivityOfConnection();

extern int keyboardShutdown;

extern INDEXMAPTYPE KeywordIndexMap;
extern INDEXMAPTYPE SHA1IndexMap;
extern INDEXMAPTYPE fileNameIndexMap;
extern LISTTYPE LRUList;
extern LISTTYPE PermFileList;

bool handleMessageAtTimer(struct message *);

void nonBeaconTimer(void *param)
{
	ndInfoTimer = (struct NodeInfo *)param;

	/*
	 * Check if the JOIN msg has timed out
	 */

	struct message *mapmessage;
	mysystime = 0;
	struct timeval tvTimer;
	bool msgstatus = true;
	no_of_hello_expired = 0;

	/*
	 * From this point onwards the timer has taken the decision that the node will participate in
	 * joining the SERVANT network
	 */

	tvTimer.tv_sec = 0;
	tvTimer.tv_usec = 100000;

	while(1)
	{
		/*
		 * Check the
		 */
		select(NULL,NULL,NULL,NULL,&tvTimer);


		pthread_mutex_lock(&systemWideLock);
		if(autoshutdownFlag == 1)
		{
			/*
			 * send signal to all the threads that are waiting:
			 * 1. Listener thread
			 * 2. dispathcher thread
			 * 3. Keyboard Thread
			 * 4. all writer threads from the connections establisthed so far
			 */

			if(keyboardShutdown == 1)
			{
				pthread_mutex_unlock(&systemWideLock);
				pthread_exit(NULL);
			}
			pthread_mutex_unlock(&systemWideLock);


			pthread_kill(nonBeaconListenerThread,SIGUSR1);


			pthread_mutex_lock(&eventDispatcherQueueLock);
			pthread_cond_signal(&eventDispatcherQueueCV);
			pthread_mutex_unlock(&eventDispatcherQueueLock);


			pthread_mutex_lock(&checkThreadLock);
			pthread_cond_signal(&checkThreadCV);
			pthread_mutex_unlock(&checkThreadLock);

			pthread_kill(keyboardThreadNonbeacon,SIGUSR1);


			connectionDetails *cd;
			pthread_mutex_lock(&connectedNeighborsMapLock);
			for(connectedNeighborsMapIter = connectedNeighborsMap.begin(); connectedNeighborsMapIter != connectedNeighborsMap.end();connectedNeighborsMapIter++)
			{
				cd = connectedNeighborsMapIter->second;
				pthread_mutex_lock(&cd->connectionLock);
				cd->notOperational = 1;
				pthread_mutex_lock(&cd->messagingQueueLock);
				pthread_cond_signal(&cd->messagingQueueCV);
				pthread_mutex_unlock(&cd->messagingQueueLock);
				pthread_mutex_unlock(&cd->connectionLock);
			}
			pthread_mutex_unlock(&connectedNeighborsMapLock);

			/*Write the indexes and File lists to Minifile system*/
			writeIndexToFile(KeywordIndexMap,kwrdIndexFileName);

			writeIndexToFile(SHA1IndexMap,sha1IndexFileName);

			writeIndexToFile(fileNameIndexMap,fileNameIndexFileName);

			writeListToFile(LRUList,lruListFileName);

			writeListToFile(PermFileList,permFileListFileName);

			pthread_exit(NULL);
		}
		pthread_mutex_unlock(&systemWideLock);

		pthread_mutex_lock(&messageCacheMapLock);
		no_of_hello_expired = 0;
		for(messageCacheMapIter = messageCacheMap.begin(); messageCacheMapIter != messageCacheMap.end(); messageCacheMapIter++)
		{
			mapmessage = messageCacheMapIter->second;
			mysystime = time(NULL);
			if(((((mysystime - mapmessage->timestamp) >= ndInfoTimer->msgLifetime && mapmessage->messType != HELLO)
				|| ((mysystime - mapmessage->timestamp) >= ndInfoTimer->keepAliveTimeout && mapmessage->messType == HELLO)) && mapmessage->messType != GET)
				|| ((mysystime - mapmessage->timestamp) >= ndInfoTimer->getMsgLifeTime && mapmessage->messType == GET))
			{
				//messageCacheMap.erase(messageCacheMapIter->first);
				if(mapmessage->myMessage == MYMESSAGE){
					if(mapmessage->messType == HELLO || mapmessage->messType == CHECK || mapmessage->messType == SEARCH)
					{
						messageCacheMap.erase(messageCacheMapIter->first);
						msgstatus = handleMessageAtTimer(mapmessage);
						if(msgstatus == false)
						{

							messageCacheMap.erase(messageCacheMapIter->first);
							pthread_mutex_unlock(&messageCacheMapLock);
							pthread_exit(NULL);
						}
					}
					else
					{
						messageCacheMap.erase(messageCacheMapIter->first);
					}
				}
				else{
					// If not mymessage and life time is expired I need to erase it from Message Cache,
					// irrespective of its type
					messageCacheMap.erase(messageCacheMapIter->first);
				}

			}
		}
		pthread_mutex_unlock(&messageCacheMapLock);


		tvTimer.tv_sec = 0;
		tvTimer.tv_usec = 50000;
		select(NULL,NULL,NULL,NULL,&tvTimer);
		scanAndEraseConnectedNeighborsMap(0);

		tvTimer.tv_sec = 0;
		tvTimer.tv_usec = 25000;
		select(NULL,NULL,NULL,NULL,&tvTimer);
		checkLastWriteActivityOfConnection();

		tvTimer.tv_sec = 0;
		tvTimer.tv_usec = 100000;
	}
	pthread_exit(NULL);
}


bool handleMessageAtTimer(struct message *mapmessage){

	switch(mapmessage->messType){


		// case JOIN:   	Not to be handled
		// case RJOIN:  	Not to be handled
		// case NOTIFY: 	Not to be handled
		// case RCHECK: 	Not to be handled
		// case STATUS: 	Not to be handled
		// case RSTATUS:	Not to be handled
		// case KEEPALIVE:	Not to be handled
		case HELLO:
					{
						/*
						 * If the hellomsg has expired and the (no_of_connection - expired hellos) > minNeighbors
						 * 		I have to close all my current connections and go for soft restart
						 */

						no_of_hello_expired++;
						return true;
					}
				break;

		case CHECK:
				//as Msg time has expired and there was no response from any beacons we need to shut down.
					{
						// Traverse Connection Details and put notify message to its message queue,
						// empty the queue before adding notify message to queue.
						struct notifyMessage *myNotifyMsg = createNotify(0);
						struct message *mymessage;

						mymessage = CreateMessageHeader(NOTIFY, /*Mess Type*/
														1, 		/* TTL */
														myNotifyMsg -> dataLength, /* DataLength Message Body */
														NULL, /* char *messageHeader */
														NULL, /* char *message */
														(void*)myNotifyMsg, /* void *messageStruct */
														NULL,/* char* temp_connectedNeighborsMapKey */
														MYMESSAGE, /* MyMessage or NotMyMessage*/
														ndInfoTimer /*Node Info Pointer*/);

							pthread_mutex_lock(&systemWideLock);
							autoshutdownFlag = 1;
							pthread_mutex_unlock(&systemWideLock);


							pthread_kill(nonBeaconListenerThread,SIGUSR1);


							pthread_mutex_lock(&eventDispatcherQueueLock);
							pthread_cond_signal(&eventDispatcherQueueCV);
							pthread_mutex_unlock(&eventDispatcherQueueLock);


							pthread_mutex_lock(&checkThreadLock);
							pthread_cond_signal(&checkThreadCV);
							pthread_mutex_unlock(&checkThreadLock);

							pthread_kill(keyboardThreadNonbeacon,SIGUSR1);


							connectionDetails *cd;
							pthread_mutex_lock(&connectedNeighborsMapLock);
							for(connectedNeighborsMapIter = connectedNeighborsMap.begin(); connectedNeighborsMapIter != connectedNeighborsMap.end();connectedNeighborsMapIter++)
							{
								cd = connectedNeighborsMapIter->second;
								pthread_mutex_lock(&cd->connectionLock);
								cd->notOperational = 1;
								pthread_mutex_lock(&cd->messagingQueueLock);
								pthread_cond_signal(&cd->messagingQueueCV);
								pthread_mutex_unlock(&cd->messagingQueueLock);
								close(cd->socketDesc);
								pthread_mutex_unlock(&cd->connectionLock);
							}
							pthread_mutex_unlock(&connectedNeighborsMapLock);

							pthread_mutex_lock(&joinMsgMapLock);
							for(joinMsgMapIter = joinMsgMap.begin();joinMsgMapIter!= joinMsgMap.end();joinMsgMapIter++)
							{
								joinMsgMap.erase(joinMsgMapIter->first);
							}
							pthread_mutex_unlock(&joinMsgMapLock);

							pthread_mutex_lock(&helloMsgMapLock);
							for(helloMsgMapIter = helloMsgMap.begin();helloMsgMapIter != helloMsgMap.end();helloMsgMapIter++)
							{
								helloMsgMap.erase(helloMsgMapIter->first);
							}
							pthread_mutex_unlock(&helloMsgMapLock);

							pthread_mutex_lock(&systemWideLock);
							restartState = 1;
							pthread_mutex_unlock(&systemWideLock);
						return false;
					}
					break;
		case SEARCH:
					{
						pthread_mutex_lock(&keyboardLock);
						pthread_cond_signal(&keyboardCV);
						pthread_mutex_unlock(&keyboardLock);
					}
					break;
		default:
				{//Error - Message Type not defined

					return false;
				}
	}

	return true;
}

