/*
 * nonBeaconNode.h
 *
 *  Created on: Mar 10, 2012
 *      Author: abhishek
 */
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
