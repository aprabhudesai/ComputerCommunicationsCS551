/*
 * timerThread.cpp
 *
 *  Created on: Mar 16, 2012
 *      Author: abhishek
 */

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

	if(participating == 0)
	{
		tvTimer.tv_sec = ndInfoTimer->joinTimeout;
		tvTimer.tv_usec = 0;
		select(NULL,NULL,NULL,NULL,&tvTimer);
		//sleep(5);

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

			printf("\n\nTimer Thread signal LISTENER\n");
			pthread_kill(nonBeaconListenerThread,SIGUSR1);

			printf("\n\nTimer Thread signal DISPATCHER\n");
			pthread_mutex_lock(&eventDispatcherQueueLock);
			pthread_cond_signal(&eventDispatcherQueueCV);
			pthread_mutex_unlock(&eventDispatcherQueueLock);

			printf("\n\nTimer Thread signal CHECK THREAD\n");
			pthread_mutex_lock(&checkThreadLock);
			pthread_cond_signal(&checkThreadCV);
			pthread_mutex_unlock(&checkThreadLock);

			pthread_kill(keyboardThreadNonbeacon,SIGUSR1);

			printf("\n\nTimer Thread signal READER-WRITER\n");
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
			pthread_exit(NULL);
		}
		pthread_mutex_unlock(&systemWideLock);

		/*
		 * First check if the JOIN message timeout has occurred
		 */
		pthread_mutex_lock(&messageCacheMapLock);
		for(messageCacheMapIter = messageCacheMap.begin(); messageCacheMapIter != messageCacheMap.end(); messageCacheMapIter++)
		{
			messageCacheMap.erase(messageCacheMapIter->first);
		}
		pthread_mutex_unlock(&messageCacheMapLock);


		/*
		 * Create a notify message to send over the temporary JOIN connection
		 */
		//struct notifyMessage *myNotifyMsg = createNotify(0);
		//struct message *mymessage;

		//mymessage = CreateMessageHeader(NOTIFY, /*Mess Type*/
		//		1, /* TTL */
		//		myNotifyMsg -> dataLength, /*DataLength Message Body */
		//		NULL, /*char *messageHeader*/
		//		NULL, /*char *message*/
		//		(void*)myNotifyMsg, /*void *messageStruct*/
		//		NULL,/*char* temp_connectedNeighborsMapKey*/
		//		MYMESSAGE, /* MyMessage or NotMyMessage*/
		//		ndInfoTimer /*Node Info Pointer*/);

		/*char keytype[256] = "\0";
		int i = 0;
		while(i<20)
		{
			keytype[i] = *(mymessage->uoid + i);
			i++;
		}
		string keyTemp = string(keytype);*/

		/*
		 * Add the message to the cache
		 */
		/*pthread_mutex_lock(&messageCacheMapLock);
		messageCacheMap.insert(MESSCACHEMAPTYPE::value_type(keytype,mymessage));
		pthread_mutex_unlock(&messageCacheMapLock);
*/
		connectionDetails *cd;
		pthread_mutex_lock(&connectedNeighborsMapLock);
		for(connectedNeighborsMapIter = connectedNeighborsMap.begin(); connectedNeighborsMapIter != connectedNeighborsMap.end(); connectedNeighborsMapIter++)
		{
			cd = connectedNeighborsMapIter->second;
			pthread_mutex_lock(&cd->connectionLock);
			cd->notOperational = 1;
			pthread_mutex_unlock(&cd->connectionLock);
			pthread_mutex_lock(&cd->messagingQueueLock);
			//cd->messagingQueue.push(mymessage);
			pthread_cond_signal(&cd->messagingQueueCV);
			pthread_mutex_unlock(&cd->messagingQueueLock);
			//close(cd->socketDesc);
		}
		pthread_mutex_unlock(&connectedNeighborsMapLock);


		pthread_mutex_lock(&systemWideLock);

		if(no_of_join_responses < ndInfoTimer->initNeighbors)
		{
			//pthread_mutex_lock(&systemWideLock);
			autoshutdownFlag = 1;
			pthread_mutex_unlock(&systemWideLock);


			pthread_mutex_lock(&eventDispatcherQueueLock);
			pthread_cond_signal(&eventDispatcherQueueCV);
			pthread_mutex_unlock(&eventDispatcherQueueLock);

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


			pthread_exit(NULL);
		}
		pthread_mutex_unlock(&systemWideLock);
	}
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
			pthread_exit(NULL);
		}
		pthread_mutex_unlock(&systemWideLock);

		pthread_mutex_lock(&messageCacheMapLock);
		no_of_hello_expired = 0;
		for(messageCacheMapIter = messageCacheMap.begin(); messageCacheMapIter != messageCacheMap.end(); messageCacheMapIter++)
		{
			mapmessage = messageCacheMapIter->second;
			mysystime = time(NULL);
			if(((mysystime - mapmessage->timestamp) >= ndInfoTimer->msgLifetime && mapmessage->messType != HELLO) || ((mysystime - mapmessage->timestamp) >= ndInfoTimer->keepAliveTimeout && mapmessage->messType == HELLO))
			{
				//messageCacheMap.erase(messageCacheMapIter->first);
				if(mapmessage->myMessage == MYMESSAGE){
					if(mapmessage->messType == HELLO || mapmessage->messType == CHECK)
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

		//tvTimer.tv_sec = 0;
		//tvTimer.tv_usec = 25000;
		//select(NULL,NULL,NULL,NULL,&tvTimer);
		//checkLastReadActivityOfConnection(0);

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

//						struct connectionDetails *connectionDetailsObj;
						/*pthread_mutex_lock(&connectedNeighborsMapLock);
							connectedNeighborsMapIter = connectedNeighborsMap.begin();

							while(connectedNeighborsMapIter != connectedNeighborsMap.end()){

								connectionDetailsObj = connectedNeighborsMapIter -> second;
								pthread_mutex_lock(&connectionDetailsObj->connectionLock);
								if(connectionDetailsObj -> notOperational == 0 && // operational connection
									connectionDetailsObj -> isJoinConnection == 0 && // not connected to node for join request
									connectionDetailsObj -> threadsExitedCount == 0 &&
									connectionDetailsObj->helloStatus == 1){ // reader/writer both are working

										pthread_mutex_lock(&connectionDetailsObj -> messagingQueueLock);
										cout << "\nPut Notify Message to queue for forward\n";
										if(connectionDetailsObj -> messagingQueue.empty())
											connectionDetailsObj -> messagingQueue.push(mymessage);
										pthread_cond_signal(&connectionDetailsObj -> messagingQueueCV);
									pthread_mutex_unlock(&connectionDetailsObj -> messagingQueueLock);
								}
								pthread_mutex_unlock(&connectionDetailsObj->connectionLock);
								connectedNeighborsMapIter++;
							}
							pthread_mutex_unlock(&connectedNeighborsMapLock);
							*/
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
		default:
				{//Error - Message Type not defined

					return false;
				}
	}

	return true;
}

