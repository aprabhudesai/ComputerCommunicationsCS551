#include <iostream>
#include "systemDataStructures.h"
#include "iniparser.h"
#include "headers.h"
//#include "messageStructures.h"

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

extern sigset_t signalSetPipe;
extern struct sigaction actPipe;


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
		while(i < HEADER_LENGTH){

			bytes_recieved = -1;

			tv.tv_sec = 0; //5 seconds wait
			tv.tv_usec = 100000;
			FD_ZERO(&readset);
			FD_SET(sockDesc, &readset);

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

			select_return = select(highSock+1, &readset, (fd_set *) 0,(fd_set *) 0, &tv);
			pthread_mutex_lock(&connectionDetailsObj->connectionLock);


			if(connectionDetailsObj -> notOperational == 1){
							// delete all the allocated memory to handled which will not be handled by timer or Cache

				connectionDetailsObj->threadsExitedCount++;
				pthread_mutex_unlock(&connectionDetailsObj->connectionLock);
				if(messageHeaderStream!=NULL)
				{
					free(messageHeaderStream);

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

				bytes_recieved = recv(sockDesc,messageHeaderStream + i,1,0);

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
			messageBodyStream = (char* )malloc(messageStruct -> dataLength);
			memset(messageBodyStream,0,messageStruct -> dataLength);
			messageStruct->message = messageBodyStream;

			while(i < messageStruct -> dataLength){

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
					bytes_recieved = recv(sockDesc,messageBodyStream + i,1,0);
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
			}
			messageStruct -> message = messageBodyStream;
			inCount++;

			// Parse the message body character stream received
			if(!parseMessage(messageStruct)){
				writeErrorToLogFile((char *)"Error Reading the Character Stream of Data Received at Reader.cpp");
				freeMessage(messageStruct);
				messageStruct = NULL;
				continue;
			}
			else{
				pthread_mutex_lock(&connectionDetailsObj->connectionLock);
				if(messageStruct->messType == HELLO){
					struct helloMessage *messageObj = (struct helloMessage *)messageStruct->messageStruct;
					//pthread_mutex_lock(&connectionDetailsObj->connectionLock);
					connectionDetailsObj->wellKnownPort = messageObj->hostPort;
					//pthread_mutex_unlock(&connectionDetailsObj->connectionLock);
					sprintf(connNodeId,"%s_%d",connectionDetailsObj->hostname,connectionDetailsObj->wellKnownPort);
				}
				if(messageStruct->messType == JOIN){
					struct joinMessage *messageObj = (struct joinMessage *)messageStruct->messageStruct;
					//pthread_mutex_lock(&connectionDetailsObj->connectionLock);
					connectionDetailsObj->wellKnownPort = messageObj->hostPort;
					//pthread_mutex_unlock(&connectionDetailsObj->connectionLock);
					sprintf(connNodeId,"%s_%d",connectionDetailsObj->hostname,connectionDetailsObj->wellKnownPort);
				}
				pthread_mutex_unlock(&connectionDetailsObj->connectionLock);

				// as soon as last byte of Message was received
				messageStruct -> howMessageFormed= 'r';
				writeToLogFile(messageStruct,connNodeId);
				messageStruct -> howMessageFormed= 'f';

				mysystime = time(NULL);
				messageStruct->timestamp = mysystime;
				messageStruct -> connectedNeighborsMapKey = portAndHostName;

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
			pthread_mutex_lock(&eventDispatcherQueueLock);

			eventDispatcherQueue.push(messageStruct);
			pthread_cond_signal(&eventDispatcherQueueCV);
			pthread_mutex_unlock(&eventDispatcherQueueLock);

		}

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
	//printConnectedNeighborInforMap();
	pthread_exit(NULL);
}

