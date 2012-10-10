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
#include "beaconNode.h"
#include <iostream>

extern void scanAndEraseMsgCache();
extern void scanAndEraseConnectedNeighborsMap(int);
extern void checkLastReadActivityOfConnection(int);
extern void checkLastWriteActivityOfConnection();
extern void resetBeaconNode();
extern int keyboardShutdown;

extern INDEXMAPTYPE KeywordIndexMap;
extern INDEXMAPTYPE SHA1IndexMap;
extern INDEXMAPTYPE fileNameIndexMap;
extern LISTTYPE LRUList;
extern LISTTYPE PermFileList;

void beaconTimer(void *param)
{
//	struct NodeInfo *ndInfoBeaconTimer = (NodeInfo *)param;
	struct timeval tvtime;
	tvtime.tv_sec = 0;
	tvtime.tv_usec = 100000;
	while(1)
	{
		select(NULL,NULL,NULL,NULL,&tvtime);
		/*
		 * Check if its time to quit
		 * If YES - 1.inform all the threads
		 * 			2.clean up the memory allocated so far
		 * 			3.pthread_exit
		 * Else
		 * 		Continue
		 */
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

			pthread_kill(listenerThread,SIGUSR1);

			pthread_mutex_lock(&eventDispatcherQueueLock);
			pthread_cond_signal(&eventDispatcherQueueCV);
			pthread_mutex_unlock(&eventDispatcherQueueLock);

			pthread_kill(keyboardThread,SIGUSR1);

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

			pthread_mutex_lock(&helloMsgMapLock);
			for(helloMsgMapIter = helloMsgMap.begin(); helloMsgMapIter != helloMsgMap.end(); helloMsgMapIter++)
			{
				helloMsgMap.erase(helloMsgMapIter->first);
			}
			pthread_mutex_unlock(&helloMsgMapLock);

			/*Write the indexes and File lists to Minifile system*/
			writeIndexToFile(KeywordIndexMap,kwrdIndexFileName);

			writeIndexToFile(SHA1IndexMap,sha1IndexFileName);

			writeIndexToFile(fileNameIndexMap,fileNameIndexFileName);

			writeListToFile(LRUList,lruListFileName);

			writeListToFile(PermFileList,permFileListFileName);

			pthread_exit(NULL);
		}
		pthread_mutex_unlock(&systemWideLock);

		// 1. Traverse Message Cache and erase Messages apart from Hello
				// For Hello:
					/*
					 *  As Hello Message Expired - so close connection
					 */
		select(NULL,NULL,NULL,NULL,&tvtime);
			scanAndEraseMsgCache();

		// 2. Connection Details to check
		tvtime.tv_sec = 0;
		tvtime.tv_usec = 50000;
		select(NULL,NULL,NULL,NULL,&tvtime);
		scanAndEraseConnectedNeighborsMap(1);

		tvtime.tv_sec = 0;
		tvtime.tv_usec = 25000;
		select(NULL,NULL,NULL,NULL,&tvtime);
		checkLastWriteActivityOfConnection();

		tvtime.tv_sec = 0;
		tvtime.tv_usec = 100000;
	}
	pthread_exit(NULL);
}

