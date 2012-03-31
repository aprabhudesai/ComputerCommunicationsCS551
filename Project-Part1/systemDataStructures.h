/*
 * systemDataStructures.h
 *
 *  Created on: Mar 9, 2012
 *      Author: abhishek
 */

#include <queue>
#include <map>
#include <vector>
#include <string>

#include "messageStructures.h"

/*#define DEBUGGING_MEMORY_CORRUPTION
    #ifdef DEBUGGING_MEMORY_CORRUPTION
    #ifdef free
    #undef free
    #endif
    #define free
    #endif
*/
using namespace std;

/*
 * Check Thread Lock and CV
 */
extern pthread_mutex_t checkThreadLock;
extern pthread_cond_t checkThreadCV;
extern int checkStatus;


/*Event Dispatcher*/
extern std::queue<message*> eventDispatcherQueue;
extern pthread_mutex_t eventDispatcherQueueLock;
extern pthread_cond_t eventDispatcherQueueCV;

extern FILE *logFilePtr;
extern pthread_mutex_t logFileLock;
extern char logFileName[256];

/*Connection Details*/
struct connectionDetails
{
	std::queue<message*> messagingQueue;
	pthread_mutex_t messagingQueueLock;
	int socketDesc;
	char hostname[256];
	uint16_t port;
	uint16_t wellKnownPort;
	pthread_cond_t messagingQueueCV;
	int notOperational;
	int isJoinConnection;
	time_t lastReadActivity;
	time_t lastWriteActivity;
	int threadsExitedCount;
	pthread_mutex_t connectionLock;
	int helloStatus;
	unsigned char hellomsguoid[21];
	int tiebreak;
	pthread_t readerThread;
	pthread_t writerThread;
};

/*
 * Join Response Information
 */
struct joinResponseDetails
{
	uint32_t distToNode;
	uint16_t portNum;
	char hostName[256];
	unsigned char uoid[21];
};

typedef std::map<string,int> CHECKMSGMAPTYPE;
extern CHECKMSGMAPTYPE checkMsgMap;
extern CHECKMSGMAPTYPE::iterator checkMsgMapIter;
extern pthread_mutex_t checkMsgMapLock;

typedef std::map<string,joinResponseDetails*> JOINMSGMAPTYPE;
extern JOINMSGMAPTYPE joinMsgMap;
extern JOINMSGMAPTYPE::iterator joinMsgMapIter;
extern pthread_mutex_t joinMsgMapLock;

typedef std::map<string,int> HELLOMSGMAPTYPE;
extern HELLOMSGMAPTYPE helloMsgMap;
extern HELLOMSGMAPTYPE::iterator helloMsgMapIter;
extern pthread_mutex_t helloMsgMapLock;

typedef std::map<uint16_t,int> STATUSMSGMAPTYPE;
extern STATUSMSGMAPTYPE statusMsgMap;
extern STATUSMSGMAPTYPE::iterator statusMsgMapIter;
extern pthread_mutex_t statusMsgMapLock;

/*Message Cache*/
typedef std::map<string,message *> MESSCACHEMAPTYPE;
extern MESSCACHEMAPTYPE messageCacheMap;
extern MESSCACHEMAPTYPE::iterator messageCacheMapIter;
extern pthread_mutex_t messageCacheMapLock;
extern pthread_cond_t messageCacheMapCV;  // not needed as of now

/*Connected neighbors information*/
typedef std::map<string,connectionDetails*> CONNMAPTYPE;
extern CONNMAPTYPE connectedNeighborsMap;
extern CONNMAPTYPE::iterator connectedNeighborsMapIter;
extern pthread_mutex_t connectedNeighborsMapLock;

void reader(void *);
void writer(void *);
void dispatcher(void *);
void nonBeaconDispatcher(void *);
void nonBeaconTimer(void *);
void beaconTimer(void *);
void checkThread(void *);

int checkNodeType(uint16_t);
int checkNodeType();
unsigned char *GetUOID(char *,char*,unsigned char*,int);
bool handleSelfMessages(struct message *myMessage);
bool sendMessageToNode(struct message *);
void removeConnectedNeighborInfo(char *);
void printConnectedNeighborInforMap();
void printJoinMessageInfoMap();
bool checkMessageValidity(struct message*);
bool writeToLogFile(struct message *,char *);
bool writeInfoToStatusFile(statusRespMessage *);
void scanAndEraseMsgCache();
void scanAndEraseConnectedNeighborsMap(int);
void checkLastReadActivityOfConnection(int);
void checkLastWriteActivityOfConnection();
void sigpipeHandler(int);
void clearStatusMsgMap();
void sigUSR1Handler(int);
void sigUSR2Handler(int);
void resetBeaconNode();
void resetNonBeaconNode();
void freeMessage(struct message *); // Deallocates Specific Message
void freeConnectionDetails(struct connectionDetails *); // De allocates specific connection Details
void freeConnectionDetailsMap(); // free entire Connection Details Map, for cleanup
void freeMessageCache(); // free entire Message Cache Map, for cleanup

extern sigset_t signalSetInt;
extern sigset_t signalSetPipe;
extern struct sigaction actInt;
extern sigset_t signalSetUSR1;
extern struct sigaction actUSR1;

extern sigset_t signalSetUSR2;
extern struct sigaction actUSR2;

extern struct sigaction actPipe;
extern int hellomsgsentflag;
extern struct NodeInfo *ndInfoTemp;
extern struct NodeInfo ndInfo;

extern FILE *initNeighborListFilePtr;
extern char extfilecmd[256];
extern int ttlcmd;
extern FILE *extfilePtr;
extern pthread_mutex_t extFileLock;
extern int checkSendFlag;
extern int autoshutdownFlag;
extern int keyboardShutdown;
extern pthread_mutex_t systemWideLock;

extern pthread_mutex_t keyboardLock;
extern pthread_cond_t keyboardCV;


extern int restartState;
