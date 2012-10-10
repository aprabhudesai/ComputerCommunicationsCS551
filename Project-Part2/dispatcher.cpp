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
#include <stdlib.h>
#include "systemDataStructures.h"
#include "iniparser.h"
#include "ntwknode.h"
#include "headers.h"

extern pthread_mutex_t keyboardLock;
extern pthread_cond_t keyboardCV;

extern unsigned char *GetUOID(char *,char*,unsigned char*,int);

extern int checkNodeType(uint16_t);
void processMessage(struct message*);
bool checkMessageValidity(struct message*);
extern bool writeToLogFile(struct message *ptr,char *);
extern bool writeInfoToStatusFile(statusRespMessage *);
extern void writeErrorToLogFile(char *);
extern int checkIfFileAlreadyPresentInCache(struct fileMetadata *);


void dispatcher(void* param){

	//node info passed to this pthread which consists of all the node information needed
	//to process incoming messages
	ndInfoTemp = (struct NodeInfo *)param;


	struct message* messageStruct;
	bool isMessageValid = false;
	while(1){
		isMessageValid = false;
		pthread_mutex_lock(&eventDispatcherQueueLock);
		if(eventDispatcherQueue.empty()){
			pthread_cond_wait(&eventDispatcherQueueCV, &eventDispatcherQueueLock);
		}
		else{
		}
		pthread_mutex_lock(&systemWideLock);
		if(autoshutdownFlag == 1)
		{
			/*
			 * its time to quit
			 */
			pthread_mutex_unlock(&systemWideLock);
			pthread_mutex_unlock(&eventDispatcherQueueLock);
			pthread_exit(NULL);
		}
		pthread_mutex_unlock(&systemWideLock);
		// pop dequeue message from queue
		messageStruct = eventDispatcherQueue.front();
		eventDispatcherQueue.pop();
		pthread_mutex_unlock(&eventDispatcherQueueLock);

		//Check if Message UOID is already in cache
		//if UOID is already message cache, then drop the message
		//if UOID is not in message cache, put the message to message cache
		isMessageValid = checkMessageValidity(messageStruct);

		if(isMessageValid){
			// Process Message
			processMessage(messageStruct);
		}
		else{
			//delete message and its contents, clear it off
			freeMessage(messageStruct);
			messageStruct = NULL;
		}

	}
}

int copyFileToCWD(char sourceFile[],char destFile[])
{
	int statusOfCopy = 1;
	FILE *src = NULL;
	FILE *dest = NULL;
	struct stat fileData;
	char line[1000];
	char ans[10] = "\0";
	int canreplace = 0;
	if(stat(destFile,&fileData) == 0)
	{
		printf("\nThe file %s already exists\nDo you want to replace the file (yes/no)? ",destFile);
		fgets(ans,10,stdin);
		if(strstr(ans,"y") != NULL)
		{
			canreplace = 1;
		}
		else
		{
			return -1;
		}
		//exit(0);
	}
	if((src = fopen(sourceFile,"r")) == NULL || (dest = fopen(destFile,"w")) == NULL)
	{
		printf("\nError opening files\n");
		fclose(src);
		fclose(dest);
		return -1;
	}
	size_t len = 0;
	while( (len = fread( line,1,sizeof(line), src)) > 0 )
	{
		fwrite( line, 1,len, dest ) ;
	}
	fclose(src);
	fclose(dest);
	printf("\nCopy Done!!!\n");
	return statusOfCopy;
}

void storeFile(struct fileMetadata *recvdFileMetaData,int location,unsigned char fileNameToStore[])
{
	int currentFileIndex = 0;
	char tempfileName[256] = "\0";
	FILE *fileIDPtr;
	currentFileIndex = getNextFileIndex();

	sprintf(tempfileName,"%s/files/%d.data",ndInfoTemp->homeDir,currentFileIndex);
	rename(recvdFileMetaData->fileWithPath,tempfileName);
	strcpy(recvdFileMetaData->fileWithPath,tempfileName);

	memset(tempfileName,0,256);
	sprintf(tempfileName,"%s/files/%d.meta",ndInfoTemp->homeDir,currentFileIndex);
	writeMetaDataToFile(recvdFileMetaData,tempfileName);

	memset(tempfileName,0,256);
	strcpy(tempfileName,ndInfoTemp->homeDir);
	sprintf(tempfileName,"%s/files/%d.fileid",ndInfoTemp->homeDir,currentFileIndex);

	//GetUOID(ndInfoTemp->nodeInstanceId, (char *)"fileid",fileId,20);

	fileIDPtr = fopen(tempfileName,"w+");
	if(fileIDPtr == NULL){
		printf("\nError Creating %s Meta File\n",tempfileName);
		return;
	}

	int i=0;
	while(i<20){
		fprintf(fileIDPtr,"%02x",recvdFileMetaData->fileId[i]);
		i++;
	}
	fclose(fileIDPtr);

	insertFileEntryIntoIndex(recvdFileMetaData,currentFileIndex);

	if(location == 1)
	{
		/*Add the entry to LRU list*/
		pthread_mutex_lock(&LRUlistLock);
		LRUIter = LRUList.end();
		LRUList.insert(LRUIter,currentFileIndex);
		pthread_mutex_unlock(&LRUlistLock);

		mycacheSize += recvdFileMetaData->fileSize;
	}
	else if(location == 2)
	{
		/*Add the entry to LRU list*/

		//printf("\n---<<< Storing File to permanent store >>>---\n");
		pthread_mutex_lock(&permFileListLock);
		PermFileListIter = PermFileList.end();
		PermFileList.insert(PermFileListIter,currentFileIndex);
		pthread_mutex_unlock(&permFileListLock);

		char fileNameForCWD[256] = "\0";
		sprintf(fileNameForCWD,"%s",fileNameToStore);

		memset(tempfileName,0,256);
		sprintf(tempfileName,"%s/files/%d.data",ndInfoTemp->homeDir,currentFileIndex);

		//printf("\nData: %s\nfilenameForCWD: %s\n",tempfileName,fileNameForCWD);
		int status = copyFileToCWD(tempfileName,fileNameForCWD);
		if(status == -1)
		{
			printf("\nCould not store file in Current Working Directory of Node\n");
		}
	}
}

struct connectionDetails *getConnectionDetailsFromMap(char *key)
{
	struct connectionDetails *connectionDetailsObj = NULL;
	string findkey = string(key);
	connectedNeighborsMapIter = connectedNeighborsMap.find(findkey);

	if(connectedNeighborsMapIter == connectedNeighborsMap.end()){
	}
	else{
		//Put the HELLO message in the queue of writer
		connectionDetailsObj = connectedNeighborsMapIter -> second;
	}
	return connectionDetailsObj;
}

void createAndSendHelloMsg(struct message *ptr)
{
	//Create HELLO message and push to writer queue
	struct message *mymessage;
	struct helloMessage *myhelloMsg = createHello(ndInfoTemp->hostname,ndInfoTemp->port);

	mymessage = CreateMessageHeader(HELLO, /*Mess Type*/
										1, /* TTL */
										myhelloMsg -> dataLength, /*DataLength Message Body */
										NULL, /*char *messageHeader*/
										NULL, /*char *message*/
										(void*)myhelloMsg, /*void *messageStruct*/
										NULL,/*char* temp_connectedNeighborsMapKey*/
										MYMESSAGE, /* MyMessage or NotMyMessage*/
										ndInfoTemp /*Node Info Pointer*/);

	pthread_mutex_lock(&connectedNeighborsMapLock);
	//Get connection details for pushing the message to writer
	struct connectionDetails *connectionDetailsObj = getConnectionDetailsFromMap(ptr->connectedNeighborsMapKey);

	if(connectionDetailsObj == NULL){
		pthread_mutex_unlock(&connectedNeighborsMapLock);
		//Handle Error - No Key-Value Found
	}
	else{
		//Put the HELLO message in the queue of writer
		//connectionDetailsObj->wellKnownPort = recvdHello->hostPort;
		pthread_mutex_lock(&connectionDetailsObj->connectionLock);
		pthread_mutex_lock(&connectionDetailsObj -> messagingQueueLock);

		//cout << "\nPut Message to queue for forward\n";
		connectionDetailsObj->helloStatus = 1;
		connectionDetailsObj -> messagingQueue.push(mymessage);
		pthread_cond_signal(&connectionDetailsObj -> messagingQueueCV);
		pthread_mutex_unlock(&connectionDetailsObj -> messagingQueueLock);
		pthread_mutex_unlock(&connectionDetailsObj->connectionLock);
		pthread_mutex_unlock(&connectedNeighborsMapLock);

	}
}

void processMessage(struct message* ptr){

	switch(ptr -> messType){

	case JOIN:
						{	// Received  Join Message
							// Steps
							/*
							 * Reply for the join
							 * Check TTL -1
							 * if (TTL - 1) != 0 --> forward
							 * else do not forward to any queue, only reply
							 *
							 */

							struct joinMessage* joinMessageObj = (struct joinMessage *)(ptr -> messageStruct);
							//printJoinMessage(joinMessageObj);

							/*
							 * Create Reply Message and Reply Message Header Accordingly
							 *
							 */
							struct joinRespMessage* joinRespMessageObj;

							joinRespMessageObj = createJoinRespMessage(joinMessageObj,ptr,ndInfoTemp);

							//Overwrite the received message port and hostname, as it is going to be forwarded
							uint32_t recv_port = joinMessageObj->hostPort;
							joinMessageObj->hostPort = ndInfoTemp -> port;
							strcpy(joinMessageObj->hostName,ndInfoTemp -> hostname);
							joinMessageObj->dataLength = (uint32_t)(6 + strlen(joinMessageObj -> hostName));

							ptr->dataLength = joinMessageObj->dataLength;

							//joinRespMessageObj -> uoidKey = string(joinRespMessageObj -> uoidKey);

							struct message* messageHeaderObj;
							messageHeaderObj = CreateMessageHeader(RJOIN, /*message Type*/
																ndInfoTemp -> ttl, /*TTL*/
																joinRespMessageObj -> dataLength, /*datalenght of body*/
																NULL, /*char *messageHeader*/
																NULL, /*char *message*/
																(void*)joinRespMessageObj, /*void *messageStruct*/
																NULL,/*char* temp_connectedNeighborsMapKey*/
																MYMESSAGE, /*MyMessage or notMyMessage */
																ndInfoTemp /*Node Info Pointer*/);

							struct connectionDetails *connectionDetailsObj;
							string connectedNeighborsMapKeyTemp = string(ptr -> connectedNeighborsMapKey);
							if(ptr -> ttl <= 1){ // no need to forward the packet
								// only send reply

								pthread_mutex_lock(&connectedNeighborsMapLock);

								connectedNeighborsMapIter = connectedNeighborsMap.find(connectedNeighborsMapKeyTemp);
								if(connectedNeighborsMapIter == connectedNeighborsMap.end()){
									pthread_mutex_unlock(&connectedNeighborsMapLock);
									//Handle Error - No Key-Value Found
								}
								connectionDetailsObj = connectedNeighborsMapIter -> second;
								//pthread_mutex_lock(&connectionDetailsObj->connectionLock);
								if(checkNodeType(recv_port) != 1)
									connectionDetailsObj->isJoinConnection = 1;
								//Push reply Message to Queue of Sender
								pthread_mutex_lock(&connectionDetailsObj -> messagingQueueLock);
								// Push the message from queue to be sent
								connectionDetailsObj -> messagingQueue.push(messageHeaderObj);
								//Release Messaging Queue Lock
								pthread_cond_signal(&connectionDetailsObj -> messagingQueueCV);
								pthread_mutex_unlock(&connectionDetailsObj -> messagingQueueLock);
								//pthread_mutex_unlock(&connectionDetailsObj->connectionLock);
								pthread_mutex_unlock(&connectedNeighborsMapLock);
							}
							else{

								ptr -> ttl = ptr -> ttl - 1; // decrement the TTL before forwarding.
								ptr -> ttl = ptr -> ttl > ndInfoTemp->ttl?ndInfoTemp->ttl:ptr -> ttl;


								//forward the packet to all connnected neighbors except the sender
								// also send reply to sender
								// only send reply
								pthread_mutex_lock(&connectedNeighborsMapLock);

								connectedNeighborsMapIter = connectedNeighborsMap.begin();
								if(connectedNeighborsMapIter == connectedNeighborsMap.end()){
									pthread_mutex_unlock(&connectedNeighborsMapLock);
									//Handle Error - No Key-Value Found
								}
								else{
									connectedNeighborsMapIter = connectedNeighborsMap.begin();
									while(connectedNeighborsMapIter != connectedNeighborsMap.end())
									{
										connectionDetailsObj = connectedNeighborsMapIter -> second;
										//pthread_mutex_lock(&connectionDetailsObj->connectionLock);
										//connectionDetailsObj->isJoinConnection = 1;
										if(connectionDetailsObj->notOperational == 0 &&
											 connectionDetailsObj->threadsExitedCount == 0)
										{
											// Not the sender, need to forward to these, Key of Sender is not Equal to ConnectionDetails Map Object Key
											//cout << "IsOperational " << connectedNeighborsMapKeyTemp << " " << connectedNeighborsMapIter -> first << endl;
											if (connectedNeighborsMapKeyTemp.compare(connectedNeighborsMapIter -> first) != 0)
											{
												if(connectionDetailsObj->isJoinConnection != 1 && connectionDetailsObj->helloStatus == 1)
												{
										//			cout << "FORWARD REQUEST"<<endl;
													pthread_mutex_lock(&connectionDetailsObj -> messagingQueueLock);
													//cout << "\nPut Message to queue for forward\n";
													ptr->sendStatus=0;
													connectionDetailsObj -> messagingQueue.push(ptr);
													pthread_cond_signal(&connectionDetailsObj -> messagingQueueCV);
													pthread_mutex_unlock(&connectionDetailsObj -> messagingQueueLock);
												}
											}
											else{ // send reply to the sender here
												//cout << "SEND REPLY\n";
												pthread_mutex_lock(&connectionDetailsObj -> messagingQueueLock);
												if(checkNodeType(recv_port) != 1)
												connectionDetailsObj->isJoinConnection = 1;
										//		cout << "\nPut Message to queue for reply\n";
												connectionDetailsObj -> messagingQueue.push(messageHeaderObj);
												pthread_cond_signal(&connectionDetailsObj -> messagingQueueCV);
												pthread_mutex_unlock(&connectionDetailsObj -> messagingQueueLock);
											}
										}
										//pthread_mutex_unlock(&connectionDetailsObj->connectionLock);
										connectedNeighborsMapIter++;
									}
									pthread_mutex_unlock(&connectedNeighborsMapLock);
								}
							}

						}
					break;
		case RJOIN:
					{
						/*
						 *	As I am a beacon, any RJOIN is of no use to me, I need to forward it to the Node Non Beacon who
						 *	has sent me the JOIN Request.
						 *
						 *	1. For Received RJOIN, Search Message Cache by the UOID included in RJOIN Message and forward the RJOIN to it.
						 */
						if(ptr-> ttl > 1){
								struct joinRespMessage* joinRespMessageObj = (struct joinRespMessage *)(ptr -> messageStruct);
								//printjoinRespMessage(joinRespMessageObj);

								ptr->ttl = ptr->ttl -1;
								ptr -> ttl = (ptr -> ttl > ndInfoTemp->ttl) ? (ndInfoTemp->ttl) : (ptr -> ttl);

								char searchCharUoidkey[21] = "\0";
								int i =0;
								while(i<20){
									searchCharUoidkey[i] = joinRespMessageObj->uoid[i];
									i++;
								}
								searchCharUoidkey[i] = '\0';

								string searchKey = string(searchCharUoidkey);

								pthread_mutex_lock(&messageCacheMapLock);
								if((messageCacheMapIter = messageCacheMap.find(searchKey)) != messageCacheMap.end()){
									struct message *sentMessage = messageCacheMapIter->second;
									string mapKey = string(sentMessage->connectedNeighborsMapKey);

									pthread_mutex_lock(&connectedNeighborsMapLock);
									struct connectionDetails *connectionDetailsObj = getConnectionDetailsFromMap(sentMessage->connectedNeighborsMapKey);

									if(connectionDetailsObj == NULL){
										pthread_mutex_unlock(&connectedNeighborsMapLock);
										//Handle Error - No Key-Value Found
									}
									else{
										//Put the HELLO message in the queue of writer
										//pthread_mutex_lock(&connectionDetailsObj->connectionLock);
										pthread_mutex_lock(&connectionDetailsObj -> messagingQueueLock);
									//	cout << "\nPut Message to queue for forward\n";
										ptr->sendStatus = 0;
										connectionDetailsObj -> messagingQueue.push(ptr);
										pthread_cond_signal(&connectionDetailsObj -> messagingQueueCV);
										pthread_mutex_unlock(&connectionDetailsObj -> messagingQueueLock);
										//pthread_mutex_unlock(&connectionDetailsObj->connectionLock);
										pthread_mutex_unlock(&connectedNeighborsMapLock);
									}

								}
								else{

									//drop the response message, as original Join request is removed from cache, may be due to timeout.
										freeMessage(ptr);
										ptr = NULL;
									}

								pthread_mutex_unlock(&messageCacheMapLock);
							}
					}
					break;

		case HELLO:
					{
						//When I get a HELLO message, it can be a response to my HELLO
						//or a new HELLO message. If its a new HELLO message then I will
						//respond back by sending a HELLO. After this I will tie break if
						//the sender is a BEACON node

						int nodeType = 0,helloState = 0;
						struct helloMessage* helloMessageObj = (struct helloMessage *)(ptr -> messageStruct);
						//printHelloMessage(helloMessageObj);

						nodeType = checkNodeType(helloMessageObj->hostPort);

						
						if(nodeType == 1)
						{
							//The node which has sent me a hello is a beacon
							pthread_mutex_lock(&helloMsgMapLock);
							string connectedNeighborsMapKey = string(ptr->connectedNeighborsMapKey);
							if((helloMsgMapIter = helloMsgMap.find(connectedNeighborsMapKey)) == helloMsgMap.end())
							{
								//I have not sent a HELLO. So I will respond back with HELLO
								helloState = 1;
								createAndSendHelloMsg(ptr);
							}

							pthread_mutex_unlock(&helloMsgMapLock);
							// Do tie breaking
							// if helloState = 0 -> The hello msg is response for my hello
							// else helloState = 1 -> the hello msg is for which i send a response
							pthread_mutex_lock(&connectedNeighborsMapLock);
							struct connectionDetails *connectionDetailsObj = getConnectionDetailsFromMap(ptr->connectedNeighborsMapKey);
							pthread_mutex_unlock(&connectedNeighborsMapLock);

							if(helloState == 0)
							{
								/*pthread_mutex_lock(&connectedNeighborsMapLock);
								if(ndInfoTemp->port < helloMessageObj->hostPort)
								{
									//printf("\n\n\t\t$$$$$$$$$$$$$$$$$$$$$$$$$$ My Port %d < Incoming Message Port: %d Kill STATUS for myself $$$$$$$$$$$$$$$$$$$$$$$$$$$$$\n\n",ndInfoTemp->port,helloMessageObj->hostPort);
									//My port is smaller. I will close my connection

									//pthread_mutex_lock(&connectedNeighborsMapLock);
									pthread_mutex_lock(&connectionDetailsObj->connectionLock);
									connectionDetailsObj->tiebreak = 1;
									connectionDetailsObj -> notOperational = 1;
									pthread_mutex_lock(&(connectionDetailsObj->messagingQueueLock));
									pthread_cond_signal(&(connectionDetailsObj->messagingQueueCV));
									pthread_mutex_unlock(&(connectionDetailsObj->messagingQueueLock));
									close(connectionDetailsObj->socketDesc);
									pthread_mutex_unlock(&connectionDetailsObj->connectionLock);
									//pthread_mutex_unlock(&connectedNeighborsMapLock);

									pthread_mutex_lock(&helloMsgMapLock);
									helloMsgMap.erase(ptr->connectedNeighborsMapKey);
									pthread_mutex_unlock(&helloMsgMapLock);
								}
								else if(ndInfoTemp->port == helloMessageObj->hostPort)
								{
									if(strcmp(ndInfoTemp->hostname,helloMessageObj->hostName) < 0)
									{
										//My host name is smaller. I will close my connection

										//close(connectionDetailsObj->socketDesc);
										//pthread_mutex_lock(&connectedNeighborsMapLock);
										pthread_mutex_lock(&connectionDetailsObj->connectionLock);
										connectionDetailsObj->tiebreak = 1;
										connectionDetailsObj -> notOperational = 1;
										pthread_mutex_lock(&(connectionDetailsObj->messagingQueueLock));
										pthread_cond_signal(&(connectionDetailsObj->messagingQueueCV));
										pthread_mutex_unlock(&(connectionDetailsObj->messagingQueueLock));
										close(connectionDetailsObj->socketDesc);
										pthread_mutex_unlock(&connectionDetailsObj->connectionLock);
										//pthread_mutex_unlock(&connectedNeighborsMapLock);

										pthread_mutex_lock(&helloMsgMapLock);
										helloMsgMap.erase(ptr->connectedNeighborsMapKey);
										pthread_mutex_unlock(&helloMsgMapLock);
									}
								}
								else{
									//pthread_mutex_lock(&connectedNeighborsMapLock);
									pthread_mutex_lock(&connectionDetailsObj->connectionLock);
									connectionDetailsObj -> helloStatus = 1;
									connectionDetailsObj->tiebreak = 0;
									pthread_mutex_unlock(&connectionDetailsObj->connectionLock);
									//pthread_mutex_unlock(&connectedNeighborsMapLock);
								}*/

								pthread_mutex_lock(&connectionDetailsObj->connectionLock);
								connectionDetailsObj -> helloStatus = 1;
								connectionDetailsObj->tiebreak = 0;
								pthread_mutex_unlock(&connectionDetailsObj->connectionLock);
								//pthread_mutex_lock(&connectedNeighborsMapLock);
								char keytype[21] = "\0";
								int i = 0;
								while(i<20)
								{
									keytype[i] = *(connectionDetailsObj->hellomsguoid + i);
									i++;
								}
								keytype[i] = '\0';
								string keyTemp = string(keytype);
								pthread_mutex_unlock(&connectedNeighborsMapLock);

									pthread_mutex_lock(&messageCacheMapLock);
									messageCacheMapIter = messageCacheMap.find(keyTemp);
									if(messageCacheMapIter != messageCacheMap.end())
									{
										messageCacheMap.erase(keyTemp);
									}
									else{

									}
									pthread_mutex_unlock(&messageCacheMapLock);
								//}
							}
							else
							{
								if(ndInfoTemp->port > helloMessageObj->hostPort)
								{
									//My port is greater. I will close peer node's connection
									/*pthread_mutex_lock(&connectedNeighborsMapLock);
									pthread_mutex_lock(&connectionDetailsObj->connectionLock);
									connectionDetailsObj->tiebreak = 1;
									connectionDetailsObj -> notOperational = 1;
									pthread_mutex_lock(&(connectionDetailsObj->messagingQueueLock));
									pthread_cond_signal(&(connectionDetailsObj->messagingQueueCV));
									pthread_mutex_unlock(&(connectionDetailsObj->messagingQueueLock));
									//close(connectionDetailsObj->socketDesc);
									pthread_mutex_unlock(&connectionDetailsObj->connectionLock);
									pthread_mutex_unlock(&connectedNeighborsMapLock);*/

								}
								else if(ndInfoTemp->port == helloMessageObj->hostPort)
								{
									if(strcmp(ndInfoTemp->hostname,helloMessageObj->hostName) > 0)
									{
										//My host name is greater. I will close peer node's connection

										//close(connectionDetailsObj->socketDesc);
										//printf("\n\n\t__________ SIGNAL WRITER about KILL STATUS _________\n\n");
										/*pthread_mutex_lock(&connectedNeighborsMapLock);

										pthread_mutex_lock(&connectionDetailsObj->connectionLock);
										connectionDetailsObj->tiebreak = 1;
										connectionDetailsObj -> notOperational = 1;

										pthread_mutex_lock(&(connectionDetailsObj->messagingQueueLock));
										pthread_cond_signal(&(connectionDetailsObj->messagingQueueCV));
										pthread_mutex_unlock(&(connectionDetailsObj->messagingQueueLock));
										//close(connectionDetailsObj->socketDesc);
										pthread_mutex_unlock(&connectionDetailsObj->connectionLock);
										pthread_mutex_unlock(&connectedNeighborsMapLock);*/

									}
								}
								else if(ndInfoTemp->port < helloMessageObj->hostPort)
								{
									char keyTemp[256] = "\0";
									sprintf(keyTemp,"%s%d",helloMessageObj->hostName,helloMessageObj->hostPort);
									//struct connectionDetails *cdTemp = getConnectionDetailsFromMap(keyTemp);
									/*if(cdTemp != NULL){
										pthread_mutex_lock(&connectedNeighborsMapLock);
										pthread_mutex_lock(&cdTemp->connectionLock);
										cdTemp->tiebreak = 1;
										cdTemp -> notOperational = 1;
										pthread_mutex_lock(&(cdTemp->messagingQueueLock));
										pthread_cond_signal(&(cdTemp->messagingQueueCV));
										pthread_mutex_unlock(&(cdTemp->messagingQueueLock));
										close(cdTemp->socketDesc);
										pthread_mutex_unlock(&cdTemp->connectionLock);
										pthread_mutex_unlock(&connectedNeighborsMapLock);
									}*/
								}
							}
							printf("\nConnected To BEACON %s:%d\n",connectionDetailsObj->hostname,connectionDetailsObj->wellKnownPort);
							printf("\nservant:%d> ",ndInfoTemp->port);
							//Set this flag to indicate that a set of hello messages have been exchanged
							//Node can now accept other type of messages
							hellomsgsentflag = 1;
						}
						else
						{
							//The node which has set me a HELLO is a non-beacon
							createAndSendHelloMsg(ptr);
						}
					}

					break;

		case KEEPALIVE:
					{

						struct connectionDetails * connectionDetailsObj;
						time_t systemTime;

						systemTime= time(NULL);
						string connectedNeighborsMapKeyTemp = string(ptr->connectedNeighborsMapKey);

						pthread_mutex_lock(&connectedNeighborsMapLock);

						connectedNeighborsMapIter = connectedNeighborsMap.find(connectedNeighborsMapKeyTemp);
						if(connectedNeighborsMapIter == connectedNeighborsMap.end()){
							pthread_mutex_unlock(&connectedNeighborsMapLock);
							//Handle Error - No Key-Value Found
						}
						else{
							connectionDetailsObj = connectedNeighborsMapIter -> second;
							//pthread_mutex_lock(&connectionDetailsObj->connectionLock);
							connectionDetailsObj->lastReadActivity = systemTime;
							//pthread_mutex_unlock(&connectionDetailsObj->connectionLock);
							pthread_mutex_unlock(&connectedNeighborsMapLock);

							/*
							 * Free the keep alive message
							 */
							freeMessage(ptr);
							ptr = NULL;
						}
					}
					break;

		case NOTIFY:
					{
						struct connectionDetails * connectionDetailsObj;
						string connectedNeighborsMapKeyTemp = string(ptr->connectedNeighborsMapKey);
						pthread_mutex_lock(&connectedNeighborsMapLock);
						connectedNeighborsMapIter = connectedNeighborsMap.find(connectedNeighborsMapKeyTemp);
						if(connectedNeighborsMapIter == connectedNeighborsMap.end()){
							pthread_mutex_unlock(&connectedNeighborsMapLock);
							//Handle Error - No Key-Value Found
						}
						else{
							connectionDetailsObj = connectedNeighborsMapIter -> second;
							//pthread_mutex_lock(&connectionDetailsObj->connectionLock);
							connectionDetailsObj -> notOperational = 1;
							pthread_mutex_lock(&connectionDetailsObj->messagingQueueLock);
							pthread_cond_signal(&connectionDetailsObj->messagingQueueCV);
							pthread_mutex_unlock(&connectionDetailsObj->messagingQueueLock);
							close(connectionDetailsObj->socketDesc);
							//pthread_mutex_unlock(&connectionDetailsObj->connectionLock);
							pthread_mutex_unlock(&connectedNeighborsMapLock);
						}
						freeMessage(ptr);
					}
					break;

		case CHECK:
					{
						struct connectionDetails * connectionDetailsObj;
						string connectedNeighborsMapKeyTemp = string(ptr->connectedNeighborsMapKey);
						pthread_mutex_lock(&connectedNeighborsMapLock);
						connectedNeighborsMapIter = connectedNeighborsMap.find(connectedNeighborsMapKeyTemp);
						if(connectedNeighborsMapIter == connectedNeighborsMap.end()){
							pthread_mutex_unlock(&connectedNeighborsMapLock);
							//Handle Error - No Key-Value Found
						}
						else{
							connectionDetailsObj = connectedNeighborsMapIter -> second;
							//pthread_mutex_lock(&connectionDetailsObj->connectionLock);
							if(connectionDetailsObj -> notOperational == 0  &&
									connectionDetailsObj->helloStatus == 1 && connectionDetailsObj->threadsExitedCount == 0){
								struct checkRespMessage* checkRespMessageObj;
								checkRespMessageObj = createCheckResp(ptr->uoid);

								struct message* messageHeaderObj;
								messageHeaderObj = CreateMessageHeader(RCHECK, /*message Type*/
													ndInfoTemp->ttl, /*TTL*/
													checkRespMessageObj -> dataLength, /*datalength of body*/
													NULL, /*char *messageHeader*/
													NULL, /*char *message*/
													(void*)checkRespMessageObj, /*void *messageStruct*/
													NULL,/*char* temp_connectedNeighborsMapKey*/
													MYMESSAGE, /*MyMessage or notMyMessage */
													ndInfoTemp /*Node Info Pointer*/);

								pthread_mutex_lock(&connectionDetailsObj -> messagingQueueLock);

								connectionDetailsObj -> messagingQueue.push(messageHeaderObj);
								pthread_cond_signal(&connectionDetailsObj -> messagingQueueCV);
								pthread_mutex_unlock(&connectionDetailsObj -> messagingQueueLock);
							}
							//pthread_mutex_unlock(&connectionDetailsObj->connectionLock);
							pthread_mutex_unlock(&connectedNeighborsMapLock);
						}
					}
					break;

		case RCHECK:
					{
						struct checkRespMessage *checkRespMsgObj = (checkRespMessage *)ptr->messageStruct;
						char keytype[256] = "\0";
						int i = 0;
						while(i<20)
						{
							keytype[i] = *(checkRespMsgObj->checkMsgHeaderUOID + i);
							i++;
						}
						string uoidKey = string(keytype);
						pthread_mutex_lock(&messageCacheMapLock);
						messageCacheMapIter = messageCacheMap.find(uoidKey);
						if(messageCacheMapIter != messageCacheMap.end())
						{
							/*
							 * Case 1: Reply for my CHECK
							 */
							if(messageCacheMapIter->second->myMessage == MYMESSAGE)
							{
								/*
								 * Do not forward the response to anyone
								 */
							}
							else
							{
								/*
								 * Case 2: Reply for someone else's CHECK
								 * forward the response to the particular connection
								 */
								message *msg = messageCacheMapIter->second;
								string connectedNeighborsMapKey = string(msg->connectedNeighborsMapKey);

								pthread_mutex_lock(&connectedNeighborsMapLock);
								if((connectedNeighborsMapIter = connectedNeighborsMap.find(connectedNeighborsMapKey)) != connectedNeighborsMap.end())
								{
									connectionDetails *connectionDetailsObj = connectedNeighborsMapIter->second;
									//pthread_mutex_lock(&connectionDetailsObj->connectionLock);
									pthread_mutex_lock(&connectionDetailsObj -> messagingQueueLock);
									//cout << "\nPut Message to queue for forward\n";
									ptr->sendStatus=0;
									connectionDetailsObj -> messagingQueue.push(ptr);
									pthread_cond_signal(&connectionDetailsObj -> messagingQueueCV);
									pthread_mutex_unlock(&connectionDetailsObj -> messagingQueueLock);
									//pthread_mutex_unlock(&connectionDetailsObj->connectionLock);
								}
								pthread_mutex_unlock(&connectedNeighborsMapLock);
							}
						}
						else
						{
							/*
							 * No Such message so drop it
							 */
							freeMessage(ptr);
							ptr = NULL;
						}
						pthread_mutex_unlock(&messageCacheMapLock);
					}
					break;

		case STATUS:
					{
						// Decrement TTL and Forward to all neighbors
						// Also send reply to the sender of STATUS Request

						if(ptr -> ttl > 1){
							ptr -> ttl = ptr -> ttl - 1; // decrement the TTL before forwarding.
							ptr -> ttl = ptr -> ttl > ndInfoTemp->ttl?ndInfoTemp->ttl:ptr -> ttl;
							/*
							 * Forward the reuqest to all the neighbors
							 * First add the msg to cache
							 */
							char keytype[256] = "\0";
							int i = 0;
							while(i<20)
							{
								keytype[i] = *(ptr->uoid + i);
								i++;
							}
							string keyTemp = string(keytype);

							pthread_mutex_lock(&messageCacheMapLock);
							messageCacheMap.insert(MESSCACHEMAPTYPE::value_type(keyTemp,ptr));
							pthread_mutex_unlock(&messageCacheMapLock);

							connectionDetails *connectionDetailsObj;
							string connectedNeighborsMapKeyTemp = string(ptr->connectedNeighborsMapKey);
							pthread_mutex_lock(&connectedNeighborsMapLock);
							connectedNeighborsMapIter = connectedNeighborsMap.begin();
							if(connectedNeighborsMapIter == connectedNeighborsMap.end()){

								pthread_mutex_unlock(&connectedNeighborsMapLock);
								//Handle Error - No Key-Value Found
							}
							else{
								while(connectedNeighborsMapIter != connectedNeighborsMap.end()){
									connectionDetailsObj = connectedNeighborsMapIter -> second;
									//pthread_mutex_lock(&connectionDetailsObj->connectionLock);
									if(connectionDetailsObj->notOperational == 0 &&
											connectionDetailsObj->helloStatus == 1 &&
											connectionDetailsObj->threadsExitedCount == 0){
										// need to forward to these, Key of Sender is not Equal to ConnectionDetails Map Object Key
										if (connectedNeighborsMapKeyTemp.compare(connectedNeighborsMapIter -> first) != 0){
											if(connectionDetailsObj->isJoinConnection != 1)
											{
												pthread_mutex_lock(&connectionDetailsObj -> messagingQueueLock);
												ptr->sendStatus = 0;
												connectionDetailsObj -> messagingQueue.push(ptr);
												pthread_cond_signal(&connectionDetailsObj -> messagingQueueCV);
												pthread_mutex_unlock(&connectionDetailsObj -> messagingQueueLock);
											}
										}
									}
									//pthread_mutex_unlock(&connectionDetailsObj->connectionLock);
									connectedNeighborsMapIter++;
								}
								pthread_mutex_unlock(&connectedNeighborsMapLock);
							}
						}
						/*
						 * Send my reply
						 */
						struct statusRespMessage *statusRespMsgObj = createStatusResp(ndInfoTemp,ptr);

						struct message* messageHeaderObj;
						messageHeaderObj = CreateMessageHeader(RSTATUS, /*message Type*/
								ndInfoTemp -> ttl, /*TTL*/
								statusRespMsgObj -> dataLength, /*datalenght of body*/
								NULL, /*char *messageHeader*/
								NULL, /*char *message*/
								(void*)statusRespMsgObj, /*void *messageStruct*/
								NULL,/*char* temp_connectedNeighborsMapKey*/
								MYMESSAGE, /*MyMessage or notMyMessage */
								ndInfoTemp /*Node Info Pointer*/);

						string connectedNeighborsMapKeyTemp = string(ptr->connectedNeighborsMapKey);
						pthread_mutex_lock(&connectedNeighborsMapLock);
						connectedNeighborsMapIter = connectedNeighborsMap.find(connectedNeighborsMapKeyTemp);
						if(connectedNeighborsMapIter == connectedNeighborsMap.end())
						{

							pthread_mutex_unlock(&connectedNeighborsMapLock);
							break;
						}
						connectionDetails *connectionDetailsObj = connectedNeighborsMapIter->second;
						//pthread_mutex_lock(&connectionDetailsObj->connectionLock);
						pthread_mutex_lock(&connectionDetailsObj -> messagingQueueLock);
						//cout << "\nPut Message to queue for forward\n";
						connectionDetailsObj -> messagingQueue.push(messageHeaderObj);
						pthread_cond_signal(&connectionDetailsObj -> messagingQueueCV);
						pthread_mutex_unlock(&connectionDetailsObj -> messagingQueueLock);
						//pthread_mutex_unlock(&connectionDetailsObj->connectionLock);
						pthread_mutex_unlock(&connectedNeighborsMapLock);

					}
					break;

		case RSTATUS:
					{
						//If STATUS was my message, write the response info in File
						// if it was not my message route back the response to specific STATUS Requester
						bool statusOfFileWrite = true;
						struct statusRespMessage *statusRespMsgObj = (statusRespMessage *) ptr->messageStruct;
						char keytype[256] = "\0";
						int i = 0;
						while(i<20)
						{
							keytype[i] = *(statusRespMsgObj->statusMsgHeaderUOID + i);
							i++;
						}
						string uoidKey = string(keytype);
						pthread_mutex_lock(&messageCacheMapLock);
						messageCacheMapIter = messageCacheMap.find(uoidKey);
						if(messageCacheMapIter != messageCacheMap.end())
						{
							/*
							 * Case 1: Reply for my STATUS
							 */

							if(messageCacheMapIter->second->myMessage == MYMESSAGE)
							{
								/*
								 * Do not forward the response to anyone.
								 * write the responses received to the extfile
								 */

								statusOfFileWrite = writeInfoToStatusFile(statusRespMsgObj);

								pthread_mutex_lock(&keyboardLock);
								pthread_cond_signal(&keyboardCV);
								pthread_mutex_unlock(&keyboardLock);
							}
							else
							{
								/*
								 * Case 2: Reply for someone else's STATUS
								 * forward the response to the particular connection
								 */
								message *msg = messageCacheMapIter->second;
								string connectedNeighborsMapKey = string(msg->connectedNeighborsMapKey);

								pthread_mutex_lock(&connectedNeighborsMapLock);
								if((connectedNeighborsMapIter = connectedNeighborsMap.find(connectedNeighborsMapKey)) != connectedNeighborsMap.end())
								{
									connectionDetails *connectionDetailsObj = connectedNeighborsMapIter->second;
									//pthread_mutex_lock(&connectionDetailsObj->connectionLock);
									pthread_mutex_lock(&connectionDetailsObj -> messagingQueueLock);
									//cout << "\nPut Message to queue for forward\n";
									ptr->sendStatus=0;
									connectionDetailsObj -> messagingQueue.push(ptr);
									pthread_cond_signal(&connectionDetailsObj -> messagingQueueCV);
									pthread_mutex_unlock(&connectionDetailsObj -> messagingQueueLock);
									//pthread_mutex_unlock(&connectionDetailsObj->connectionLock);
								}
								pthread_mutex_unlock(&connectedNeighborsMapLock);
							}
						}
						else
						{
							/*
							 * No Such message so drop it
							 */
						//	printf("\n\n ( ____________ ) No Such msg in cache (_______________)\n");
							freeMessage(ptr);
							ptr = NULL;
						}
						pthread_mutex_unlock(&messageCacheMapLock);
					}
					break;
		// Project Part 2

		case SEARCH:
					{
						/*1.	Determine the type of search the user has requested. It can be based on
							keyword, SHA1 or filename

						2.	Depending on the type search the appropriate INDX map for the entry.

							if(keyword)
								-	Compute bitvector for the keywords provided in the query
								-	Find on the kwrd_indx map for this bitvector
								-	If no match
									->	NO ACTION TO BE TAKEN
								-	If MATCH
									->	Get the list of file numbers that have matched, from the multimap
									->	For each of these files check the entry that matches exaclty
									->	Add the metadata for these files to an array of Metadata
									->	Create search response with this metadata and send it over,
										the connection on which this msg came

							if(SHA1)
								-	Find on sha1_indx map for the SHA1 provided
								-	If no match
									->	NO ACTION TO BE TAKEN
								-	If MATCH
									->	Get the list of file numbers that have matched, from the multimap
									->	For each of these files check the entry that matches exaclty
									->	Add the metadata for these files to an array of Metadata
									->	Create search response with this metadata and send it over,
										the connection on which this msg came

							if(filename)
								-	Find on sha1_indx map for the SHA1 provided
								-	If no match
									->	NO ACTION TO BE TAKEN
								-	If MATCH
									->	Get the list of file numbers that have matched, from the multimap
									->	For each of these files check the entry that matches exaclty
									->	Add the metadata for these files to an array of Metadata
									->	Create search response with this metadata and send it over,
										the connection on which this msg came

						FORWARD the SEARCH msg to my neighbors (probabilistically) decrementing the TTL*/

						ptr->ttl = ptr->ttl - 1;
						struct searchMessage *mySearchMsg = (struct searchMessage*)ptr->messageStruct;
						struct fileMetadata *metaDataArr[50];
						int fileNumbers[50];
						int searchRespCount = 0;

						if(mySearchMsg->searchType == 1)
						{
							//Search for Filename
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
						/*Send My Search Response*/

						if(searchRespCount > 0)
						{
							int fileNo=0;
							pthread_mutex_lock(&LRUlistLock);
							for(int l = 0; l < searchRespCount; l++)
							{
								fileNo = fileNumbers[l];
								for(LRUIter = LRUList.begin(); LRUIter != LRUList.end(); LRUIter++)
								{
									if(fileNo == *LRUIter)
									{
										LRUList.erase(LRUIter);
										LRUList.insert(LRUList.end(),fileNo);
										break;
									}
								}
							}
							pthread_mutex_unlock(&LRUlistLock);

							struct searchRespMessage *mySearchResp = createSearchResp(metaDataArr,searchRespCount,ptr);
							struct message* messageHeaderObj;
							messageHeaderObj = CreateMessageHeader(RSEARCH, /*message Type*/
									ndInfoTemp -> ttl, /*TTL*/
									mySearchResp -> dataLength, /*datalenght of body*/
									NULL, /*char *messageHeader*/
									NULL, /*char *message*/
									(void*)mySearchResp, /*void *messageStruct*/
									NULL,/*char* temp_connectedNeighborsMapKey*/
									MYMESSAGE, /*MyMessage or notMyMessage */
									ndInfoTemp /*Node Info Pointer*/);

							string connectedNeighborsMapKeyTemp = string(ptr->connectedNeighborsMapKey);
							pthread_mutex_lock(&connectedNeighborsMapLock);
							connectedNeighborsMapIter = connectedNeighborsMap.find(connectedNeighborsMapKeyTemp);
							if(connectedNeighborsMapIter == connectedNeighborsMap.end())
							{
								pthread_mutex_unlock(&connectedNeighborsMapLock);
								break;
							}
							connectionDetails *connectionDetailsObj = connectedNeighborsMapIter->second;
							pthread_mutex_lock(&connectionDetailsObj -> messagingQueueLock);
							connectionDetailsObj -> messagingQueue.push(messageHeaderObj);
							pthread_cond_signal(&connectionDetailsObj -> messagingQueueCV);
							pthread_mutex_unlock(&connectionDetailsObj -> messagingQueueLock);
							pthread_mutex_unlock(&connectedNeighborsMapLock);
						}

						/*Forward SEARCH message if ttl > 1*/
						if(ptr->ttl >= 1)
						{
							char keytype[256] = "\0";
							int i = 0;
							while(i<20)
							{
								keytype[i] = *(ptr->uoid + i);
								i++;
							}
							string keyTemp = string(keytype);

							connectionDetails *connectionDetailsObj;
							string connectedNeighborsMapKeyTemp = string(ptr->connectedNeighborsMapKey);
							pthread_mutex_lock(&connectedNeighborsMapLock);
							connectedNeighborsMapIter = connectedNeighborsMap.begin();
							if(connectedNeighborsMapIter == connectedNeighborsMap.end()){

								pthread_mutex_unlock(&connectedNeighborsMapLock);
								//Handle Error - No Key-Value Found
							}
							else{
								while(connectedNeighborsMapIter != connectedNeighborsMap.end()){
									connectionDetailsObj = connectedNeighborsMapIter -> second;
									if(connectionDetailsObj->notOperational == 0 &&
											connectionDetailsObj->helloStatus == 1 &&
											connectionDetailsObj->threadsExitedCount == 0){
										// need to forward to these, Key of Sender is not Equal to ConnectionDetails Map Object Key
										if (connectedNeighborsMapKeyTemp.compare(connectedNeighborsMapIter -> first) != 0){
											if(connectionDetailsObj->isJoinConnection != 1)
											{
												pthread_mutex_lock(&connectionDetailsObj -> messagingQueueLock);
												//			cout << "\nPut Message to queue for forward\n";
												ptr->sendStatus=0;
												connectionDetailsObj -> messagingQueue.push(ptr);
												pthread_cond_signal(&connectionDetailsObj -> messagingQueueCV);
												pthread_mutex_unlock(&connectionDetailsObj -> messagingQueueLock);
											}
										}
									}
									connectedNeighborsMapIter++;
								}
								pthread_mutex_unlock(&connectedNeighborsMapLock);
							}
						}
					}
					break;

		case RSEARCH:
					{
						/*1.	Forward the response to the connection over which the initial SEARCH msg was
							received*/

						struct searchRespMessage *mySearchResp = (struct searchRespMessage*)ptr->messageStruct;

						ptr->ttl = ptr->ttl - 1;

						char keytype[256] = "\0";
						int i = 0;
						while(i<20)
						{
							keytype[i] = *(mySearchResp->searchUOID + i);
							i++;
						}
						string uoidKey = string(keytype);
						pthread_mutex_lock(&messageCacheMapLock);
						messageCacheMapIter = messageCacheMap.find(uoidKey);
						if(messageCacheMapIter != messageCacheMap.end())
						{
							/*
							 * Case 1: Reply for my SEARCH
							 */

							if(messageCacheMapIter->second->myMessage == MYMESSAGE)
							{
								/*
								 * Do not forward the response to anyone.
								 * write the responses received to the extfile
								 */
								int j = 0;
								no_of_search_responses += mySearchResp->respCount;
								int k = 0;

								for(j = 0; j < mySearchResp->respCount;j++)
								{
									if(searchResponses[searchRespIndex] != NULL)
									{
										free(searchResponses[searchRespIndex]);
										searchResponses[searchRespIndex] = NULL;
									}

									searchResponses[searchRespIndex] = mySearchResp->respMetadata[j];
									searchRespIndex++;

									k = 0;
									printf("\n[%d] ",searchRespIndex+j);
									printf("FileID=");
									while(k < 20)
									{
										printf("%02x",mySearchResp->respMetadata[j]->fileId[k]);
										k++;
									}
									printf("\n    FileName=%s\n",mySearchResp->respMetadata[j]->fileName);
									printf("    FileSize=%d\n",mySearchResp->respMetadata[j]->fileSize);
									printf("    SHA1=");
									k = 0;
									while(k < 20)
									{
										printf("%02x",mySearchResp->respMetadata[j]->SHA1val[k]);
										k++;
									}
									printf("\n    Nonce=");
									k=0;
									while(k < 20)
									{
										printf("%02x",mySearchResp->respMetadata[j]->nonce[k]);
										k++;
									}
									printf("\n    Keywords=%s",mySearchResp->respMetadata[j]->keywordsWithSpace);

								}
								printf("\n");

							}
							else
							{
								/*
								 * Case 2: Reply for someone else's SEARCH
								 * forward the response to the particular connection
								 */

								message *msg = messageCacheMapIter->second;
								string connectedNeighborsMapKey = string(msg->connectedNeighborsMapKey);

								pthread_mutex_lock(&connectedNeighborsMapLock);
								if((connectedNeighborsMapIter = connectedNeighborsMap.find(connectedNeighborsMapKey)) != connectedNeighborsMap.end())
								{
									connectionDetails *connectionDetailsObj = connectedNeighborsMapIter->second;
									//pthread_mutex_lock(&connectionDetailsObj->connectionLock);
									pthread_mutex_lock(&connectionDetailsObj -> messagingQueueLock);
									//cout << "\nPut Message to queue for forward\n";
									ptr->sendStatus=0;
									connectionDetailsObj -> messagingQueue.push(ptr);
									pthread_cond_signal(&connectionDetailsObj -> messagingQueueCV);
									pthread_mutex_unlock(&connectionDetailsObj -> messagingQueueLock);
									//pthread_mutex_unlock(&connectionDetailsObj->connectionLock);
								}
								pthread_mutex_unlock(&connectedNeighborsMapLock);

							}
						}
						else
						{
							/*
							 * No Such message so drop it
							 */

							freeMessage(ptr);
							ptr = NULL;
						}
						pthread_mutex_unlock(&messageCacheMapLock);

					}
					break;
		case DELETE:
					{
						/*1.	From the file Spec which looks as follows:

								FileName=foo
  	  	  	  	  	  	  		SHA1=63de...
  	  	  	  	  	  	  		Nonce=fcca...
  	  	  	  	  	  	    	Password=bac9...
							-> Check if I have the file
								-|	If not then Just forward the msg (decrementing the TTL)

  	  	  	  	  	  	    	-|	Else Use the password, calculate its SHA1.

  	  	  	  	  	  	2.	Compare this SHA1 value with the Nonce in the delete msg
  	  	  	  	  	  		-> 	If they match then valid delete msg (owner of the file has issued)
  	  	  	  	  	  			delete the file from the file system. Remove its entry from the LRU list
  	  	  	  	  	  		->	If no match, NO ACTION TO BE TAKEN. Forward the msg to neighbors*/

						ptr->ttl = ptr->ttl - 1;

						struct deleteMessage *myDeleteMsg = (struct deleteMessage*)ptr->messageStruct;
						int i=0;


						int fileMatch = 0,fileNo = 0;


						char keytype[256] = "\0";
						i = 0;
						while(myDeleteMsg->fileName[i] != '\0')
						{
							keytype[i] = *(myDeleteMsg->fileName + i);
							i++;
						}
						keytype[i] = '\0';

						string searchKey = string(keytype);


						pthread_mutex_lock(&fileNameIndexMapLock);
						fileNameIndexMapIter = fileNameIndexMap.find(searchKey);
						if(fileNameIndexMapIter != fileNameIndexMap.end())
						{
							fileNameIndexIt = fileNameIndexMap.equal_range(fileNameIndexMapIter->first);
							for(fileNameIndexITIter = fileNameIndexIt.first; fileNameIndexITIter != fileNameIndexIt.second ; fileNameIndexITIter++)
							{
								fileMatch = checkForFileWithNonceAndSHA1(myDeleteMsg,fileNameIndexITIter->second,1);
								if(fileMatch == 1)
								{
									fileNo = fileNameIndexITIter->second;
									break;
								}
							}
						}
						pthread_mutex_unlock(&fileNameIndexMapLock);

						if(fileMatch == 1)
						{
							/*File found with matching nonce and owner is valid*/
							char cacheFileName[256] = "\0";
							removeFileEntryFromIndexes(fileNo);


							memset(cacheFileName,0,256);
							sprintf(cacheFileName,"%s/files/%d.meta",ndInfoTemp->homeDir,fileNo);
							remove(cacheFileName);

							memset(cacheFileName,0,256);
							sprintf(cacheFileName,"%s/files/%d.data",ndInfoTemp->homeDir,fileNo);
							remove(cacheFileName);

							memset(cacheFileName,0,256);
							sprintf(cacheFileName,"%s/files/%d.fileid",ndInfoTemp->homeDir,fileNo);
							remove(cacheFileName);


							/*Temp Code */
							pthread_mutex_lock(&permFileListLock);
							for(PermFileListIter = PermFileList.begin(); PermFileListIter != PermFileList.end(); PermFileListIter++)
							{
								if(fileNo == *PermFileListIter)
								{
									PermFileList.erase(PermFileListIter);
									break;
								}
							}
							pthread_mutex_unlock(&permFileListLock);

							pthread_mutex_lock(&LRUlistLock);
							for(LRUIter = LRUList.begin(); LRUIter != LRUList.end(); LRUIter++)
							{
								if(fileNo == *LRUIter)
								{
									LRUList.erase(LRUIter);
									break;
								}
							}
							pthread_mutex_unlock(&LRUlistLock);

							printf("\nFile DELETED Successfully!!!\n");
						}
						else if(fileMatch == 2)
						{
							printf("\nPassword OR Nonce for file is incorrect\n");
						}
						else if(fileMatch == 0)
						{
							printf("\nThe File Requested for DELETION not present\n");
						}

						if(ptr->ttl >= 1)
						{
							/*Forward the msg (probabilistically) to my neighbors*/
							struct connectionDetails *connectionDetailsObj = NULL;
							string connectedNeighborsMapKeyTemp = string(ptr->connectedNeighborsMapKey);
							pthread_mutex_lock(&connectedNeighborsMapLock);
							connectedNeighborsMapIter = connectedNeighborsMap.begin();
							if(connectedNeighborsMapIter == connectedNeighborsMap.end()){

								pthread_mutex_unlock(&connectedNeighborsMapLock);
								//Handle Error - No Key-Value Found
							}
							else{
								while(connectedNeighborsMapIter != connectedNeighborsMap.end()){
									connectionDetailsObj = connectedNeighborsMapIter -> second;

									if(connectionDetailsObj->notOperational == 0 &&
											connectionDetailsObj->helloStatus == 1 &&
											connectionDetailsObj->threadsExitedCount == 0){
										// need to forward to these, Key of Sender is not Equal to ConnectionDetails Map Object Key
										if (connectedNeighborsMapKeyTemp.compare(connectedNeighborsMapIter -> first) != 0){
											if(connectionDetailsObj->isJoinConnection != 1)
											{
												pthread_mutex_lock(&connectionDetailsObj -> messagingQueueLock);
												ptr->sendStatus=0;
												connectionDetailsObj -> messagingQueue.push(ptr);
												pthread_cond_signal(&connectionDetailsObj -> messagingQueueCV);
												pthread_mutex_unlock(&connectionDetailsObj -> messagingQueueLock);
											}
										}
									}
									connectedNeighborsMapIter++;
								}
								pthread_mutex_unlock(&connectedNeighborsMapLock);
							}
						}
						else{
							freeMessage(ptr);
						}
						printf("\nservant:%d> ",ndInfoTemp->port);
  					}
					break;
		case STORE:
					{
						/*NeighborStoreProb - When a node originates or receives a store request, it asks its
						neighbors to cache a copy of the file. For each neighbor, it flips a coin (with this
						probability of getting a positive outcome) to decide if it should forward the store
						request to the corresponding neighbor. The default value is 0.2.

						StoreProb - When a node receives a store request from a neighbor, it flips a coin
						(with this probability of getting a positive outcome) to decide if it should cache
						a copy of it. If the result is positive, a copy of the file is stored. (Please note
						that if a node decides not to keep a copy of a file this way, it should not continue
						to flood the store request message.) The node where this store request was originated
						ignores this value. The default value is 0.1.

						1.	Flip a coin to determine whether I am going to cache the file
						2. 	If No, Then delete the temporary file (created by reader - This must be
							specified in the message that the reader gives the dispatcher)

							- 	I will not forward the STORE msg further (as I am dropping the file)
							BREAK;
						3. 	If Yes,
							a. 	Check if the file given is valid. Compute the SHA1 hash value of the file and
								compare it against the stored file description. If it does not match, the
								file should be discarded.

								- 	I will not forward the STORE msg further (as I am dropping the file)

								BREAK;
							b.	if the SHA1 matches the description, I have to store the file.
								-> 	I need to add the file to the LRU list (for caching)
									-|	If there is no space for file to be stored in the cache, I will make
										space in the cache by removing the LRU files (files at TOP of LRU list)

									-|	Rename the file (getting the current file index to be used
									-|	save the metadata of the file
									-|	Generate a fileid for the file and save it
									-|	Add the file to LRU list*/

						ptr->ttl = ptr->ttl - 1;

						struct storeMessage *myStoreMsg = (struct storeMessage*)ptr->messageStruct;
													struct fileMetadata *recvdFileMetaData = myStoreMsg->respMetadata;
						double myStoreProb = getProbability();
						if(myStoreProb < ndInfoTemp->storeProb)
						{
							if(recvdFileMetaData->fileSize > (ndInfoTemp->cacheSize * 1000))
							{
								/*File Too big for cache, DROP it*/
								remove(recvdFileMetaData->fileWithPath);
								freeMessage(ptr);
								break;
								//As I am dropping the file I will not flood the msg further
							}
							else
							{
								int status = 0;
								status = checkIfFileAlreadyPresentInCache(recvdFileMetaData);
								if(status == 0)
								{
									mycacheSize = getCurrentCacheSize();
									if(recvdFileMetaData->fileSize + mycacheSize < (ndInfoTemp->cacheSize * 1000))
									{
										//Space available in cache, I will store the file

										storeFile(recvdFileMetaData,1,NULL);
									}
									else
									{
										//I will first make space in my cache
										char cacheFileName[256] = "\0";
										int fileNo=0;
										printf("\nCache Full. Need to make space\n");
										while(recvdFileMetaData->fileSize + mycacheSize > (ndInfoTemp->cacheSize * 1000))
										{
											removeFileEntryFromIndexes(fileNo);

											pthread_mutex_lock(&LRUlistLock);
											fileNo = LRUList.front();
											LRUList.erase(LRUList.begin());
											memset(cacheFileName,0,256);
											sprintf(cacheFileName,"%s/files/%d.meta",ndInfoTemp->homeDir,fileNo);
											remove(cacheFileName);

											memset(cacheFileName,0,256);
											sprintf(cacheFileName,"%s/files/%d.data",ndInfoTemp->homeDir,fileNo);
											remove(cacheFileName);

											memset(cacheFileName,0,256);
											sprintf(cacheFileName,"%s/files/%d.fileid",ndInfoTemp->homeDir,fileNo);
											remove(cacheFileName);
											pthread_mutex_unlock(&LRUlistLock);

											mycacheSize = getCurrentCacheSize();
										}

										storeFile(recvdFileMetaData,1,NULL);

										printf("\nservant:%d> ",ndInfoTemp->port);
									}
								}
								else
								{
									//DISPATCHER: FILE Already Present
								}

								if(ptr->ttl >= 1)
								{
									/*Forward the msg (probabilistically) to my neighbors*/
									double sendProb = 0.0;
									struct connectionDetails *connectionDetailsObj = NULL;
									string connectedNeighborsMapKeyTemp = string(ptr->connectedNeighborsMapKey);
									pthread_mutex_lock(&connectedNeighborsMapLock);
									connectedNeighborsMapIter = connectedNeighborsMap.begin();
									if(connectedNeighborsMapIter == connectedNeighborsMap.end()){

										pthread_mutex_unlock(&connectedNeighborsMapLock);
										freeMessage(ptr);
										//Handle Error - No Key-Value Found
									}
									else{
										while(connectedNeighborsMapIter != connectedNeighborsMap.end()){
											connectionDetailsObj = connectedNeighborsMapIter -> second;
											sendProb = getProbability();
											if(sendProb > ndInfoTemp->neighborStoreProb)
												continue;
											if(connectionDetailsObj->notOperational == 0 &&
													connectionDetailsObj->helloStatus == 1 &&
													connectionDetailsObj->threadsExitedCount == 0){
												// need to forward to these, Key of Sender is not Equal to ConnectionDetails Map Object Key
												if (connectedNeighborsMapKeyTemp.compare(connectedNeighborsMapIter -> first) != 0){
													if(connectionDetailsObj->isJoinConnection != 1)
													{
														pthread_mutex_lock(&connectionDetailsObj -> messagingQueueLock);
														ptr->sendStatus=0;
														connectionDetailsObj -> messagingQueue.push(ptr);
														pthread_cond_signal(&connectionDetailsObj -> messagingQueueCV);
														pthread_mutex_unlock(&connectionDetailsObj -> messagingQueueLock);
													}
												}
											}
											connectedNeighborsMapIter++;
										}
										pthread_mutex_unlock(&connectedNeighborsMapLock);
									}
								}
								else{
									freeMessage(ptr);
								}
							}
						}
						else
						{
							remove(recvdFileMetaData->fileWithPath);
							freeMessage(ptr);
							break;
						}
					}
					break;

		case GET:
					{
						/*1.	Check if I have the requested file using the File ID provided in GET msg
							->	If no Match then I DONT have the file. Forward the msg to all my neighbors
							->	If Match found, create a Get response
								Forward this message over the connection on which I got the GET message*/
						ptr->ttl = ptr->ttl - 1;

						struct getMessage *myGetMsg = (struct getMessage *)ptr->messageStruct;
						char keytype[256] = "\0";
						int i = 0,haveFile = 0;
						i = 0;
						while(i < 20)
						{
							keytype[i] = *(myGetMsg->fileSHA + i);
							i++;
						}
						keytype[i] = '\0';

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
							fileidFound = checkFileWithFileId(fileidList,myGetMsg->fileUOID,cnt);
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

						if(haveFile == 1)
						{
							/*I have the file so send the GET response*/
							printf("\nFound Requested file\nSending the File to Requester\n");
							char foundFileName[256] = "\0";
							sprintf(foundFileName,"%s/files/%d.meta",ndInfoTemp->homeDir,fileidFound);
							struct fileMetadata *myFileMeta = createMetaDataFromFile(foundFileName);

							sprintf(myFileMeta->fileWithPath,"%s/files/%d.data",ndInfoTemp->homeDir,fileidFound);

							struct getRespMessage *myGetResponse = createGetResp(myFileMeta,ptr);

							struct message* messageHeaderObj;
							messageHeaderObj = CreateMessageHeader(RGET, /*message Type*/
									ndInfoTemp -> ttl, /*TTL*/
									myGetResponse -> dataLength, /*datalenght of body*/
									NULL, /*char *messageHeader*/
									NULL, /*char *message*/
									(void*)myGetResponse, /*void *messageStruct*/
									NULL,/*char* temp_connectedNeighborsMapKey*/
									MYMESSAGE, /*MyMessage or notMyMessage */
									ndInfoTemp /*Node Info Pointer*/);

							string connectedNeighborsMapKeyTemp = string(ptr->connectedNeighborsMapKey);
							pthread_mutex_lock(&connectedNeighborsMapLock);
							connectedNeighborsMapIter = connectedNeighborsMap.find(connectedNeighborsMapKeyTemp);
							if(connectedNeighborsMapIter == connectedNeighborsMap.end())
							{

								pthread_mutex_unlock(&connectedNeighborsMapLock);
								break;
							}
							connectionDetails *connectionDetailsObj = connectedNeighborsMapIter->second;
							pthread_mutex_lock(&connectionDetailsObj -> messagingQueueLock);
							connectionDetailsObj -> messagingQueue.push(messageHeaderObj);
							pthread_cond_signal(&connectionDetailsObj -> messagingQueueCV);
							pthread_mutex_unlock(&connectionDetailsObj -> messagingQueueLock);
							pthread_mutex_unlock(&connectedNeighborsMapLock);
							printf("\nservant:%d> ",ndInfoTemp->port);
						}
						else if(haveFile == 0)
						{
							/*I dont have the file. I will cache the msg and fwd it to my neighbors*/
							if(ptr->ttl >= 1)
							{
								char keytype[256] = "\0";
								int i = 0;
								while(i<20)
								{
									keytype[i] = *(ptr->uoid + i);
									i++;
								}
								string keyTemp = string(keytype);

								connectionDetails *connectionDetailsObj;
								string connectedNeighborsMapKeyTemp = string(ptr->connectedNeighborsMapKey);
								pthread_mutex_lock(&connectedNeighborsMapLock);
								connectedNeighborsMapIter = connectedNeighborsMap.begin();
								if(connectedNeighborsMapIter == connectedNeighborsMap.end()){

									pthread_mutex_unlock(&connectedNeighborsMapLock);
									//Handle Error - No Key-Value Found
								}
								else{
									while(connectedNeighborsMapIter != connectedNeighborsMap.end()){
										connectionDetailsObj = connectedNeighborsMapIter -> second;
										if(connectionDetailsObj->notOperational == 0 &&
												connectionDetailsObj->helloStatus == 1 &&
												connectionDetailsObj->threadsExitedCount == 0){
											// need to forward to these, Key of Sender is not Equal to ConnectionDetails Map Object Key
											if (connectedNeighborsMapKeyTemp.compare(connectedNeighborsMapIter -> first) != 0){
												if(connectionDetailsObj->isJoinConnection != 1)
												{
													pthread_mutex_lock(&connectionDetailsObj -> messagingQueueLock);
													ptr->sendStatus=0;
													connectionDetailsObj -> messagingQueue.push(ptr);
													pthread_cond_signal(&connectionDetailsObj -> messagingQueueCV);
													pthread_mutex_unlock(&connectionDetailsObj -> messagingQueueLock);
												}
											}
										}
										connectedNeighborsMapIter++;
									}
									pthread_mutex_unlock(&connectedNeighborsMapLock);
								}
							}
						}
					}
					break;

		case RGET:
					{
						/*CacheProb - When a node forwards a get response message from a neighbor, it flips
						a coin (with this probability of getting a positive outcome) to decide if it should
						cache a copy of it. If the result is positive, a copy of the file is stored. The
						default value is 0.1.

						1.	Check if I want to cache the file or not (using the probability above)
						2.	If I decide to cache then
							-|	If there is no space for file to be stored in the cache, I will make
										space in the cache by removing the LRU files (files at TOP of LRU list)

							-|	Rename the file (getting the current file index to be used
							-|	save the metadata of the file
							-|	Generate a fileid for the file and save it
							-|	Add the file to LRU list

						Forward the Response over the connection on which the initial GET was received*/

						struct getRespMessage *myGetResponse = (struct getRespMessage*)ptr->messageStruct;
						ptr->ttl = ptr->ttl - 1;

						char keytype[256] = "\0";
						int i = 0;
						while(i<20)
						{
							keytype[i] = *(myGetResponse->getUOID + i);
							i++;
						}

						string uoidKey = string(keytype);
						pthread_mutex_lock(&messageCacheMapLock);
						messageCacheMapIter = messageCacheMap.find(uoidKey);
						if(messageCacheMapIter != messageCacheMap.end())
						{
							/*
							 * Case 1: Reply for my GET
							 */

							if(messageCacheMapIter->second->myMessage == MYMESSAGE)
							{
								/*
								 * Do not forward the response to anyone.
								 * write the received file to the extfile
								 */
								struct message *storedMsg = messageCacheMapIter->second;
								struct getMessage *storedGet = (struct getMessage*)storedMsg->messageStruct;

								/*
								 *   Check if storedGet->fileName in current Directory.
								 *   	if yes: ask user to remove and overwrite.
								 * 	else store the received file in RGET in current directory with storedGet->fileName filename
								 *
								 * 	myGetResponse->respMetadata->filename = storedGet->fileName
								 *
								 * 	Now check if this filename, SHA of file and nonce is in permanent store.
								 *  if yes: do nothing
								 *  if no: store in perm with new index to make ist sharable.
								 *
								 */

								int status = checkIfFileAlreadyPresentInCache(myGetResponse->respMetadata);
								if(status == 0)
								{
									storeFile(myGetResponse->respMetadata,2,storedGet->fileName);
								}
								else
								{
									FILE *src = NULL;
									FILE *dest = NULL;
									struct stat fileData;
									char line[1000];
									char ans[10] = "\0";
									int canreplace = 1;
									char fileNameForCWD[256] = "\0";
									sprintf(fileNameForCWD,"%s",storedGet->fileName);

									if(stat(fileNameForCWD,&fileData) == 0)
									{
										printf("\nThe file %s already exists\nDo you want to replace the file (yes/no)? ",fileNameForCWD);
										fgets(ans,10,stdin);
										if(strstr(ans,"y") != NULL)
										{
											canreplace = 1;
										}
										else
										{
											canreplace = 0;
										}
									}
									if(canreplace == 1)
									{
										if((src = fopen(myGetResponse->respMetadata->fileWithPath,"r")) == NULL || (dest = fopen(fileNameForCWD,"w")) == NULL)
										{
											printf("\nError opening files\n");
											fclose(src);
											fclose(dest);
										}
										else
										{
											size_t len = 0;
											while( (len = fread( line,1,sizeof(line), src)) > 0 )
											{
												fwrite( line, 1,len, dest ) ;
											}
											fclose(src);
											fclose(dest);
											printf("\nFile Copied Successfully!!!\n");
										}
										remove(myGetResponse->respMetadata->fileWithPath);
									}
								}
								freeMessage(ptr);
								ptr = NULL;
								pthread_mutex_lock(&keyboardLock);
								pthread_cond_signal(&keyboardCV);
								pthread_mutex_unlock(&keyboardLock);
							}
							else
							{
								/*
								 * Case 2: Reply for someone else's GET
								 * Determine if I want to cache the file
								 * forward the response to the particular connection
								 */

								struct fileMetadata *recvdFileMetaData = myGetResponse->respMetadata;
								double cacheProb = getProbability();
								if(cacheProb < ndInfoTemp->cacheProb)
								{
									/*I will cache the file*/
									if(recvdFileMetaData->fileSize < (ndInfoTemp->cacheSize * 1000))
									{
										int status = 0;
										status = checkIfFileAlreadyPresentInCache(recvdFileMetaData);
										if(status == 0)
										{
											if(recvdFileMetaData->fileSize + mycacheSize < (ndInfoTemp->cacheSize * 1000))
											{
												//Space available in cache, I will store the file

												storeFile(recvdFileMetaData,1,NULL);
											}
											else
											{
												//I will first make space in my cache
												char cacheFileName[256] = "\0";
												int fileNo=0;
												printf("\nCache Full. Need to make space\n");
												while(recvdFileMetaData->fileSize + mycacheSize > (ndInfoTemp->cacheSize * 1000))
												{
													removeFileEntryFromIndexes(fileNo);

													pthread_mutex_lock(&LRUlistLock);
													fileNo = LRUList.front();
													LRUList.erase(LRUList.begin());
													memset(cacheFileName,0,256);
													sprintf(cacheFileName,"%s/files/%d.meta",ndInfoTemp->homeDir,fileNo);
													remove(cacheFileName);

													memset(cacheFileName,0,256);
													sprintf(cacheFileName,"%s/files/%d.data",ndInfoTemp->homeDir,fileNo);
													remove(cacheFileName);

													memset(cacheFileName,0,256);
													sprintf(cacheFileName,"%s/files/%d.fileid",ndInfoTemp->homeDir,fileNo);
													remove(cacheFileName);
													pthread_mutex_unlock(&LRUlistLock);

													mycacheSize = getCurrentCacheSize();
												}

												storeFile(recvdFileMetaData,1,NULL);
											}
										}
										else
										{
											//DISPATCHER: FILE Already Present
										}
									}
								}
								if(ptr->ttl >= 1)
								{
									message *msg = messageCacheMapIter->second;
									string connectedNeighborsMapKey = string(msg->connectedNeighborsMapKey);

									pthread_mutex_lock(&connectedNeighborsMapLock);
									if((connectedNeighborsMapIter = connectedNeighborsMap.find(connectedNeighborsMapKey)) != connectedNeighborsMap.end())
									{
										connectionDetails *connectionDetailsObj = connectedNeighborsMapIter->second;
										pthread_mutex_lock(&connectionDetailsObj -> messagingQueueLock);
										ptr->sendStatus=0;
										connectionDetailsObj -> messagingQueue.push(ptr);
										pthread_cond_signal(&connectionDetailsObj -> messagingQueueCV);
										pthread_mutex_unlock(&connectionDetailsObj -> messagingQueueLock);
									}
									pthread_mutex_unlock(&connectedNeighborsMapLock);
								}
							}
						}
						else
						{
							/*
							 * No Such message so drop it
							 */
							freeMessage(ptr);
							ptr = NULL;
						}
						pthread_mutex_unlock(&messageCacheMapLock);

					}
					break;


		default: return;

	}
	return;
}


bool checkMessageValidity(struct message* ptr){

	if(ptr->messType == HELLO || ptr->messType == RSTATUS || ptr->messType == RCHECK || ptr->messType == RGET || ptr->messType == STORE
			|| ptr->messType == RSEARCH || ptr->messType == RJOIN || ptr->messType == KEEPALIVE || ptr->messType == NOTIFY)
		return true;

	pthread_mutex_lock(&messageCacheMapLock);

	char UOIDcharkey[21]="\0";
	int i =0;
	while(i<20){
		UOIDcharkey[i] = ptr->uoid[i];
		i++;
	}
	UOIDcharkey[i]='\0';

	string uoidKey = string(UOIDcharkey);
	messageCacheMapIter = messageCacheMap.find(uoidKey);
	// UOID not found in Message Cache, thats looks like good news
	// lets put the message to message cache and process it further
	if(messageCacheMapIter == messageCacheMap.end()){
		messageCacheMap.insert(MESSCACHEMAPTYPE::value_type(uoidKey,ptr));
		pthread_mutex_unlock(&messageCacheMapLock);
		return true;
	}
	else{
		if(ptr->messType == STORE)
		{
			//Delete the temporary file created in reader
			struct storeMessage *tempStore = (struct storeMessage *)ptr->messageStruct;
			if(strcmp(tempStore->respMetadata->fileWithPath, "\0") != 0)
				remove(tempStore->respMetadata->fileWithPath);
		}
		pthread_mutex_unlock(&messageCacheMapLock);
		return false;
	}
}
