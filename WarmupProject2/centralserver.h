/*
*	centralserver.h: This is the header file for the project. It contains all the variables
*	that are needed for the entire simulation.
*			 
*/

#include <iostream>
#include <pthread.h>
#include <queue>
#include <sys/time.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <math.h>

#define MAX_CUST 10000
#define NO_OF_SERVERS 2

using namespace std;

class customerData
{
	public:
	double systemEntry;
	double timeQueued;
	double timeDeQueued;
	double timeServiceStart;
	double timeServiceEnd;
	double systemExit;
	int serviceTime;
	int interArrivalTime;
	int id;
	double systemTime;
	int serverid;
	int dropped;
};
extern queue <customerData*> q1;
void *processCustomer(int *i);
void *createCustomer(int *i);
extern pthread_mutex_t qmutex;
extern pthread_mutex_t custCountMutex;
extern double lambda;
extern double mu;
extern long int seed;
extern int qsize;
extern int noOfCust;
extern int noOfCust1;
extern int pdist;		// 0 - exp ; 1 - det
extern char tsfile[256];
extern int sflag;
extern pthread_cond_t custArrived;
extern pthread_mutex_t timeMutex;
extern int tflag;
extern int custServiced;
extern sigset_t signalSet;
extern int quitStatus;
extern double simStart;
extern double simEnd;
extern double serverBusyTime[NO_OF_SERVERS];
extern double serverTotalTime[NO_OF_SERVERS];
extern double avgCustAtQueue;
extern int noOfCustArrived;
extern double noOfCustDropped;
extern double interArrTime;
extern double custServerServiceTime;
extern double custQueuedTime;
extern double custSystemTime;
extern double custSystemTimeSqr;
extern double systemTime;
