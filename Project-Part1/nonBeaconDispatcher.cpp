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
//#include "messageStructures.h"

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
			//cout << "\nDISPATCHER WAITING ON QUEUE\n";
			pthread_cond_wait(&eventDispatcherQueueCV, &eventDispatcherQueueLock);
			//printf("\nDISPATCHER OUT OF WAIT GOT SIGNAL\n");
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
							//printJoinMessage(joinMessageObj);

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

							//joinRespMessageObj -> uoidKey = string(joinRespMessageObj -> uoidKey);

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
								//cout << "\nTTL <= 1: No forward only reply\n";
								pthread_mutex_lock(&connectedNeighborsMapLock);
								connectedNeighborsMapIter = connectedNeighborsMap.find(connectedNeighborsMapKeyTemp);
								if(connectedNeighborsMapIter == connectedNeighborsMap.end()){
									pthread_mutex_unlock(&connectedNeighborsMapLock);
									//Handle Error - No Key-Value Found
								}
								connectionDetailsObj = connectedNeighborsMapIter -> second;

								pthread_mutex_lock(&connectionDetailsObj->connectionLock);
								//connectionDetailsObj->isJoinConnection = 1;
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
													//cout << "\nPut Message to queue for forward\n";
													connectionDetailsObj -> messagingQueue.push(ptr);
													pthread_cond_signal(&connectionDetailsObj -> messagingQueueCV);
													pthread_mutex_unlock(&connectionDetailsObj -> messagingQueueLock);
												}
											}
											else{ // send reply to the sender here
												pthread_mutex_lock(&connectionDetailsObj -> messagingQueueLock);
												//connectionDetailsObj->isJoinConnection = 1;
												//cout << "\nPut Message to queue for reply\n";
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
						//struct message *mymessage;
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
							//printf("\n\n ( ____________ ) Check if this is my message (_______________)\n");
							if(messageCacheMapIter->second->myMessage == MYMESSAGE)
							{
								/*
								 * Do not forward the response to anyone; Add it to the map of joinresponses
								 */
							//	printf("\nReceived response for My JOIN Request\n");
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

								//printf("\n\n---------------No of JOIN RESP = %d ---------------\n",no_of_join_responses);
								pthread_mutex_lock(&joinMsgMapLock);
								joinMsgMap.insert(JOINMSGMAPTYPE::value_type(keyStr,joinResp));
								pthread_mutex_unlock(&joinMsgMapLock);

								//printJoinMessageInfoMap();
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
										//pthread_mutex_unlock(&messageCacheMapLock);
										//break;
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
								//printJoinMessageInfoMap();
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

						//struct helloMessage* helloMessageObj = (struct helloMessage *)(ptr -> messageStruct);
						//printHelloMessage(helloMessageObj);
						//printf("\n\n--<<--<<--Got Hello From : %s Port : %d ->>->>--\n",helloMessageObj->hostName,helloMessageObj->hostPort);
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
						//printConnectedNeighborInforMap();
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
						//pthread_mutex_lock(&connectionDetailsObj->connectionLock);
						connectionDetailsObj->lastReadActivity = mysystime;
						//pthread_mutex_unlock(&connectionDetailsObj->connectionLock);
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
						//pthread_mutex_lock(&connectionDetailsObj->connectionLock);
						connectionDetailsObj->notOperational = 0;
						pthread_mutex_lock(&connectionDetailsObj->messagingQueueLock);
						pthread_cond_signal(&connectionDetailsObj->messagingQueueCV);
						pthread_mutex_unlock(&connectionDetailsObj->messagingQueueLock);
						close(connectionDetailsObj->socketDesc);
						//pthread_mutex_unlock(&connectionDetailsObj->connectionLock);
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
								//printf("\nconnectedNeighborsMap Empty\n");
								pthread_mutex_unlock(&connectedNeighborsMapLock);
								//Handle Error - No Key-Value Found
							}
							else{
								while(connectedNeighborsMapIter != connectedNeighborsMap.end()){
									connectionDetailsObj = connectedNeighborsMapIter -> second;
									//pthread_mutex_lock(&connectionDetailsObj->connectionLock);
									if(connectionDetailsObj->notOperational == 0){
										// need to forward to these, Key of Sender is not Equal to ConnectionDetails Map Object Key
										if (connectedNeighborsMapKeyTemp.compare(connectedNeighborsMapIter -> first) != 0){
											if(connectionDetailsObj->isJoinConnection != 1)
											{
												pthread_mutex_lock(&connectionDetailsObj -> messagingQueueLock);
												//cout << "\nPut Message to queue for forward\n";
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
										//pthread_mutex_lock(&connectionDetailsObj->connectionLock);
										pthread_mutex_lock(&connectionDetailsObj -> messagingQueueLock);

										connectionDetailsObj -> messagingQueue.push(ptr);
										pthread_cond_signal(&connectionDetailsObj -> messagingQueueCV);
										pthread_mutex_unlock(&connectionDetailsObj -> messagingQueueLock);
										//pthread_mutex_unlock(&connectionDetailsObj->connectionLock);
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
									//pthread_mutex_lock(&connectionDetailsObj->connectionLock);
									if(connectionDetailsObj->notOperational == 0){
										// need to forward to these, Key of Sender is not Equal to ConnectionDetails Map Object Key
										if (connectedNeighborsMapKeyTemp.compare(connectedNeighborsMapIter -> first) != 0){
											if(connectionDetailsObj->isJoinConnection != 1)
											{
												pthread_mutex_lock(&connectionDetailsObj -> messagingQueueLock);
												//cout << "\nPut Message to queue for forward\n";
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
								if(ptr->ttl > 1)
								{
									ptr -> ttl = ptr -> ttl > nonbeaconndInfoTemp->ttl?nonbeaconndInfoTemp->ttl:ptr -> ttl;
									message *msg = messageCacheMapIter->second;
									string connectedNeighborsMapKey = string(msg->connectedNeighborsMapKey);

									pthread_mutex_lock(&connectedNeighborsMapLock);
									if((connectedNeighborsMapIter = connectedNeighborsMap.find(connectedNeighborsMapKey)) != connectedNeighborsMap.end())
									{
										connectionDetails *connectionDetailsObj = connectedNeighborsMapIter->second;
										//pthread_mutex_lock(&connectionDetailsObj->connectionLock);
										pthread_mutex_lock(&connectionDetailsObj -> messagingQueueLock);
										//	cout << "\nPut Message to queue for forward\n";
										connectionDetailsObj -> messagingQueue.push(ptr);
										pthread_cond_signal(&connectionDetailsObj -> messagingQueueCV);
										pthread_mutex_unlock(&connectionDetailsObj -> messagingQueueLock);
										//pthread_mutex_unlock(&connectionDetailsObj->connectionLock);
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

		default: return;

	}
	return;
}
