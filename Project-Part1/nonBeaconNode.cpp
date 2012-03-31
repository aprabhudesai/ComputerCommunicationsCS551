/*
 * nonBeaconNode.cpp
 *
 *  Created on: Mar 10, 2012
 *      Author: abhishek
 */

#include "headers.h"
#include "nonBeaconNode.h"
#include "keyboardThread.h"
#include "systemDataStructures.h"
#include <iostream>

struct NodeInfo *nonBeaconNodeInfo;

char initFile[256] = "\0";
pthread_t nonBeaconDispatcherThread, nonBeaconTimerThread, nonBeaconConnectorThread,nonBeaconListenerThread,keyboardThreadNonbeacon,nonBeaconCheckThread;
int mynonbeaconSocket = 0;
struct  sockaddr_in my_non_beacon_addr;
int nonbeaconsockOptLen = sizeof(int);
bool nonbeaconsockOpt = true;
struct sockaddr_in accept_addr_nonbeacon;
char *initNeighborsHostnameList[100];
uint16_t initNeighborsPortList[100];

int no_of_join_responses = 0;
int no_of_hello_responses = 0;
int no_of_connection = 0;
int participating = 0;


extern void reader(void *);
extern void writer(void *);
extern int checkFileExists(char *);
extern void sigUSR1Handler(int);
extern void freeConnectionDetailsMap();
extern void writeCommentToLogFile(char *);
extern void writeErrorToLogFile(char *);
extern sigset_t signalSetUSR1;
extern struct sigaction actUSR1;
extern int restartState;
extern int checkStatus;

int joinServantNtwk()
{
	int i=0,status = 0;

	struct sockaddr_in beacon_addr;
	struct hostent *beaconNode;
	int beasockOptLen = sizeof(int);
	bool beasockOpt = true;
	time_t mysystime;
	pthread_t myThreadsTemp[2];
	int myThreadsTempCtr = 0;
	int joinSocket = 0;
	for(i=0 ; i < nonBeaconNodeInfo->no_of_beacons ; i++)
	{

		joinSocket = 0;
		if((joinSocket = socket(AF_INET,SOCK_STREAM,0)) == -1)
		{
			writeErrorToLogFile((char *)"\nError creating socket\n");
			i--;
			continue;
		}
		if((setsockopt(joinSocket,SOL_SOCKET,SO_REUSEADDR,&beasockOpt,beasockOptLen)) == -1)
		{
			writeErrorToLogFile((char *)"Error in setting socket opt");
			i--;
			continue;
		}
		char beaconip[16];
		beacon_addr.sin_family = AF_INET;
		beaconNode = gethostbyname(nonBeaconNodeInfo->beaconsHostNames[i]);
		memset(beaconip,0,16);
		strcpy(beaconip,inet_ntoa(*((struct in_addr *)beaconNode->h_addr_list[0])));
		free(beaconNode);
		beacon_addr.sin_addr.s_addr = inet_addr(beaconip);
		beacon_addr.sin_port = htons(nonBeaconNodeInfo->beaconport[i]);

		/*
		 * Send connection request
		 */
		status = connect(joinSocket,(struct sockaddr *)&beacon_addr,sizeof(beacon_addr));			// connect to the host and port specified in
		if (status < 0)
		{
			writeErrorToLogFile((char *)"\nError connecting to server\n");
		}
		else
		{
			status = i;
			break;
		}
	}
	if(status < 0)
	{
		return -1;
	}
	else
	{
		/*
		 * Create connection details
		 */
		connectionDetails *cd = new connectionDetails;
		strcpy(cd->hostname,nonBeaconNodeInfo->beaconsHostNames[status]);
		cd->port = nonBeaconNodeInfo->beaconport[status];
		cd->socketDesc = joinSocket;
		pthread_cond_init(&cd->messagingQueueCV,NULL);
		pthread_mutex_init(&cd->messagingQueueLock,NULL);
		pthread_mutex_init(&cd->connectionLock,NULL);
		cd->threadsExitedCount = 0;
		cd->notOperational = 0;
		cd->isJoinConnection = 0;
		cd->tiebreak = 0;
		mysystime = time(NULL);
		cd->lastReadActivity = mysystime;
		cd->lastWriteActivity = mysystime;
		cd->helloStatus = 0;
		cd->wellKnownPort = nonBeaconNodeInfo->beaconport[status];
		char charkey[100] = "\0";
		sprintf(charkey,"%s%d",nonBeaconNodeInfo->beaconsHostNames[status],nonBeaconNodeInfo->beaconport[status]);
		string key = string(charkey);

		/*
		 * Add connection details to the map
		 */
		pthread_mutex_lock(&connectedNeighborsMapLock);
		connectedNeighborsMap.insert(CONNMAPTYPE::value_type(key,cd));
		pthread_mutex_unlock(&connectedNeighborsMapLock);

		char *mapk = (char *)malloc(100);
		memset(mapk,0,100);
		strcpy(mapk,charkey);

		/*
		 *	Create JOIN message and send over this temporary connection
		 */
		struct joinMessage *myJoinMsg = createJoin(nonBeaconNodeInfo->location,nonBeaconNodeInfo->port,nonBeaconNodeInfo->hostname);
		struct message *mymessage;

		mymessage = CreateMessageHeader(JOIN, /*Mess Type*/
												nonBeaconNodeInfo->ttl, /* TTL */
												myJoinMsg -> dataLength, /*DataLength Message Body */
												NULL, /*char *messageHeader*/
												NULL, /*char *message*/
												(void*)myJoinMsg, /*void *messageStruct*/
												mapk,/*char* temp_connectedNeighborsMapKey*/
												MYMESSAGE, /* MyMessage or NotMyMessage*/
												nonBeaconNodeInfo /*Node Info Pointer*/);

		/*
		 * Add the message to my message cache
		 */

		char keytype[256] = "\0";
		int i = 0;
		while(i<20)
		{
			keytype[i] = *(mymessage->uoid + i);
			i++;
		}
		string keyTemp = string(keytype);

		pthread_mutex_lock(&messageCacheMapLock);
		messageCacheMap.insert(MESSCACHEMAPTYPE::value_type(keyTemp,mymessage));
		pthread_mutex_unlock(&messageCacheMapLock);
		sendMessageToNode(mymessage);
		/*
		 * create child threads for reader and writer
		 */
		usleep(1000);
		pthread_t childThread1;
		pthread_create(&childThread1,NULL,(void* (*)(void*))reader,mapk);
		myThreadsTemp[myThreadsTempCtr] = childThread1;
		myThreadsTempCtr++;
		pthread_t childThread2;
		pthread_create(&childThread2,NULL,(void* (*)(void*))writer,mapk);
		myThreadsTemp[myThreadsTempCtr] = childThread2;
		myThreadsTempCtr++;

		for(i = 0; i < myThreadsTempCtr; i++)
		{
			pthread_join(myThreadsTemp[i],NULL);
		}

	}
	pthread_mutex_lock(&connectedNeighborsMapLock);
	for(connectedNeighborsMapIter = connectedNeighborsMap.begin(); connectedNeighborsMapIter != connectedNeighborsMap.end();connectedNeighborsMapIter++)
	{
		connectedNeighborsMap.erase(connectedNeighborsMapIter->first);
	}
	pthread_mutex_unlock(&connectedNeighborsMapLock);
	return 1;
}

int createInitNeighborListFileForNonBeacon()
{
	/*
	 * Create the init_neighbor_list file
	 */
	int i=0,count = 0;
	uint32_t minDist=INT_MAX,dist=0;
	struct joinResponseDetails *joinRespDet;
	string keyToWrite;
	char line[256] = "\0";
	initNeighborListFilePtr = fopen(initFile,"a");
	/*
	 * Determine the closest neighbors
	 */
	pthread_mutex_lock(&joinMsgMapLock);
	for(i = 0; i < nonBeaconNodeInfo->initNeighbors ; i++)
	{
		for(joinMsgMapIter = joinMsgMap.begin(); joinMsgMapIter != joinMsgMap.end(); joinMsgMapIter++)
		{
			joinRespDet = joinMsgMapIter->second;
			dist = joinRespDet->distToNode;
			if(dist < minDist)
			{
				minDist = dist;
				keyToWrite = joinMsgMapIter->first;
			}
		}
		joinMsgMapIter = joinMsgMap.find(keyToWrite);
		if(joinMsgMapIter != joinMsgMap.end())
		{
			joinRespDet = joinMsgMapIter->second;
			sprintf(line,"%s:%d",joinRespDet->hostName,joinRespDet->portNum);

			strcat(line,"\n");
			fwrite(line,1,strlen(line),initNeighborListFilePtr);
			fflush(initNeighborListFilePtr);
			joinMsgMap.erase(keyToWrite);
			minDist = INT_MAX;
			memset(line,0,256);
			count++;
		}
	}
	pthread_mutex_unlock(&joinMsgMapLock);
	fclose(initNeighborListFilePtr);

	return count;
}

void nonBeaconListener(int *param)
{
	pthread_t myChildThreads_nonbeacon[1000];
	time_t mysystime;
	int myChildThreadCtr = 0,status=0;
	int acceptSock=0;

	char peername[256] = "\0";

	actUSR1.sa_handler = sigUSR1Handler;
	sigaction(SIGUSR1, &actUSR1, NULL);
	pthread_sigmask(SIG_UNBLOCK, &signalSetUSR1, NULL);

	if((mynonbeaconSocket = socket(AF_INET,SOCK_STREAM,0)) == -1)
	{
		writeErrorToLogFile((char *)"\nError creating socket\n");
		exit(1);
	}
	memset(&my_non_beacon_addr,0,sizeof(struct sockaddr_in));
	my_non_beacon_addr.sin_family = AF_INET;
	my_non_beacon_addr.sin_addr.s_addr = inet_addr(nonBeaconNodeInfo->hostip);
	my_non_beacon_addr.sin_port = htons(nonBeaconNodeInfo->port);
	if ((setsockopt(mynonbeaconSocket,SOL_SOCKET,SO_REUSEADDR,&nonbeaconsockOpt,nonbeaconsockOptLen)) == -1)
	{
		writeErrorToLogFile((char *)"Error in setting socket opt");
	}
	if((bind(mynonbeaconSocket,(struct sockaddr *)&my_non_beacon_addr,sizeof(my_non_beacon_addr))) == -1)
	{
		writeErrorToLogFile((char *)"\nError binding to socket\n");
		exit(1);
	}

	if((listen(mynonbeaconSocket, 5)) == -1)
	{
		writeErrorToLogFile((char *)"\nError listening to socket\n");
		exit(1);
	}
	while(1)
	{
		acceptSock=0;
		socklen_t acceptlen;
		acceptlen = sizeof(accept_addr_nonbeacon);

		memset(&accept_addr_nonbeacon,0,sizeof(sockaddr));
		acceptSock = accept(mynonbeaconSocket,(struct sockaddr *) &accept_addr_nonbeacon,&acceptlen);

		memset(peername,0,256);
		status = getnameinfo((struct sockaddr *) &accept_addr_nonbeacon,sizeof(accept_addr_nonbeacon),peername,sizeof(peername),NULL,0,0);
		char char_key[100] = "\0";
		sprintf(char_key,"%s%d",peername,htons(accept_addr_nonbeacon.sin_port));
		string key = string(char_key);

		/*
		 * Add the information to connection detail structure
		 */

		connectionDetails *cd = new connectionDetails;
		strcpy(cd->hostname,peername);
		cd->port = htons(accept_addr_nonbeacon.sin_port);
		pthread_cond_init(&cd->messagingQueueCV, NULL);
		pthread_mutex_init(&cd->messagingQueueLock,NULL);
		pthread_mutex_init(&cd->connectionLock,NULL);
		cd->socketDesc = acceptSock;
		cd->notOperational = 0;
		cd->isJoinConnection = 0;
		cd->threadsExitedCount = 0;
		mysystime = time(NULL);
		cd->lastReadActivity = mysystime;
		cd->lastWriteActivity = mysystime;
		cd->helloStatus=0;
		cd->tiebreak = 0;

		/*
		 * Put the connection details in the map
		 */
		pthread_mutex_lock(&connectedNeighborsMapLock);
		connectedNeighborsMap.insert(CONNMAPTYPE::value_type(key,cd));
		pthread_mutex_unlock(&connectedNeighborsMapLock);

		char *mapkey = (char *)malloc(100);
		memset(mapkey,0,100);
		strcpy(mapkey,char_key);

		/*
		 * Create reader and writer threads and give them the connection key
		 */
		pthread_t childThread1;
		pthread_create(&childThread1,NULL,(void* (*)(void*))reader,mapkey);
		myChildThreads_nonbeacon[myChildThreadCtr] = childThread1;
		myChildThreadCtr++;
		pthread_t childThread2;
		pthread_create(&childThread2,NULL,(void* (*)(void*))writer,mapkey);
		myChildThreads_nonbeacon[myChildThreadCtr] = childThread2;
		myChildThreadCtr++;
		//printConnectedNeighborInforMap();
	}
	close(mynonbeaconSocket);

	pthread_exit(NULL);
}

void readInitFile()
{
	char line[256] = "\0";
	char *token;
	int cntr = 0;
	initNeighborListFilePtr = fopen(initFile,"r");
	while(fgets(line,sizeof(line),initNeighborListFilePtr) != NULL)
	{
		token = strtok(line,":");
		initNeighborsHostnameList[cntr] = (char *)malloc(100);
		strcpy(initNeighborsHostnameList[cntr],line);
		token = strtok(NULL,"\n");
		initNeighborsPortList[cntr] = atoi(token);
		cntr++;
	}
	fclose(initNeighborListFilePtr);
}

void nonBeaconConnector(int *param)
{
	int i=0,status = 0;
	pthread_t myChildThreads_nonbeacon[1000];
	int myChildThreadCtr = 0;
	struct hostent *beacon;
	time_t mysystime;
	no_of_connection = 0;

	readInitFile();
	for(i = 0; i < nonBeaconNodeInfo->initNeighbors; i++)
	{
		beacon = NULL;
		int neighborbeaconSock=0;
		if((neighborbeaconSock = socket(AF_INET,SOCK_STREAM,0)) == -1)
		{
			writeErrorToLogFile((char *)"\nError creating socket\n");
			i--;
			continue;
		}
		struct sockaddr_in serv_addr;
		char beaconip[16];
		serv_addr.sin_family = AF_INET;
		char char_key[100] = "\0";
		sprintf(char_key,"%s%d",initNeighborsHostnameList[i],initNeighborsPortList[i]);
		string key = string(char_key);

		//Get the Peer information
		beacon = gethostbyname(initNeighborsHostnameList[i]);
		memset(beaconip,0,16);
		strcpy(beaconip,inet_ntoa(*((struct in_addr *)beacon->h_addr_list[0])));
		free(beacon);

		serv_addr.sin_addr.s_addr = inet_addr(beaconip);
		serv_addr.sin_port = htons(initNeighborsPortList[i]);

		//Send connection request
		status = connect(neighborbeaconSock,(struct sockaddr *)&serv_addr,sizeof(serv_addr));			// connect to the host and port specified in
		if (status == 0)
		{

			connectionDetails *cd = new connectionDetails;
			no_of_connection++;
			//Get Peer information
			socklen_t len;
			struct sockaddr_storage addr;
			char ipstr[16];
			int port;
			len = sizeof(addr);
			getpeername(neighborbeaconSock, (struct sockaddr*)&addr, &len);
			struct sockaddr_in *s = (struct sockaddr_in *)&addr;
			port = ntohs(s->sin_port);
			inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof ipstr);

			//Prepare the connection details
			strcpy(cd->hostname,initNeighborsHostnameList[i]);
			cd->port = initNeighborsPortList[i];
			cd->socketDesc = neighborbeaconSock;
			cd->wellKnownPort = initNeighborsPortList[i];
			cd->notOperational=0;
			mysystime = time(NULL);
			cd->lastReadActivity = mysystime;
			cd->lastWriteActivity = mysystime;
			cd->isJoinConnection = 0;
			cd->threadsExitedCount = 0;
			cd->helloStatus=0;
			cd->tiebreak = 0;
			pthread_mutex_init(&cd->connectionLock,NULL);
			pthread_cond_init(&cd->messagingQueueCV, NULL);
			pthread_mutex_init(&cd->messagingQueueLock,NULL);



			char *mapkey = (char *)malloc(100);
			memset(mapkey,0,100);
			strcpy(mapkey,char_key);

			//Create a hello message to be sent to all the beacons
			struct message *mymessage;
			struct helloMessage *myhelloMsg;

			myhelloMsg = createHello(nonBeaconNodeInfo->hostname,nonBeaconNodeInfo->port);

			mymessage = CreateMessageHeader(HELLO, /*Message Type */
					1, /* TTL */
					myhelloMsg -> dataLength, /*data body length*/
					NULL, /*char *messageHeader*/
					NULL, /*char *message*/
					(void*)myhelloMsg, /*void *messageStruct*/
					mapkey,/*char* temp_connectedNeighborsMapKey*/
					MYMESSAGE, /*My Message or notmymessage*/
					nonBeaconNodeInfo /*Node Info Pointer*/);

			//Create messsage header and add msg body created above
			pthread_mutex_lock(&helloMsgMapLock);
			helloMsgMap.insert(HELLOMSGMAPTYPE::value_type(mapkey,1));
			pthread_mutex_unlock(&helloMsgMapLock);

			char keytype[256] = "\0";
			int j = 0;
			while(j<20)
			{
				keytype[j] = *(mymessage->uoid + j);
				*(cd->hellomsguoid + j) = *(mymessage->uoid + j);
				j++;
			}
			string uoidKey = string(keytype);
			cd->hellomsguoid[j] = '\0';

			pthread_mutex_lock(&connectedNeighborsMapLock);
			connectedNeighborsMap.insert(CONNMAPTYPE::value_type(key,cd));
			pthread_mutex_unlock(&connectedNeighborsMapLock);

			pthread_mutex_lock(&messageCacheMapLock);
			messageCacheMap.insert(MESSCACHEMAPTYPE::value_type(uoidKey,mymessage));
			pthread_mutex_unlock(&messageCacheMapLock);

			//Create reader and writer threads and give them the connection key
			pthread_t childThread1;
			pthread_create(&childThread1,NULL,(void* (*)(void*))reader,mapkey);
			myChildThreads_nonbeacon[myChildThreadCtr] = childThread1;
			myChildThreadCtr++;
			pthread_t childThread2;
			pthread_create(&childThread2,NULL,(void* (*)(void*))writer,mapkey);
			myChildThreads_nonbeacon[myChildThreadCtr] = childThread2;
			myChildThreadCtr++;

			//Call the function that sends the message to all the connections
			sendMessageToNode(mymessage);
		}
	}

	if(no_of_connection < nonBeaconNodeInfo->minNeighbors)
	{
		/*
		 * Could not connect to MinNeighbors
		 * Delete init_neighbor_list file and rejoin the network
		 */

		/*
		 * Ask all the threads created till now to kill themselves and exit
		 */
		pthread_mutex_lock(&systemWideLock);
		autoshutdownFlag = 1;
		keyboardShutdown = 1;
		pthread_mutex_unlock(&systemWideLock);

		pthread_kill(keyboardThreadNonbeacon,SIGUSR1);

		pthread_mutex_lock(&eventDispatcherQueueLock);
		pthread_cond_signal(&eventDispatcherQueueCV);
		pthread_mutex_unlock(&eventDispatcherQueueLock);

		pthread_kill(nonBeaconListenerThread,SIGUSR1);

		pthread_mutex_lock(&checkThreadLock);
		pthread_cond_signal(&checkThreadCV);
		pthread_mutex_unlock(&checkThreadLock);
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

		pthread_mutex_lock(&helloMsgMapLock);
		for(helloMsgMapIter = helloMsgMap.begin();helloMsgMapIter != helloMsgMap.end();helloMsgMapIter++)
		{
			helloMsgMap.erase(helloMsgMapIter->first);
		}
		pthread_mutex_unlock(&helloMsgMapLock);

		pthread_mutex_lock(&checkMsgMapLock);
		for(checkMsgMapIter = checkMsgMap.begin();checkMsgMapIter != checkMsgMap.end();checkMsgMapIter++)
		{
			checkMsgMap.erase(checkMsgMapIter->first);
		}
		pthread_mutex_unlock(&checkMsgMapLock);

		pthread_mutex_lock(&systemWideLock);
		restartState = 1;
		checkStatus = 0;
		pthread_mutex_unlock(&systemWideLock);

		pthread_exit(NULL);
	}

	pthread_exit(NULL);
}

void createNonBeacon(struct NodeInfo *ndInfo)
{
	nonBeaconNodeInfo = ndInfo;
	int joinstatus = 0,initfilestat = 0,nouse=0;
	strcpy(initFile,nonBeaconNodeInfo->homeDir);
	strcat(initFile,"/init_neighbor_list");
	while(1)
	{
		if(checkFileExists(initFile) == 0)
		{
			pthread_create(&keyboardThreadNonbeacon,NULL,(void* (*)(void*))createKeyboardThread,(void *)ndInfo);
			pthread_create(&nonBeaconDispatcherThread,NULL,(void* (*)(void*))nonBeaconDispatcher,(void *)ndInfo);
			pthread_create(&nonBeaconTimerThread,NULL,(void* (*)(void*))nonBeaconTimer,(void *)ndInfo);



			joinstatus = joinServantNtwk();
			if(joinstatus == -1)
			{
				/*
				 * Clean up and Exit
				 */
				goto EXIT;
			}
			pthread_mutex_lock(&systemWideLock);
			if(autoshutdownFlag == 1)
			{
				pthread_mutex_unlock(&systemWideLock);

				pthread_join(nonBeaconDispatcherThread,NULL);
				pthread_join(nonBeaconTimerThread,NULL);
				pthread_join(keyboardThreadNonbeacon,NULL);
				goto EXIT;
			}
			pthread_mutex_unlock(&systemWideLock);
			if((initfilestat = createInitNeighborListFileForNonBeacon()) < ndInfo->initNeighbors)
			{
				remove(initFile);
				exit(1);
			}
		}
		else
		{
			participating = 1;	//This is to indicate the timer that I am not joining so do not check the join responses
			pthread_create(&keyboardThreadNonbeacon,NULL,(void* (*)(void*))createKeyboardThread,(void *)ndInfo);
			pthread_create(&nonBeaconDispatcherThread,NULL,(void* (*)(void*))nonBeaconDispatcher,(void *)ndInfo);
			pthread_create(&nonBeaconTimerThread,NULL,(void* (*)(void*))nonBeaconTimer,(void *)ndInfo);
		}
		/*
		 * Go for soft Restart - send HELLO messages to neighbors
		 * in the init_neighbor_list
		 */
		pthread_create(&nonBeaconListenerThread,NULL,(void* (*)(void*))nonBeaconListener,&nouse);
		pthread_create(&nonBeaconConnectorThread,NULL,(void* (*)(void*))nonBeaconConnector,&nouse);
		pthread_create(&nonBeaconCheckThread,NULL,(void* (*)(void*))checkThread,(void *)ndInfo);

		pthread_join(keyboardThreadNonbeacon,NULL);
		pthread_join(nonBeaconDispatcherThread,NULL);
		pthread_join(nonBeaconTimerThread,NULL);
		pthread_join(nonBeaconListenerThread,NULL);
		close(mynonbeaconSocket);
		pthread_join(nonBeaconConnectorThread,NULL);
		pthread_join(nonBeaconCheckThread,NULL);

		if(restartState == 0)
		{
			goto EXIT;
		}
		else if(restartState == 1)
		{
			participating = 0;
			no_of_join_responses = 0;
			no_of_hello_responses = 0;
			no_of_connection = 0;
			keyboardShutdown = 0;
			restartState = 0;
			remove(initFile);
			usleep(10000);

			pthread_mutex_lock(&connectedNeighborsMapLock);
			freeConnectionDetailsMap();
			pthread_mutex_unlock(&connectedNeighborsMapLock);

			pthread_mutex_lock(&systemWideLock);
			autoshutdownFlag = 0;
			checkStatus = 0;
			joinstatus = 0;
			initfilestat = 0;
			no_of_hello_responses = 0;
			no_of_connection = 0;
			pthread_mutex_unlock(&systemWideLock);

			pthread_mutex_lock(&messageCacheMapLock);
			for(messageCacheMapIter = messageCacheMap.begin(); messageCacheMapIter != messageCacheMap.end();messageCacheMapIter++)
			{
				messageCacheMap.erase(messageCacheMapIter->first);
			}
			pthread_mutex_unlock(&messageCacheMapLock);

			clearStatusMsgMap();

			mynonbeaconSocket = 0;
			nonbeaconsockOptLen = sizeof(int);
			nonbeaconsockOpt = true;

			initNeighborsPortList[100] = 0;
		}
		else if(restartState == 2)
		{
			no_of_hello_responses = 0;
			no_of_connection = 0;

			usleep(10000);

			pthread_mutex_lock(&connectedNeighborsMapLock);
			for(connectedNeighborsMapIter = connectedNeighborsMap.begin(); connectedNeighborsMapIter != connectedNeighborsMap.end();connectedNeighborsMapIter++)
			{
				connectedNeighborsMap.erase(connectedNeighborsMapIter->first);
			}
			pthread_mutex_unlock(&connectedNeighborsMapLock);
			pthread_mutex_lock(&systemWideLock);
			autoshutdownFlag = 0;
			checkStatus = 0;
			pthread_mutex_unlock(&systemWideLock);
			clearStatusMsgMap();
			pthread_mutex_lock(&messageCacheMapLock);
			for(messageCacheMapIter = messageCacheMap.begin(); messageCacheMapIter != messageCacheMap.end();messageCacheMapIter++)
			{
				messageCacheMap.erase(messageCacheMapIter->first);
			}
			pthread_mutex_unlock(&messageCacheMapLock);
		}
	}
	EXIT:
	printf("\n\n----------NON BEACON NODE EXITED -----------\n\n");
}
