/*
*	server.cpp: This is the file implements the server thread functions.
*	The server processes the customer by removing it from the start of the 	
*	queue and timestamps the customer as and when its actions are performed
*/

#include <iostream>
#include "centralserver.h"

using namespace std;

/*
*	getTimeStamp: returns the timestamp according to the current system time
*/

double getTimeStamp()
{
	double ts = 0;
	timeval tim;
	gettimeofday(&tim, NULL);
	ts = (tim.tv_sec * 1000000) + (tim.tv_usec);
	return ts;
}

/*
*	pricessCustomer(int): This function represents the main server functionality.
*	It dequeues the customer and then processes it as per the service time.
*/

void* processCustomer(int *i)
{
	int sid = *i;
	double servicetime = 0.0, dequedtime = 0.0,begintime = 0.0,endtime = 0.0,temp = 0.0,srvStart = 0, srvEnd = 0, srvServingStart = 0, srvServingEnd = 0;
	if(quitStatus == 1)
		pthread_exit(NULL);
	srvStart = getTimeStamp();
	while(1)
	{
		customerData *n;
		pthread_mutex_lock(&qmutex);
		if(quitStatus == 1)			//check if its time to quit
		{
			pthread_mutex_unlock(&qmutex);
			break;
		}
		while(q1.empty())
		{
			pthread_cond_wait(&custArrived,&qmutex);	// Wait till the time queue is empty
			if((noOfCust)==0)
			{
				break;
			}
		}
		if((noOfCust)==0)		// If all the customers are served then quit
		{
			pthread_cond_signal(&custArrived);
			pthread_mutex_unlock(&qmutex);
			break;
		}
		srvServingStart = getTimeStamp();
		n = q1.front();
		q1.pop();
		n->dropped = 1;
		custServiced += 1;
		noOfCust--;
		pthread_mutex_unlock(&qmutex);
		n->serverid = sid;
		dequedtime = getTimeStamp();
		n->timeDeQueued = dequedtime;
		temp = dequedtime - n->timeQueued;
		n->systemTime += temp;
		systemTime += n->timeDeQueued - n->timeQueued;
		printf("\n%012.3lfms: c%d leaves Q1, time in Q1 = %.3lfms",systemTime/1000,n->id,(dequedtime - n->timeQueued)/1000);
		servicetime = (double) n->serviceTime;
		begintime = getTimeStamp();
		n->timeServiceStart = getTimeStamp();
		temp = begintime - dequedtime;
		n->systemTime += temp;
		pthread_mutex_lock(&timeMutex);
		systemTime += n->timeServiceStart - n->timeDeQueued;
		pthread_mutex_unlock(&timeMutex);
		printf("\n%012.3lfms: c%d begin service as S%d",systemTime/1000,n->id,sid);
		usleep(servicetime * 1000);					//Sleep for the total service time for the customer
		endtime = getTimeStamp();
		n->timeServiceEnd = getTimeStamp();
		custServerServiceTime += (n->timeServiceEnd - n->timeServiceStart)/1000;
		n->systemExit = getTimeStamp();
		pthread_mutex_lock(&qmutex);
		custQueuedTime += (n->timeDeQueued - n->timeQueued)/1000;
		custSystemTime += (n->systemExit - n->systemEntry)/1000;
		custSystemTimeSqr += (((n->systemExit - n->systemEntry)/1000) * ((n->systemExit - n->systemEntry)/1000));
		pthread_mutex_unlock(&qmutex);
		temp = endtime - begintime;
		n->systemTime += temp;
		pthread_mutex_lock(&timeMutex);
		systemTime += (n->timeServiceEnd - n->timeServiceStart);
		pthread_mutex_unlock(&timeMutex);
		printf("\n%012.3lfms: c%d departs from S%d, service time = %.3lfms time in system = %.3lfms",systemTime/1000,n->id,sid,(endtime-begintime)/1000,(n->systemExit - n->systemEntry)/1000);
		srvServingEnd = getTimeStamp();
		serverBusyTime[sid - 1] += (srvServingEnd - srvServingStart)/1000;
		if(quitStatus == 1)
			break;
		if((noOfCust) == 0)
		{
			pthread_mutex_lock(&qmutex);
			pthread_cond_broadcast(&custArrived);
			pthread_mutex_unlock(&qmutex);
			break;
		}
		delete n;
	}
	simEnd = getTimeStamp();
	srvEnd = getTimeStamp();
	serverTotalTime[sid - 1] = (srvEnd - srvStart)/1000;
	pthread_exit(NULL);
}
