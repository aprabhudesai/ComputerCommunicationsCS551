/*
 * ntwknode.c
 *
 *  Created on: Feb 28, 2012
 *      Author: abhishek
 */
#include <iostream>
#include "systemDataStructures.h"
#include "iniparser.h"
#include "ntwknode.h"
#include "headers.h"
#include "beaconNode.h"
#include "nonBeaconNode.h"
int checkSendFlag;
using namespace std;

FILE *logFilePtr;
pthread_mutex_t logFileLock = PTHREAD_MUTEX_INITIALIZER;
char logFileName[256] = "\0";

FILE *extFilePtr;
pthread_mutex_t extFileLock = PTHREAD_MUTEX_INITIALIZER;
char extfilecmd[256];
int ttlcmd;

struct NodeInfo *ndInfoTemp;

FILE *initNeighborListFilePtr;
struct stat fileData;
int resetFlag = 0;
int initState = 0;
std::queue<message*> eventDispatcherQueue;
pthread_mutex_t eventDispatcherQueueLock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t eventDispatcherQueueCV = PTHREAD_COND_INITIALIZER;

HELLOMSGMAPTYPE helloMsgMap;
HELLOMSGMAPTYPE::iterator helloMsgMapIter;
pthread_mutex_t helloMsgMapLock = PTHREAD_MUTEX_INITIALIZER;

//This flag is used to check if the hello messages have been exchanged
//with all the neighbors. If flag is 0 - cannot accept any other type of messages
int hellomsgsentflag = 0;

/*
 * This map is used for the JOIN messages that Non-Beacon sends
 */

JOINMSGMAPTYPE joinMsgMap;
JOINMSGMAPTYPE::iterator joinMsgMapIter;
pthread_mutex_t joinMsgMapLock = PTHREAD_MUTEX_INITIALIZER;

MESSCACHEMAPTYPE messageCacheMap;
MESSCACHEMAPTYPE::iterator messageCacheMapIter;
pthread_mutex_t messageCacheMapLock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t messageCacheMapCV = PTHREAD_COND_INITIALIZER;

CONNMAPTYPE connectedNeighborsMap;
CONNMAPTYPE::iterator connectedNeighborsMapIter;
pthread_mutex_t connectedNeighborsMapLock = PTHREAD_MUTEX_INITIALIZER;

STATUSMSGMAPTYPE statusMsgMap;
STATUSMSGMAPTYPE::iterator statusMsgMapIter;
pthread_mutex_t statusMsgMapLock = PTHREAD_MUTEX_INITIALIZER;


CHECKMSGMAPTYPE checkMsgMap;
CHECKMSGMAPTYPE::iterator checkMsgMapIter;
pthread_mutex_t checkMsgMapLock = PTHREAD_MUTEX_INITIALIZER;

//Signal data
sigset_t signalSetInt;
sigset_t signalSetPipe;
struct sigaction actInt;
struct sigaction actPipe;
sigset_t signalSetUSR1;
struct sigaction actUSR1;
sigset_t signalSetUSR2;
struct sigaction actUSR2;

/*
 * This flag then = 1 -> its time for autoshutdown
 */
int autoshutdownFlag = 0;

int keyboardShutdown = 0;

/*
 * When = 0 -> exit
 * when = 1 -> softrestart
 * when = 2 -> hardrestart
 */
int restartState = 0;

pthread_mutex_t keyboardLock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t keyboardCV = PTHREAD_COND_INITIALIZER;
pthread_mutex_t systemWideLock = PTHREAD_MUTEX_INITIALIZER;

struct NodeInfo ndInfo;

extern void writeCommentToLogFile(char *);
extern void writeErrorToLogFile(char *);

char fileName[100];

void initializeNDInfo()
{
	ndInfo.autoShutdown = 900;
	ndInfo.ttl = 30;
	ndInfo.msgLifetime = 30;
	ndInfo.getMsgLifeTime = 300;
	ndInfo.initNeighbors = 3;
	ndInfo.joinTimeout = 15;
	ndInfo.keepAliveTimeout = 60;
	ndInfo.minNeighbors = 2;
	ndInfo.noCheck = 0;
	ndInfo.cacheProb = 0.1;
	ndInfo.storeProb = 0.1;
	ndInfo.neighborStoreProb = 0.2;
	ndInfo.cacheSize = 500;
	ndInfo.retry = 30;
}

void parsecommandline(int argc,char *argv[])
{
	int noOfArgs = argc -1,cnt=0;
	if(argc == 1)
	{
		printf("\nInsufficient no. of arguments\nUsage: ./node <ini filename>\n");
		exit(1);
	}
	else
	{
		cnt = 1;
		while(noOfArgs)
		{
			if(strcmp(argv[cnt],"-reset") == 0)
			{
				resetFlag = 1;
			}
			else
			{
				strcpy(fileName,argv[cnt]);
			}
			cnt++;
			noOfArgs--;
		}
	}
}

int checkNodeType(uint16_t port)
{
	int i = 0;
	for(i = 0; i < NO_OF_BEACON; i++)
	{
		if(port == ndInfo.beaconport[i])
		{
			return 1;
		}
	}
	return 0;
}

int checkNodeType()
{
	int i = 0;
	for(i = 0; i < NO_OF_BEACON; i++)
	{
		if(ndInfo.port == ndInfo.beaconport[i])
		{
			return 1;
		}
	}
	return 0;
}

int checkFileExists(char *fileNm)
{
	if(stat(fileNm,&fileData) == 0)
		return 1;
	else
		return 0;
}

int resetNode(char filename[])
{
	if(checkFileExists(filename) == 1)
	{
		//The init_neighbor_list file is present; delete it
		if(remove(filename) != 0)
		{
			printf("\nError deleting init_neighbor_list file\n");
			return 0;
		}
	}
	else
		return 0;
	return 1;
}

void sigAlrmHandler(int signum)
{
	/*
	 * When the alarm goes off I have to signal all the threads the quit.
	 * I will do this by setting a global varaible that tells all my threads to quit.
	 */
	writeCommentToLogFile((char *)"Autoshutdown Timer Went Off");
	pthread_mutex_lock(&systemWideLock);
	autoshutdownFlag = 1;
	pthread_mutex_unlock(&systemWideLock);
}

int main(int argc,char *argv[])
{
	int status = 0;
		char tempFn[100];
		char hostname[256],nodeid[256],nodeinstanceid[256];
		char hostip[15];
		char logFileMode[3] = "a+";
		time_t systime;
		struct hostent *server_addr;
		sigemptyset(&signalSetInt);
		sigemptyset(&signalSetPipe);
		sigemptyset(&signalSetUSR2);
		sigaddset(&signalSetInt, SIGINT);
		sigaddset(&signalSetPipe, SIGPIPE);
		pthread_sigmask(SIG_BLOCK, &signalSetInt, NULL);
		pthread_sigmask(SIG_BLOCK, &signalSetPipe, NULL);

		initializeNDInfo();

		parsecommandline(argc,argv);
		parseIniFile(fileName,&ndInfo);

		strcpy(logFileName,ndInfo.homeDir);
		strcat(logFileName,"/");

		if(ndInfo.logFileName != NULL)
			strcat(logFileName,ndInfo.logFileName);
		else
			strcat(logFileName,"servant.log");

		signal(SIGALRM,sigAlrmHandler);
		alarm(ndInfo.autoShutdown);
		if(resetFlag == 1)
		{
			strcpy(logFileMode,"w+");
			status = resetNode(logFileName);
			if(status == 0)
			{
				writeErrorToLogFile((char *)"\nError resetting servent node\n");
			}
		}
		status = checkNodeType(ndInfo.port);
		hostname[256] = '\0';
		gethostname(hostname,255);
		server_addr = gethostbyname(hostname);
		strcpy(hostip,inet_ntoa(*((struct in_addr *)server_addr->h_addr_list[0])));
		strcpy(ndInfo.hostname,hostname);
		strcpy(ndInfo.hostip,hostip);
		sprintf(nodeid,"%s_%d",hostname,ndInfo.port);
		strcpy(ndInfo.nodeID,nodeid);
		systime = time(NULL);
		systime *= 1000000;
		sprintf(nodeinstanceid,"%s_%ld",nodeid,systime);
		strcpy(ndInfo.nodeInstanceId,nodeinstanceid);

		logFilePtr = fopen(logFileName,logFileMode);
		if(logFilePtr == NULL)
		{
			writeErrorToLogFile((char *)"\n\nLog File could not be opened\n");
		}
		if(status == 0)
		{
			//I am non beacon node
			checkSendFlag = ndInfo.noCheck;
			createNonBeacon(&ndInfo);
		}
		else
		{
			//I am beacon node
			checkSendFlag = 1;
			createBeacon(&ndInfo);
		}
		fclose(logFilePtr);
		strcpy(tempFn,"init_neighbor_list");
		status = checkFileExists(tempFn);
		if(status == 0)
		{
			//File not present; The node has to send JOIN message to beacon
		}
		else
		{
			//File present; The node has to send HELLO message to its neighbors
		}
		return 0;
}
