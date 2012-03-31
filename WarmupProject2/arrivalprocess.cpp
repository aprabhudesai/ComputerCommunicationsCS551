/*
*	arrivalprocess.cpp: This is the file implements the functions of the customer
*	creation thread. It generates the customers using either a trace file or using
*	an equation based on the lambda and mu values.
*/

#include <iostream>
#include "centralserver.h"
#define round(X) (((X)>= 0)?(int)((X)+0.5):(int)((X)-0.5))
using namespace std;

int *custInterArrTime;
int *custServiceTime;
struct sigaction act2;

/*
*	InitRandom(long): sets the random value
*/

void InitRandom(long l_seed)
{
	if (l_seed == 0L) {
		time_t localtime=(time_t)0;
        time(&localtime);
		srand48((long)localtime);
    } 
	else {
        srand48(l_seed);
    }
}

/*
*	ExponentialInterval(double,double): Using the rate and the random value it calculates the "w"
*	value for the probability distribution function.
*/

int ExponentialInterval(double dval,double rate)
{
	int w = log(1 - dval) / (-rate);
	return w;
}
/* 	GetInterval(int,double): Returns a value according to the following rule:    
* 	exponential == 0 if "-d det" is used
*	exponential != 0 if "-d exp" is used 
*/

int GetInterval(int exponential, double rate)
{
	int result;
    if (exponential) {
        double dval=(double)drand48();
		result = ExponentialInterval(dval, rate);
	} else {
        double millisecond=((double)1000)/rate;
		result = round(millisecond);
    }
	if(result < 1)
		result = 1;
	if(result > 10000)
		result = 10000;
    return result;
}

/*
*	readTraceFile: This function reads the values from trace file if "-t" flag is specified
*/

void readTraceFile()
{
	int j = 0,k=0;
	FILE *fptr = NULL;
	char temp1[20];
	char temp2[20];
	char buffer[100];
	fptr = fopen(tsfile,"r");
	fgets(buffer,20,fptr);
	noOfCust = atoi(buffer);
	custInterArrTime = new int[noOfCust];
	custServiceTime = new int[noOfCust];
	for(int i = 0; i < noOfCust ; i++)
	{
		memset(buffer,0,100);
		fgets(buffer,20,fptr);
		j=0;
		k=0;
		while(buffer[j] != ' ')
		{
			temp1[k] = buffer[j];
			j++;k++;
		}
		temp1[k] = '\0';
		custInterArrTime[i] = (double)atoi(temp1);
		k=0;
		j++;
		while(buffer[j] != '\n')
		{
			temp2[k] = buffer[j];
			j++;k++;
		}
		temp2[k] = '\0';
		custServiceTime[i] = (double)atoi(temp2);
	}
	fclose(fptr);
}

/*
*	displayParam: Displays the parameters that are going to be used for the simulation
*/

void displayParam()
{
	printf("\nParameters:");
	if(tflag != 1)
	{
		printf("\n\tlambda = %lf",lambda);
		printf("\n\tmu = %lf",mu);
	}
	if(sflag == 1)
		printf("\n\tsystem = M/M/1");
	else
		printf("\n\tsystem = M/M/2");
	printf("\n\tseed = %ld",seed);
	printf("\n\tsize = %d",qsize);
	printf("\n\tnumber = %d",noOfCust);
	if(tflag != 1)
	{
		if(pdist == 1)
			printf("\n\tdistribution = exp\n");
		else
			printf("\n\tdistribution = det\n");
	}
	if(tflag == 1)
		printf("\n\ttsfile = %s\n",tsfile);
}


/*
*	cleanupqueue: removes all the customers from the queue when SIGINT is received
*/

void cleanupqueue()
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
*	interruptHandler: Function to handle SIGINT
*/

void interruptHandler(int a)
{
	cleanupqueue();
	quitStatus = 1;			//set a global variable to tell everyone that its time to quit
}

double getTimeStamp1()
{
	double ts = 0;
	timeval tim1;
	gettimeofday(&tim1, NULL);
	ts = (tim1.tv_sec * 1000000) + (tim1.tv_usec);
	return ts;
}

/*
*	createCustomer(int): This function is responsible for creating the customer threads as
*	per the inter arrival time. It then queues the customer and then boradcasts the servers
*	so that they can process the customer.
*/

void *createCustomer(int *nouse)
{
	int i=0,custservice=0;
	double sleeptime = 0.0, time1 = 0.0, time2 = 0.0, start = 0.0, end = 0.0,custarr=0;
	act2.sa_handler = interruptHandler;
	sigaction(SIGINT, &act2, NULL);
	pthread_sigmask(SIG_UNBLOCK, &signalSet, NULL);		//Unblock SIGINT inorder to receive the signal
	if(tflag == 1)
	{
		readTraceFile();
	}
	displayParam();
	noOfCust1 = noOfCust;
	simStart = getTimeStamp1();
	printf("\n%012.3lfms: emulation begins",systemTime);
	for(i=0; i < noOfCust1 ; i++)
	{
		customerData *c1 = new customerData;
		InitRandom(seed);
		custarr = GetInterval(pdist,lambda);
		InitRandom(seed);
		custservice = GetInterval(pdist,mu);
		c1->id = i+1;
		if(tflag == 1)
			c1->interArrivalTime = custInterArrTime[i];
		else
			c1->interArrivalTime = custarr;
		if((double)c1->interArrivalTime > ((end - start)/1000))
			sleeptime = (double)c1->interArrivalTime - ((end - start)/1000);
		else
		{
			c1->interArrivalTime = ((end - start)/1000);
			sleeptime = 0;		
		}
		interArrTime += c1->interArrivalTime;
		usleep(sleeptime*1000);
		time2 = getTimeStamp1();
		pthread_mutex_lock(&qmutex);
		if((int)q1.size() == qsize)					//check if queue size is greater than the capacity
		{
			pthread_mutex_unlock(&qmutex);
			time1 = getTimeStamp1();
			pthread_mutex_lock(&timeMutex);
			systemTime += (time1 - time2);
			pthread_mutex_unlock(&timeMutex);
			noOfCustDropped++;						//if yes then drop the customer
			noOfCustArrived++;
			noOfCust--;
			printf("\n%012.3lfms: c%d dropped",systemTime/1000,i+1);
			continue;
		}
		time2 = getTimeStamp1();
		c1->systemEntry = time2;
		start = time2;
		pthread_mutex_lock(&timeMutex);
		systemTime += c1->interArrivalTime * 1000;
		pthread_mutex_unlock(&timeMutex);
		printf("\n%012.3lfms: c%d arrives, inter-arrival time = %.3lfms",systemTime/1000,c1->id,(double)c1->interArrivalTime);
		time1 = getTimeStamp1();
		c1->timeQueued = getTimeStamp1();
		if(tflag == 1)
			c1->serviceTime = custServiceTime[i];
		else
			c1->serviceTime = custservice;
		time2 = getTimeStamp1();
		pthread_mutex_lock(&timeMutex);
		systemTime += (time2-time1);
		pthread_mutex_unlock(&timeMutex);
		printf("\n%012.3lfms: c%d enters Q1",systemTime/1000,c1->id);
		q1.push(c1);												//Queue the customer
		noOfCustArrived++;
		pthread_cond_broadcast(&custArrived);						//Broadcast on the condition
		pthread_mutex_unlock(&qmutex);
		if(quitStatus == 1)
		{
			pthread_mutex_lock(&qmutex);
			pthread_cond_broadcast(&custArrived);						//Broadcast on the condition
			pthread_mutex_unlock(&qmutex);
			pthread_exit(NULL);
		}
		end = getTimeStamp1();
	}
	pthread_exit(NULL);
}
