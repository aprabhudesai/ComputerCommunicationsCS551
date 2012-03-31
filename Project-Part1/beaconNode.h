/*
 * beaconNode.h
 *
 *  Created on: Mar 10, 2012
 *      Author: abhishek
 */

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
