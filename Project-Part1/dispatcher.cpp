#include <iostream>
#include <stdlib.h>
#include "systemDataStructures.h"
#include "iniparser.h"
#include "ntwknode.h"
#include "headers.h"
//#include "messageStructures.h"

extern pthread_mutex_t keyboardLock;
extern pthread_cond_t keyboardCV;

extern unsigned char *GetUOID(char *,char*,unsigned char*,int);

extern int checkNodeType(uint16_t);
void processMessage(struct message*);
bool checkMessageValidity(struct message*);
extern bool writeToLogFile(struct message *ptr,char *);
extern bool writeInfoToStatusFile(statusRespMessage *);
extern void writeErrorToLogFile(char *);

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
	//struct helloMessage *recvdHello = (helloMessage *)ptr->messageStruct;
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
							else
							{
								//pthread_mutex_unlock(&helloMsgMapLock);
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
								pthread_mutex_lock(&connectedNeighborsMapLock);
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
								}

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
									pthread_mutex_lock(&connectedNeighborsMapLock);
									pthread_mutex_lock(&connectionDetailsObj->connectionLock);
									connectionDetailsObj->tiebreak = 1;
									connectionDetailsObj -> notOperational = 1;
									pthread_mutex_lock(&(connectionDetailsObj->messagingQueueLock));
									pthread_cond_signal(&(connectionDetailsObj->messagingQueueCV));
									pthread_mutex_unlock(&(connectionDetailsObj->messagingQueueLock));
									//close(connectionDetailsObj->socketDesc);
									pthread_mutex_unlock(&connectionDetailsObj->connectionLock);
									pthread_mutex_unlock(&connectedNeighborsMapLock);

								}
								else if(ndInfoTemp->port == helloMessageObj->hostPort)
								{
									if(strcmp(ndInfoTemp->hostname,helloMessageObj->hostName) > 0)
									{
										//My host name is greater. I will close peer node's connection

										//close(connectionDetailsObj->socketDesc);
										//printf("\n\n\t__________ SIGNAL WRITER about KILL STATUS _________\n\n");
										pthread_mutex_lock(&connectedNeighborsMapLock);

										pthread_mutex_lock(&connectionDetailsObj->connectionLock);
										connectionDetailsObj->tiebreak = 1;
										connectionDetailsObj -> notOperational = 1;

										pthread_mutex_lock(&(connectionDetailsObj->messagingQueueLock));
										pthread_cond_signal(&(connectionDetailsObj->messagingQueueCV));
										pthread_mutex_unlock(&(connectionDetailsObj->messagingQueueLock));
										//close(connectionDetailsObj->socketDesc);
										pthread_mutex_unlock(&connectionDetailsObj->connectionLock);
										pthread_mutex_unlock(&connectedNeighborsMapLock);

									}
								}
								else if(ndInfoTemp->port < helloMessageObj->hostPort)
								{
									char keyTemp[256] = "\0";
									sprintf(keyTemp,"%s%d",helloMessageObj->hostName,helloMessageObj->hostPort);
									struct connectionDetails *cdTemp = getConnectionDetailsFromMap(keyTemp);
									if(cdTemp != NULL){
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
									}
								}
							}

							//Set this flag to indicate that a set of hello messages have been exchanged
							//Node can now accpet other type of messages
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
						//printConnectedNeighborInforMap();
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
													checkRespMessageObj -> dataLength, /*datalenght of body*/
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
									//			cout << "\nPut Message to queue for forward\n";
												ptr->sendStatus=0;
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


bool checkMessageValidity(struct message* ptr){

	/* Message Cache */
	// extern std::map<string,message *> messageCacheMap;
	// extern std::map<string,message *>::iterator messageCacheMapIter;
	// extern pthread_mutex_t messageCacheMapLock;
	// extern pthread_cond_t messageCacheMapCV;

	//printHeader(ptr);

	/*struct connectionDetails *cd;
	pthread_mutex_lock(&connectedNeighborsMapLock);
	cd = getConnectionDetailsFromMap(ptr->connectedNeighborsMapKey);
	if(cd == NULL){
		pthread_mutex_unlock(&connectedNeighborsMapLock);
		return false;
	}
	if(cd->helloStatus == 0 && ptr->messType != HELLO){
		pthread_mutex_unlock(&connectedNeighborsMapLock);
		return false;
	}
	pthread_mutex_unlock(&connectedNeighborsMapLock);
*/
	if(ptr->messType == HELLO || ptr->messType == RSTATUS || ptr->messType == RCHECK
			|| ptr->messType == RJOIN || ptr->messType == KEEPALIVE || ptr->messType == NOTIFY)
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
		//printf("\n\n (____________) Msg not in cache so INSERT (____________)");
		//string uoidKey = string(reinterpret_cast<const char*>(ptr -> uoid));
		messageCacheMap.insert(MESSCACHEMAPTYPE::value_type(uoidKey,ptr));
		pthread_mutex_unlock(&messageCacheMapLock);
		return true;
	}
	else{
		//printf("\n\n (____________) Msg not in cache so DROP (____________)");
		pthread_mutex_unlock(&messageCacheMapLock);

		//freeMessage(ptr);
		return false;
	}
}
