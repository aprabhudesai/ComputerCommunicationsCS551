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
#include "nonBeaconNode.h"
#include <iostream>
#include "stdio.h"

struct NodeInfo *ndInf;

extern char extfilecmd[256];
extern char extfilecmdForFile[256];
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

extern INDEXMAPTYPE KeywordIndexMap;
extern INDEXMAPTYPE SHA1IndexMap;
extern INDEXMAPTYPE fileNameIndexMap;
extern LISTTYPE LRUList;
extern LISTTYPE PermFileList;
extern string searchMsgUOID;

extern struct statusRespMessage * createSelfStatus(struct NodeInfo *,uint8_t);
extern bool writeInfoToStatusFile(statusRespMessage *);
extern void clearStatusMsgMap();
extern bool handleStoreCommand(unsigned char *, uint8_t, unsigned char *);
extern void writeIndexToFile(INDEXMAPTYPE,char *);

void removeParanthesis(char *str)
{
	char temp[256] = "\0";
	for(int i = 0,k=0 ; str[i] != '\0'; i++)
	{
		if(str[i] != '\"')
		{
			temp[k] = str[i];
			k++;
		}
	}
	strcpy(str,temp);
}

void getTTLAndExtFileFromCmd(char *cmd,int msgtype)
{
	char *rem;
	rem = strchr(cmd,' ');
	ttlcmd = 0;
	memset(extfilecmd,0,256);
	memset(extfilecmdForFile,0,256);
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
			if(msgtype == 1)
			{
				// For neighbors
				strcpy(extfilecmd,token);
				extfilecmd[strlen(token) - 1] = '\0';
			}
			else if(msgtype == 2)
			{
				//For Files
				strcpy(extfilecmd,token);
				extfilecmdForFile[strlen(token) - 1] = '\0';
			}
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
	else if(strstr(cmd,"status") != NULL)
	{
		msgtype = getStatusMsgType(cmd);
		getTTLAndExtFileFromCmd(cmd,msgtype);
		return msgtype;
	}
	else if(strstr(cmd,"store") != NULL)
	{
		return 4;
	}
	else if(strstr(cmd,"search") != NULL)
	{
		return 5;
	}
	else if(strstr(cmd,"get") != NULL)
	{
		return 6;
	}
	else if(strstr(cmd,"delete") != NULL)
	{
		if(strstr(cmd,"FileName") != NULL && strstr(cmd,"SHA1") != NULL && strstr(cmd,"Nonce") != NULL)
			return 7;
		else
			return -1;
	}
	return -1;
}

int checkValidityOfCommand(char *cmd)
{
	if((strstr(cmd,"shutdown") != NULL) || strstr(cmd,"status neighbors") != NULL || strstr(cmd,"status files") != NULL
		|| strstr(cmd,"store") != NULL || strstr(cmd,"get") != NULL || strstr(cmd,"search") != NULL || strstr(cmd,"delete") != NULL)
	{
		if(strstr(cmd,"status neighbors") != NULL || strstr(cmd,"status files") != NULL)
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
	clearStatusMsgMap();
	actInt.sa_handler = sigintHandler;
	sigaction(SIGINT, &actInt, NULL);
	if(!searchMsgUOID.empty())
	{
		pthread_mutex_lock(&messageCacheMapLock);
		messageCacheMap.erase(searchMsgUOID);
		pthread_mutex_unlock(&messageCacheMapLock);
	}
	printf("\nservant:%d> ",ndInf->port);
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
					printf("\nservant:%d> ",ndInf->port);
				}
				else
				{
					//The user command is valid determine the action to be taken
					cmdtype = parseCmd(usercmd);
					if(cmdtype == -1)
					{
						printf("\nInvalid User Command\n");
						printf("\nservant:%d> ",ndInf->port);
					}
					else if(cmdtype == 3)
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
					else if(cmdtype == 1)
					{
						/*
						 * The user has asked for status neighbors
						 */

						struct statusMessage *myStatusMsg;
						myStatusMsg = createStatus(0x01);

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
						printf("\nservant:%d> ",ndInf->port);
					}
					else if(cmdtype == 2)
					{
						/*
						 * Status files command
						 */

						struct statusMessage *myStatusMsg;
						myStatusMsg = createStatus(0x02);

						pthread_mutex_lock(&extFileLock);
						extfilecmd[strlen(extfilecmd)-1] = '\0';
						extFilePtr = fopen(extfilecmd,"w+");
						fclose(extFilePtr);
						pthread_mutex_unlock(&extFileLock);

						if(ttlcmd == 0)
						{
							struct statusRespMessage *myStatusResp = createSelfStatus(ndInf,0x02);
							bool retStatus;
							retStatus = writeInfoToStatusFile(myStatusResp);
						}
						else
						{

							struct statusRespMessage *myStatusResp = createSelfStatus(ndInf,0x02);
							bool retStatus;
							retStatus = writeInfoToStatusFile(myStatusResp);

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

							pthread_mutex_lock(&messageCacheMapLock);
							messageCacheMap.insert(MESSCACHEMAPTYPE::value_type(keytype,myStatusMsgToSend));
							pthread_mutex_unlock(&messageCacheMapLock);

							/*
							 * Write the first line of the file
							 */

							connectionDetails *connectionDetailsObj;
							pthread_mutex_lock(&connectedNeighborsMapLock);
							int cnt = 0;
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

								 /*Print own status*/

							}
							else
							{
								pthread_mutex_lock(&keyboardLock);
								pthread_cond_wait(&keyboardCV,&keyboardLock);
								pthread_mutex_unlock(&keyboardLock);
							}
						}
						printf("\nservant:%d> ",ndInf->port);
					}
					else if(cmdtype == 4)
					{
						/*
						 * Store command
						 */
						char *tempstr = NULL;
						unsigned char fileName[256] = "\0";
						char keywordStream[512] = "\0";
						unsigned char keywordStreamFormatted[512] = "\0";
						uint8_t ttlFromUser = 0;
						tempstr = strchr(usercmd,' ');
						if(tempstr!=NULL)
						{
							char *token;
							token = strtok(tempstr + 1," ");
							strcpy((char*)fileName,token);

							token = NULL;
							token = strtok(NULL," ");
							ttlFromUser = atoi(token);

							token = NULL;
							token = strtok(NULL,"\n");
							strcpy(keywordStream,token);
						}

						for(int j = 0 ; keywordStream[j] != '\0'; j++)
						{
							if(keywordStream[j] == '=')
							{
								keywordStream[j] = ' ';
							}
						}
						for(int j = 0,k = 0 ; keywordStream[j] != '\0'; j++)
						{
							if(keywordStream[j] != '\"')
							{
								keywordStreamFormatted[k] = keywordStream[j];
								k++;
							}
						}
						handleStoreCommand(fileName,ttlFromUser, keywordStreamFormatted);
						printf("\nservant:%d> ",ndInf->port);
					}
					else if(cmdtype == 5)
					{
						/*
						 * Search command
						 */
						char *tempstr = NULL;
						char searchKey[256] = "\0";
						char searchValue[512] = "\0";
						uint8_t searchType = 0;

						searchRespIndex = 0;

						tempstr = strchr(usercmd,' ');

						if(tempstr != NULL)
						{
							char *token = NULL;
							token = strtok(tempstr + 1,"=");
							strcpy(searchKey,token);

							if(strcmp(searchKey,"filename") == 0 || strcmp(searchKey,"sha1hash") == 0)
							{
								token = NULL;
								token = strtok(NULL,"\n");
								strcpy(searchValue,token);
							}
							else if(strcmp(searchKey,"keywords") == 0)
							{
								token = NULL;
								token = strtok(NULL,"\n");
								strcpy(searchValue,token);

								if(strstr(searchValue,"\"") != NULL)
								{
									removeParanthesis(searchValue);
								}
							}
						}

						/*
						 * Determine search type
						 */
						unsigned char searchQuery[256] = "\0";
						int k = 0;
						while(searchValue[k] != '\0')
						{
							searchQuery[k] = searchValue[k];
							k++;
						}
						searchQuery[k] = '\0';
						if(strcmp(searchKey,"filename") == 0)
						{
							searchType = 1;
						}
						else if(strcmp(searchKey,"sha1hash") == 0)
						{
							searchType = 2;
						}
						else if(strcmp(searchKey,"keywords") == 0)
						{
							searchType = 3;
						}

						struct searchMessage *mySearchMsg = createSearch(searchQuery,searchType);


						struct fileMetadata *metaDataArr[50];
						int fileNumbers[50];
						int searchRespCount = 0;

						if(mySearchMsg->searchType == 1)
						{
							//Search for filename
							FileNameSearch(mySearchMsg->query,metaDataArr,&searchRespCount,fileNumbers);
						}
						if(mySearchMsg->searchType == 2)
						{
							//Search for SHA1
							SHA1Search(mySearchMsg->query,metaDataArr,&searchRespCount,fileNumbers);
						}
						if(mySearchMsg->searchType == 3)
						{
							//Search for Keywords
							KeywordSearch(mySearchMsg->query,metaDataArr,&searchRespCount,fileNumbers);
						}

						if(searchRespCount > 0)
						{
							for(int j = 0; j < searchRespCount;j++)
							{
								if(searchResponses[searchRespIndex] != NULL)
								{
									free(searchResponses[searchRespIndex]);
									searchResponses[searchRespIndex] = NULL;
								}
								searchResponses[searchRespIndex] = metaDataArr[j];
								searchRespIndex++;
								printf("\n[%d] ",searchRespIndex+j);
								printf("FileID=");
								k=0;
								while(k < 20)
								{
									printf("%02x",metaDataArr[j]->fileId[k]);
									k++;
								}
								printf("\n    FileName=%s\n",metaDataArr[j]->fileName);
								printf("    FileSize=%d\n",metaDataArr[j]->fileSize);
								printf("    SHA1=");
								k = 0;
								while(k < 20)
								{
									printf("%02x",metaDataArr[j]->SHA1val[k]);
									k++;
								}
								printf("\n    Nonce=");
								k=0;
								while(k < 20)
								{
									printf("%02x",metaDataArr[j]->nonce[k]);
									k++;
								}
								printf("\n    Keywords=%s\n",metaDataArr[j]->keywordsWithSpace);
							}
						}

						struct message *mySearchMsgToSend = CreateMessageHeader(SEARCH, /*message Type*/
								ttlcmd, /*TTL*/
								mySearchMsg -> dataLength, /*datalenght of body*/
								NULL, /*char *messageHeader*/
								NULL, /*char *message*/
								(void*)mySearchMsg, /*void *messageStruct*/
								NULL,/*char* temp_connectedNeighborsMapKey*/
								MYMESSAGE, /*MyMessage or notMyMessage */
								ndInf /*Node Info Pointer*/);


						char keytype[256] = "\0";
						int i = 0;
						while(i<20)
						{
							keytype[i] = *(mySearchMsgToSend->uoid + i);
							i++;
						}
						string keyTemp = string(keytype);

						searchMsgUOID = keyTemp;

						pthread_mutex_lock(&messageCacheMapLock);
						messageCacheMap.insert(MESSCACHEMAPTYPE::value_type(keyTemp,mySearchMsgToSend));
						pthread_mutex_unlock(&messageCacheMapLock);


						connectionDetails *connectionDetailsObj;
						pthread_mutex_lock(&connectedNeighborsMapLock);
						int cnt = 0;
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
									connectionDetailsObj -> messagingQueue.push(mySearchMsgToSend);
									pthread_cond_signal(&connectionDetailsObj -> messagingQueueCV);
									pthread_mutex_unlock(&connectionDetailsObj -> messagingQueueLock);
								}
							}
							connectedNeighborsMapIter++;
						}
						pthread_mutex_unlock(&connectedNeighborsMapLock);

						/*
						 * Keyboard thread waits for dispatcher to signal that
						 * all the responses have been received
						 */
						pthread_mutex_lock(&keyboardLock);
						pthread_cond_wait(&keyboardCV,&keyboardLock);
						pthread_mutex_unlock(&keyboardLock);

						if(cnt > 0)
							printf("\nservant:%d> ",ndInf->port);
						/*
						 * The keyboard thread then displays all the results from the search response
						 */

					}
					else if(cmdtype == 6)
					{
						/*
						 * Get command
						 */

						/*Check if there was a search done earlier*/
						if(searchResponses[0] == NULL)
						{
							printf("\nNo SEARCH done previously. Please Issue a SEARCH command before GET\n");
							printf("\nservant:%d> ",ndInf->port);
							goto PROMPT;
						}
						char *tempstr = NULL;
						unsigned char getFile[256] = "\0";
						uint8_t getTTL = 0;

						int blanks = 0;
						for(int i = 0; usercmd[i] != '\n'; i++)
						{
							if(usercmd[i] == ' ')
								blanks++;
						}
						if(blanks == 0)
						{
							printf("\nIncorrect format for get message\nUsage get <File#> [<extfile>]\n");
							printf("\nservant:%d> ",ndInf->port);
							goto PROMPT;
						}
						else
						{
							tempstr = strchr(usercmd,' ');
							if(tempstr != NULL)
							{
								char *token = NULL;
								if(blanks == 1)
								{
									//extfile not specified
									token = strtok(tempstr+1,"\n");
									getTTL = atoi(token);
								}
								else if(blanks == 2)
								{
									//extfile is specified
									token = strtok(tempstr+1," ");
									getTTL = atoi(token);
									token = NULL;
									token = strtok(NULL,"\n");
									size_t i = 0;
									while(i < strlen(token))
									{
										getFile[i] = token[i];
										i++;
									}
									getFile[i] = '\0';
								}
							}
						}
						if(searchRespIndex < getTTL)
						{
							printf("\nThe file number to retrieve is not in SEARCH results\n");
							printf("\nservant:%d> ",ndInf->port);
							goto PROMPT;
						}
						int k = 0;
						struct fileMetadata *fileMD = searchResponses[getTTL-1];

						char keytype[256] = "\0";
						int haveFile = 0;
						k = 0;
						while(k < 20)
						{
							keytype[k] = *(fileMD->SHA1val + k);
							k++;
						}
						keytype[k] = '\0';

						string shasearchKey = string(keytype);

						int fileidList[50] = {0};
						int cnt = 0;
						int fileidFound = -1;
						pthread_mutex_lock(&SHA1IndexMapLock);
						SHA1IndexMapIter = SHA1IndexMap.find(shasearchKey);
						if(SHA1IndexMapIter != SHA1IndexMap.end())
						{
							SHA1IndexIt = SHA1IndexMap.equal_range(SHA1IndexMapIter->first.c_str());
							for(SHA1IndexITIter = SHA1IndexIt.first; SHA1IndexITIter != SHA1IndexIt.second ; SHA1IndexITIter++)
							{
								fileidList[cnt] = SHA1IndexITIter->second;
								cnt++;
							}
							fileidFound = checkFileWithFileId(fileidList,fileMD->fileId,cnt);
							if(fileidFound != -1)
							{
								haveFile = 1;
							}
							else
								haveFile = 0;
						}
						else
						{
							haveFile = 0;
						}
						pthread_mutex_unlock(&SHA1IndexMapLock);

						struct getMessage *myGetMsg = createGet(fileMD,getFile);

						if(haveFile == 1)
						{
							char fileNameForCWD[256] = "\0";
							char tempfileName[256] = "\0";
							sprintf(fileNameForCWD,"%s",myGetMsg->fileName);

							memset(tempfileName,0,256);
							sprintf(tempfileName,"%s/files/%d.data",ndInfoTemp->homeDir,fileidFound);

							int status = copyFileToCWD(tempfileName,fileNameForCWD);
							if(status == -1)
							{
								printf("\nCould not store file in Current Working Directory of Node\n");
								printf("\nservant:%d> ",ndInf->port);
								goto PROMPT;
							}
							/*Remove file from LRU cache & put it in the permanent list*/
							pthread_mutex_lock(&LRUlistLock);
							for(LRUIter = LRUList.begin(); LRUIter != LRUList.end(); LRUIter++)
							{
								if(fileidFound == *LRUIter)
								{
									LRUList.erase(LRUIter);
									break;
								}
							}
							pthread_mutex_unlock(&LRUlistLock);
							int insertFlag = 0;
							pthread_mutex_lock(&permFileListLock);
							for(PermFileListIter = PermFileList.begin(); PermFileListIter != PermFileList.end(); PermFileListIter++)
							{
								if(fileidFound == *PermFileListIter)
								{
									insertFlag = 1;
									break;
								}
							}
							if(insertFlag == 0)
								PermFileList.insert(PermFileList.end(),fileidFound);
							pthread_mutex_unlock(&permFileListLock);
							printf("\nservant:%d> ",ndInf->port);
						}
						else
						{
							struct message *myGetMsgToSend = CreateMessageHeader(GET, /*message Type*/
									ndInf->ttl, /*TTL*/
									myGetMsg -> dataLength, /*datalenght of body*/
									NULL, /*char *messageHeader*/
									NULL, /*char *message*/
									(void*)myGetMsg, /*void *messageStruct*/
									NULL,/*char* temp_connectedNeighborsMapKey*/
									MYMESSAGE, /*MyMessage or notMyMessage */
									ndInf /*Node Info Pointer*/);


							char keytype[256] = "\0";
							int i = 0;
							while(i<20)
							{
								keytype[i] = *(myGetMsgToSend->uoid + i);
								i++;
							}
							string keyTemp = string(keytype);

							pthread_mutex_lock(&messageCacheMapLock);
							messageCacheMap.insert(MESSCACHEMAPTYPE::value_type(keyTemp,myGetMsgToSend));
							pthread_mutex_unlock(&messageCacheMapLock);


							connectionDetails *connectionDetailsObj;
							pthread_mutex_lock(&connectedNeighborsMapLock);
							int cnt = 0;
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
										connectionDetailsObj -> messagingQueue.push(myGetMsgToSend);
										pthread_cond_signal(&connectionDetailsObj -> messagingQueueCV);
										pthread_mutex_unlock(&connectionDetailsObj -> messagingQueueLock);
									}
								}
								connectedNeighborsMapIter++;
							}
							pthread_mutex_unlock(&connectedNeighborsMapLock);

							/*
							 * Keyboard thread waits for dispatcher to signal that
							 * all the responses have been received
							 */
							pthread_mutex_lock(&keyboardLock);
							pthread_cond_wait(&keyboardCV,&keyboardLock);
							pthread_mutex_unlock(&keyboardLock);
							if(cnt > 0)
								printf("\nservant:%d> ",ndInf->port);
						}
						//printf("\nservant:%d> ",ndInf->port);
					}
					else if(cmdtype == 7)
					{
						/*
						 * Delete command
						 */
						unsigned char userCommand[300] = "\0";
						int status = 0;
						int i = 0;
						char ans[10] = "\0";
						while(usercmd[i] != '\n')
						{
							userCommand[i] = usercmd[i];
							i++;
						}
						userCommand[i] = '\0';
						struct deleteMessage *myDeleteMsg = createDelete(userCommand);

						status = handleDeleteCommand(myDeleteMsg);
						if(status == 0)
						{
							printf("\nSpecified file not present in the File System\n");
							printf("\nservant:%d> ",ndInf->port);
							goto PROMPT;
						}
						else if(status == 1)
						{
							/*File is in FileSystem but password absent i.e. this file
							 * belongs to the cache space
							 */
							int canSend = 0;
							printf("\nNo one-time password found.\nOkay to use a random password [yes/no]? ");
							fgets(ans,sizeof(ans),stdin);
							if(strstr(ans,"y") != NULL)
							{
								canSend = 1;
								//Generate random password
								GetUOID(ndInf->nodeInstanceId, (char *)"pass",myDeleteMsg->password,20);
							}
							else
							{
								printf("\nservant:%d> ",ndInf->port);
								goto PROMPT;
							}
						}

						/*Send the delete message to all my neighbors*/
						struct message *myDeleteMsgToSend = CreateMessageHeader(DELETE, /*message Type*/
								ndInf->ttl, /*TTL*/
								myDeleteMsg -> dataLength, /*datalenght of body*/
								NULL, /*char *messageHeader*/
								NULL, /*char *message*/
								(void*)myDeleteMsg, /*void *messageStruct*/
								NULL,/*char* temp_connectedNeighborsMapKey*/
								MYMESSAGE, /*MyMessage or notMyMessage */
								ndInf /*Node Info Pointer*/);

						char keytype[256] = "\0";
						i = 0;
						while(i<20)
						{
							keytype[i] = *(myDeleteMsgToSend->uoid + i);
							i++;
						}
						string keyTemp = string(keytype);

						pthread_mutex_lock(&messageCacheMapLock);
						messageCacheMap.insert(MESSCACHEMAPTYPE::value_type(keyTemp,myDeleteMsgToSend));
						pthread_mutex_unlock(&messageCacheMapLock);


						connectionDetails *connectionDetailsObj;
						pthread_mutex_lock(&connectedNeighborsMapLock);
						int cnt = 0;
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
									connectionDetailsObj -> messagingQueue.push(myDeleteMsgToSend);
									pthread_cond_signal(&connectionDetailsObj -> messagingQueueCV);
									pthread_mutex_unlock(&connectionDetailsObj -> messagingQueueLock);
								}
							}
							connectedNeighborsMapIter++;
						}
						pthread_mutex_unlock(&connectedNeighborsMapLock);
						printf("\nservant:%d> ",ndInf->port);
					}
				}
				PROMPT:
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
