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

#include <pthread.h>
#include "ntwknode.h"

extern pthread_t listenerThread;
extern pthread_t connectorThread;
extern pthread_t keyboardThread;
extern pthread_t timerThread;
extern pthread_t dispatcherThread;
extern pthread_t beaconCheckThread;
extern int mybeaconSocket;
extern struct  sockaddr_in my_beacon_addr;
extern int beaconsockOptLen;
extern bool beaconsockOpt;
extern int no_of_neighbors_connected_to;

void createBeacon(struct NodeInfo *ndInfo);
