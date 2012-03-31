/*
 * iniparser.c
 *
 *  Created on: Feb 24, 2012
 *      Author: abhishek
 */

#include "iniparser.h"
//#include "ntwknode.h"

//using namespace std;

int beacon_loc = 0;
char key[100] = "\0";
char value[100] = "\0";
int intValue = 0;

void initializeNodeInfo(char *key, char *value,struct NodeInfo *ndInfo)
{
	int intVal;
	double dVal;
	if(strcmp(key,"Port")==0)
	{
		intVal = atoi(value);
		ndInfo->port = intVal;
	}
	else if(strcmp(key,"Location")==0)
	{
		intVal = atoi(value);
		ndInfo->location = intVal;
	}
	else if(strcmp(key,"HomeDir")==0)
	{
		ndInfo->homeDir = (char *)malloc(strlen(value)+1);
		strcpy(ndInfo->homeDir, value);
	}
	else if(strcmp(key,"LogFilename")==0)
	{
		ndInfo->logFileName = (char *)malloc(strlen(value)+1);
		strcpy(ndInfo->logFileName,value);
	}
	else if(strcmp(key,"AutoShutdown")==0)
	{
		intVal = atoi(value);
		ndInfo->autoShutdown = intVal;
	}
	else if(strcmp(key,"TTL")==0)
	{
		intVal = atoi(value);
		ndInfo->ttl = intVal;
	}
	else if(strcmp(key,"MsgLifetime")==0)
	{
		intVal = atoi(value);
		ndInfo->msgLifetime = intVal;
	}
	else if(strcmp(key,"GetMsgLifetime")==0)
	{
		intVal = atoi(value);
		ndInfo->getMsgLifeTime = intVal;
	}
	else if(strcmp(key,"InitNeighbors")==0)
	{
		intVal = atoi(value);
		ndInfo->initNeighbors = intVal;
	}
	else if(strcmp(key,"JoinTimeout")==0)
	{
		intVal = atoi(value);
		ndInfo->joinTimeout = intVal;
	}
	else if(strcmp(key,"KeepAliveTimeout")==0)
	{
		intVal = atoi(value);
		ndInfo->keepAliveTimeout = intVal;
	}
	else if(strcmp(key,"MinNeighbors")==0)
	{
		intVal = atoi(value);
		ndInfo->minNeighbors = intVal;
	}
	else if(strcmp(key,"NoCheck")==0)
	{
		intVal = atoi(value);
		ndInfo->noCheck = intVal;
	}
	else if(strcmp(key,"CacheProb")==0)
	{
		dVal = atof(value);
		ndInfo->cacheProb = dVal;
	}
	else if(strcmp(key,"StoreProb")==0)
	{
		dVal = atof(value);
		ndInfo->storeProb = dVal;
	}
	else if(strcmp(key,"NeighborStoreProb")==0)
	{
		dVal = atof(value);
		ndInfo->neighborStoreProb = dVal;
	}
	else if(strcmp(key,"CacheSize")==0)
	{
		intVal = atoi(value);
		ndInfo->cacheSize = intVal;
	}
	else if(strcmp(key,"Retry")==0)
	{
		intVal = atoi(value);
		ndInfo->retry = intVal;
	}
	else
	{
		printf("\nWrong key in ini file\n");
	}
}

char * getKey(char *line)
{
	char *tempstr = NULL;
	int noofchars = 0, i = 0;
	if((tempstr = strchr(line,'=')) == NULL)
	{
		printf("\nInvalid entry in the ini file\n");
		return NULL;
	}
	noofchars = tempstr - line;
	while(noofchars--)
	{
		key[i] = *line;
		i++;
		line++;
	}
	key[i] = '\0';
	return tempstr;
}

int getValue(char *line)
{
	int i = 0;
	line++;
	while(*line != '\n')
	{
		value[i] = *line;
		line++;
		i++;
	}
	value[i] = '\0';
	return 1;
}

void getBeaconInfo(char *beaconInfo,struct NodeInfo *ndInfo)
{
	char *tempstr = NULL;
	char beaconName[100] = "\0";
	char beaconPort[10] = "\0";
	int i = 0, noofchar = 0;
	if((tempstr = strchr(beaconInfo,':')) == NULL)
	{
		printf("\nInvalid beacon information in ini file\n");
		return;
	}
	noofchar = tempstr - beaconInfo;
	while(noofchar--)
	{
		beaconName[i] = *beaconInfo;
		beaconInfo++;
		i++;
	}
	beaconName[i] = '\0';
	i = 0;
	tempstr++;
	while(*tempstr != '\0')
	{
		beaconPort[i] = *tempstr;
		tempstr++;
		i++;
	}
	beaconPort[i] = '\0';
	ndInfo->beaconsHostNames[beacon_loc] = (char *)malloc(strlen(beaconName)+1);
	strcpy(ndInfo->beaconsHostNames[beacon_loc],beaconName);
	ndInfo->beaconport[beacon_loc] = atoi(beaconPort);
	ndInfo->no_of_beacons++;
	beacon_loc++;
}

void parseIniFile(char *fileName,struct NodeInfo *ndInfo)
{
	FILE *iniFptr = NULL;
	struct stat fileData;
	char line[100];
	char *str = NULL, * strkey = NULL;
	char *section = (char *)malloc(10);
	if(stat(fileName,&fileData) < 0)
	{
		printf("\nThe specified ini file %s does not exist\n",fileName);
		exit(0);
	}
	if((iniFptr = fopen(fileName,"r")) == NULL)
	{
		printf("\nError opening ini file: %s\n",fileName);
		exit(0);
	}
	while(fgets(line,sizeof(line),iniFptr) != NULL)
	{
		if((str = strchr(line,'[')) != NULL)
		{
			//This is a section, so find the ']' bracket
			int i = 0;
			str++;
			while(*str != ']')
			{
				section[i] = *str;
				str++;
				i++;
			}
			section[i] = '\0';
		}
		else if(strcmp(line,"\n") != 0)
		{
			//This is not a section, these are key value pairs
			if(strcmp(section,"init") == 0)
			{
				strkey = getKey(line);
				//printf("\n%s",key);
				if(strkey != NULL)
					getValue(strkey);
				//printf(" %s",value);
				initializeNodeInfo(key,value,ndInfo);
			}
			else if(strcmp(section,"beacons") == 0)
			{
				strkey = getKey(line);
				if(strcmp(key,"Retry") == 0)
				{
					if(strkey != NULL)
						getValue(strkey);
					initializeNodeInfo(key,value,ndInfo);
				}
				//printf("\n%s",key);
				else
				{
					getBeaconInfo(key,ndInfo);
				}
			}
		}
	}
	free(section);
}

void showNodeInfo(struct NodeInfo *ndInfo)
{
	printf("\nNode Information:\n");
	printf("\tPort = %d\n",ndInfo->port);
	printf("\tRetry = %d",ndInfo->retry);
	printf("\tBeacon: %s",ndInfo->beaconsHostNames[0]);
	printf("\tBeaconPort: %d",ndInfo->beaconport[0]);
}

