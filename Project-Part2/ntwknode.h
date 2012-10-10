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

#pragma once

#include <stdint.h>

#define NO_OF_BEACON 10

struct NodeInfo
{
	char nodeID[256];
	char nodeInstanceId[256];
	uint32_t port;
	uint64_t location;
	char * homeDir;
	char hostname[256];
	char hostip[16];

	char *logFileName;
	uint16_t autoShutdown; //seconds
	uint8_t ttl; //seconds
	uint16_t msgLifetime;//seconds
	uint16_t getMsgLifeTime; //seconds
	uint16_t initNeighbors; //used to pick the neighbors during join process
						// For non Beacon Nodes.
	uint16_t joinTimeout; //seconds // Checked for join response + Check Messages
						// Need to have response from atleaset 'initneighbors' number of responses
						// Not used for Beacon Nodes
    uint16_t keepAliveTimeout; //seconds //if no activity of messages close connection
	uint16_t minNeighbors; // For Non Beacon Nodes
						// If lesser number of connections, delete initneighborlist file and rejoin n/w
	uint16_t noCheck; // Non Beacon Nodes
				// if '0' check messages enabled else disabled
	double cacheProb; // Cache probability for File
	double storeProb; //store file on system
	double neighborStoreProb; //Originates or Receives a store request
					// request with store request to neighbor
	uint32_t cacheSize; //in kilobytes
	uint32_t permSize;
	/*  Beacon Info */
	uint16_t retry; // retry connecting to beacons

	char *beaconsHostNames[NO_OF_BEACON]; //list of all beacons
	uint32_t beaconport[NO_OF_BEACON];
	int no_of_beacons;
};
