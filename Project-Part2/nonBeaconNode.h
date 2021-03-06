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

extern pthread_t nonBeaconDispatcherThread;
extern pthread_t nonBeaconTimerThread;
extern pthread_t nonBeaconConnectorThread,nonBeaconListenerThread;
extern pthread_t nonBeaconCheckThread;
extern pthread_t keyboardThreadNonbeacon;
extern int mynonbeaconSocket;
extern struct  sockaddr_in my_non_beacon_addr;
extern int nonbeaconsockOptLen;
extern bool nonbeaconsockOpt;
extern struct sockaddr_in accept_addr_nonbeacon;
extern int no_of_join_responses;
extern int no_of_hello_responses;
extern int no_of_connection;
extern int participating;

void createNonBeacon(struct NodeInfo *);
