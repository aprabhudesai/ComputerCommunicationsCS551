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

#include <iostream>
#include <stdlib.h>
#include "systemDataStructures.h"
#include "iniparser.h"
#include "ntwknode.h"
#include "headers.h"
#include "message.h"
#include <time.h>
#include <cstring>
#include <openssl/sha.h> /* please read this */
#include <openssl/md5.h>

extern FILE *logFilePtr;
extern pthread_mutex_t logFileLock;
extern FILE *extFilePtr;
extern char extfilecmd[256];
extern pthread_mutex_t extFileLock;
extern struct NodeInfo ndInfo;
extern pthread_mutex_t checkThreadLock;
extern pthread_cond_t checkThreadCV;
extern int checkStatus;

extern STATUSMSGMAPTYPE statusMsgMap;
extern STATUSMSGMAPTYPE::iterator statusMsgMapIter;
extern pthread_mutex_t statusMsgMapLock;

extern pthread_t listenerThread;
extern pthread_mutex_t eventDispatcherQueueLock;
extern pthread_cond_t eventDispatcherQueueCV;
extern pthread_t keyboardThread;
extern NodeInfo ndInfo;


void ReConnectToBeaconNode(void *);
void freeMessage(struct message *);
void writeCommentToLogFile(char *);

void sigpipeHandler(int signum)
{

}

void sigUSR1Handler(int signum)
{
	//printf("\n\n----;;;-----Got a signal to Quit ---;;;-----\n\n");
	pthread_exit(NULL);
}

void sigUSR2Handler(int signum)
{
	//printf("\n\n----;;;-----Got a signal to Quit ---;;;-----\n\n");
	pthread_exit(NULL);
}

uint32_t getCurrentCacheSize()
{
	uint32_t cacheSize = 0;
	pthread_mutex_lock(&LRUlistLock);
	char cacheFileName[256] = "\0";
	struct fileMetadata *fMD;
	for(LRUIter = LRUList.begin(); LRUIter != LRUList.end(); LRUIter++)
	{
		fMD = NULL;
		memset(cacheFileName,0,256);
		sprintf(cacheFileName,"%s/files/%d.meta",ndInfo.homeDir,*LRUIter);
		fMD = createMetaDataFromFile(cacheFileName);
		cacheSize += fMD->fileSize;
		free(fMD);
	}
	pthread_mutex_unlock(&LRUlistLock);
	return cacheSize;
}

void removeFileEntryFromIndexes(int fileNo)
{
	/*Remove entry from Kwrd Index*/
	pthread_mutex_lock(&KeywordIndexMapLock);
	for(KeywordIndexMapIter = KeywordIndexMap.begin(); KeywordIndexMapIter != KeywordIndexMap.end(); KeywordIndexMapIter++)
	{
		if(KeywordIndexMapIter->second == fileNo)
		{
			KeywordIndexMap.erase(KeywordIndexMapIter->first);
		}
	}
	pthread_mutex_unlock(&KeywordIndexMapLock);
	/*Remove entry from SHA1 Index*/
	pthread_mutex_lock(&SHA1IndexMapLock);
	for(SHA1IndexMapIter = SHA1IndexMap.begin(); SHA1IndexMapIter != SHA1IndexMap.end(); SHA1IndexMapIter++)
	{
		if(SHA1IndexMapIter->second == fileNo)
		{
			SHA1IndexMap.erase(SHA1IndexMapIter->first);
		}
	}
	pthread_mutex_unlock(&SHA1IndexMapLock);

	/*Remove entry from fileName Index*/
	pthread_mutex_lock(&fileNameIndexMapLock);
	for(fileNameIndexMapIter = fileNameIndexMap.begin(); fileNameIndexMapIter != fileNameIndexMap.end(); fileNameIndexMapIter++)
	{
		if(fileNameIndexMapIter->second == fileNo)
		{
			fileNameIndexMap.erase(fileNameIndexMapIter->first);
		}
	}
	pthread_mutex_unlock(&fileNameIndexMapLock);
}

void insertFileEntryIntoIndex(struct fileMetadata *recvdFileMetaData,int currentFileIndex)
{
	/*Add the entry to FileName Index*/
	char keytype[256] = "\0";
	int i = 0;
	while(recvdFileMetaData->fileName[i] != '\0')
	{
		keytype[i] = *(recvdFileMetaData->fileName + i);
		i++;
	}
	keytype[i] = '\0';

	string insertKey = string(keytype);
	pthread_mutex_lock(&fileNameIndexMapLock);
	fileNameIndexMap.insert(pair<string,int>(insertKey,currentFileIndex));
	pthread_mutex_unlock(&fileNameIndexMapLock);


	/*Add the entry to SHA1 Index*/
	memset(keytype,0,256);
	i = 0;
	while(i < 20)
	{
		keytype[i] = *(recvdFileMetaData->SHA1val + i);
		i++;
	}
	keytype[i] = '\0';

	string insertshaKey = string(keytype);
	pthread_mutex_lock(&SHA1IndexMapLock);
	SHA1IndexMap.insert(pair<string,int>(insertshaKey,currentFileIndex));
	pthread_mutex_unlock(&SHA1IndexMapLock);


	/*Add the entry to Keyword Index*/
	memset(keytype,0,256);
	i = 0;

	char Vkeytype[260];
	while(i < 128)
	{
		sprintf(&Vkeytype[2 * i],"%02x",recvdFileMetaData->bitVector[i]);
		i++;
	}
	Vkeytype[256] = '\0';

	string insertkwrdKey = string(Vkeytype);

	pthread_mutex_lock(&KeywordIndexMapLock);
	KeywordIndexMap.insert(pair<string,int>(insertkwrdKey,currentFileIndex));
	pthread_mutex_unlock(&KeywordIndexMapLock);
}

double getProbability()
{
	double probVal = 0.0,num = 0.0;
	struct timeval tv;
	gettimeofday(&tv,NULL);
	num = tv.tv_sec ^ tv.tv_usec;
	srand48(num);
	probVal = drand48();
	return probVal;
}

char metakey[100] = "\0";
char metavalue[512] = "\0";

char * getMetaFileKey(char *line)
{
	char *tempstr = NULL;
	int noofchars = 0, i = 0;
	if((tempstr = strchr(line,'=')) == NULL)
	{
		printf("\nInvalid entry in the meta file\n");
		return NULL;
	}
	noofchars = tempstr - line;
	while(noofchars--)
	{
		metakey[i] = *line;
		i++;
		line++;
	}
	metakey[i] = '\0';
	return tempstr;
}

int getMetaFileValue(char *line)
{
	int i = 0;
	line++;
	while(*line != '\r')
	{
		metavalue[i] = *line;
		line++;
		i++;
	}
	metavalue[i] = '\0';
	return 1;
}

struct fileMetadata *createMetaDataFromFile(char *fileName)
{
	FILE *Fptr = NULL;
	struct stat fileData;
	char line[512] = "\0";
	char *str = NULL, * strkey = NULL;

	char *section = (char *)malloc(10);
	struct fileMetadata *fm = (struct fileMetadata *)malloc(sizeof(fileMetadata));

	if(stat(fileName,&fileData) < 0)
	{
		//The specified file does not exist
		return NULL;
	}
	if((Fptr = fopen(fileName,"r")) == NULL)
	{
		printf("\nError opening file: %s\n",fileName);
		return NULL;
	}
	for(int i = 0; i < 3; i++)
	{
		if(fgets(line,sizeof(line),Fptr) != NULL)
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
				strkey = getMetaFileKey(line);
				//printf("\n%s",key);
				if(strkey != NULL)
					getMetaFileValue(strkey);
				//printf(" %s",value);
				if(strcmp(metakey,"FileName") == 0)
				{
					int i = 0;
					while(metavalue[i] != '\0')
					{
						fm->fileName[i] = metavalue[i];
						i++;
					}
					fm->fileName[i] = '\0';
				}
				if(strcmp(metakey,"FileSize") == 0)
				{
					uint32_t fz = atoi(metavalue);
					fm->fileSize = fz;
				}
			}
		}
	}
	unsigned int temp;

	fseek(Fptr,5,SEEK_CUR);
	int k = 0;

	while(k<20){
		fscanf(Fptr,"%02x",&temp);
		fm->SHA1val[k] = temp;
		k++;
	}
	fm->SHA1val[k] = '\0';

	fseek(Fptr,8,SEEK_CUR);
		k = 0;

	while(k<20){
		fscanf(Fptr,"%02x",&temp);
		fm->nonce[k] = temp;
		k++;
	}
	fm->nonce[k] = '\0';

	fseek(Fptr,2,SEEK_CUR);
	fgets(line,sizeof(line),Fptr);
	strkey = getMetaFileKey(line);
	if(strkey != NULL)
		getMetaFileValue(strkey);

	memcpy(fm->keywordsWithSpace,metavalue,strlen(metavalue));
	fm->keywordsWithSpace[strlen(metavalue)] = '\0';

	fseek(Fptr,11,SEEK_CUR);
	k = 127;
	while(k >= 0){
		fscanf(Fptr,"%02x",&temp);
		fm->bitVector[k] = temp;
		k--;
	}
	fclose(Fptr);

	// = File Name + 4 of file Size + 20 SHA1 + 20 nonce +
	// keywords length including space + 128 bit Vector + file ID 20 + 12 bytes for all <CR> and <LF>
	// DO NOT Include keywordCount, fileType size in this size
	//*** Include <CR><LF> for all entries in the meta Size (there are 6 <CR><LF> in total = 12 bytes)
	fm->metaSize = strlen((char*)fm->fileName) + 4 + 20 +20 + strlen((char*)fm->keywordsWithSpace) + 128 + 20 + 12;

	parseKeywords(fm);

	char *tempCh;
	tempCh = strchr(fileName,'.');
	strcpy(tempCh + 1,"fileid");

	if((Fptr = fopen(fileName,"r")) == NULL)
	{
		printf("\nError opening file: %s\n",fileName);
		return fm;
	}
	k = 0;
	while(k < 20){
		fscanf(Fptr,"%02x",&temp);
		fm->fileId[k] = temp;
		k++;
	}
	fm->fileId[k] = '\0';
	fclose(Fptr);

	return fm;
}

int checkFileWithFileId(int fileidList[],unsigned char *fileUOID,int cnt)
{
	int foundFile = -1,k=0,flag = 0;
	char fileNameToFind[256] = "\0";
	struct stat fileData;
	FILE *tempFilePtr;
	unsigned char line[256] = "\0";
	unsigned int temp;
	for(int i = 0 ; i < cnt ; i++)
	{
		temp = 0;
		k=0;
		flag = 0;
		sprintf(fileNameToFind,"%s/files/%d.fileid",ndInfo.homeDir,fileidList[i]);
		if(stat(fileNameToFind,&fileData) < 0)
		{
			printf("\nFile with the id [%d] not present\n",fileidList[i]);
			continue;
		}
		k=0;
		tempFilePtr = fopen(fileNameToFind,"r");
		memset(line,0,256);
		while(k < 20){
			fscanf(tempFilePtr,"%02x",&temp);
			line[k] = temp;
			k++;
		}
		fclose(tempFilePtr);
		if(strncmp((char *)line,(char *)fileUOID,20) == 0)
		{
			flag = 1;
		}
		if(flag == 1)
		{
			foundFile = fileidList[i];
			//printf("\nfoundFile = %d\n",foundFile);
			break;
		}
	}
	return foundFile;
}

int checkForFileWithNonceAndSHA1(struct deleteMessage *recvdDeleteMsg,int fileNum,int selfDelete)
{
	int status = 1;
	char tempMetaFile[256] = "\0";
	struct stat fileData;
	FILE *passFilePtr;
	sprintf(tempMetaFile,"%s/files/%d.meta",ndInfo.homeDir,fileNum);
	if(stat(tempMetaFile,&fileData) < 0)
	{
		printf("\nFile with the id [%d] not present\n",fileNum);
		return 0;
	}

	struct fileMetadata *fileMD = createMetaDataFromFile(tempMetaFile);
	int k = 0;
	/*Compare SHA1*/
	if((strncmp((char *)fileMD->SHA1val,(char *)recvdDeleteMsg->fileSHA,20)) != 0)
	{
		//SHA1 Not Matching
		return 0;
	}

	/*Compare Nonce*/
	if((strncmp((char *)fileMD->nonce,(char *)recvdDeleteMsg->nonce,20)) != 0)
	{
		// Nonce Not Matching
		return 2;
	}

	unsigned char newNonce[21] = "\0";
	if(selfDelete == 0)
	{
		/*This msg is Issued on the same node so I will check if password file present*/

		/*SHA1 & Nonce Matched I check if I have the password file*/
		sprintf(tempMetaFile,"%s/files/%d.pass",ndInfo.homeDir,fileNum);
		if(stat(tempMetaFile,&fileData) < 0)
		{
			printf("\nPassword File with the id [%d] not present\n",fileNum);
			//free(fileMD);
			return 2;
		}

		unsigned char passwordFromFile[21] = "\0";
		/*I have the password file. I will verify if the password matches the nonce stored in meta file*/
		passFilePtr = fopen(tempMetaFile,"r");
		memset(passwordFromFile,0,30);
		unsigned int temp;
		while(k < 20){
			fscanf(passFilePtr,"%02x",&temp);
			passwordFromFile[k] = temp;
			recvdDeleteMsg->password[k] = temp;
			k++;
		}
		fclose(passFilePtr);

		SHA1(passwordFromFile, 20 , newNonce);
	}
	else
	{
		SHA1(recvdDeleteMsg->password, 20 , newNonce);
	}

	/*Compare Nonce*/
	if((strncmp((char *)newNonce,(char *)fileMD->nonce,20)) != 0)
	{
		//Nonce Not Matching After password read
		return 2;
	}
	/*Valid delete message so return 1*/

	return status;
}

int compareHexStrings(unsigned char *stored,unsigned char *recvd)
{
	int status = 1;
		if(strncmp((char *)stored,(char *)recvd,20) != 0)
			return 0;
	return status;
}

int checkIfFileAlreadyPresentInCache(struct fileMetadata *recvdFileMetaData)
{
	int present = 0,i = 0;
	/*Check if fileName Match*/

	struct fileMetadata *storedFileMetadata = NULL;
	char keytype[256] = "\0";
	i = 0;
	while(recvdFileMetaData->fileName[i] != '\0')
	{
		keytype[i] = *(recvdFileMetaData->fileName + i);
		i++;
	}
	keytype[i] = '\0';

	string searchKey = string(keytype);

	char storedFileName[256] = "\0";
	int compareStatus = 0;
	pthread_mutex_lock(&fileNameIndexMapLock);
	fileNameIndexMapIter = fileNameIndexMap.find(searchKey);
	if(fileNameIndexMapIter != fileNameIndexMap.end())
	{
		//File Name Matched
		fileNameIndexIt = fileNameIndexMap.equal_range(fileNameIndexMapIter->first);
		for(fileNameIndexITIter = fileNameIndexIt.first; fileNameIndexITIter != fileNameIndexIt.second ; fileNameIndexITIter++)
		{
			sprintf(storedFileName,"%s/files/%d.meta",ndInfo.homeDir,fileNameIndexITIter->second);
			storedFileMetadata = createMetaDataFromFile(storedFileName);
			compareStatus = compareHexStrings(storedFileMetadata->SHA1val,recvdFileMetaData->SHA1val);
			if(compareStatus == 0)
				continue;
			compareStatus = compareHexStrings(storedFileMetadata->nonce,recvdFileMetaData->nonce);
			if(compareStatus == 0)
				continue;
			else
			{
				present = 1;
				break;
			}
		}
	}
	pthread_mutex_unlock(&fileNameIndexMapLock);
	return present;
}

int checkIfFileAlreadyPresentInPermList(struct fileMetadata *recvdFileMetaData)
{
	int present = 0;
	/*Check if fileName Match*/

	struct fileMetadata *storedFileMetadata = NULL;

	char storedFileName[256] = "\0";
	int compareStatus = 0;
	pthread_mutex_lock(&permFileListLock);
	for(PermFileListIter = PermFileList.begin();PermFileListIter != PermFileList.end(); PermFileListIter++)
	{
		sprintf(storedFileName,"%s/files/%d.meta",ndInfo.homeDir,*PermFileListIter);
		storedFileMetadata = createMetaDataFromFile(storedFileName);
		if(strcmp((char *)storedFileMetadata->fileName,(char *)recvdFileMetaData->fileName) != 0)
			continue;
		compareStatus = compareHexStrings(storedFileMetadata->SHA1val,recvdFileMetaData->SHA1val);
		if(compareStatus == 0)
			continue;
		compareStatus = compareHexStrings(storedFileMetadata->nonce,recvdFileMetaData->nonce);
		if(compareStatus == 0)
			continue;
		else
		{
			present = 1;
			break;
		}
	}
	pthread_mutex_unlock(&permFileListLock);
	return present;
}

void writeMetaDataToFile(struct fileMetadata *fileMeta,char *metaFile)
{
	int i = 0;
	unsigned char tempChar[2048]="\0";
	char CRLF[3]="\r\n";
	FILE * metaFP;
	char metaInit[]="[metadata]\n";

	metaFP = fopen(metaFile,"w+");

	if(metaFP == NULL){
		printf("\nError Creating %s Meta File\n",metaFile);
		return;
	}
	// Write [metadata]
	fwrite(metaInit,1,11,metaFP);

	//Write FileName + CRLF
	sprintf((char *)tempChar,"FileName=%s",fileMeta->fileName);
	fwrite(tempChar,1,strlen((char *)tempChar),metaFP);
	fwrite(CRLF,1,2,metaFP);

	//Write File Size + CRLF
	sprintf((char *)tempChar,"FileSize=%d",fileMeta->fileSize);
	fwrite(tempChar,1,strlen((char *)tempChar),metaFP);
	fwrite(CRLF,1,2,metaFP);

	//Write SHA of File + CRLF
	fprintf(metaFP,"%s","SHA1=");
	i=0;
	while(i<20){
		fprintf(metaFP,"%02x",fileMeta->SHA1val[i]);
		i++;
	}
	fwrite(CRLF,1,2,metaFP);

	//Write Nonce + CRLF
	fprintf(metaFP,"%s","Nonce=");
	i=0;
	while(i<20){
		fprintf(metaFP,"%02x",fileMeta->nonce[i]);
		i++;
	}
	fwrite(CRLF,1,2,metaFP);

	//Write keywords + CRLF
	fprintf(metaFP,"Keywords=%s",fileMeta->keywordsWithSpace);
	fwrite(CRLF,1,2,metaFP);

	//Write Bit Vector and CRLF
	fprintf(metaFP,"%s","Bit-vector=");
	i = 127;
	while(i>=0){
		fprintf(metaFP,"%02x",fileMeta->bitVector[i]);
		i--;
	}
	fwrite(CRLF,1,2,metaFP);

	fclose(metaFP);
}


void scanAndEraseMsgCache()
{
	struct message *mapmessage;
	time_t mysystime;
	uint16_t msgLifeTime = ndInfo.msgLifetime * 1.1;
	uint16_t getMsgLifeTime = ndInfo.getMsgLifeTime * 1.1;
	pthread_mutex_lock(&messageCacheMapLock);
	for(messageCacheMapIter = messageCacheMap.begin(); messageCacheMapIter != messageCacheMap.end(); messageCacheMapIter++)
	{
		mapmessage = messageCacheMapIter->second;
		mysystime = time(NULL);
		if(((mysystime - mapmessage->timestamp) >= msgLifeTime && mapmessage->messType != GET) ||
				((mysystime - mapmessage->timestamp) >=  getMsgLifeTime && mapmessage->messType == GET))
		{
			if(mapmessage->messType == HELLO){
				pthread_mutex_lock(&connectedNeighborsMapLock);
				string connectionMapKey = string(mapmessage->connectedNeighborsMapKey);
				connectedNeighborsMapIter = connectedNeighborsMap.find(connectionMapKey);
				if(connectedNeighborsMapIter != connectedNeighborsMap.end())
				{
					/*
					 * Tell reader and writer to exit as this connection is no longer
					 * useful
					 */
					struct connectionDetails *cd =connectedNeighborsMapIter->second;
					pthread_mutex_lock(&(cd->connectionLock));
					pthread_mutex_lock(&(cd->messagingQueueLock));
					cd->notOperational = 1;
					pthread_cond_signal(&(cd->messagingQueueCV));
					pthread_mutex_unlock(&(cd->messagingQueueLock));
					close(cd->socketDesc);
					pthread_mutex_unlock(&(cd->connectionLock));
					//pthread_mutex_unlock(&connectedNeighborsMapLock);
				}
				pthread_mutex_unlock(&connectedNeighborsMapLock);
			}
			// Clear Message Cache
			// Free Memory
			else if(mapmessage->sendStatus == 0){
				time_t temptime;
				char timechar[100] = "\0";
				time(&temptime);
				strcpy(timechar,ctime(&temptime));

				string tempUOID = string(timechar);
				messageCacheMap.erase(messageCacheMapIter->first);
				messageCacheMap.insert(MESSCACHEMAPTYPE::value_type(tempUOID,mapmessage));
			}
			else{
				// Clear Message Cache
				if(mapmessage->messType == SEARCH)
				{
					pthread_mutex_lock(&keyboardLock);
					pthread_cond_signal(&keyboardCV);
					pthread_mutex_unlock(&keyboardLock);
				}
				struct message *tempmsg = messageCacheMapIter->second;
				messageCacheMap.erase(messageCacheMapIter->first);
				freeMessage(tempmsg);
				tempmsg = NULL;
				// Free Memory
			}
		}
	}
	pthread_mutex_unlock(&messageCacheMapLock);
}

void scanAndEraseConnectedNeighborsMap(int nodetype)
{
	/*
	 * Check Connected neighbors map to delete any unused connections
	 * unused connections are: notOperational = 1 & threadsExitedCount = 2
	 */
	struct connectionDetails *cd;
	time_t mysystime;

	pthread_mutex_lock(&messageCacheMapLock);
		pthread_mutex_lock(&connectedNeighborsMapLock);
		for(connectedNeighborsMapIter = connectedNeighborsMap.begin(); connectedNeighborsMapIter != connectedNeighborsMap.end(); connectedNeighborsMapIter++)
		{
			mysystime = time(NULL);

			cd = connectedNeighborsMapIter->second;
			pthread_mutex_lock(&cd->connectionLock);

			//if node is Beacon and disconnected i.e not operational, we need to reconnect to it.
			if(nodetype == 1)
			{
				/*
				 * BEACON
				 */
				if(checkNodeType(cd->wellKnownPort) && (cd->tiebreak == 0) && (cd->helloStatus==1) &&
						((cd->notOperational == 0 && (cd->threadsExitedCount == 2 || cd->threadsExitedCount == 1)))){
					printf("\n\nLost Connection with a BEACON node hence reconnecting\n\n");
					writeCommentToLogFile((char *)"Lost Connection with a BEACON node hence reconnecting\n");
					printf("\nservant:%d> ",ndInfo.port);
					struct connectionDetails *cdTemp = connectedNeighborsMapIter->second;

					connectedNeighborsMap.erase(connectedNeighborsMapIter->first);
					close(connectedNeighborsMapIter->second->socketDesc);
					pthread_mutex_unlock(&cd->connectionLock);
					pthread_t ReconnectThread;
					pthread_create(&ReconnectThread,NULL,(void* (*)(void*))ReConnectToBeaconNode,(void *)cdTemp);
					//ReConnectToBeaconNode(cdTemp,connectedNeighborsMapIter->first);

				}
				else if((cd->notOperational == 1 && cd->threadsExitedCount >= 2) ||
						(cd->isJoinConnection == 1 && cd->threadsExitedCount >= 2) ||
						(cd->notOperational == 1 && cd->threadsExitedCount >= 2 &&
								!checkNodeType(cd->wellKnownPort)))
				{
					struct connectionDetails * tempcd = connectedNeighborsMapIter->second;
					close(cd->socketDesc);
					connectedNeighborsMap.erase(connectedNeighborsMapIter->first);
					// Free Memory
					freeConnectionDetails(tempcd);
				}
			}
			else
			{
				/*
				 * NON BEACON
				 */
				if((cd->notOperational == 1 && cd->threadsExitedCount == 2) && !checkNodeType(cd->wellKnownPort))
				{
					//no_of_notoper++;
					struct connectionDetails * tempcd = connectedNeighborsMapIter->second;
					close(cd->socketDesc);
					connectedNeighborsMap.erase(connectedNeighborsMapIter->first);
					freeConnectionDetails(tempcd);
					// Free Memory
					//printConnectedNeighborInforMap();
				}
				else if((cd->notOperational == 1 && cd->threadsExitedCount == 2 && cd->isJoinConnection == 1))
				{
					struct connectionDetails * tempcd = connectedNeighborsMapIter->second;
					close(cd->socketDesc);
					connectedNeighborsMap.erase(connectedNeighborsMapIter->first);
					// Free Memory
					freeConnectionDetails(tempcd);
					//printConnectedNeighborInforMap();
				}

			}

			pthread_mutex_unlock(&cd->connectionLock);
		}
		pthread_mutex_unlock(&connectedNeighborsMapLock);
	pthread_mutex_unlock(&messageCacheMapLock);
	//printConnectedNeighborInforMap();
}


void checkLastReadActivityOfConnection(int nodetype)
{
	/*
	 * Check the last Read Activity for the connection If last read activity exceeds the
	 * keepalive timeout then this connection is probably dead.
	 * 1. make this connection not operational
	 * 2. inform the reader and writer threads to exit
	 */
	struct connectionDetails *cd;
	time_t mysystime;
	uint16_t keepAliveTimeout = ndInfo.keepAliveTimeout;
	pthread_mutex_lock(&connectedNeighborsMapLock);
	for(connectedNeighborsMapIter = connectedNeighborsMap.begin(); connectedNeighborsMapIter != connectedNeighborsMap.end(); connectedNeighborsMapIter++)
	{
		mysystime = time(NULL);
		cd = connectedNeighborsMapIter->second;
		pthread_mutex_lock(&cd->connectionLock);
		if((mysystime - cd->lastReadActivity) >= keepAliveTimeout &&(cd->helloStatus == 1) &&
				(cd->notOperational == 0) && (cd->threadsExitedCount == 0))
		{
			if(!checkNodeType(cd->wellKnownPort))
				cd->notOperational = 1;
			pthread_mutex_lock(&cd->messagingQueueLock);
			pthread_cond_signal(&cd->messagingQueueCV);
			pthread_mutex_unlock(&cd->messagingQueueLock);
			close(cd->socketDesc);
			pthread_mutex_unlock(&cd->connectionLock);

			if(nodetype == 0 && checkSendFlag == 0)
			{
				/*
				 * I am a nonBeacon; Send a check message over all the remaining operational connections
				 * and wait for the response to check if I am connected to SERVANT network
				 */
				pthread_mutex_lock(&checkThreadLock);
				checkStatus = 1;
				pthread_cond_signal(&checkThreadCV);
				pthread_mutex_unlock(&checkThreadLock);
			}
		}
		else
		{
			pthread_mutex_unlock(&cd->connectionLock);
		}
	}
	pthread_mutex_unlock(&connectedNeighborsMapLock);
}

void checkLastWriteActivityOfConnection()
{
	/*
	 * Check the last Write Activity for the connection. If the last write activity exceeds the
	 * keepalive timeout then I have to send a KEEPALIVE message over this connection
	 * to inform the neighbor that the connection is still active
	 */
	struct connectionDetails *cd;
	time_t mysystime;
	struct message *mymessage;
	uint32_t msglenforkeelalive = 0;
	struct NodeInfo *ndInfoCommon = &ndInfo;
	uint16_t keepAliveTimeout = (ndInfo.keepAliveTimeout/2);
	pthread_mutex_lock(&connectedNeighborsMapLock);
	for(connectedNeighborsMapIter = connectedNeighborsMap.begin(); connectedNeighborsMapIter != connectedNeighborsMap.end(); connectedNeighborsMapIter++)
	{
		mysystime = time(NULL);
		//mysystime *= 1000000;
		cd = connectedNeighborsMapIter->second;
		pthread_mutex_lock(&cd->connectionLock);
		if((mysystime - cd->lastWriteActivity) >= keepAliveTimeout &&(cd->helloStatus == 1) &&
				(cd->notOperational == 0) && (cd->threadsExitedCount == 0))
		{
			mymessage = CreateMessageHeader(KEEPALIVE, /*Mess Type*/
					1, /* TTL */
					msglenforkeelalive, /*DataLength Message Body */
					NULL, /*char *messageHeader*/
					NULL, /*char *message*/
					(void*)NULL, /*void *messageStruct*/
					NULL,/*char* temp_connectedNeighborsMapKey*/
					MYMESSAGE, /* MyMessage or NotMyMessage*/
					ndInfoCommon /*Node Info Pointer*/);
			cd->lastWriteActivity = mysystime;

			pthread_mutex_lock(&cd->messagingQueueLock);
			cd->messagingQueue.push(mymessage);
			pthread_cond_signal(&cd->messagingQueueCV);
			pthread_mutex_unlock(&cd->messagingQueueLock);
		}
		pthread_mutex_unlock(&cd->connectionLock);
	}
	pthread_mutex_unlock(&connectedNeighborsMapLock);
}

void clearStatusMsgMap()
{
	pthread_mutex_lock(&statusMsgMapLock);
	for(statusMsgMapIter = statusMsgMap.begin(); statusMsgMapIter != statusMsgMap.end();statusMsgMapIter++)
	{
		statusMsgMap.erase(statusMsgMapIter->first);
	}
	pthread_mutex_unlock(&statusMsgMapLock);
}


bool writeInfoToStatusFile(statusRespMessage *statusRespMsgObj)
{
	bool retStatus = true;
	int i=0;
	int no_of_records = statusRespMsgObj->dataRecordCount;
	if(statusRespMsgObj->statusType == 0x01){
		STATUSMSGMAPTYPE::iterator statusMsgMapIterTemp;
		pthread_mutex_lock(&extFileLock);

		extFilePtr = fopen(extfilecmd,"a+");

		pthread_mutex_lock(&statusMsgMapLock);
		statusMsgMapIter = statusMsgMap.find(statusRespMsgObj->hostPort);
		if(statusMsgMapIter == statusMsgMap.end())
		{
			statusMsgMap.insert(STATUSMSGMAPTYPE::value_type(statusRespMsgObj->hostPort,1));
			fprintf(extFilePtr,"n -t * -s %d -c red -i black\n",statusRespMsgObj->hostPort);
		}
		/*
		 * write the nodes in the file
		 */
		for(i = 0; i < no_of_records; i++)
		{
			statusMsgMapIter = statusMsgMap.find(statusRespMsgObj->neighHostPorts[i]);
			if(statusMsgMapIter == statusMsgMap.end())
			{
				statusMsgMap.insert(STATUSMSGMAPTYPE::value_type(statusRespMsgObj->neighHostPorts[i],1));
				fprintf(extFilePtr,"n -t * -s %d -c red -i black\n",statusRespMsgObj->neighHostPorts[i]);
			}
		}

		/*
		 * write the links in the file
		 */
		for(i = 0; i < no_of_records; i++)
		{
			fprintf(extFilePtr,"l -t * -s %d -d %d -c blue\n",statusRespMsgObj->hostPort,statusRespMsgObj->neighHostPorts[i]);
		}
		pthread_mutex_unlock(&statusMsgMapLock);

		fclose(extFilePtr);
		pthread_mutex_unlock(&extFileLock);
	}
	else{
		pthread_mutex_lock(&extFileLock);
		extFilePtr = fopen(extfilecmd,"a+");

		char firstline[256]="\0";
		char metaInit[]="[metadata]\n";
		char CRLF[4]="\r\n";
		unsigned char tempChar[2048]="\0";
		if(no_of_records == 0){
			sprintf(firstline,"%s:%d has no file\r\n",statusRespMsgObj->hostName,statusRespMsgObj->hostPort);
		}else if(no_of_records == 1){
			sprintf(firstline,"%s:%d has following file\r\n",statusRespMsgObj->hostName,statusRespMsgObj->hostPort);
		}else{
			sprintf(firstline,"%s:%d has following files\r\n",statusRespMsgObj->hostName,statusRespMsgObj->hostPort);
		}
		fwrite(firstline,1,strlen(firstline),extFilePtr);
		int k=0;
		while(k < no_of_records){
				// Write [metadata]
				fwrite(metaInit,1,11,extFilePtr);

				//Write FileName + CRLF
				sprintf((char *)tempChar,"FileName=%s",statusRespMsgObj->fileMetadataObj[k]->fileName);
				fwrite(tempChar,1,strlen((char *)tempChar),extFilePtr);
				fwrite(CRLF,1,2,extFilePtr);

				//Write File Size + CRLF
				sprintf((char *)tempChar,"FileSize=%d",statusRespMsgObj->fileMetadataObj[k]->fileSize);
				fwrite(tempChar,1,strlen((char *)tempChar),extFilePtr);
				fwrite(CRLF,1,2,extFilePtr);

				//Write SHA of File + CRLF
				fprintf(extFilePtr,"%s","SHA1=");
				i=0;
				while(i<20){
					fprintf(extFilePtr,"%02x",statusRespMsgObj->fileMetadataObj[k]->SHA1val[i]);
					i++;
				}
				fwrite(CRLF,1,2,extFilePtr);

				//Write Nonce + CRLF
				fprintf(extFilePtr,"%s","Nonce=");
				i=0;
				while(i<20){
					fprintf(extFilePtr,"%02x",statusRespMsgObj->fileMetadataObj[k]->nonce[i]);
					i++;
				}
				fwrite(CRLF,1,2,extFilePtr);

				//Write keywords + CRLF
				fprintf(extFilePtr,"Keywords=%s",statusRespMsgObj->fileMetadataObj[k]->keywordsWithSpace);
				fwrite(CRLF,1,2,extFilePtr);

				//Write Bit Vector and CRLF
				fprintf(extFilePtr,"%s","Bit-vector=");
				i = 127;
				while(i>=0){
					fprintf(extFilePtr,"%02x",statusRespMsgObj->fileMetadataObj[k]->bitVector[i]);
					i--;
				}
				fwrite(CRLF,1,2,extFilePtr);
				k++;
		}
		fclose(extFilePtr);
		pthread_mutex_unlock(&extFileLock);
	}
	return retStatus;
}

void writeErrorToLogFile(char *info){
	pthread_mutex_lock(&logFileLock);
		fprintf(logFilePtr,"**%s\n",info);
	pthread_mutex_unlock(&logFileLock);
	return;
}

void writeCommentToLogFile(char *info){
	pthread_mutex_lock(&logFileLock);
		fprintf(logFilePtr,"//%s\n",info);
	pthread_mutex_unlock(&logFileLock);
	return;
}


bool writeToLogFile(struct message *ptr, char *nodeId){

	if(logFilePtr == NULL)
		return false;

	gettimeofday(&ptr->logTime, NULL);
	pthread_mutex_lock(&logFileLock);
	switch(ptr -> messType){

		case JOIN:
					{
						struct joinMessage *joinMessageObj = (struct joinMessage *)ptr->messageStruct;
						if(ptr->howMessageFormed == 'r'){
							  // r <time> <from> <msgtype> <size> <ttl> <msgid> <data>
							fprintf(logFilePtr,"%c %10ld.%03d %s JNRQ %d %d %02x%02x%02x%02x %d %s\n",
									ptr->howMessageFormed,
									ptr->logTime.tv_sec,(int)(ptr->logTime.tv_usec/1000),nodeId,
									HEADER_LENGTH + ptr->dataLength,
									ptr->ttl,
									ptr->uoid[16],ptr->uoid[17],ptr->uoid[18],ptr->uoid[19],
									joinMessageObj->hostPort,joinMessageObj->hostName);
						}
						else{
							  // f/s <time> <from> <msgtype> <size> <ttl> <msgid> <data>
							fprintf(logFilePtr,"%c %10ld.%03d %s JNRQ %d %d %02x%02x%02x%02x %d %s\n",
									ptr->howMessageFormed,
									ptr->logTime.tv_sec,(int)(ptr->logTime.tv_usec/1000),nodeId,
									HEADER_LENGTH + ptr->dataLength,
									ptr->ttl,
									ptr->uoid[16],ptr->uoid[17],ptr->uoid[18],ptr->uoid[19],
									joinMessageObj->hostPort,joinMessageObj->hostName);
						}

					}
					break;
		case RJOIN:
					{
						struct joinRespMessage *joinRespMessageObj = (struct joinRespMessage *)ptr->messageStruct;
						if(ptr->howMessageFormed == 'r'){
							fprintf(logFilePtr,"%c %10ld.%03d %s JNRS %d %d %02x%02x%02x%02x %02x%02x%02x%02x %d %d %s\n",
							ptr->howMessageFormed,
							ptr->logTime.tv_sec,(int)(ptr->logTime.tv_usec/1000),nodeId,
							HEADER_LENGTH + ptr->dataLength,
							ptr->ttl,
							ptr->uoid[16],ptr->uoid[17],ptr->uoid[18],ptr->uoid[19],
							joinRespMessageObj->uoid[16],joinRespMessageObj->uoid[17],joinRespMessageObj->uoid[18],joinRespMessageObj->uoid[19],
							joinRespMessageObj->distance,joinRespMessageObj->hostPort,joinRespMessageObj->hostName);
						}
						else{
							fprintf(logFilePtr,"%c %10ld.%03d %s JNRS %d %d %02x%02x%02x%02x %02x%02x%02x%02x %d %d %s\n",
							ptr->howMessageFormed,
							ptr->logTime.tv_sec,(int)(ptr->logTime.tv_usec/1000),nodeId,
							HEADER_LENGTH + ptr->dataLength,
							ptr->ttl,
							ptr->uoid[16],ptr->uoid[17],ptr->uoid[18],ptr->uoid[19],
							joinRespMessageObj->uoid[16],joinRespMessageObj->uoid[17],joinRespMessageObj->uoid[18],joinRespMessageObj->uoid[19],
							joinRespMessageObj->distance,joinRespMessageObj->hostPort,joinRespMessageObj->hostName);
						}

					}
					break;

		case HELLO:
					{
						struct helloMessage *helloMessageObj = (struct helloMessage *)ptr->messageStruct;
						if(ptr->howMessageFormed == 'r'){
							fprintf(logFilePtr,"%c %10ld.%03d %s HLLO %d %d %02x%02x%02x%02x %d %s\n",
							ptr->howMessageFormed,
							ptr->logTime.tv_sec,(int)(ptr->logTime.tv_usec/1000),nodeId,
							HEADER_LENGTH + ptr->dataLength,
							ptr->ttl,
							ptr->uoid[16],ptr->uoid[17],ptr->uoid[18],ptr->uoid[19],
							helloMessageObj->hostPort, helloMessageObj->hostName);
						}
						else{
							fprintf(logFilePtr,"%c %10ld.%03d %s HLLO %d %d %02x%02x%02x%02x %d %s\n",
							ptr->howMessageFormed,
							ptr->logTime.tv_sec,(int)(ptr->logTime.tv_usec/1000),nodeId,
							HEADER_LENGTH + ptr->dataLength,
							ptr->ttl,
							ptr->uoid[16],ptr->uoid[17],ptr->uoid[18],ptr->uoid[19],
							helloMessageObj->hostPort, helloMessageObj->hostName);
						}
					}
					break;

		case KEEPALIVE:
					{
						if(ptr->howMessageFormed == 'r'){
							fprintf(logFilePtr,"%c %10ld.%03d %s KPAV %d %d %02x%02x%02x%02x\n",
							ptr->howMessageFormed,
							ptr->logTime.tv_sec,(int)(ptr->logTime.tv_usec/1000),nodeId,
							HEADER_LENGTH + ptr->dataLength,
							ptr->ttl,
							ptr->uoid[16],ptr->uoid[17],ptr->uoid[18],ptr->uoid[19]);
						}
						else{
							fprintf(logFilePtr,"%c %10ld.%03d %s KPAV %d %d %02x%02x%02x%02x\n",
							ptr->howMessageFormed,
							ptr->logTime.tv_sec,(int)(ptr->logTime.tv_usec/1000),nodeId,
							HEADER_LENGTH + ptr->dataLength,
							ptr->ttl,
							ptr->uoid[16],ptr->uoid[17],ptr->uoid[18],ptr->uoid[19]);
						}
					}
					break;

		case NOTIFY:
					{
						struct notifyMessage* notifyMessageObj = (struct notifyMessage*)ptr->messageStruct;
						if(ptr->howMessageFormed == 'r'){
							fprintf(logFilePtr,"%c %10ld.%03d %s NTFY %d %d %02x%02x%02x%02x %d\n",
							ptr->howMessageFormed,
							ptr->logTime.tv_sec,(int)(ptr->logTime.tv_usec/1000),nodeId,
							HEADER_LENGTH + ptr->dataLength,
							ptr->ttl,
							ptr->uoid[16],ptr->uoid[17],ptr->uoid[18],ptr->uoid[19],
							notifyMessageObj->errorCode);
						}
						else{
							fprintf(logFilePtr,"%c %10ld.%03d %s NTFY %d %d %02x%02x%02x%02x %d\n",
							ptr->howMessageFormed,
							ptr->logTime.tv_sec,(int)(ptr->logTime.tv_usec/1000),nodeId,
							HEADER_LENGTH + ptr->dataLength,
							ptr->ttl,
							ptr->uoid[16],ptr->uoid[17],ptr->uoid[18],ptr->uoid[19],
							notifyMessageObj->errorCode);
						}
					}

					break;

		case CHECK:
					{
						if(ptr->howMessageFormed == 'r'){
							fprintf(logFilePtr,"%c %10ld.%03d %s CKRQ %d %d %02x%02x%02x%02x\n",
							ptr->howMessageFormed,
							ptr->logTime.tv_sec,(int)(ptr->logTime.tv_usec/1000),nodeId,
							HEADER_LENGTH + ptr->dataLength,
							ptr->ttl,
							ptr->uoid[16],ptr->uoid[17],ptr->uoid[18],ptr->uoid[19]);
						}
						else{
							fprintf(logFilePtr,"%c %10ld.%03d %s CKRQ %d %d %02x%02x%02x%02x\n",
							ptr->howMessageFormed,
							ptr->logTime.tv_sec,(int)(ptr->logTime.tv_usec/1000),nodeId,
							HEADER_LENGTH + ptr->dataLength,
							ptr->ttl,
							ptr->uoid[16],ptr->uoid[17],ptr->uoid[18],ptr->uoid[19]);
						}
					}
					break;

		case RCHECK:
					{
						struct checkRespMessage* checkResponseMessageObj = (struct checkRespMessage*)ptr->messageStruct;
						if(ptr->howMessageFormed == 'r'){
							fprintf(logFilePtr,"%c %10ld.%03d %s CKRS %d %d %02x%02x%02x%02x %02x%02x%02x%02x\n",
							ptr->howMessageFormed,
							ptr->logTime.tv_sec,(int)(ptr->logTime.tv_usec/1000),nodeId,
							HEADER_LENGTH + ptr->dataLength,
							ptr->ttl,
							ptr->uoid[16],ptr->uoid[17],ptr->uoid[18],ptr->uoid[19],
							checkResponseMessageObj->checkMsgHeaderUOID[16],
							checkResponseMessageObj->checkMsgHeaderUOID[17],
							checkResponseMessageObj->checkMsgHeaderUOID[18],
							checkResponseMessageObj->checkMsgHeaderUOID[19]);
						}
						else{
							fprintf(logFilePtr,"%c %10ld.%03d %s CKRS %d %d %02x%02x%02x%02x %02x%02x%02x%02x\n",
							ptr->howMessageFormed,
							ptr->logTime.tv_sec,(int)(ptr->logTime.tv_usec/1000),nodeId,
							HEADER_LENGTH + ptr->dataLength,
							ptr->ttl,
							ptr->uoid[16],ptr->uoid[17],ptr->uoid[18],ptr->uoid[19],
							checkResponseMessageObj->checkMsgHeaderUOID[16],
							checkResponseMessageObj->checkMsgHeaderUOID[17],
							checkResponseMessageObj->checkMsgHeaderUOID[18],
							checkResponseMessageObj->checkMsgHeaderUOID[19]);
						}
					}

					break;

		case STATUS:
					{
						struct statusMessage* statusMessageObj = (struct statusMessage*)ptr->messageStruct;

						if(statusMessageObj->statusType == 0x01){

							if(ptr -> howMessageFormed == 'r'){
								fprintf(logFilePtr,"%c %10ld.%03d %s STRQ %d %d %02x%02x%02x%02x %s\n",
								ptr->howMessageFormed,
								ptr->logTime.tv_sec,(int)(ptr->logTime.tv_usec/1000),nodeId,
								HEADER_LENGTH + ptr->dataLength,
								ptr->ttl,
								ptr->uoid[16],ptr->uoid[17],ptr->uoid[18],ptr->uoid[19],
								"neighbors");
							}
							else{
								fprintf(logFilePtr,"%c %10ld.%03d %s STRQ %d %d %02x%02x%02x%02x %s\n",
								ptr->howMessageFormed,
								ptr->logTime.tv_sec,(int)(ptr->logTime.tv_usec/1000),nodeId,
								HEADER_LENGTH + ptr->dataLength,
								ptr->ttl,
								ptr->uoid[16],ptr->uoid[17],ptr->uoid[18],ptr->uoid[19],
								"neighbors");
							}
						}
						else if(statusMessageObj->statusType == 0x02){

							if(ptr -> howMessageFormed == 'r'){
								fprintf(logFilePtr,"%c %10ld.%03d %s STRQ %d %d %02x%02x%02x%02x %s\n",
								ptr->howMessageFormed,
								ptr->logTime.tv_sec,(int)(ptr->logTime.tv_usec/1000),nodeId,
								HEADER_LENGTH + ptr->dataLength,
								ptr->ttl,
								ptr->uoid[16],ptr->uoid[17],ptr->uoid[18],ptr->uoid[19],
								"files");
							}
							else{
								fprintf(logFilePtr,"%c %10ld.%03d %s STRQ %d %d %02x%02x%02x%02x %s\n",
								ptr->howMessageFormed,
								ptr->logTime.tv_sec,(int)(ptr->logTime.tv_usec/1000),nodeId,
								HEADER_LENGTH + ptr->dataLength,
								ptr->ttl,
								ptr->uoid[16],ptr->uoid[17],ptr->uoid[18],ptr->uoid[19],
								"files");
							}
						}
					}
					break;

		case RSTATUS:
					{

						struct statusRespMessage* statusResponseMessageObj = (struct statusRespMessage*)ptr->messageStruct;
						if(ptr->howMessageFormed == 'r'){
							fprintf(logFilePtr,"%c %10ld.%03d %s STRS %d %d %02x%02x%02x%02x %02x%02x%02x%02x\n",
							ptr->howMessageFormed,
							ptr->logTime.tv_sec,(int)(ptr->logTime.tv_usec/1000),nodeId,
							HEADER_LENGTH + ptr->dataLength,
							ptr->ttl,
							ptr->uoid[16],ptr->uoid[17],ptr->uoid[18],ptr->uoid[19],
							statusResponseMessageObj->statusMsgHeaderUOID[16],
							statusResponseMessageObj->statusMsgHeaderUOID[17],
							statusResponseMessageObj->statusMsgHeaderUOID[18],
							statusResponseMessageObj->statusMsgHeaderUOID[19]);
						}
						else{
							fprintf(logFilePtr,"%c %10ld.%03d %s STRS %d %d %02x%02x%02x%02x %02x%02x%02x%02x\n",
							ptr->howMessageFormed,
							ptr->logTime.tv_sec,(int)(ptr->logTime.tv_usec/1000),nodeId,
							HEADER_LENGTH + ptr->dataLength,
							ptr->ttl,
							ptr->uoid[16],ptr->uoid[17],ptr->uoid[18],ptr->uoid[19],
							statusResponseMessageObj->statusMsgHeaderUOID[16],
							statusResponseMessageObj->statusMsgHeaderUOID[17],
							statusResponseMessageObj->statusMsgHeaderUOID[18],
							statusResponseMessageObj->statusMsgHeaderUOID[19]);
						}
					}
					break;
		// Project Part 2

		case SEARCH:
					{
						struct searchMessage* searchMessageObj = (struct searchMessage*)ptr->messageStruct;
						char searchType[][50] = {
												"filename",
												"sha1hash",
												"keywords"
												};

						if(ptr->howMessageFormed == 'r'){
							fprintf(logFilePtr,"%c %10ld.%03d %s SHRQ %d %d %02x%02x%02x%02x %s %s\n",
									ptr->howMessageFormed,
									ptr->logTime.tv_sec,(int)(ptr->logTime.tv_usec/1000),nodeId,
									HEADER_LENGTH + ptr->dataLength,
									ptr->ttl,
									ptr->uoid[16],ptr->uoid[17],ptr->uoid[18],ptr->uoid[19],
									searchType[searchMessageObj->searchType-1],
									searchMessageObj->query);
						}
						else{
							fprintf(logFilePtr,"%c %10ld.%03d %s SHRQ %d %d %02x%02x%02x%02x %s %s\n",
									ptr->howMessageFormed,
									ptr->logTime.tv_sec,(int)(ptr->logTime.tv_usec/1000),nodeId,
									HEADER_LENGTH + ptr->dataLength,
									ptr->ttl,
									ptr->uoid[16],ptr->uoid[17],ptr->uoid[18],ptr->uoid[19],
									searchType[searchMessageObj->searchType-1],
									searchMessageObj->query);
						}

					}
					break;

		case RSEARCH:
					{
						struct searchRespMessage* searchRespMessageObj = (struct searchRespMessage*)ptr->messageStruct;
						if(ptr->howMessageFormed == 'r'){
							fprintf(logFilePtr,"%c %10ld.%03d %s SHRS %d %d %02x%02x%02x%02x %02x%02x%02x%02x\n",
									ptr->howMessageFormed,
									ptr->logTime.tv_sec,(int)(ptr->logTime.tv_usec/1000),nodeId,
									HEADER_LENGTH + ptr->dataLength,
									ptr->ttl,
									ptr->uoid[16],ptr->uoid[17],ptr->uoid[18],ptr->uoid[19],
									searchRespMessageObj->searchUOID[16],
									searchRespMessageObj->searchUOID[17],
									searchRespMessageObj->searchUOID[18],
									searchRespMessageObj->searchUOID[19]);
						}
						else{
							fprintf(logFilePtr,"%c %10ld.%03d %s SHRS %d %d %02x%02x%02x%02x %02x%02x%02x%02x\n",
									ptr->howMessageFormed,
									ptr->logTime.tv_sec,(int)(ptr->logTime.tv_usec/1000),nodeId,
									HEADER_LENGTH + ptr->dataLength,
									ptr->ttl,
									ptr->uoid[16],ptr->uoid[17],ptr->uoid[18],ptr->uoid[19],
									searchRespMessageObj->searchUOID[16],
									searchRespMessageObj->searchUOID[17],
									searchRespMessageObj->searchUOID[18],
									searchRespMessageObj->searchUOID[19]);
						}
					}
					break;

		case DELETE:
					{
						if(ptr->howMessageFormed == 'r'){
							fprintf(logFilePtr,"%c %10ld.%03d %s DELT %d %d %02x%02x%02x%02x\n",
									ptr->howMessageFormed,
									ptr->logTime.tv_sec,(int)(ptr->logTime.tv_usec/1000),nodeId,
									HEADER_LENGTH + ptr->dataLength,
									ptr->ttl,
									ptr->uoid[16],ptr->uoid[17],ptr->uoid[18],ptr->uoid[19]);
						}
						else{
							fprintf(logFilePtr,"%c %10ld.%03d %s DELT %d %d %02x%02x%02x%02x\n",
									ptr->howMessageFormed,
									ptr->logTime.tv_sec,(int)(ptr->logTime.tv_usec/1000),nodeId,
									HEADER_LENGTH + ptr->dataLength,
									ptr->ttl,
									ptr->uoid[16],ptr->uoid[17],ptr->uoid[18],ptr->uoid[19]);
						}
					}
					break;

		case STORE:
					{
						if(ptr->howMessageFormed == 'r'){
							// r <time> <from> <msgtype> <size> <ttl> <msgid>
							fprintf(logFilePtr,"%c %10ld.%03d %s STOR %d %d %02x%02x%02x%02x\n",
									ptr->howMessageFormed,
									ptr->logTime.tv_sec,(int)(ptr->logTime.tv_usec/1000),nodeId,
									HEADER_LENGTH + ptr->dataLength,
									ptr->ttl,
									ptr->uoid[16],ptr->uoid[17],ptr->uoid[18],ptr->uoid[19]);
						}
						else{
							// f/s <time> <from> <msgtype> <size> <ttl> <msgid>
							fprintf(logFilePtr,"%c %10ld.%03d %s STOR %d %d %02x%02x%02x%02x\n",
									ptr->howMessageFormed,
									ptr->logTime.tv_sec,(int)(ptr->logTime.tv_usec/1000),nodeId,
									HEADER_LENGTH + ptr->dataLength,
									ptr->ttl,
									ptr->uoid[16],ptr->uoid[17],ptr->uoid[18],ptr->uoid[19]);
						}
					}
					break;

		case GET:
					{
						struct getMessage* getMessageObj = (struct getMessage*)ptr->messageStruct;
						if(ptr->howMessageFormed == 'r'){
							fprintf(logFilePtr,"%c %10ld.%03d %s GTRQ %d %d %02x%02x%02x%02x %02x%02x%02x%02x\n",
									ptr->howMessageFormed,
									ptr->logTime.tv_sec,(int)(ptr->logTime.tv_usec/1000),nodeId,
									HEADER_LENGTH + ptr->dataLength,
									ptr->ttl,
									ptr->uoid[16],ptr->uoid[17],ptr->uoid[18],ptr->uoid[19],
									getMessageObj->fileUOID[16],
									getMessageObj->fileUOID[17],
									getMessageObj->fileUOID[18],
									getMessageObj->fileUOID[19]);
						}
						else{
							fprintf(logFilePtr,"%c %10ld.%03d %s GTRQ %d %d %02x%02x%02x%02x %02x%02x%02x%02x\n",
									ptr->howMessageFormed,
									ptr->logTime.tv_sec,(int)(ptr->logTime.tv_usec/1000),nodeId,
									HEADER_LENGTH + ptr->dataLength,
									ptr->ttl,
									ptr->uoid[16],ptr->uoid[17],ptr->uoid[18],ptr->uoid[19],
									getMessageObj->fileUOID[16],
									getMessageObj->fileUOID[17],
									getMessageObj->fileUOID[18],
									getMessageObj->fileUOID[19]);
						}

					}
					break;

		case RGET:
					{
						struct getRespMessage* getRespMessageObj = (struct getRespMessage*)ptr->messageStruct;
						if(ptr->howMessageFormed == 'r'){
							fprintf(logFilePtr,"%c %10ld.%03d %s GTRS %d %d %02x%02x%02x%02x %02x%02x%02x%02x\n",
									ptr->howMessageFormed,
									ptr->logTime.tv_sec,(int)(ptr->logTime.tv_usec/1000),nodeId,
									HEADER_LENGTH + ptr->dataLength,
									ptr->ttl,
									ptr->uoid[16],ptr->uoid[17],ptr->uoid[18],ptr->uoid[19],
									getRespMessageObj->getUOID[16],
									getRespMessageObj->getUOID[17],
									getRespMessageObj->getUOID[18],
									getRespMessageObj->getUOID[19]);
						}
						else{
							fprintf(logFilePtr,"%c %10ld.%03d %s GTRS %d %d %02x%02x%02x%02x %02x%02x%02x%02x\n",
									ptr->howMessageFormed,
									ptr->logTime.tv_sec,(int)(ptr->logTime.tv_usec/1000),nodeId,
									HEADER_LENGTH + ptr->dataLength,
									ptr->ttl,
									ptr->uoid[16],ptr->uoid[17],ptr->uoid[18],ptr->uoid[19],
									getRespMessageObj->getUOID[16],
									getRespMessageObj->getUOID[17],
									getRespMessageObj->getUOID[18],
									getRespMessageObj->getUOID[19]);
						}
					}
					break;


		default:
				pthread_mutex_unlock(&logFileLock);
				return false;

	}
	fflush(logFilePtr);
	pthread_mutex_unlock(&logFileLock);
	return true;


}

bool handleSelfMessages(struct message *myMessage){
	struct connectionDetails *connectionDetailsObj;
	
	pthread_mutex_lock(&connectedNeighborsMapLock);

	connectedNeighborsMapIter = connectedNeighborsMap.begin();
	if(connectedNeighborsMapIter == connectedNeighborsMap.end()){

		pthread_mutex_unlock(&connectedNeighborsMapLock);
		//Handle Error - No Key-Value Found
		return false;
	}
	else{
		while(connectedNeighborsMapIter != connectedNeighborsMap.end()){
			connectionDetailsObj = connectedNeighborsMapIter -> second;
			
			pthread_mutex_lock(&connectionDetailsObj -> messagingQueueLock);
			connectionDetailsObj -> messagingQueue.push(myMessage);
			pthread_cond_signal(&connectionDetailsObj -> messagingQueueCV);
			pthread_mutex_unlock(&connectionDetailsObj -> messagingQueueLock);
			connectedNeighborsMapIter++;
		}
		pthread_mutex_unlock(&connectedNeighborsMapLock);
		return true;
	}
}

bool sendMessageToNode(struct message *myMessage){
	struct connectionDetails *connectionDetailsObj;

	string connectedNeighborsMapKeyTemp = string(myMessage -> connectedNeighborsMapKey);
	pthread_mutex_lock(&connectedNeighborsMapLock);

	connectedNeighborsMapIter = connectedNeighborsMap.find(connectedNeighborsMapKeyTemp);
	if(connectedNeighborsMapIter == connectedNeighborsMap.end()){

		pthread_mutex_unlock(&connectedNeighborsMapLock);
		//Handle Error - No Key-Value Found
		return false;
	}
	else{
		//while(connectedNeighborsMapIter != connectedNeighborsMap.end()){
			connectionDetailsObj = connectedNeighborsMapIter -> second;
			//pthread_mutex_lock(&connectionDetailsObj->connectionLock);
			pthread_mutex_lock(&connectionDetailsObj -> messagingQueueLock);
			connectionDetailsObj -> messagingQueue.push(myMessage);
			pthread_cond_signal(&connectionDetailsObj -> messagingQueueCV);
			pthread_mutex_unlock(&connectionDetailsObj -> messagingQueueLock);
			//pthread_mutex_unlock(&connectionDetailsObj->connectionLock);
		//}
		pthread_mutex_unlock(&connectedNeighborsMapLock);
		return true;
	}
}

void removeConnectedNeighborInfo(char *keyToMap)
{
	string key = string(keyToMap);
	pthread_mutex_lock(&connectedNeighborsMapLock);
	connectedNeighborsMapIter = connectedNeighborsMap.find(key);
	if(connectedNeighborsMapIter != connectedNeighborsMap.end())
	{
		//The connection details are present in the map so delete the entry
		//connectionDetails *cd = connectedNeighborsMapIter->second;
		//if(cd->toBeDeleted == 1)
		{

			//If reader/ Writer has marked the entry to be deleted I will delete it
			//close(cd->socketDesc);
			//free(cd);
			//connectedNeighborsMap.erase(key);
		}
		//else
		{

			//If not then I will indicate that the entry can be deleted as I am Exiting
			//cd->toBeDeleted = 1;
			//connectedNeighborsMap.insert(CONNMAPTYPE::value_type(key,cd));
		}
	}
	else
	{

	}
	pthread_mutex_unlock(&connectedNeighborsMapLock);
}


void ReConnectToBeaconNode(void *param){

	int status = 0;
	time_t mysystime;

	struct NodeInfo *nodeInfoCommon = &ndInfo;
	struct hostent *beacon;

	struct connectionDetails* cdTemp = (struct connectionDetails *)param;

	uint16_t retryTimer = 0;

	writeCommentToLogFile((char *)"Trying To Reconnect To Beacon");
	while(1){

			sleep(retryTimer);
			retryTimer = nodeInfoCommon->retry;
			int neighborbeaconSock=0;
			if((neighborbeaconSock = socket(AF_INET,SOCK_STREAM,0)) == -1)
			{
				printf("\nError creating socket\n");
				continue;
			}
			struct sockaddr_in serv_addr;
			char beaconip[16];
			serv_addr.sin_family = AF_INET;

			char char_key[100] = "\0";
			sprintf(char_key,"%s%d",cdTemp->hostname,cdTemp->wellKnownPort);
			string key = string(char_key);

			//Get the Peer information
			beacon = gethostbyname(cdTemp->hostname);
			memset(beaconip,0,16);
			strcpy(beaconip,inet_ntoa(*((struct in_addr *)beacon->h_addr_list[0])));
			free(beacon);

			serv_addr.sin_addr.s_addr = inet_addr(beaconip);
			serv_addr.sin_port = htons(cdTemp->wellKnownPort);

			//Send connection request
			status = connect(neighborbeaconSock,(struct sockaddr *)&serv_addr,sizeof(serv_addr));
			// connect to the host and port specified in
			if (status < 0)
			{
				free(beacon);
				continue;
			}
			else
			{
				connectionDetails *cd = new connectionDetails;

				//Get Peer information
				socklen_t len;
				struct sockaddr_storage addr;
				char ipstr[16];
				int port;
				len = sizeof(addr);
				getpeername(neighborbeaconSock, (struct sockaddr*)&addr, &len);
				struct sockaddr_in *s = (struct sockaddr_in *)&addr;
				port = ntohs(s->sin_port);
				inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof ipstr);

				//Prepare the connection details
				strcpy(cd->hostname,cdTemp->hostname);
				cd->port = cdTemp->wellKnownPort;
				cd->socketDesc = neighborbeaconSock;
				cd->notOperational=0;
				cd->isJoinConnection = 0;
				cd->threadsExitedCount = 0;
				cd->wellKnownPort = cdTemp->wellKnownPort;
				mysystime = time(NULL);
				cd->lastReadActivity = mysystime;
				cd->lastWriteActivity = mysystime;
				cd->helloStatus = 0;
				cd->tiebreak = 0;
				pthread_mutex_init(&cd->connectionLock,NULL);
				pthread_cond_init(&cd->messagingQueueCV, NULL);
				pthread_mutex_init(&cd->messagingQueueLock,NULL);

				//connectedNeighborsMap.erase(keyTemp);

				pthread_mutex_lock(&connectedNeighborsMapLock);
				freeConnectionDetails(cdTemp);
				cdTemp = NULL;
				connectedNeighborsMap.insert(CONNMAPTYPE::value_type(key,cd));
				pthread_mutex_unlock(&connectedNeighborsMapLock);

				char *mapkey = (char *)malloc(100);
				memset(mapkey,0,100);
				strcpy(mapkey,char_key);

				//Create reader and writer threads and give them the connection key
				pthread_t childThread1;
				pthread_create(&childThread1,NULL,(void* (*)(void*))reader,mapkey);


				pthread_t childThread2;
				pthread_create(&childThread2,NULL,(void* (*)(void*))writer,mapkey);

				//Create a hello message to be sent to all the beacons
				struct message *mymessage;
				struct helloMessage *myhelloMsg;

				myhelloMsg = createHello(nodeInfoCommon->hostname,nodeInfoCommon->port);

				mymessage = CreateMessageHeader(HELLO, /*Message Type */
												1, /* TTL */
												myhelloMsg -> dataLength, /*data body length*/
												NULL, /*char *messageHeader*/
												NULL, /*char *message*/
												(void*)myhelloMsg, /*void *messageStruct*/
												mapkey,/*char* temp_connectedNeighborsMapKey*/
												MYMESSAGE, /*My Message or notmymessage*/
												nodeInfoCommon /*Node Info Pointer*/);


				char keytype[21] = "\0";
				int i = 0;
				while(i<20)
				{
					keytype[i] = *(mymessage->uoid + i);
					*(cd->hellomsguoid + i) = *(mymessage->uoid + i);
					i++;
				}
				string uoidKey = string(keytype);
				cd->hellomsguoid[i] = '\0';
				//Add Hello Message to Cache
				pthread_mutex_lock(&messageCacheMapLock);
				messageCacheMap.insert(MESSCACHEMAPTYPE::value_type(uoidKey,mymessage));
				pthread_mutex_unlock(&messageCacheMapLock);

				//Create messsage header and add msg body created above
				pthread_mutex_lock(&helloMsgMapLock);
				helloMsgMap.insert(HELLOMSGMAPTYPE::value_type(mapkey,1));
				pthread_mutex_unlock(&helloMsgMapLock);
				//Call the function that sends the message to all the connections
				sendMessageToNode(mymessage);

				pthread_exit(NULL);
			}
	}
}

/*
 *  Free Memory Data Structures
 */

void freeMessage(struct message *ptr){
		if(ptr==NULL)
			return;

		if(ptr!= NULL){

			switch(ptr->messType){
				case JOIN:
				{
					struct joinMessage* generalPtr = (struct joinMessage*)ptr ->messageStruct;
					if(generalPtr != NULL)
						free(generalPtr);
					ptr ->messageStruct = NULL;
					generalPtr = NULL;
				}
				break;
				case RJOIN:
				{
					struct joinRespMessage* generalPtr = (struct joinRespMessage*) ptr ->messageStruct;
					if(generalPtr != NULL)
						free(generalPtr);
					ptr ->messageStruct = NULL;
					generalPtr = NULL;
				}
				break;

				case HELLO:
				{
					struct helloMessage* generalPtr = (struct helloMessage*) ptr ->messageStruct;
					if(generalPtr != NULL)
						free(generalPtr);
					ptr ->messageStruct = NULL;
					generalPtr = NULL;


				}
				break;
				case RCHECK:
				{
					struct checkRespMessage* generalPtr = (struct checkRespMessage*) ptr ->messageStruct;
					if(generalPtr != NULL)
						free(generalPtr);
					ptr ->messageStruct = NULL;
					generalPtr = NULL;
				}
				break;

				case STATUS:
				{

					struct statusMessage* generalPtr = (struct statusMessage*) ptr ->messageStruct;
					if(generalPtr != NULL)
						free(generalPtr);
					ptr ->messageStruct = NULL;
					generalPtr = NULL;
				}
				break;

				case RSTATUS:
				{

					struct statusRespMessage* generalPtr = (struct statusRespMessage*) ptr ->messageStruct;
					if(generalPtr != NULL)
						free(generalPtr);
					ptr ->messageStruct = NULL;
					generalPtr = NULL;
				}
				break;

				case SEARCH:
				{

					struct searchMessage* generalPtr = (struct searchMessage*) ptr ->messageStruct;
					if(generalPtr != NULL)
						free(generalPtr);
					ptr->messageStruct = NULL;
					generalPtr = NULL;
				}
				break;

				case RSEARCH:
				{

					struct searchRespMessage* generalPtr = (struct searchRespMessage*) ptr ->messageStruct;
					int i = 0;
					while(i < generalPtr->respCount){
						if(generalPtr->respMetadata[i] != NULL){
							free(generalPtr->respMetadata[i]);
							generalPtr->respMetadata[i] = NULL;
						}
						i++;
					}
					if(generalPtr != NULL)
						free(generalPtr);
					ptr->messageStruct = NULL;
					generalPtr = NULL;
				}
				break;

				case DELETE:
				{

					struct deleteMessage* generalPtr = (struct deleteMessage*) ptr ->messageStruct;
					if(generalPtr != NULL)
						free(generalPtr);
					ptr->messageStruct = NULL;
					generalPtr = NULL;
				}
				break;

				case STORE:
				{

					struct storeMessage* generalPtr = (struct storeMessage*) ptr ->messageStruct;
					if(generalPtr->respMetadata != NULL){
						free(generalPtr->respMetadata);
						generalPtr->respMetadata = NULL;
					}

					if(generalPtr != NULL)
						free(generalPtr);
					ptr->messageStruct = NULL;
					generalPtr = NULL;
				}
				break;

				case GET:
				{

					struct getMessage* generalPtr = (struct getMessage*) ptr ->messageStruct;

					if(generalPtr != NULL)
						free(generalPtr);
					ptr->messageStruct = NULL;
					generalPtr = NULL;
				}
				break;

				case RGET:
				{

					struct getRespMessage* generalPtr = (struct getRespMessage*) ptr ->messageStruct;

					if(generalPtr->respMetadata != NULL){
						free(generalPtr->respMetadata);
						generalPtr->respMetadata = NULL;
					}

					if(generalPtr != NULL)
						free(generalPtr);
					ptr->messageStruct = NULL;
					generalPtr = NULL;
				}
				break;

				default: break;
			}

			free(ptr);
		}

		ptr = NULL;
		return;
}

void freeConnectionDetails(struct connectionDetails *ptr){

	if(ptr == NULL)
		return;

	struct message *messageStruct;

	while(!ptr->messagingQueue.empty()){
		messageStruct = ptr -> messagingQueue.front();
		ptr -> messagingQueue.pop();
		freeMessage(messageStruct);
		messageStruct = NULL;
	}
	close(ptr->socketDesc);
	free(ptr);
	ptr = NULL;
	return;
}

void freeConnectionDetailsMap(){

	connectedNeighborsMapIter = connectedNeighborsMap.begin();

	while(connectedNeighborsMapIter != connectedNeighborsMap.end()){
		struct connectionDetails *cd;
		cd = connectedNeighborsMapIter->second;
		connectedNeighborsMap.erase(connectedNeighborsMapIter->first);

		freeConnectionDetails(cd);
		cd = NULL;
		connectedNeighborsMapIter++;
	}
	return;
}

void freeMessageCache(){
	messageCacheMapIter = messageCacheMap.begin();

	while(messageCacheMapIter != messageCacheMap.end()){
		struct message *tempmsg;
		tempmsg = messageCacheMapIter->second;
		freeMessage(tempmsg);
		tempmsg = NULL;
		messageCacheMapIter++;
	}
	return;
}

void CreateBitVectorForSearchKeywords(unsigned char keywords[100][100],int keyCount,unsigned char *bitVector){

	int keywordCount=0;

	memset(bitVector,0,128);

	while(keywordCount < keyCount){

		unsigned char keyword[60] = "\0";

		memcpy(keyword,keywords[keywordCount],strlen((char*)keywords[keywordCount]));
		keywordCount++;
		unsigned char md5out[16]="\0";
		unsigned char sha1out[20]="\0";

		unsigned char sha1char[3] = "\0";
		unsigned char md5char[3] = "\0";
		unsigned char temp = 0x00001;

		SHA1(keyword,strlen((char *)keyword),sha1out);
		MD5(keyword,strlen((char *)keyword),md5out);

		sha1char[1] = sha1out[19];
		sha1char[0] = sha1out[18] & temp;

		md5char[1] = md5out[15];
		md5char[0] = md5out[14] & temp;

		uint16_t md5val = 0,sha1val = 0;

		md5val = md5val | md5char[0];
		md5val = (md5val << 8);
		md5val = md5val | md5char[1];

		sha1val = sha1val | sha1char[0];
		sha1val = (sha1val << 8);
		sha1val = sha1val | sha1char[1];

		sha1val = sha1val + 512;
		unsigned char tempo;

		int rem, index;
		rem = sha1val % 8;
		index = sha1val / 8;

		tempo = 0x01;
		tempo = tempo << rem;
		bitVector[index] = bitVector[index] | tempo;

		rem = md5val % 8;
		index = md5val / 8;

		tempo = 0x01;
		tempo = tempo << rem;
		bitVector[index] = bitVector[index] | tempo;
	}

	return;
}

void parseKeywordsWithSpace(unsigned char *keywordsWithSpace, int *keyWordCount,unsigned char (*searchKeywords)[100]){
	unsigned char *pch;
	int i = 0;
	*keyWordCount = 0;

	pch = keywordsWithSpace;

	while(*pch != '\0'){

		if(*pch == ' '){
			searchKeywords[*keyWordCount][i++] = '\0';
			*keyWordCount+=1;
			pch++;
			i=0;
		}

		searchKeywords[*keyWordCount][i++] = tolower(*pch);
		pch++;
	}
	searchKeywords[*keyWordCount][i++]='\0';
	*keyWordCount+=1;

	return;
}


bool SHA1Search(unsigned char *sha1, struct fileMetadata *ptrArr[], int *searchCount,int fileNumbers[]){

	char keytype[256] = "\0";
	char filename[256] = "\0";
	int i = 0,indx=0;
	struct fileMetadata* ptr;
	i = 0;
	while(i<20)
	{
		keytype[i] = *(sha1 + i);
		i++;
	}
	keytype[i] = '\0';

	string shasearchKey = string(keytype);

	*searchCount = 0;
	pthread_mutex_lock(&SHA1IndexMapLock);
	SHA1IndexMapIter = SHA1IndexMap.find(shasearchKey);
	if(SHA1IndexMapIter != SHA1IndexMap.end())
	{
		SHA1IndexIt = SHA1IndexMap.equal_range(SHA1IndexMapIter->first.c_str());
		for(SHA1IndexITIter = SHA1IndexIt.first; SHA1IndexITIter != SHA1IndexIt.second ; SHA1IndexITIter++)
		{
			fileNumbers[indx] = SHA1IndexITIter->second;
			indx++;
			sprintf(filename,"%s/files/%d.meta",ndInfo.homeDir,SHA1IndexITIter->second);
			ptr = createMetaDataFromFile(filename);
			ptrArr[*searchCount] = ptr;
			*searchCount = *searchCount+1;
		}
	}
	pthread_mutex_unlock(&SHA1IndexMapLock);
	return true;
}

bool FileNameSearch(unsigned char *fileName, struct fileMetadata *ptrArr[], int *searchCount,int fileNumbers[]){

	char keytype[256] = "\0";
	char filename[256] = "\0";
	int i = 0,indx=0;
	struct fileMetadata* ptr;
	i = 0;
	while(i<20)
	{
		keytype[i] = *(fileName + i);
		i++;
	}
	keytype[i] = '\0';
	string searchKey = string(keytype);

	*searchCount = 0;
	int k = 0 ;
	pthread_mutex_lock(&fileNameIndexMapLock);
	fileNameIndexMapIter = fileNameIndexMap.find(searchKey);
	if(fileNameIndexMapIter != fileNameIndexMap.end())
	{
		fileNameIndexIt = fileNameIndexMap.equal_range(fileNameIndexMapIter->first.c_str());
		for(fileNameIndexITIter = fileNameIndexIt.first; fileNameIndexITIter != fileNameIndexIt.second ; fileNameIndexITIter++)
		{
			k = 0;
			fileNumbers[indx] = fileNameIndexITIter->second;
			indx++;
			sprintf(filename,"%s/files/%d.meta",ndInfo.homeDir,fileNameIndexITIter->second);
			ptr = createMetaDataFromFile(filename);
			if(ptr != NULL)
			{
				ptrArr[*searchCount] = ptr;
				*searchCount = *searchCount+1;
			}
		}
	}
	pthread_mutex_unlock(&fileNameIndexMapLock);
	return true;
}

bool KeywordSearch(unsigned char *keywordsWithSpace, struct fileMetadata *ptrArr[], int *searchCount,int fileNumbers[]){

	unsigned char keywords[100][100];
	int keyCount=0;

	parseKeywordsWithSpace(keywordsWithSpace, &keyCount,keywords);

	unsigned char bitVector[128];
	unsigned char AndedBitVector[128];
	int flagL=0, flagR=0;
	CreateBitVectorForSearchKeywords(keywords,keyCount,bitVector);

	char filename[256];
	struct fileMetadata* ptr;
	*searchCount=0;

	//--LOOP
	// Do this for all Files
	// Assign filename
	pthread_mutex_lock(&KeywordIndexMapLock);
	for(KeywordIndexMapIter = KeywordIndexMap.begin(); KeywordIndexMapIter != KeywordIndexMap.end(); KeywordIndexMapIter++)
	{
			ptr=NULL;
			int i=0; flagL=0;flagR=0;
			int found = 0;
			int indx=0;
			sprintf(filename,"%s/files/%d.meta",ndInfo.homeDir,KeywordIndexMapIter->second);
			ptr = createMetaDataFromFile(filename);

			memset(AndedBitVector,0,128);
			// Compare Bit Vectors:
			while(i < 128){
				AndedBitVector[i] = bitVector[i] & ptr->bitVector[i];
				if(AndedBitVector[i] != 0 && i < 64){
					flagL = 1 ;
					if(flagR == 1) break;
				}
				else if(AndedBitVector[i] != 0 && i >= 64){
					flagR = 1;
					if(flagL == 1) break;
				}
				i++;
			}
			if(flagL == 1 && flagR == 1){
				int j = 0,k = 0;
				found = 0;

				while(j < keyCount){ // Search Request Keywords
					k=0;found=0;
					while(k < ptr->keyWordCount){
						if(strcmp((char*)keywords[j],(char*)ptr->keywords[k]) == 0){
							found = 1;
							break;
						}
						k++;
					}
					if(found == 0) break;
					j++;
				}
				if(found == 1){
					fileNumbers[indx] = KeywordIndexMapIter->second;
					indx++;
					ptrArr[*searchCount] = ptr;
					*searchCount = *searchCount+1;
				}
				else{
					free(ptr);
					break;
				}
			}
			else{
				free(ptr);
			}
	}
	pthread_mutex_unlock(&KeywordIndexMapLock);
	return true;
}


int GetMyFilesMetaData(struct fileMetadata *ptrArr[]){
		int count = 0;
		char filename[256];
		struct fileMetadata* ptr;

		pthread_mutex_lock(&fileNameIndexMapLock);
		for(fileNameIndexMapIter = fileNameIndexMap.begin(); fileNameIndexMapIter != fileNameIndexMap.end(); fileNameIndexMapIter++)
		{
			sprintf(filename,"%s/files/%d.meta",ndInfo.homeDir,fileNameIndexMapIter->second);
			ptr = createMetaDataFromFile(filename);
			if(ptr != NULL)
			{
				ptrArr[count] = ptr;
				count++;
			}
		}
		pthread_mutex_unlock(&fileNameIndexMapLock);
		return count;
}

void printConnectedNeighborInforMap()
{
	pthread_mutex_lock(&connectedNeighborsMapLock);
	printf("\n\n---------- Connected Peer Map -------------------------------\n");
	for(connectedNeighborsMapIter=connectedNeighborsMap.begin();connectedNeighborsMapIter != connectedNeighborsMap.end();connectedNeighborsMapIter++)
	{
		cout<<"\nHost : "<<connectedNeighborsMapIter->first<< " Operational State: " << connectedNeighborsMapIter->second->notOperational <<" IsJoin : "<<connectedNeighborsMapIter->second->isJoinConnection<<" ThreadsExitedCount = "<<connectedNeighborsMapIter->second->threadsExitedCount<<" HelloState : "<<connectedNeighborsMapIter->second->helloStatus<<" Tie : "<<connectedNeighborsMapIter->second->tiebreak<<"\n";
	}
	printf("\n---------------------------------------------------------------\n");
	pthread_mutex_unlock(&connectedNeighborsMapLock);
}

void printJoinMessageInfoMap()
{
	struct joinResponseDetails *joinRespDet;
	pthread_mutex_lock(&joinMsgMapLock);
	printf("\n\n---------- Join Responses Map ---------\n");
	for(joinMsgMapIter=joinMsgMap.begin();joinMsgMapIter != joinMsgMap.end();joinMsgMapIter++)
	{
		joinRespDet = joinMsgMapIter->second;
		cout<<"\nKey : "<<joinMsgMapIter->first<< " HostName: " << joinRespDet->hostName <<" Port: "<<joinRespDet->portNum<<" Distance: "<<joinRespDet->distToNode<<"\n";
	}
	printf("\n----------------------------------------\n");
	pthread_mutex_unlock(&joinMsgMapLock);
}

void printKeyWordIndexMap()
{
	pthread_mutex_lock(&KeywordIndexMapLock);
	printf("\n-------------------KeyWord Index Map---------------------\n");
	for(KeywordIndexMapIter = KeywordIndexMap.begin(); KeywordIndexMapIter != KeywordIndexMap.end(); KeywordIndexMapIter++)
	{
		cout<<KeywordIndexMapIter->first<<" => "<<KeywordIndexMapIter->second<<endl;
	}
	printf("\n---------------------------------------------------------\n");
	pthread_mutex_unlock(&KeywordIndexMapLock);
}

void printSHA1IndexMap()
{
	pthread_mutex_lock(&SHA1IndexMapLock);
	printf("\n--------------------SHA1 Index Map--------------------\n");
	for(SHA1IndexMapIter = SHA1IndexMap.begin(); SHA1IndexMapIter != SHA1IndexMap.end(); SHA1IndexMapIter++)
	{
		cout<<SHA1IndexMapIter->first<<" => "<<SHA1IndexMapIter->second<<endl;
	}
	printf("\n------------------------------------------------------\n");
	pthread_mutex_unlock(&SHA1IndexMapLock);
}

void printFilenameIndexMap()
{
	pthread_mutex_lock(&fileNameIndexMapLock);
	printf("\n--------------------FileName Index Map--------------------\n");
	for(fileNameIndexMapIter = fileNameIndexMap.begin(); fileNameIndexMapIter != fileNameIndexMap.end(); fileNameIndexMapIter++)
	{
		cout<<fileNameIndexMapIter->first<<" => "<<fileNameIndexMapIter->second<<endl;
	}
	printf("\n----------------------------------------------------------\n");
	pthread_mutex_unlock(&fileNameIndexMapLock);
}

void printLRUList()
{
	pthread_mutex_lock(&LRUlistLock);
	printf("\n--------------------LRU List--------------------\n");
	for(LRUIter = LRUList.begin(); LRUIter != LRUList.end() ; LRUIter++)
	{
		printf("[ %d ]",*LRUIter);
	}
	printf("\n-------------------------------------------------\n");
	pthread_mutex_unlock(&LRUlistLock);
}


void printPermanentFileList()
{
	pthread_mutex_lock(&permFileListLock);
	printf("\n--------------------LRU List--------------------\n");
	for(PermFileListIter = PermFileList.begin(); PermFileListIter != PermFileList.end() ; PermFileListIter++)
	{
		printf("[ %d ]",*PermFileListIter);
	}
	printf("\n-------------------------------------------------\n");
	pthread_mutex_unlock(&permFileListLock);
}
