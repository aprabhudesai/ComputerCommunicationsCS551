/*
*	centralserver.cpp: This is the file implements the main function that is responsible for:
*	1. Creation of the customer creation thread
*	2. Creation of the server threads
*	3. After the simulation is done, printing the statistics
*			 
*/

#include "centralserver.h"

using namespace std;

//Global Variables

queue <customerData*> q1;
double lambda = 0.5;
double mu = 0.35;
long int seed = 0;
int qsize = 5;
int noOfCust = 20;
int pdist = 1;		// 1 - exp ; 0 - det
char tsfile[256] = {'\0'};
int sflag = 0;
pthread_mutex_t qmutex;
pthread_cond_t custArrived;
pthread_mutex_t timeMutex;
pthread_mutex_t custCountMutex;
int arriveFlag = 0;
int tflag = 0;
int custServiced = 0;
sigset_t signalSet;
int quitStatus = 0;
double simStart = 0;
double simEnd = 0;
double serverBusyTime[NO_OF_SERVERS] = {0};
double serverTotalTime[NO_OF_SERVERS] = {0};
double avgCustAtQueue = 0;
int noOfCustArrived = 0;
double noOfCustDropped = 0;
int noOfCust1 = 20;
double interArrTime = 0;
double custServerServiceTime = 0;
double custQueuedTime = 0;
double custSystemTime = 0;
double custSystemTimeSqr = 0;
double systemTime = 0;

/*
*	Function: parsecmdline
*	Parses the command line arguments provided by the user. If they are invalid then
*	it does not accept them and throws an error message.
*/

void parseCmdLineArgs(int argc,char *argv[])
{
	int noOfArgs = argc - 1,cnt=1, i=0;
	char temp[256] = {'\0'};
	while(noOfArgs != 0)
	{
		if(strcmp(argv[cnt],"-lambda") == 0)
		{
			i=0;
			strncpy(temp,argv[cnt+1],strlen(argv[cnt+1]));
			lambda = atof(argv[cnt+1]);
			if(lambda < 0)
			{
				printf("\nThe lambda value specified is incorrect\n");
				exit(1);
			}
			noOfArgs-=2;
			cnt+=2;
		}	
		else if(strcmp(argv[cnt],"-mu") == 0)
		{
			mu = atof(argv[cnt+1]);
			if(mu < 0)
			{
				printf("\nThe mu value specified is incorrect\n");
				exit(1);
			}
			noOfArgs-=2;
			cnt+=2;
		}
		else if(strcmp(argv[cnt],"-s") == 0)
		{
			sflag = 1;
			noOfArgs-=1;
			cnt+=1;
		}
		else if(strcmp(argv[cnt],"-seed") == 0)
		{
			seed = atoi(argv[cnt+1]);
			noOfArgs-=2;
			cnt+=2;
		}
		else if(strcmp(argv[cnt],"-size") == 0)
		{
			qsize = atoi(argv[cnt+1]);
			if(qsize < 0)
			{
				printf("\nThe size value specified is incorrect\n");
				exit(1);
			}
			noOfArgs-=2;
			cnt+=2;
		}
		else if(strcmp(argv[cnt],"-n") == 0)
		{
			noOfCust = atoi(argv[cnt+1]);
			if(noOfCust < 0)
			{
				printf("\nThe n (no. of Customers) value specified is incorrect\n");
				exit(1);
			}
			noOfArgs-=2;
			cnt+=2;
		}
		else if(strcmp(argv[cnt],"-d") == 0)
		{
			if(strcmp(argv[cnt+1],"exp") == 0)
				pdist = 1;
			else if(strcmp(argv[cnt+1],"det") == 0)
				pdist = 0;
			noOfArgs-=2;
			cnt+=2;
		}
		else if(strcmp(argv[cnt],"-t") == 0)
		{
			strncpy(tsfile,argv[cnt+1],strlen(argv[cnt+1]));
			tflag = 1;
			noOfArgs-=2;
			cnt+=2;
		}
		else
		{
			printf("\nIncorrect syntax. Correct syntax is :  'mm2 [-lambda lambda] [-mu mu] [-s] [-seed seedval] [-size sz] [-n num] [-d {exp|det}] [-t tsfile]'\n");
			exit(1);
		}
	}
}

/*
*	calculateStats(int,int): This function accepts the type of calculation that needs to be performed
*	and returns the appropriate value
*/

double calculateStats(int reqType,int serverid)
{
	double res = 0;
	switch (reqType)
	{
		case 1:
				{	//calculate average interval arrival time
					res = interArrTime / noOfCustArrived;
					break;
				}
		case 2:
				{	//calculate average service time
					res = custServerServiceTime / custServiced;
					break;
				}
		case 3:
				{	//calculatge average no of customers in Q1
					res = custQueuedTime / ((simEnd - simStart)/1000);
					break;
				}
		case 4:
				{	//calculate average no of customers at server
					res = serverBusyTime[serverid - 1]/((simEnd - simStart)/1000);
					break;
				}
		case 5:
				{	//calculate average time spent in system
					res =  custSystemTime/custServiced;
					break;
				}
		case 6:
				{	//calculate std deviation for time spent in system
					double avgOfSqr = 0,sqrOfAvg = 0;
					avgOfSqr = custSystemTimeSqr/custServiced;
					sqrOfAvg = ((custSystemTime/custServiced)*(custSystemTime/custServiced));
					res = sqrt(avgOfSqr - sqrOfAvg);
					break;
				}
		case 7:
				{	//calculate customer drop prabability
					res = noOfCustDropped/noOfCust1;
					break;
				}
	}
	return res;
}

void cleanupSystem()
{
	pthread_mutex_lock(&qmutex);
	while(!q1.empty())
	{
		customerData *n1;
		n1 = q1.front();
		n1->dropped = 0;
		q1.pop();
		delete n1;
	}
	pthread_mutex_unlock(&qmutex);
}

/*
*	sigintHandler: interrupt handler for SIGINT
*/

void sigintHandler(int b)
{
	cleanupSystem();
	quitStatus = 1;
}

/*
*	printStats: This function is responsible for printing the statistics calculated
*	for the entire simulation.
*/

void printStats()
{
	printf("\n\nStatistics:\n");
	printf("\n\taverage inter-arrival time = %.6lf",calculateStats(1,1)/1000);
    printf("\n\taverage service time = %.6lf\n",calculateStats(2,1)/1000);
    printf("\n\taverage number of customers in Q1 = %.6lf",calculateStats(3,1));
    printf("\n\taverage number of customers at S1 = %.6lf",calculateStats(4,1));
	if(sflag != 1)
    printf("\n\taverage number of customers at S2 = %.6lf",calculateStats(4,2));
    printf("\n\n\taverage time spent in system = %.6lf",calculateStats(5,1)/1000);
    printf("\n\tstandard deviation for time spent in system = %.6lf\n",calculateStats(6,1)/1000);
    printf("\n\tcustomer drop probability = %.6lf\n",calculateStats(7,1));
}	

int main(int argc, char *argv[])
{
	int i,j;
	timeval tim;
	struct sigaction act1;
	sigemptyset(&signalSet);
	sigaddset(&signalSet, SIGINT);
	pthread_sigmask(SIG_BLOCK, &signalSet, NULL);
	parseCmdLineArgs(argc,argv);
	pthread_mutex_init(&qmutex, NULL);
	pthread_mutex_init(&timeMutex,NULL);
	pthread_mutex_init(&custCountMutex, NULL);
	pthread_t server1,server2,arrivalthread;
	pthread_cond_init(&custArrived,NULL);
	gettimeofday(&tim, NULL);
	i = 1;
	pthread_create(&server1,NULL,(void* (*)(void*))processCustomer,&i);
	if(sflag != 1)
	{
		j = 2;
		pthread_create(&server2,NULL,(void* (*)(void*))processCustomer,&j);
	}
	pthread_create(&arrivalthread,NULL,(void* (*)(void*))createCustomer,&i);
	pthread_join(arrivalthread,NULL);
	act1.sa_handler = sigintHandler;
	sigaction(SIGINT, &act1, NULL);
	pthread_sigmask(SIG_UNBLOCK, &signalSet, NULL);
	pthread_join(server1,NULL);
	if(sflag != 1)
	{
		pthread_join(server2,NULL);
	}
	printStats();
	return 0;
}
