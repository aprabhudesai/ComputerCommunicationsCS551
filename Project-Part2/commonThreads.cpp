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
#include "commonThreads.h"
#include <iostream>

int ttlcmd = 0;
char extfilecmd[256] = "\n";

void getTTLAndExtFileFromCmd(char *cmd,int msgtype)
{
	char *rem;
	rem = strchr(cmd,' ');
	ttlcmd = 0;
	memset(extfilecmd,0,256);
	while(rem!=NULL)
	{
		if(strstr(rem,"neighbors") != NULL || strstr(rem,"files") != NULL)
		{
			rem = strchr(rem+1,' ');
			char *token;
			token = strtok(rem," ");
			ttlcmd = atoi(token);
			rem = strchr(rem+1,' ');
			token = NULL;
			token = strtok(rem," ");
			strcpy(extfilecmd,token);
			break;
		}
		cmd = rem + 1;
		rem = strchr(rem,' ');
	}
}

int getStatusMsgType(char *cmd)
{
	if(strstr(cmd,"neighbors") != NULL)
	{
		return 1;
	}
	if(strstr(cmd,"files") != NULL)
	{
		return 2;
	}
	else
		return 0;
}

void parseCmd(char *cmd)
{
	int msgtype = 0;
	if(strcmp(cmd,"shutdown") == 0)
	{
		//Set the system shutdown flag
	}
	else
	{
		msgtype = getStatusMsgType(cmd);
		getTTLAndExtFileFromCmd(cmd,msgtype);
	}
}

int checkValidityOfCommand(char *cmd)
{
	if((strcmp(cmd,"shutdown") == 0) || strstr(cmd,"status neighbors") != NULL || strstr(cmd,"status files") != NULL)
		return 1;
	else
		return 0;
}

void * createKeyboardThread(int *arg)
{
	struct timeval tm;
	char usercmd[256];
	tm.tv_sec = 0;
	tm.tv_usec = 100000;
	printf("\nWaiting for user Input . . . \n");
	while(1)
	{
		memset(usercmd,0,256);
		select(NULL,NULL,NULL,NULL,&tm);
		/*if(timeToQuit == 1)
			{
				break;
			}
			else*/
		{
			if(fgets(usercmd,256,stdin)!=NULL)
			{
				if(checkValidityOfCommand(usercmd) == 0)
				{
					printf("\nInvalid User Command\n");
				}
				else
				{
					//The user command is valid determine the action to be taken
					parseCmd(usercmd);
					cout<<"\nTTL = "<<ttlcmd<<"\nExtfile = "<<extfilecmd<<"\n";
				}
			}
		}
	}
	pthread_exit(NULL);
}

void * createTimerThread(int *arg)
{
	pthread_exit(NULL);
}
