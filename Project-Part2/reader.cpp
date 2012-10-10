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
#include <openssl/sha.h> /* please read this */
#include <openssl/md5.h>
#include "systemDataStructures.h"
#include "iniparser.h"
#include "headers.h"

void doitHeader(char *);
void doitData(char*);

extern void removeConnectedNeighborInfo(char *);
extern void printConnectedNeighborInforMap();
extern bool writeToLogFile(struct message *,char *);
extern void writeErrorToLogFile(char*);
extern void writeCommentToLogFile(char*);
extern void freeMessage(struct message *);

extern int checkNodeType();
extern void sigpipeHandler(int);
extern void writeMetaDataToFile(struct fileMetadata *,char *);

extern sigset_t signalSetPipe;
extern struct sigaction actPipe;
extern struct NodeInfo ndInfo;

bool receiveFileOverNetwork(struct fileMetadata *,int);

void reader(void *param){

	static int inCount=0;
	char *portAndHostName;  // used as key to get structure pointer, this structure has queue reference and socket desc
	time_t mysystime;
	portAndHostName = (char *)param;
	string portAndHostNameKey = string(portAndHostName);

	/*
	*  Code to get reference from key and get socket desc
	*
	*/
	int sockDesc;
	char connNodeId[300];
	struct connectionDetails *connectionDetailsObj;

	pthread_mutex_lock(&connectedNeighborsMapLock);

	connectedNeighborsMapIter = connectedNeighborsMap.find(portAndHostNameKey);
	if(connectedNeighborsMapIter == connectedNeighborsMap.end()){
		pthread_mutex_unlock(&connectedNeighborsMapLock);
		writeErrorToLogFile((char *)"Connection Details not Found in Reader from Connected Neighbors Map Key");
		pthread_exit(NULL);
		//Handle Error - No Key-Value Found
	}
	connectionDetailsObj = connectedNeighborsMapIter -> second;
	sockDesc = connectionDetailsObj -> socketDesc;
	sprintf(connNodeId,"%s_%d",connectionDetailsObj->hostname,connectionDetailsObj->wellKnownPort);

	pthread_mutex_unlock(&connectedNeighborsMapLock);

	 /* Other variables needed
	 */
	char* messageHeaderStream;
	char* messageBodyStream;
	struct message* messageStruct;
	int nodetype = checkNodeType();
	//sleep(5);

	/*
	 * SIGPIPE handler details
	 */
	actPipe.sa_handler = sigpipeHandler;
	sigaction(SIGINT, &actPipe, NULL);
	pthread_sigmask(SIG_UNBLOCK, &signalSetPipe, NULL);
	/*
	 *
	 *
	 */
	struct timeval tv;
	fd_set readset;
	int select_return = 0;
	int highSock=sockDesc;

	// Keep receiving information at the socket in infinite loop
	while(1){
		messageStruct = NULL;
		messageBodyStream = NULL;
		messageHeaderStream = NULL;

		messageHeaderStream = (char *)malloc(HEADER_LENGTH);
		if(messageHeaderStream == NULL){
			writeErrorToLogFile((char*)"Malloc for messageHeaderStream returned NULL in READER");
			usleep(1000);
			continue;
		}
		memset(messageHeaderStream,0,HEADER_LENGTH);
		int bytes_recieved;
		uint32_t i = 0;

		// Receive Header Message
		while(1){

			bytes_recieved = -1;

			tv.tv_sec = 0; //5 seconds wait
			tv.tv_usec = 100000;
			FD_ZERO(&readset);
			FD_SET(sockDesc, &readset);

			select_return = select(highSock+1, &readset, (fd_set *) 0,(fd_set *) 0, &tv);
			pthread_mutex_lock(&connectionDetailsObj->connectionLock);


			if(connectionDetailsObj -> notOperational == 1){
							// delete all the allocated memory to handled which will not be handled by timer or Cache

				connectionDetailsObj->threadsExitedCount++;
				pthread_mutex_unlock(&connectionDetailsObj->connectionLock);
				if(messageHeaderStream!=NULL)
				{
					free(messageHeaderStream);
					messageHeaderStream = NULL;
				}
				goto OUT;
			}
			pthread_mutex_unlock(&connectionDetailsObj->connectionLock);


			if(select_return < 0){
				writeErrorToLogFile((char*)"Select Error in Reader.cpp");
				if(messageHeaderStream != NULL)
				{
					free(messageHeaderStream);
					messageHeaderStream = NULL;
				}

				pthread_mutex_lock(&connectionDetailsObj->connectionLock);
				connectionDetailsObj->threadsExitedCount++;
				if(nodetype == 0)
				{
					if(connectionDetailsObj->threadsExitedCount == 1 && connectionDetailsObj -> isJoinConnection == 0 && checkSendFlag == 0){
						//connectionDetailsObj -> notOperational = 1;

						pthread_mutex_lock(&checkThreadLock);
						checkStatus = 1;
						pthread_cond_signal(&checkThreadCV);
						pthread_mutex_unlock(&checkThreadLock);
					}
				}
				pthread_mutex_unlock(&connectionDetailsObj->connectionLock);
				goto OUT;
				//pthread_exit(NULL);
			}
			if(select_return != 0){

				bytes_recieved = recv(sockDesc,messageHeaderStream,HEADER_LENGTH,0);

				if(bytes_recieved <= 0){
					//Error Handling
					pthread_mutex_lock(&connectionDetailsObj->connectionLock);
					connectionDetailsObj->threadsExitedCount++;
					if(nodetype == 0)
					{
						if(connectionDetailsObj->threadsExitedCount == 1 && connectionDetailsObj -> isJoinConnection == 0 && checkSendFlag == 0){
							//connectionDetailsObj -> notOperational = 1;

							pthread_mutex_lock(&checkThreadLock);
							checkStatus = 1;
							pthread_cond_signal(&checkThreadCV);
							pthread_mutex_unlock(&checkThreadLock);
						}
					}
					close(sockDesc);
					pthread_mutex_unlock(&connectionDetailsObj->connectionLock);
					if(messageHeaderStream!=NULL)
					{
						free(messageHeaderStream);
						messageHeaderStream = NULL;
					}
					goto OUT;
				}
				i++;
			}
			//I am told to close down by deleting all the memory
			pthread_mutex_lock(&connectionDetailsObj->connectionLock);
			if(connectionDetailsObj -> notOperational == 1){
				// delete all the allocated memory to handled which will not be handled by timer or Cache
				connectionDetailsObj->threadsExitedCount++;
				pthread_mutex_unlock(&connectionDetailsObj->connectionLock);
				if(messageHeaderStream!=NULL)
				{
					free(messageHeaderStream);
					messageHeaderStream = NULL;
				}
				goto OUT;
			}
			/*
			 * update last read activity time
			 */
			mysystime = time(NULL);
			connectionDetailsObj->lastReadActivity = mysystime;
			pthread_mutex_unlock(&connectionDetailsObj->connectionLock);
			if(select_return != 0) break;
		} // END FOR RECEIVING HEADER


		// Parse the character stream of header received above to get the structure
		messageStruct = parseHeader(messageHeaderStream);
		if(messageStruct == NULL){
			writeErrorToLogFile((char*)"Error Reading the Character Stream Received from Sender");
			if(messageHeaderStream!=NULL)
			{
				free(messageHeaderStream);
				messageHeaderStream = NULL;
			}
			continue;
		}
		messageStruct -> message = NULL;
		messageStruct -> messageStruct = NULL;
		messageStruct -> messageHeader = messageHeaderStream;
		messageStruct -> connectedNeighborsMapKey = portAndHostName;

		if(messageStruct -> dataLength > 0){
			// from above parsed header we can get the body of message
			i = 0;

			if(messageStruct->messType != STORE){
				messageBodyStream = (char* )malloc(messageStruct -> dataLength);
				memset(messageBodyStream,0,messageStruct -> dataLength);
				messageStruct->message = messageBodyStream;
			}
			while(1){

				bytes_recieved = -1;
				tv.tv_sec = 0; //5 seconds wait
				tv.tv_usec = 100000;
				FD_ZERO(&readset);
				FD_SET(sockDesc, &readset);

				select_return = select(highSock+1, &readset, (fd_set *) 0,(fd_set *) 0, &tv);

				pthread_mutex_lock(&connectionDetailsObj->connectionLock);
				if(connectionDetailsObj -> notOperational == 1){
					// delete all the allocated memory to handled which will not be handled by timer or Cache
					connectionDetailsObj->threadsExitedCount++;
					pthread_mutex_unlock(&connectionDetailsObj->connectionLock);

					freeMessage(messageStruct);
					messageStruct = NULL;
					goto OUT;
				}
				pthread_mutex_unlock(&connectionDetailsObj->connectionLock);
				if(select_return < 0){
					writeErrorToLogFile((char*)"Select Error in Reader.cpp");
					
					pthread_mutex_lock(&connectionDetailsObj->connectionLock);
					connectionDetailsObj->threadsExitedCount++;
					if(nodetype == 0)
					{
						if(connectionDetailsObj->threadsExitedCount == 1 && connectionDetailsObj -> isJoinConnection == 0 && checkSendFlag == 0){

							pthread_mutex_lock(&checkThreadLock);
							checkStatus = 1;
							pthread_cond_signal(&checkThreadCV);
							pthread_mutex_unlock(&checkThreadLock);
						}
					}
					pthread_mutex_unlock(&connectionDetailsObj->connectionLock);
					freeMessage(messageStruct);
					messageStruct = NULL;
					goto OUT;
				}
				if(select_return != 0){
					if(messageStruct->messType != STORE && messageStruct->messType != RGET){
						bytes_recieved = recv(sockDesc,messageBodyStream,messageStruct -> dataLength,0);
						messageStruct -> message = messageBodyStream;
					}
					else if(messageStruct->messType == STORE){
						char metaSize[4];
						bytes_recieved = recv(sockDesc,metaSize,4,0);
						uint32_t templong;
						memcpy(&templong,metaSize,4);
						templong = (uint32_t)ntohl((templong));
						templong+=4;
						messageBodyStream = (char* )malloc(templong);
						memset(messageBodyStream,0,templong);
						memcpy(messageBodyStream,metaSize,4);
						messageStruct->message = messageBodyStream;
						bytes_recieved = recv(sockDesc,messageBodyStream+4,templong-4,0);
					}
					else if(messageStruct->messType == RGET){
						char messUOID[21];
						memset(messUOID,0,20);
						bytes_recieved = recv(sockDesc,messUOID,20,0);
						unsigned char metaSize[4];
						bytes_recieved = recv(sockDesc,metaSize,4,0);

						uint32_t templong;
						memcpy(&templong,metaSize,4);
						templong = (uint32_t)ntohl(((uint32_t)templong));
						templong+=24;
						messageBodyStream = (char* )malloc(templong);
						memset(messageBodyStream,0,templong);

						memcpy(messageBodyStream,messUOID,20);
						memcpy(messageBodyStream+20,metaSize,4);
						bytes_recieved = recv(sockDesc,messageBodyStream+24,templong-24,0);

						messageStruct->message = messageBodyStream;
					}
					if(bytes_recieved <= 0){
						//Error Handling
						pthread_mutex_lock(&connectionDetailsObj->connectionLock);
						connectionDetailsObj->threadsExitedCount++;
						if(nodetype == 0)
						{
							if(connectionDetailsObj->threadsExitedCount == 1 && connectionDetailsObj -> isJoinConnection == 0 && checkSendFlag == 0){
								//connectionDetailsObj -> notOperational = 1;

								pthread_mutex_lock(&checkThreadLock);
								checkStatus = 1;
								pthread_cond_signal(&checkThreadCV);
								pthread_mutex_unlock(&checkThreadLock);
							}
						}
						close(sockDesc);
						pthread_mutex_unlock(&connectionDetailsObj->connectionLock);
						freeMessage(messageStruct);
						messageStruct = NULL;
						goto OUT;
					}
					i++;
				}

				//I am told to close down by deleting all the memory
				pthread_mutex_lock(&connectionDetailsObj->connectionLock);
				if(connectionDetailsObj -> notOperational == 1){
					// delete all the allocated memory to handled which will not be handled by timer or Cache
					// close socket
					connectionDetailsObj->threadsExitedCount++;
					pthread_mutex_unlock(&connectionDetailsObj->connectionLock);

					freeMessage(messageStruct);
					messageStruct = NULL;
					goto OUT;
				}
					/*
					 * update last read activity time
					 */
					mysystime = time(NULL);
					connectionDetailsObj->lastReadActivity = mysystime;
					pthread_mutex_unlock(&connectionDetailsObj->connectionLock);
					if(select_return != 0) break;
			}
			inCount++;

			// Parse the message body character stream received
			if(!parseMessage(messageStruct)){
				writeErrorToLogFile((char *)"Error Reading the Character Stream of Data Received at Reader.cpp");
				freeMessage(messageStruct);
				messageStruct = NULL;
				continue;
			}
			else{
				if(messageStruct->messType == STORE){
					struct storeMessage *bptr;
					bptr = (struct storeMessage *)messageStruct->messageStruct;
					if(receiveFileOverNetwork(bptr->respMetadata,sockDesc) != true){  // && Store Probability Check -- add later){
						messageStruct -> howMessageFormed= 'r';
						writeToLogFile(messageStruct,connNodeId);
						messageStruct -> howMessageFormed= 'f';

						mysystime = time(NULL);
						messageStruct->timestamp = mysystime;
						messageStruct -> connectedNeighborsMapKey = portAndHostName;

						free(messageStruct->message);
						free(messageStruct->messageHeader);
						messageStruct->message=NULL;
						messageStruct->messageHeader=NULL;
						messageBodyStream = NULL;
						messageHeaderStream = NULL;
						freeMessage(messageStruct);
						goto DROP;
					}
				}
				if(messageStruct->messType == RGET){
					struct getRespMessage *bptr;
					bptr = (struct getRespMessage *)messageStruct->messageStruct;
					if(receiveFileOverNetwork(bptr->respMetadata,sockDesc) != true){
						// Handle Error
					}

				}
				if(messageStruct->messType == HELLO){
					pthread_mutex_lock(&connectionDetailsObj->connectionLock);
					struct helloMessage *messageObj = (struct helloMessage *)messageStruct->messageStruct;
					connectionDetailsObj->wellKnownPort = messageObj->hostPort;
					sprintf(connNodeId,"%s_%d",connectionDetailsObj->hostname,connectionDetailsObj->wellKnownPort);
					pthread_mutex_unlock(&connectionDetailsObj->connectionLock);
				}
				if(messageStruct->messType == JOIN){
					pthread_mutex_lock(&connectionDetailsObj->connectionLock);
					struct joinMessage *messageObj = (struct joinMessage *)messageStruct->messageStruct;
					connectionDetailsObj->wellKnownPort = messageObj->hostPort;
					sprintf(connNodeId,"%s_%d",connectionDetailsObj->hostname,connectionDetailsObj->wellKnownPort);
					pthread_mutex_unlock(&connectionDetailsObj->connectionLock);
				}

				// as soon as last byte of Message was received
				messageStruct -> howMessageFormed= 'r';
				writeToLogFile(messageStruct,connNodeId);
				messageStruct -> howMessageFormed= 'f';

				mysystime = time(NULL);
				messageStruct->timestamp = mysystime;
				messageStruct -> connectedNeighborsMapKey = portAndHostName;

				free(messageStruct->message);
				free(messageStruct->messageHeader);
				messageStruct->message=NULL;
				messageStruct->messageHeader=NULL;
				messageBodyStream = NULL;
				messageHeaderStream = NULL;

				pthread_mutex_lock(&eventDispatcherQueueLock);
				eventDispatcherQueue.push(messageStruct);
				pthread_cond_signal(&eventDispatcherQueueCV);
				pthread_mutex_unlock(&eventDispatcherQueueLock);
			}
		}
		else{
			messageStruct -> howMessageFormed= 'r';
			writeToLogFile(messageStruct,connNodeId);
			messageStruct -> howMessageFormed= 'f';
			messageStruct -> message = NULL;
			messageStruct -> messageStruct = NULL;

			mysystime = time(NULL);
			messageStruct->timestamp = mysystime;
			messageStruct -> connectedNeighborsMapKey = portAndHostName;

			free(messageStruct->message);
			free(messageStruct->messageHeader);
			messageStruct->message=NULL;
			messageStruct->messageHeader=NULL;
			messageBodyStream = NULL;
			messageHeaderStream = NULL;

			pthread_mutex_lock(&eventDispatcherQueueLock);
			eventDispatcherQueue.push(messageStruct);
			pthread_cond_signal(&eventDispatcherQueueCV);
			pthread_mutex_unlock(&eventDispatcherQueueLock);

		}
		DROP:
		pthread_mutex_lock(&connectionDetailsObj->connectionLock);
		if(connectionDetailsObj -> notOperational == 1){
			// delete all the allocated memory to handled which will not be handled by timer or Cache
			// close socket
			connectionDetailsObj->threadsExitedCount++;
			pthread_mutex_unlock(&connectionDetailsObj->connectionLock);
			freeMessage(messageStruct);
			messageStruct = NULL;
			goto OUT;
		}
		pthread_mutex_unlock(&connectionDetailsObj->connectionLock);
	}
	OUT:
	close(sockDesc);
	pthread_exit(NULL);
}


bool receiveFileOverNetwork(struct fileMetadata *metaptr,int sockDesc){

	FILE *fp;
	unsigned char fileContent[1000]="\0";
	//uint32_t i =0;
	int bytes_received;
	char tempMetaFileName[256] = "\0";
	char tempDataFileName[256] = "\0";
	time_t currsystime;

	currsystime = time(NULL);

	sprintf(tempDataFileName,"%s/files/%ld.temp",ndInfo.homeDir,currsystime);
	strcpy(metaptr->fileWithPath,tempDataFileName);
	fp = fopen(tempDataFileName,"w+");
	if(fp == NULL){
		writeErrorToLogFile((char*)"Could not Create File in Reader");
		return false;
	}

	memset(fileContent,0,1000);
	uint32_t fileSize = metaptr->fileSize;

	/*
	 *  SHA1 Declarations
	 */
	unsigned char sha1out[SHA_DIGEST_LENGTH];
	unsigned char *sha1val;
	SHA_CTX sha1obj;

	// SHA1 Initialization
	sha1val = &sha1out[0];
	SHA1_Init(&sha1obj);

	//READ AND SEND FILE
	while (fileSize > 0) {
		memset(fileContent,0,1000);
		bytes_received = recv(sockDesc,fileContent,1000, 0);
		fwrite(fileContent,1,bytes_received,fp);

		SHA1_Update(&sha1obj,fileContent,bytes_received);
		fileSize-=bytes_received;
		bytes_received=0;
		fflush(fp);
	}
	SHA1_Final(sha1val, &sha1obj);
	fclose(fp);

	if(strncmp((char*)sha1out,(char*)metaptr->SHA1val,20) != 0){
		// SHA Not Same so Delete file
		writeErrorToLogFile((char*)"\nSHA Not Same for received file: READER\n");
		remove(tempMetaFileName);
		return false;
	}

	return true;
}

