/*
 * commonThreads.cpp
 *
 *  Created on: Mar 10, 2012
 *      Author: abhishek
 */

#include "headers.h"
#include "systemDataStructures.h"
#include "ntwknode.h"
#include "keyboardThread.h"
#include "nonBeaconNode.h"
#include <iostream>
#include "stdio.h"

struct NodeInfo *ndInf;

extern char extfilecmd[256];
extern int ttlcmd;
extern FILE *extFilePtr;
extern pthread_mutex_t extFileLock;

extern int keyboardShutdown;

extern sigset_t signalSetInt;
extern struct sigaction actInt;
extern sigset_t signalSetUSR1;
extern struct sigaction actUSR1;

extern STATUSMSGMAPTYPE statusMsgMap;
extern STATUSMSGMAPTYPE::iterator statusMsgMapIter;
extern pthread_mutex_t statusMsgMapLock;

extern pthread_t nonBeaconListenerThread;
extern pthread_t listenerThread;
extern pthread_mutex_t eventDispatcherQueueLock;
extern pthread_cond_t eventDispatcherQueueCV;

extern pthread_mutex_t keyboardLock;
extern pthread_cond_t keyboardCV;

extern struct statusRespMessage * createSelfStatus(struct NodeInfo *,uint8_t);
extern bool writeInfoToStatusFile(statusRespMessage *);
extern void clearStatusMsgMap();

void getTTLAndExtFileFromCmd(char *cmd,int msgtype)
{
	char *rem;
	rem = strchr(cmd,' ');
	ttlcmd = 0;
	memset(extfilecmd,0,256);
	while(rem!=NULL)
	{
		if(strstr(rem,"neighbors") != NULL || strstr(rem,"files") != NULL)
		{
			rem = strchr(rem+1,' ');
			char *token;
			token = strtok(rem," ");
			ttlcmd = atoi(token);
			rem = strchr(rem+1,' ');
			token = NULL;
			token = strtok(rem," ");
			//strcpy(extfilecmd,ndInf->homeDir);
			//strcat(extfilecmd,"/");
			strcpy(extfilecmd,token);
			extfilecmd[strlen(token) - 1] = '\0';
			break;
		}
		cmd = rem + 1;
		rem = strchr(rem,' ');
	}
}

int getStatusMsgType(char *cmd)
{
	if(strstr(cmd,"neighbors") != NULL)
	{
		return 1;
	}
	if(strstr(cmd,"files") != NULL)
	{
		return 2;
	}
	else
		return 0;
}

int parseCmd(char *cmd)
{
	int msgtype = 0;
	if(strstr(cmd,"shutdown") != NULL)
	{
		//Set the system shutdown flag
		return 3;
	}
	else
	{
		msgtype = getStatusMsgType(cmd);
		getTTLAndExtFileFromCmd(cmd,msgtype);
		return msgtype;
	}
}

int checkValidityOfCommand(char *cmd)
{
	if((strstr(cmd,"shutdown") != NULL) || strstr(cmd,"status neighbors") != NULL || strstr(cmd,"status files") != NULL)
	{
		if(strstr(cmd,"status neighbors") != NULL)
		{
			int i = 0,no=0,len = strlen(cmd);
			while(i < len)
			{
				if(cmd[i] == ' ')
					no++;
				i++;
			}
			if(no != 3)
				return 0;
			else
				return 1;
		}
		else
		{
			return 1;
		}
	}

	else
		return 0;
}

void sigintHandler(int signo)
{
	/*
	 * Handle the SIG INT signal here
	 */
	//printf("\n\n-----------<-----><-----> In SIGINT handler <-----><----->------------\n\n");
	clearStatusMsgMap();
	actInt.sa_handler = sigintHandler;
		sigaction(SIGINT, &actInt, NULL);
	//printf("\nservant:%d> ",ndInf->port);
}


void * createKeyboardThread(void *param)
{
	ndInf = (NodeInfo *)param;
	struct timeval tm;
	int cmdtype = 0;
	char usercmd[256] = "\0";
	int nodetype = checkNodeType(ndInf->port);
	actInt.sa_handler = sigintHandler;
	sigaction(SIGINT, &actInt, NULL);
	//signal(SIGUSR1,sigUSR1Handler);
	pthread_sigmask(SIG_UNBLOCK, &signalSetInt, NULL);
	actUSR1.sa_handler = sigUSR1Handler;
	sigaction(SIGUSR1, &actUSR1, NULL);
	pthread_sigmask(SIG_UNBLOCK, &signalSetUSR1, NULL);
	fd_set readset;
	FD_ZERO(&readset);
	FD_SET(fileno(stdin), &readset);
	tm.tv_sec = 0;
	tm.tv_usec = 100000;
	printf("\nservant:%d> ",ndInf->port);
	while(1)
	{
		memset(usercmd,0,256);
		int state = select(NULL, &readset,(fd_set *) 0,(fd_set *) 0,&tm);
		select(NULL, NULL,NULL,NULL,&tm);
		/*if(timeToQuit == 1)
			{
				break;
			}
			else*/
		if(state < 0)
		{
			pthread_mutex_lock(&systemWideLock);

			if(autoshutdownFlag == 1)
			{
				//Time to quit

				pthread_mutex_unlock(&systemWideLock);
				pthread_exit(NULL);
			}
			pthread_mutex_unlock(&systemWideLock);
		}
		/*
		 * Display prompt hostname:port>
		 */
		if(FD_ISSET(fileno(stdin), &readset))
		{
			if(fgets(usercmd,256,stdin)!=NULL)
			{
				if(checkValidityOfCommand(usercmd) == 0)
				{
					printf("\nInvalid User Command\n");
				}
				else
				{
					//The user command is valid determine the action to be taken
					cmdtype = parseCmd(usercmd);
					if(cmdtype == 3)
					{
						pthread_mutex_lock(&systemWideLock);
						autoshutdownFlag = 1;
						restartState = 0;
						keyboardShutdown = 1;
						pthread_mutex_unlock(&systemWideLock);




						pthread_mutex_lock(&eventDispatcherQueueLock);
						pthread_cond_signal(&eventDispatcherQueueCV);
						pthread_mutex_unlock(&eventDispatcherQueueLock);

						if(nodetype == 0)
						{

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
									ndInf /*Node Info Pointer*/);

							struct connectionDetails *connectionDetailsObj;
							pthread_mutex_lock(&connectedNeighborsMapLock);
							connectedNeighborsMapIter = connectedNeighborsMap.begin();

							while(connectedNeighborsMapIter != connectedNeighborsMap.end()){

								connectionDetailsObj = connectedNeighborsMapIter -> second;
								pthread_mutex_lock(&connectionDetailsObj->connectionLock);
								if(connectionDetailsObj -> notOperational == 0 && // operational connection
										connectionDetailsObj -> isJoinConnection == 0 && // not connected to node for join request
										connectionDetailsObj -> threadsExitedCount == 0 &&
										connectionDetailsObj->helloStatus == 1){ // reader/writer both are working

									pthread_mutex_lock(&connectionDetailsObj -> messagingQueueLock);

									if(connectionDetailsObj -> messagingQueue.empty())
										connectionDetailsObj -> messagingQueue.push(mymessage);
									pthread_cond_signal(&connectionDetailsObj -> messagingQueueCV);
									pthread_mutex_unlock(&connectionDetailsObj -> messagingQueueLock);
								}
								pthread_mutex_unlock(&connectionDetailsObj->connectionLock);
								connectedNeighborsMapIter++;
							}
							pthread_mutex_unlock(&connectedNeighborsMapLock);


							pthread_kill(nonBeaconListenerThread,SIGUSR1);

							close(mynonbeaconSocket);


							pthread_mutex_lock(&checkThreadLock);
							pthread_cond_signal(&checkThreadCV);
							pthread_mutex_unlock(&checkThreadLock);
						}
						else
						{

							pthread_kill(listenerThread,SIGUSR1);

						}
						//pthread_kill(keyboardThreadNonbeacon,SIGUSR1);




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
					else
					{
						/*
						 * The user has asked for status
						 */


						struct statusMessage *myStatusMsg;
						//pushStatusMsgToDispatcherQueue
						if(cmdtype == 1)
						{
							myStatusMsg = createStatus(0x01);
						}
						else if(cmdtype == 2)
						{
							myStatusMsg = createStatus(0x02);
						}
						else
						{
							continue;
						}
						if(ttlcmd == 0)
						{
							clearStatusMsgMap();
							pthread_mutex_lock(&extFileLock);
							extFilePtr = fopen(extfilecmd,"w+");
							fwrite("V -t * -v 1.0a5\n",1,16,extFilePtr);
							fclose(extFilePtr);
							pthread_mutex_unlock(&extFileLock);
							struct statusRespMessage *myStatusResp = createSelfStatus(ndInf,0x01);
							bool retStatus;
							retStatus = writeInfoToStatusFile(myStatusResp);
						}
						else
						{
							struct message *myStatusMsgToSend = CreateMessageHeader(STATUS, /*message Type*/
									ttlcmd, /*TTL*/
									myStatusMsg -> dataLength, /*datalenght of body*/
									NULL, /*char *messageHeader*/
									NULL, /*char *message*/
									(void*)myStatusMsg, /*void *messageStruct*/
									NULL,/*char* temp_connectedNeighborsMapKey*/
									MYMESSAGE, /*MyMessage or notMyMessage */
									ndInf /*Node Info Pointer*/);

							char keytype[256] = "\0";
							int i = 0;
							while(i<20)
							{
								keytype[i] = *(myStatusMsgToSend->uoid + i);
								i++;
							}
							string keyTemp = string(keytype);

							clearStatusMsgMap();

							pthread_mutex_lock(&messageCacheMapLock);
							messageCacheMap.insert(MESSCACHEMAPTYPE::value_type(keytype,myStatusMsgToSend));
							pthread_mutex_unlock(&messageCacheMapLock);

							/*
							 * Write the first line of the file
							 */
							pthread_mutex_lock(&extFileLock);
							extFilePtr = fopen(extfilecmd,"w+");
							fwrite("V -t * -v 1.0a5\n",1,16,extFilePtr);
							fclose(extFilePtr);
							pthread_mutex_unlock(&extFileLock);

							connectionDetails *connectionDetailsObj;
							pthread_mutex_lock(&connectedNeighborsMapLock);
							int cnt = 0;
							//printf("\n\n------<><><> Sending STATUS message to all my neighbors <><><>------\n\n");
							connectedNeighborsMapIter = connectedNeighborsMap.begin();
							while(connectedNeighborsMapIter != connectedNeighborsMap.end()){
								connectionDetailsObj = connectedNeighborsMapIter -> second;
								if(connectionDetailsObj->notOperational == 0 &&
										connectionDetailsObj->helloStatus == 1 &&
										connectionDetailsObj->threadsExitedCount == 0){
									// need to forward to these, Key of Sender is not Equal to ConnectionDetails Map Object Key
									if(connectionDetailsObj->isJoinConnection != 1)
									{
										cnt++;
										pthread_mutex_lock(&connectionDetailsObj -> messagingQueueLock);

										connectionDetailsObj -> messagingQueue.push(myStatusMsgToSend);
										pthread_cond_signal(&connectionDetailsObj -> messagingQueueCV);
										pthread_mutex_unlock(&connectionDetailsObj -> messagingQueueLock);
									}
								}
								connectedNeighborsMapIter++;
							}
							pthread_mutex_unlock(&connectedNeighborsMapLock);

							if(cnt == 0)
							{
								/*
								 * Print own status
								 */
								clearStatusMsgMap();
								pthread_mutex_lock(&extFileLock);
								extFilePtr = fopen(extfilecmd,"w+");
								fwrite("V -t * -v 1.0a5\n",1,16,extFilePtr);
								fclose(extFilePtr);
								pthread_mutex_unlock(&extFileLock);
								struct statusRespMessage *myStatusResp = createSelfStatus(ndInf,0x01);
								bool retStatus;
								retStatus = writeInfoToStatusFile(myStatusResp);
							}
								pthread_mutex_lock(&keyboardLock);
								pthread_cond_wait(&keyboardCV,&keyboardLock);
								pthread_mutex_unlock(&keyboardLock);
						}
					}

				}
				printf("\nservant:%d> ",ndInf->port);
				memset(usercmd,0,256);
			}
		}
	}
	pthread_exit(NULL);
}

void * createTimerThread(int *arg)
{
	pthread_exit(NULL);
}
