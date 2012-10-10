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

#include "headers.h"
#include "systemDataStructures.h"

extern INDEXMAPTYPE KeywordIndexMap;
extern INDEXMAPTYPE::iterator KeywordIndexMapIter;
extern pair<INDEXMAPTYPE::iterator, INDEXMAPTYPE::iterator> KeywordIndexIt;
extern pthread_mutex_t KeywordIndexMapLock;

/*SHA1 Search Map*/
extern INDEXMAPTYPE SHA1IndexMap;
extern INDEXMAPTYPE::iterator SHA1IndexMapIter;
extern pair<INDEXMAPTYPE::iterator, INDEXMAPTYPE::iterator> SHA1IndexIt;
extern pthread_mutex_t SHA1IndexMapLock;

/*FileName Search Map*/
extern INDEXMAPTYPE fileNameIndexMap;
extern INDEXMAPTYPE::iterator fileNameIndexMapIter;
extern pair<INDEXMAPTYPE::iterator, INDEXMAPTYPE::iterator> fileNameIndexIt;
extern pthread_mutex_t fileNameIndexMapLock;

typedef std::vector<int>LISTTYPE;
extern LISTTYPE::iterator LRUIter;
extern LISTTYPE LRUList;
extern LISTTYPE::iterator PermFileListIter;
extern LISTTYPE PermFileList;

extern char lastFileIndexFileName[256];
extern char kwrdIndexFileName[256];
extern char sha1IndexFileName[256];
extern char fileNameIndexFileName[256];


char indexkey[100] = "\0";
char indexvalue[100] = "\0";

char * getIndexKey(char *line)
{
	char *tempstr = NULL;
	int noofchars = 0, i = 0;
	if((tempstr = strchr(line,'=')) == NULL)
	{
		printf("\nInvalid entry in the indx file\n");
		return NULL;
	}
	noofchars = tempstr - line;
	while(noofchars--)
	{
		indexkey[i] = *line;
		i++;
		line++;
	}
	indexkey[i] = '\0';
	return tempstr;
}

int getIndexValue(char *line)
{
	int i = 0;
	line++;
	while(*line != '\n')
	{
		indexvalue[i] = *line;
		line++;
		i++;
	}
	indexvalue[i] = '\0';
	return 1;
}

void populateIndexMap(char *keyValue,char *valueStr,int maptype)
{
	int i = 0,val = 0,k=0;
	char temp[5] = "\0";
	string key = string(keyValue);
	while(valueStr[i] != '\0')
	{
		if(valueStr[i] != ',')
		{
			temp[k] = valueStr[i];
			k++;
		}
		else
		{
			val = atoi(temp);
			k = 0;
			if(maptype == 0)
			{
				// Insert into kwrd Map
				KeywordIndexMap.insert(pair<string,int>(key,val));
			}
			else if(maptype == 1)
			{
				// Insert into SHA1 Map
				SHA1IndexMap.insert(pair<string,int>(key,val));
			}
			else if(maptype == 2)
			{
				//Insert into Filename Map
				fileNameIndexMap.insert(pair<string,int>(key,val));
			}
		}
		i++;
	}
	val = atoi(temp);
	k = 0;
	if(maptype == 0)
	{
		// Insert into kwrd Map
		KeywordIndexMap.insert(pair<string,int>(key,val));
	}
	else if(maptype == 1)
	{
		// Insert into SHA1 Map
		SHA1IndexMap.insert(pair<string,int>(key,val));
	}
	else if(maptype == 2)
	{
		//Insert into Filename Map
		fileNameIndexMap.insert(pair<string,int>(key,val));
	}
}

void populateLRUList(char *valueStr)
{
	int i = 0,val = 0,k=0;
	char temp[5] = "\0";
	LRUIter = LRUList.begin();
	while(valueStr[i] != '\0')
	{
		if(valueStr[i] != ',')
		{
			temp[k] = valueStr[i];
			k++;
		}
		else
		{
			val = atoi(temp);
			k = 0;
			LRUIter = LRUList.insert(LRUIter,val);
		}
		i++;
	}
	val = atoi(temp);
	k = 0;
	LRUIter = LRUList.insert(LRUIter,val);
}

void populatePermanentFileList(char *valueStr)
{
	int i = 0,val = 0,k=0;
	char temp[5] = "\0";
	PermFileListIter = PermFileList.begin();
	while(valueStr[i] != '\0')
	{
		if(valueStr[i] != ',')
		{
			temp[k] = valueStr[i];
			k++;
		}
		else
		{
			val = atoi(temp);
			k = 0;

			PermFileListIter = PermFileList.insert(PermFileListIter,val);
		}
		i++;
	}
	val = atoi(temp);
	k = 0;
	PermFileListIter = PermFileList.insert(PermFileListIter,val);
}


/*
 * This function Writes the in memory list to the respective file on the disk
 */
void writeListToFile(LISTTYPE list,char *file)
{
	FILE *filePtr;
	LISTTYPE::iterator listIter;
	int val = 0;
	if((filePtr = fopen(file,"w+")) == NULL)
	{
		printf("\nError opening file: %s\n",file);
		exit(0);
	}
	for(listIter = list.begin(); listIter != list.end() ; listIter++)
	{
		val = *listIter;
		if(++listIter != list.end())
		{
		fprintf(filePtr,"%d",val);
		fputc(',',filePtr);
		--listIter;
		}
		else
		{
			fprintf(filePtr,"%d",val);
			--listIter;
		}
	}
	fputc('\n',filePtr);
	fclose(filePtr);
}

/*
 * This function Writes the in memory indexes to the respective index file on the disk
 */
void writeIndexToFile(INDEXMAPTYPE index,char *file)
{
	INDEXMAPTYPE::iterator mapIter;
	INDEXMAPTYPE::iterator mapIterTemp;
	pair<INDEXMAPTYPE::iterator, INDEXMAPTYPE::iterator> indexItTemp;
	FILE *filePtr;
	int val = 0,cnt = 0;
	if((filePtr = fopen(file,"w+")) == NULL)
	{
		printf("\nError opening file: %s\n",file);
		exit(0);
	}
	for(mapIter = index.begin(); mapIter != index.end() ; )
	{
		fprintf(filePtr,"%s=",mapIter->first.c_str());
		cnt = 0;
		indexItTemp = index.equal_range(mapIter->first.c_str());
		for(mapIterTemp = indexItTemp.first; mapIterTemp != indexItTemp.second ; mapIterTemp++)
		{
			val = mapIterTemp->second;
			if(++mapIterTemp != indexItTemp.second)
			{
				fprintf(filePtr,"%d",val);
				fputc(',',filePtr);
				--mapIterTemp;
			}
			else
			{
				fprintf(filePtr,"%d",val);
				--mapIterTemp;
			}
			mapIter++;
		}
		fputc('\n',filePtr);
	}
	fclose(filePtr);
}

void parseIndexFile(int maptype)
{
	FILE *iniFptr = NULL;
	struct stat fileData;
	char line[300];
	char * strkey = NULL;
	char fileName[256] = "\0";
	/*
	 *	0 - kwrd Map
	 *	1 - SHA1 Map
	 *	2 - name Map
	 */

	if(maptype == 0)
	{
		// Insert into kwrd Map
		strcpy(fileName,kwrdIndexFileName);
	}
	else if(maptype == 1)
	{
		// Insert into SHA1 Map
		strcpy(fileName,sha1IndexFileName);
	}
	else if(maptype == 2)
	{
		//Insert into Filename Map
		strcpy(fileName,fileNameIndexFileName);
	}
	/*
	 * Parse Index File
	 */
	if(stat(fileName,&fileData) < 0)
	{
		//The specified ini file does not exist
		iniFptr = fopen(fileName,"w");
		fclose(iniFptr);
	}
	if((iniFptr = fopen(fileName,"r")) == NULL)
	{
		printf("\nError opening ini file: %s\n",fileName);
		exit(0);
	}
	while(fgets(line,sizeof(line),iniFptr) != NULL)
	{
		if(strcmp(line,"\n") != 0)
		{
			//This is not a section, these are key value pairs
			strkey = getIndexKey(line);
			if(strkey != NULL)
				getIndexValue(strkey);
			populateIndexMap(indexkey,indexvalue,maptype);
		}
	}

	fclose(iniFptr);
}

void parseLRUListFile(char *fileName)
{
	/*
	 * Parse LRU List File
	 */
	FILE *Fptr = NULL;
	struct stat fileData;
	char line[100];

	if(stat(fileName,&fileData) < 0)
	{
		//The specified ini file does not exist
		Fptr = fopen(fileName,"w");
		fclose(Fptr);
	}
	if((Fptr = fopen(fileName,"r")) == NULL)
	{
		printf("\nError opening ini file: %s\n",fileName);
		exit(0);
	}
	while(fgets(line,sizeof(line),Fptr) != NULL)
	{
		if(strcmp(line,"\n") != 0)
		{
			populateLRUList(line);
		}
	}
	fclose(Fptr);
	reverse(LRUList.begin(),LRUList.end());
}

void parsePermFileListFile(char *fileName)
{
	/*
	 * Parse Permanent File List
	 */
	FILE *Fptr = NULL;
	struct stat fileData;
	char line[100];

	if(stat(fileName,&fileData) < 0)
	{
		//The specified ini file does not exist
		Fptr = fopen(fileName,"w");
		fclose(Fptr);
	}
	if((Fptr = fopen(fileName,"r")) == NULL)
	{
		printf("\nError opening ini file: %s\n",fileName);
		exit(0);
	}
	while(fgets(line,sizeof(line),Fptr) != NULL)
	{
		if(strcmp(line,"\n") != 0)
		{
			populatePermanentFileList(line);
		}
	}
	fclose(Fptr);
	reverse(PermFileList.begin(),PermFileList.end());
}

int getNextFileIndex()
{
	int indx = 0;
	char line[10] = "\0";
	char ch;
	pthread_mutex_lock(&lastFileIndexFileLock);
	if((lastFileIndexFile = fopen(lastFileIndexFileName,"r")) == NULL)
	{
		printf("\nError opening file: lastFileIndexFile\n");
		//exit(0);
	}
	int i = 0;
	ch = fgetc(lastFileIndexFile);
	while(ch != '\n')
	{
		line[i] = ch;
		i++;
		ch = fgetc(lastFileIndexFile);
	}
	line[i] = '\0';
	indx = atoi(line);
	fclose(lastFileIndexFile);

	if((lastFileIndexFile = fopen(lastFileIndexFileName,"w")) == NULL)
	{
		printf("\nError opening file: lastFileIndexFile\n");
		//exit(0);
	}
	fprintf(lastFileIndexFile,"%d\n",indx+1);

	fclose(lastFileIndexFile);
	pthread_mutex_unlock(&lastFileIndexFileLock);

	return indx;
}
