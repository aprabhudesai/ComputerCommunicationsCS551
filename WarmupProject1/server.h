/*
*	server.h: This is the header file for server code
*/

#define MAX_THREADS 5000
int servdisplayFlag = 0;
int timeout = 60;
int portNum = 0;
pthread_t childThreadArray[MAX_THREADS];
int childCount=0;
pthread_mutex_t stdoutLock;
pthread_mutex_t quitMutex;
int sigType = 0;
int nSocket = 0;
struct timeval tv;
int quitStat=0;
