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

#include "beaconNode.h"
#include "keyboardThread.h"
#include "systemDataStructures.h"
#include "headers.h"
#include <iostream>

extern void reader(void *);
extern void writer(void *);
extern bool handleSelfMessages(struct message*);
extern bool sendMessageToNode(struct message *);
extern unsigned char *GetUOID(char *,char*,unsigned char*,int);
extern void sigUSR1Handler(int);

pthread_t listenerThread,connectorThread,keyboardThread,timerThread,dispatcherThread,beaconCheckThread;
struct NodeInfo *beaconNodeInfo;
int mybeaconSocket = 0;
struct  sockaddr_in my_beacon_addr;
int beaconsockOptLen = sizeof(int);
bool beaconsockOpt = true;
struct sockaddr_in accept_addr;

extern sigset_t signalSetUSR1;
extern struct sigaction actUSR1;

int no_of_neighbors_connected_to = 0;

void *createBeaconListener(int *arg)
{
	pthread_t myChildThreads[1000];
	time_t mysystime;
	int myChildThreadCtr = 0,status=0;
	int acceptSock=0;
	//signal(SIGUSR1,sigUSR1Handler);
	actUSR1.sa_handler = sigUSR1Handler;
	sigaction(SIGUSR1, &actUSR1, NULL);
	pthread_sigmask(SIG_UNBLOCK, &signalSetUSR1, NULL);


	char peername[256] = "\0";
	if((mybeaconSocket = socket(AF_INET,SOCK_STREAM,0)) == -1)
	{

		exit(1);
	}
	memset(&my_beacon_addr,0,sizeof(struct sockaddr_in));
	my_beacon_addr.sin_family = AF_INET;
	my_beacon_addr.sin_addr.s_addr = inet_addr(beaconNodeInfo->hostip);
	my_beacon_addr.sin_port = htons(beaconNodeInfo->port);
	if ((setsockopt(mybeaconSocket,SOL_SOCKET,SO_REUSEADDR,&beaconsockOpt,beaconsockOptLen)) == -1)
	{
		printf("Error in setting socket opt");
	}
	if((bind(mybeaconSocket,(struct sockaddr *)&my_beacon_addr,sizeof(my_beacon_addr))) == -1)
	{
		printf("\nError binding to socket\n");
		exit(1);
	}

	if((listen(mybeaconSocket, 5)) == -1)
	{
		printf("\nError listening to socket\n");
		exit(1);
	}
	while(1)
	{
		acceptSock=0;
		socklen_t acceptlen;
		acceptlen = sizeof(accept_addr);

		memset(&accept_addr,0,sizeof(sockaddr));
		acceptSock = accept(mybeaconSocket,(struct sockaddr *) &accept_addr,&acceptlen);
		if(acceptSock < 0)
		{
			break;
		}

		memset(peername,0,256);
		status = getnameinfo((struct sockaddr *) &accept_addr,sizeof(accept_addr),peername,sizeof(peername),NULL,0,0);

		char char_key[100] = "\0";
		sprintf(char_key,"%s%d",peername,htons(accept_addr.sin_port));
		string key = string(char_key);

		//Add the information to connection detail structure
		connectionDetails *cd = new connectionDetails;
		strcpy(cd->hostname,peername);
		cd->port = htons(accept_addr.sin_port);
		pthread_cond_init(&cd->messagingQueueCV, NULL);
		pthread_mutex_init(&cd->messagingQueueLock,NULL);
		pthread_mutex_init(&cd->connectionLock,NULL);
		cd->socketDesc = acceptSock;
		cd->notOperational = 0;
		cd->isJoinConnection = 0;
		cd->threadsExitedCount = 0;
		cd->tiebreak = 0;
		mysystime = time(NULL);
		cd->lastReadActivity = mysystime;
		cd->lastWriteActivity = mysystime;
		cd->helloStatus = 0;
		cd->wellKnownPort = 0;

		//Put the connection details in the map
		pthread_mutex_lock(&connectedNeighborsMapLock);
		connectedNeighborsMap.insert(CONNMAPTYPE::value_type(key,cd));
		pthread_mutex_unlock(&connectedNeighborsMapLock);

		char *mapkey = (char *)malloc(100);
		memset(mapkey,0,100);
		strcpy(mapkey,char_key);

		//Create reader and writer threads and give them the connection key
		pthread_t childThread1;
		pthread_create(&childThread1,NULL,(void* (*)(void*))reader,mapkey);
		myChildThreads[myChildThreadCtr] = childThread1;
		myChildThreadCtr++;
		pthread_t childThread2;
		pthread_create(&childThread2,NULL,(void* (*)(void*))writer,mapkey);
		myChildThreads[myChildThreadCtr] = childThread2;
		myChildThreadCtr++;
	}
	pthread_exit(NULL);
}

void *createBeaconConnector(int *arg)
{
	int i=0,status = 0;
	time_t mysystime;
	pthread_t myChildThreads[1000];
	int myChildThreadCtr = 0;
	struct hostent *beacon;
	int dontSend = 0;
	while(1)
	{
		for(i = 0; i < beaconNodeInfo->no_of_beacons; i++)
		{
			beacon = NULL;
			dontSend = 0;
			int neighborbeaconSock=0;
			if((neighborbeaconSock = socket(AF_INET,SOCK_STREAM,0)) == -1)
			{
				printf("\nError creating socket\n");
				continue;
			}
			struct sockaddr_in serv_addr;
			char beaconip[16];
			serv_addr.sin_family = AF_INET;
			if(beaconNodeInfo->beaconport[i] == beaconNodeInfo->port)
				continue;

			char char_key[100] = "\0";
			sprintf(char_key,"%s%d",beaconNodeInfo->beaconsHostNames[i],beaconNodeInfo->beaconport[i]);
			string key = string(char_key);

			//Check if connection already established
			struct connectionDetails *compCD = NULL;
			pthread_mutex_lock(&connectedNeighborsMapLock);
			for(connectedNeighborsMapIter = connectedNeighborsMap.begin();connectedNeighborsMapIter != connectedNeighborsMap.end();connectedNeighborsMapIter++)
			{
				compCD = connectedNeighborsMapIter->second;
				if(compCD->wellKnownPort == beaconNodeInfo->beaconport[i])
				{
					no_of_neighbors_connected_to++;
					//pthread_mutex_unlock(&connectedNeighborsMapLock);
					dontSend = 1;
					break;
				}
			}
			pthread_mutex_unlock(&connectedNeighborsMapLock);

			if(dontSend == 1)
				continue;
			//Get the Peer information
			beacon = gethostbyname(beaconNodeInfo->beaconsHostNames[i]);
			memset(beaconip,0,16);
			strcpy(beaconip,inet_ntoa(*((struct in_addr *)beacon->h_addr_list[0])));
			free(beacon);

			serv_addr.sin_addr.s_addr = inet_addr(beaconip);
			serv_addr.sin_port = htons(beaconNodeInfo->beaconport[i]);

			//Send connection request
			status = connect(neighborbeaconSock,(struct sockaddr *)&serv_addr,sizeof(serv_addr));			// connect to the host and port specified in
			if (status < 0)
			{
				//Error connecting to server
			}
			else
			{
				no_of_neighbors_connected_to += 1;
				connectionDetails *cd = new connectionDetails;

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
				strcpy(cd->hostname,beaconNodeInfo->beaconsHostNames[i]);
				cd->port = beaconNodeInfo->beaconport[i];
				cd->socketDesc = neighborbeaconSock;
				cd->notOperational=0;
				cd->isJoinConnection = 0;
				cd->threadsExitedCount = 0;
				cd->wellKnownPort = beaconNodeInfo->beaconport[i];
				mysystime = time(NULL);
				cd->lastReadActivity = mysystime;
				cd->lastWriteActivity = mysystime;
				cd->helloStatus = 0;
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

				myhelloMsg = createHello(beaconNodeInfo->hostname,beaconNodeInfo->port);

				mymessage = CreateMessageHeader(HELLO, /*Message Type */
												1, /* TTL */
												myhelloMsg -> dataLength, /*data body length*/
												NULL, /*char *messageHeader*/
												NULL, /*char *message*/
												(void*)myhelloMsg, /*void *messageStruct*/
												mapkey,/*char* temp_connectedNeighborsMapKey*/
												MYMESSAGE, /*My Message or notmymessage*/
												beaconNodeInfo /*Node Info Pointer*/);

				char keytype[21] = "\0";
				int i = 0;
				while(i<20)
				{
					keytype[i] = *(mymessage->uoid + i);
					*(cd->hellomsguoid + i) = *(mymessage->uoid + i);
					i++;
				}
				string uoidKey = string(keytype);
				cd->hellomsguoid[i] = '\0';

				pthread_mutex_lock(&connectedNeighborsMapLock);
				connectedNeighborsMap.insert(CONNMAPTYPE::value_type(key,cd));
				pthread_mutex_unlock(&connectedNeighborsMapLock);

				//Create reader and writer threads and give them the connection key
				pthread_t childThread1;
				pthread_create(&childThread1,NULL,(void* (*)(void*))reader,mapkey);
				myChildThreads[myChildThreadCtr] = childThread1;
				myChildThreadCtr++;
				pthread_t childThread2;
				pthread_create(&childThread2,NULL,(void* (*)(void*))writer,mapkey);
				myChildThreads[myChildThreadCtr] = childThread2;
				myChildThreadCtr++;

				pthread_mutex_lock(&messageCacheMapLock);
				messageCacheMap.insert(MESSCACHEMAPTYPE::value_type(uoidKey,mymessage));
				pthread_mutex_unlock(&messageCacheMapLock);

				//Create messsage header and add msg body created above
				pthread_mutex_lock(&helloMsgMapLock);
				helloMsgMap.insert(HELLOMSGMAPTYPE::value_type(mapkey,1));
				pthread_mutex_unlock(&helloMsgMapLock);
				//Call the function that sends the message to all the connections

				sendMessageToNode(mymessage);

			}
		}
		if(no_of_neighbors_connected_to+1 >= (beaconNodeInfo->no_of_beacons))
		{

			pthread_exit(NULL);
			//break;
		}
		else
		{
			sleep(beaconNodeInfo->retry);
		}
	}

	pthread_exit(NULL);
}

void createBeacon(struct NodeInfo *ndInfo)
{
	beaconNodeInfo = ndInfo;
	int i = 1;
	pthread_mutex_lock(&systemWideLock);
	if(autoshutdownFlag == 1)
	{
		pthread_mutex_unlock(&systemWideLock);
		return;
	}
	pthread_mutex_unlock(&systemWideLock);
	//Create Threads
	pthread_create(&keyboardThread,NULL,(void* (*)(void*))createKeyboardThread,(void *)ndInfo);
	pthread_create(&listenerThread,NULL,(void* (*)(void*))createBeaconListener,&i);
	pthread_create(&connectorThread,NULL,(void* (*)(void*))createBeaconConnector,&i);
	pthread_create(&dispatcherThread,NULL,(void* (*)(void*))dispatcher,(void *)ndInfo);
	pthread_create(&timerThread,NULL,(void* (*)(void*))beaconTimer,(void *)ndInfo);

	pthread_join(keyboardThread,NULL);
	pthread_join(listenerThread,NULL);
	pthread_join(dispatcherThread,NULL);
	pthread_join(timerThread,NULL);
	printf("\n\n----------BEACON NODE EXITED----------\n\n");
}
