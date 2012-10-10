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
#include "nonBeaconNode.h"
#include "headers.h"
#include "string.h"
#include "stdlib.h"
#include "stdio.h"

extern unsigned char *GetUOID(char *,char*,unsigned char*,int);
extern struct connectionDetails *getConnectionDetailsFromMap(char *key);
extern void createAndSendHelloMsg(struct message *ptr);
extern bool checkMessageValidity(struct message* ptr);
void processNonBeaconMessage(struct message* ptr);
extern bool writeInfoToStatusFile(statusRespMessage *);
struct NodeInfo *nonbeaconndInfoTemp;
extern struct NodeInfo *ndInfoTemp;
extern int no_of_join_responses;
extern int no_of_hello_responses;
extern int no_of_connection;

void nonBeaconDispatcher(void* param){

	//node info passed to this pthread which consists of all the node information needed
	//to process incoming messages
	nonbeaconndInfoTemp = (struct NodeInfo *)param;
	ndInfoTemp = (struct NodeInfo *)param;

	struct message* messageStruct;
	bool isMessageValid = false;
	while(1){
		isMessageValid = false;
		pthread_mutex_lock(&eventDispatcherQueueLock);

		if(eventDispatcherQueue.empty()){
			pthread_cond_wait(&eventDispatcherQueueCV, &eventDispatcherQueueLock);
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
			processNonBeaconMessage(messageStruct);
		}
		else{
			//delete message and its contents, clear it off
			freeMessage(messageStruct);
			messageStruct = NULL;
		}
	}
}

void processNonBeaconMessage(struct message* ptr){

	time_t mysystime;
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
							/*
							 * Create Reply Message and Reply Message Header Accordingly
							 *
							 */
							struct joinRespMessage* joinRespMessageObj;

							joinRespMessageObj = createJoinRespMessage(joinMessageObj,ptr,nonbeaconndInfoTemp);

							//Overwrite the received message port and hostname, as it is going to be forwarded
							joinMessageObj->hostPort = nonbeaconndInfoTemp -> port;
							strcpy(joinMessageObj->hostName,nonbeaconndInfoTemp -> hostname);
							joinMessageObj->dataLength = (uint32_t)(6 + strlen(joinMessageObj -> hostName));

							ptr->dataLength = joinMessageObj->dataLength;

							struct message* messageHeaderObj;
							messageHeaderObj = CreateMessageHeader(RJOIN, /*message Type*/
																nonbeaconndInfoTemp -> ttl, /*TTL*/
																joinRespMessageObj -> dataLength, /*datalenght of body*/
																NULL, /*char *messageHeader*/
																NULL, /*char *message*/
																(void*)joinRespMessageObj, /*void *messageStruct*/
																NULL,/*char* temp_connectedNeighborsMapKey*/
																MYMESSAGE, /*MyMessage or notMyMessage */
																nonbeaconndInfoTemp /*Node Info Pointer*/);

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

								pthread_mutex_lock(&connectionDetailsObj->connectionLock);
								//Push reply Message to Queue of Sender
								pthread_mutex_lock(&connectionDetailsObj -> messagingQueueLock);
								// Push the message from queue to be sent
								connectionDetailsObj -> messagingQueue.push(messageHeaderObj);
								//Release Messaging Queue Lock
								pthread_cond_signal(&connectionDetailsObj -> messagingQueueCV);
								pthread_mutex_unlock(&connectionDetailsObj -> messagingQueueLock);
								pthread_mutex_unlock(&connectionDetailsObj->connectionLock);
								pthread_mutex_unlock(&connectedNeighborsMapLock);
							}
							else{

								ptr -> ttl = ptr -> ttl - 1; // decrement the TTL before forwarding.
								ptr -> ttl = ptr -> ttl > nonbeaconndInfoTemp->ttl?nonbeaconndInfoTemp->ttl:ptr -> ttl;


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
									while(connectedNeighborsMapIter != connectedNeighborsMap.end()){
										connectionDetailsObj = connectedNeighborsMapIter -> second;
										pthread_mutex_lock(&connectionDetailsObj ->connectionLock);
										if(connectionDetailsObj->notOperational == 0
											&& connectionDetailsObj->threadsExitedCount == 0
											&& connectionDetailsObj->helloStatus == 1){
											// Not the sender, need to forward to these, Key of Sender is not Equal to ConnectionDetails Map Object Key
											if (connectedNeighborsMapKeyTemp.compare(connectedNeighborsMapIter -> first) != 0){
												if(connectionDetailsObj->isJoinConnection != 1)
												{
													pthread_mutex_lock(&connectionDetailsObj -> messagingQueueLock);
													connectionDetailsObj -> messagingQueue.push(ptr);
													pthread_cond_signal(&connectionDetailsObj -> messagingQueueCV);
													pthread_mutex_unlock(&connectionDetailsObj -> messagingQueueLock);
												}
											}
											else{ // send reply to the sender here
												pthread_mutex_lock(&connectionDetailsObj -> messagingQueueLock);
												connectionDetailsObj -> messagingQueue.push(messageHeaderObj);
												pthread_cond_signal(&connectionDetailsObj -> messagingQueueCV);
												pthread_mutex_unlock(&connectionDetailsObj -> messagingQueueLock);
											}
										}
										pthread_mutex_unlock(&connectionDetailsObj ->connectionLock);
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
						 * This can be for two types of messages:
						 * 1. My JOIN request sent
						 * 2. Someone else's request which I forwarded
						 */

						struct joinRespMessage *joinRespMsgObj = (joinRespMessage *)ptr->messageStruct;

						char keytype[256] = "\0";
						int i = 0;
						while(i<20)
						{
							keytype[i] = *(joinRespMsgObj->uoid + i);
							i++;
						}
						string uoidKey = string(keytype);
						pthread_mutex_lock(&messageCacheMapLock);
						messageCacheMapIter = messageCacheMap.find(uoidKey);
						if(messageCacheMapIter != messageCacheMap.end())
						{
							/*
							 * Case 1: Reply for my JOIN
							 */
							if(messageCacheMapIter->second->myMessage == MYMESSAGE)
							{
								/*
								 * Do not forward the response to anyone; Add it to the map of joinresponses
								 */
								struct joinResponseDetails *joinResp = new joinResponseDetails;
								joinResp->distToNode = joinRespMsgObj->distance;
								strcpy(joinResp->hostName,joinRespMsgObj->hostName);
								joinResp->portNum = joinRespMsgObj->hostPort;
								strcpy((char *)joinResp->uoid,(char *)joinRespMsgObj->uoid);

								char mapKey[256]="\0";
								sprintf(mapKey,"%s%d",joinRespMsgObj->hostName,joinRespMsgObj->hostPort);
								string keyStr = string(mapKey);

								pthread_mutex_lock(&systemWideLock);
								no_of_join_responses++;
								pthread_mutex_unlock(&systemWideLock);

								pthread_mutex_lock(&joinMsgMapLock);
								joinMsgMap.insert(JOINMSGMAPTYPE::value_type(keyStr,joinResp));
								pthread_mutex_unlock(&joinMsgMapLock);
							}
							else
							{
								/*
								 * Case 2: Reply for someone else's JOIN
								 * forward the response to the particular connection
								 */
								ptr -> ttl = ptr -> ttl - 1; // decrement the TTL before forwarding.
								struct connectionDetails *connectionDetailsObj;
								string connectedNeighborsMapKeyTemp = string(messageCacheMapIter->second->connectedNeighborsMapKey);
								if(ptr -> ttl > 1)
								{ // forward the packet only if ttl > 1
									ptr -> ttl = ptr -> ttl > nonbeaconndInfoTemp->ttl?nonbeaconndInfoTemp->ttl:ptr -> ttl;
									pthread_mutex_lock(&connectedNeighborsMapLock);
									connectedNeighborsMapIter = connectedNeighborsMap.find(connectedNeighborsMapKeyTemp);
									if(connectedNeighborsMapIter == connectedNeighborsMap.end())
									{
										pthread_mutex_unlock(&connectedNeighborsMapLock);
										//Handle Error - No Key-Value Found
									}
									else
									{
										connectionDetailsObj = connectedNeighborsMapIter -> second;
										pthread_mutex_lock(&connectionDetailsObj->connectionLock);
										pthread_mutex_lock(&connectionDetailsObj -> messagingQueueLock);
										// Push the message from queue to be sent
										connectionDetailsObj -> messagingQueue.push(ptr);
										//Release Messaging Queue Lock
										pthread_cond_signal(&connectionDetailsObj -> messagingQueueCV);
										pthread_mutex_unlock(&connectionDetailsObj -> messagingQueueLock);
										pthread_mutex_unlock(&connectionDetailsObj->connectionLock);
										pthread_mutex_unlock(&connectedNeighborsMapLock);
									}
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

		case HELLO:
					{
						/*
						 *  When I get hello message, either it is a response to my Hello or
						 *  original Hello from a node
						 *
						 *  If it is a response to my hello, but my "Timer" has removed it from message cache,
						 *  then the timer is going to close the connection with that specific node as reply was not received in time for Hello.
						 */

						//The node which has sent me a hello is a beacon
						pthread_mutex_lock(&helloMsgMapLock);
						string connectedNeighborsMapKey = string(ptr->connectedNeighborsMapKey);
						// No Hello Message sent to the Hello Message Sender, so reply back with a hello Message
						if((helloMsgMapIter = helloMsgMap.find(connectedNeighborsMapKey)) == helloMsgMap.end())
						{
							createAndSendHelloMsg(ptr);
							pthread_mutex_unlock(&helloMsgMapLock);
						}else{

							/*
							 * Erase the message from the cache
							 */
							pthread_mutex_unlock(&helloMsgMapLock);

							pthread_mutex_lock(&connectedNeighborsMapLock);
							//Get connection details
							struct connectionDetails *connectionDetailsObj = getConnectionDetailsFromMap(ptr->connectedNeighborsMapKey);

							char keytype[256] = "\0";
							int i = 0;
							while(i<20)
							{
								keytype[i] = *(connectionDetailsObj->hellomsguoid + i);
								i++;
							}
							string uoidKey = string(keytype);

							if(connectionDetailsObj == NULL){

								pthread_mutex_unlock(&connectedNeighborsMapLock);
								//Handle Error - No Key-Value Found
							}
							else{
								pthread_mutex_lock(&connectionDetailsObj->connectionLock);
								connectionDetailsObj->helloStatus = 1;
								pthread_mutex_unlock(&connectionDetailsObj->connectionLock);
								pthread_mutex_unlock(&connectedNeighborsMapLock);

							}
							pthread_mutex_lock(&messageCacheMapLock);
							no_of_hello_responses++;
							messageCacheMap.erase(uoidKey);
							freeMessage(ptr);
							ptr = NULL;
							pthread_mutex_unlock(&messageCacheMapLock);

						}
					}
					break;

		case KEEPALIVE:
					{
						/*
						 *  As soon as Keep Alive is received, update the time for that specific connection in connection details.
						 *
						 *  This time will be used by Timer Thread to keep of record to connection is alive with specific connection.
						 */

						/*
						 *  Timer thread traverses connection Details map and sees the last activity of Keep alive from
						 *  peer node and respectively sends Keep Alive if necessary.
						 */

						mysystime = time(NULL);
						connectionDetails *connectionDetailsObj;
						string connectedNeighborsMapKey = string(ptr->connectedNeighborsMapKey);
						pthread_mutex_lock(&connectedNeighborsMapLock);
						connectedNeighborsMapIter = connectedNeighborsMap.find(connectedNeighborsMapKey);
						if(connectedNeighborsMapIter == connectedNeighborsMap.end()){
							pthread_mutex_unlock(&connectedNeighborsMapLock);
							//Handle Error - No Key-Value Found

							break;
						}
						connectionDetailsObj = connectedNeighborsMapIter -> second;
						connectionDetailsObj->lastReadActivity = mysystime;
						pthread_mutex_unlock(&connectedNeighborsMapLock);

						freeMessage(ptr);
						ptr = NULL;
					}
					break;

		case NOTIFY:
					{
						/*
						 *  If This message is received from specific node, it means that the node is informing us that
						 *  it is closing its connection and shutting down.
						 */

						connectionDetails *connectionDetailsObj;
						string connectedNeighborsMapKey = string(ptr->connectedNeighborsMapKey);
						pthread_mutex_lock(&connectedNeighborsMapLock);
						connectedNeighborsMapIter = connectedNeighborsMap.find(connectedNeighborsMapKey);
						if(connectedNeighborsMapIter == connectedNeighborsMap.end()){
							pthread_mutex_unlock(&connectedNeighborsMapLock);
							//Handle Error - No Key-Value Found

							break;
						}
						connectionDetailsObj = connectedNeighborsMapIter -> second;
						connectionDetailsObj->notOperational = 0;
						pthread_mutex_lock(&connectionDetailsObj->messagingQueueLock);
						pthread_cond_signal(&connectionDetailsObj->messagingQueueCV);
						pthread_mutex_unlock(&connectionDetailsObj->messagingQueueLock);
						close(connectionDetailsObj->socketDesc);
						pthread_mutex_unlock(&connectedNeighborsMapLock);
					}
					break;

		case CHECK:
					{
						/*
						 * 	As I am a Non Beacon Node I need to store the message and
						 * 	forward it to all neighbors with one TTL decremented.
						 * 	if TTL <= 1, then I will not forward.
						 *
						 * 	This message is used to check reachability to beacon Node by some non beacon node.
						 */

						if(ptr -> ttl > 1){
							ptr -> ttl = ptr -> ttl - 1; // decrement the TTL before forwarding.
							ptr -> ttl = ptr -> ttl > nonbeaconndInfoTemp->ttl?nonbeaconndInfoTemp->ttl:ptr -> ttl;
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
							messageCacheMap.insert(MESSCACHEMAPTYPE::value_type(keytype,ptr));
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
									if(connectionDetailsObj->notOperational == 0){
										// need to forward to these, Key of Sender is not Equal to ConnectionDetails Map Object Key
										if (connectedNeighborsMapKeyTemp.compare(connectedNeighborsMapIter -> first) != 0){
											if(connectionDetailsObj->isJoinConnection != 1)
											{
												pthread_mutex_lock(&connectionDetailsObj -> messagingQueueLock);
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
						/*
						 * Drop the packet
						 */
						}
					}
					break;

		case RCHECK:
					{
						/*
						 *  1. if this is response for my check message, then it is confirmed that I can reach some SERVANT BEACON Node
						 *  2. if this response is not for my message, forward the reply to respective node which has sent CHECK Message.
						 */


						struct checkRespMessage *checkRespMsgObj = (checkRespMessage *)ptr->messageStruct;

						char keytype[256] = "\0";
						int i = 0;
						while(i<20)
						{
							keytype[i] = *(checkRespMsgObj->checkMsgHeaderUOID + i);
							i++;
						}
						keytype[i] = '\0';
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

								printf("\n\nGot CHECK response\nNode is connected to SERVANT network\n");
								printf("\nservant:%d> ",nonbeaconndInfoTemp->port);
								messageCacheMap.erase(uoidKey);
								struct message * tempmsg = messageCacheMapIter->second;
								freeMessage(tempmsg);
							}
							else
							{
								/*
								 * Case 2: Reply for someone else's CHECK
								 * forward the response to the particular connection
								 */
								ptr -> ttl = ptr -> ttl - 1; // decrement the TTL before forwarding.
								if(ptr->ttl > 1)
								{
									ptr -> ttl = ptr -> ttl > nonbeaconndInfoTemp->ttl?nonbeaconndInfoTemp->ttl:ptr -> ttl;
									message *msg = messageCacheMapIter->second;
									string connectedNeighborsMapKey = string(msg->connectedNeighborsMapKey);

									pthread_mutex_lock(&connectedNeighborsMapLock);
									if((connectedNeighborsMapIter = connectedNeighborsMap.find(connectedNeighborsMapKey)) != connectedNeighborsMap.end())
									{
										connectionDetails *connectionDetailsObj = connectedNeighborsMapIter->second;
										pthread_mutex_lock(&connectionDetailsObj -> messagingQueueLock);

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

		case STATUS:
					{
						/*
						 * Check if the TTL of the message is still valid
						 * If its valid then FWD it to all the neighbors &
						 * Also send own reply back over the same connection
						 */

						if(ptr -> ttl > 1){
							ptr -> ttl = ptr -> ttl - 1; // decrement the TTL before forwarding.
							ptr -> ttl = ptr -> ttl > nonbeaconndInfoTemp->ttl?nonbeaconndInfoTemp->ttl:ptr -> ttl;
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
							messageCacheMap.insert(MESSCACHEMAPTYPE::value_type(keytype,ptr));
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
									if(connectionDetailsObj->notOperational == 0){
										// need to forward to these, Key of Sender is not Equal to ConnectionDetails Map Object Key
										if (connectedNeighborsMapKeyTemp.compare(connectedNeighborsMapIter -> first) != 0){
											if(connectionDetailsObj->isJoinConnection != 1)
											{
												pthread_mutex_lock(&connectionDetailsObj -> messagingQueueLock);
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
						/*
						 * Send my reply
						 */
						struct statusRespMessage *statusRespMsgObj = createStatusResp(nonbeaconndInfoTemp,ptr);

						struct message* messageHeaderObj;
						messageHeaderObj = CreateMessageHeader(RSTATUS, /*message Type*/
								nonbeaconndInfoTemp -> ttl, /*TTL*/
								statusRespMsgObj -> dataLength, /*datalenght of body*/
								NULL, /*char *messageHeader*/
								NULL, /*char *message*/
								(void*)statusRespMsgObj, /*void *messageStruct*/
								NULL,/*char* temp_connectedNeighborsMapKey*/
								MYMESSAGE, /*MyMessage or notMyMessage */
								nonbeaconndInfoTemp /*Node Info Pointer*/);

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
					break;

		case RSTATUS:
					{
						/*
						 * Check if this message is in the cache. If found in cache, then check
						 * if its a reply for my own STATUS message. In that case I will not forward it
						 * but I will write it to the extfile provided by user.
						 * else I will forward it to the appropriate connection
						 */
						bool statusOfFileWrite = true;
						struct statusRespMessage *statusRespMsgObj = (statusRespMessage *) ptr->messageStruct;
						char keytype[21] = "\0";
						int i = 0;
						while(i<20)
						{
							keytype[i] = *(statusRespMsgObj->statusMsgHeaderUOID + i);
							i++;
						}
						keytype[i] = '\0';
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
								ptr->ttl = ptr->ttl - 1;
								if(ptr->ttl >= 1)
								{
									ptr -> ttl = ptr -> ttl > nonbeaconndInfoTemp->ttl?nonbeaconndInfoTemp->ttl:ptr -> ttl;
									message *msg = messageCacheMapIter->second;
									string connectedNeighborsMapKey = string(msg->connectedNeighborsMapKey);

									pthread_mutex_lock(&connectedNeighborsMapLock);
									if((connectedNeighborsMapIter = connectedNeighborsMap.find(connectedNeighborsMapKey)) != connectedNeighborsMap.end())
									{
										connectionDetails *connectionDetailsObj = connectedNeighborsMapIter->second;
										pthread_mutex_lock(&connectionDetailsObj -> messagingQueueLock);
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
		// Project Part 2
		case SEARCH:
					{
						ptr->ttl = ptr->ttl - 1;
						struct searchMessage *mySearchMsg = (struct searchMessage*)ptr->messageStruct;
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
									pthread_mutex_lock(&connectionDetailsObj -> messagingQueueLock);
									ptr->sendStatus=0;
									connectionDetailsObj -> messagingQueue.push(ptr);
									pthread_cond_signal(&connectionDetailsObj -> messagingQueueCV);
									pthread_mutex_unlock(&connectionDetailsObj -> messagingQueueLock);
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
							//DISPATCHER: STORE Probability is -ve so drop the file
							remove(recvdFileMetaData->fileWithPath);
							freeMessage(ptr);
							break;
						}
					}
					break;

		case GET:
					{
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
