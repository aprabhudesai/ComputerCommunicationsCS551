/*
*	server.c: This is the server code. The server is a multithreaded application which can cater to the
*	client requests simultaneously. It accepts requests from clients and depending upon the type provdes
* 	with an appropriate response. The server also has an alarm signal that is set to 60 sec by default after
*	which the server will auto-shutdown (unless the user specifies a smaller value using "-t" flag
*/

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "server.h"
#include <pthread.h>
#include <sys/stat.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/select.h>
#include <sys/time.h>
#include <stdbool.h>
#include "messageFunctions.h"

struct sockaddr_in accept_addr;
struct hostent *server_addr = NULL;

/*
*	Function: readcmdline
*	It parses the commandline arguments apecified by the user and checks their validity
*/

void readcmdline(int argc, char *argv[])
{
	int loc = 1,noOfArgs = argc - 2,cnt=1, i=0;
	char tempStr[5],tempTout[10];
	if(argc < 2)
	{
		printf("\nInsufficient number of arguments specified\n");
		exit(0);
	}
	if(strcmp(argv[argc - 1],"-m") == 0 || strcmp(argv[argc - 1],"-t") ==0)		// if the commandline argument is other that "-m" or "-t" then it is incorrect
	{
		printf("\nIncorrect syntax. Correct syntax is :  'server [-t seconds] [-m] port'\n");
		exit(1);
	}
	while(noOfArgs != 0)
	{
		if(strcmp(argv[cnt],"-t") == 0)		// if the "-t" flag is specified then check the validity of the 
		{
			i=0;
			strcpy(tempTout,argv[cnt+1]);
			while(tempTout[i] != '\0')
			{
				if(tempTout[i] < 48 || tempTout[i] > 57)
				{
					printf("\nSpecified timeout is not in correct format\n");
					exit(1);
				}
				i++;
			}
			timeout = atoi(argv[cnt+1]);
			noOfArgs-=2;
			loc+=2;
			cnt+=2;
		}	
		else if(strcmp(argv[cnt],"-m") == 0)	// if the "=m" flag is specified then set the display flag to be ON
		{
			pthread_mutex_init(&stdoutLock, NULL);
			servdisplayFlag = 1;
			noOfArgs-=1;
			loc++;
			cnt+=1;
		}
		else
		{
			printf("\nIncorrect syntax. Correct syntax is :  'server [-t seconds] [-m] port'\n");
			exit(1);
		}
	}
	snprintf(tempStr,strlen(argv[loc])+1,"%s",argv[loc]);
	i=0;
	while(tempStr[i] != '\0')
	{
		if(tempStr[i] < 48 || tempStr[i] > 57)
		{
			printf("\nSpecified port is not in correct format\n");
			exit(1);
		}
		i++;
	}
	portNum = atoi(tempStr);
	if(portNum < 1024)					// check if the port number specified in not a well known port
	{
		printf("\nYou have specified a well known port. Please use a dfferent port\n");
		exit(1);
	}
}

/*
*	Function: processClientRequest
*	This function processes the request that the client sends. Depending upon the type of request it takes the appropriate
*	action on the resource specified
*/

void processClientRequest(int reqType,int acceptSocket,int offset,int delay,unsigned char *reqData)
{
	int n=0,i=0,dataFlag = 0,fSize = 0,openFlag = 1,state = 0;
	struct stat fileData;
	char host[15],fileSizeStr[30];
	char buffer[512] = {'\0'};
	FILE *filePtr = NULL;
	size_t bytesRead = 0;
	unsigned char msgHeader[11] = {'\0'};
	int sendBufferSize = 8000;
	setsockopt(acceptSocket,SOL_SOCKET,SO_SNDBUF,&sendBufferSize,sizeof(sendBufferSize));
	switch(reqType)
	{
		case 0xfe10:				//ADR type of request
					{
						if((server_addr = gethostbyname((char *)reqData)) != NULL)		// get the ip of the specified host
						{
							strcpy(host,inet_ntoa(*((struct in_addr *)server_addr->h_addr_list[0])));
							prepareMsgHeader(0xFE11,strlen(host),msgHeader,offset,delay,1);
							dataFlag = 1;
						}
						else
						{
							prepareMsgHeader(0xFE12,0,msgHeader,offset,delay,1);
						}
						sleep(delay);
						for(i=0;i<11;i++)
						{
							pthread_mutex_lock(&quitMutex);
							if(quitStat != 1)
							{
								if((n = write(acceptSocket,&msgHeader[i],1)) < 0)
								{
									pthread_mutex_unlock(&quitMutex);
									break;
								}
							}
							else
							{
								pthread_mutex_unlock(&quitMutex);
								break;
							}
							pthread_mutex_unlock(&quitMutex);
						}
						if(dataFlag == 1)
						{
							for(i=0;i<strlen(host);i++)									//send out the response data
							{
								pthread_mutex_lock(&quitMutex);
								if(quitStat != 1)	
								{
									if((n = write(acceptSocket,&host[i],1)) < 0)
									{
										pthread_mutex_unlock(&quitMutex);
										break;
									}
								}
								else
								{
									pthread_mutex_unlock(&quitMutex);
									break;
								}
								pthread_mutex_unlock(&quitMutex);
							}
						}
						break;
					}
		case 0xfe20:					//FSZ type of request
					{	
						if(stat((char *)reqData,&fileData) < 0) 		//check if file exists or not
						{
							prepareMsgHeader(0xFE22,0,msgHeader,offset,delay,1);	
						}
						else 
						{
							sprintf(fileSizeStr,"%lu", fileData.st_size);
							prepareMsgHeader(0xFE21,strlen(fileSizeStr),msgHeader,offset,delay,1);
							dataFlag = 1;
						}
						sleep(delay);
						for(i=0;i<11;i++)
						{
							pthread_mutex_lock(&quitMutex);
							if(quitStat != 1)
							{
								if((n = write(acceptSocket,&msgHeader[i],1)) < 0)
								{
									pthread_mutex_unlock(&quitMutex);
									break;
								}
							}
							else
							{
								pthread_mutex_unlock(&quitMutex);
								break;
							}
							pthread_mutex_unlock(&quitMutex);
						}
						if(dataFlag == 1)
						{
							for(i=0;i<strlen(fileSizeStr);i++)							// send out the response data ie. the file size
							{
								pthread_mutex_lock(&quitMutex);
								if(quitStat != 1)	
								{
									if((n = write(acceptSocket,&fileSizeStr[i],1)) < 0)
									{
										pthread_mutex_unlock(&quitMutex);
										break;
									}
								}
								else
								{
									pthread_mutex_unlock(&quitMutex);
									break;
								}
								pthread_mutex_unlock(&quitMutex);
							}
						}
						break;
					}
		case 0xfe30:					//GET type request
					{
						if(stat((char *)reqData,&fileData) < 0) 		// check if the file exists
						{
							prepareMsgHeader(0xFE32,0,msgHeader,offset,delay,1);
							for(i=0;i<11;i++)
							{
								pthread_mutex_lock(&quitMutex);
								if(quitStat != 1)
								{
									if((n = write(acceptSocket,&msgHeader[i],1)) < 0)
									{
										pthread_mutex_unlock(&quitMutex);
										break;
									}
								}
								else
								{
									pthread_mutex_unlock(&quitMutex);
									break;
								}
								pthread_mutex_unlock(&quitMutex);
							}
						}
						else if((filePtr = fopen((char *)reqData,"r")) == NULL)
						{
							printf("\nError opening the requested file\n");
							prepareMsgHeader(0xFE32,0,msgHeader,offset,delay,1);
							openFlag = 0;
							for(i=0;i<11;i++)
							{
								pthread_mutex_lock(&quitMutex);
								if(quitStat != 1)
								{
									if((n = write(acceptSocket,&msgHeader[i],1)) < 0)
									{
										pthread_mutex_unlock(&quitMutex);
										break;
									}
								}
								else
								{
									pthread_mutex_unlock(&quitMutex);
									break;
								}
								pthread_mutex_unlock(&quitMutex);
							}
						}
						else 
						{
							if(offset > fileData.st_size)			// check if the offset value is within the file size
							{
								prepareMsgHeader(0xFE32,0,msgHeader,offset,delay,1);
								for(i=0;i<11;i++)
								{
									pthread_mutex_lock(&quitMutex);
									if(quitStat != 1)
									{
										if((n = write(acceptSocket,&msgHeader[i],1)) < 0)
										{
											pthread_mutex_unlock(&quitMutex);
											break;
										}
									}
									else
									{
										pthread_mutex_unlock(&quitMutex);
										break;
									}
									pthread_mutex_unlock(&quitMutex);
								}
							}
							else
							{
								sprintf(fileSizeStr,"%lu", fileData.st_size);
								fSize = atoi(fileSizeStr);
								fSize = fSize - offset;
								prepareMsgHeader(0xFE31, fSize,msgHeader,offset,delay,1);
								sleep(delay);
								for(i=0;i<11;i++)
								{
									pthread_mutex_lock(&quitMutex);
									if(quitStat != 1)
									{
										if((n = write(acceptSocket,&msgHeader[i],1)) < 0)
										{
											pthread_mutex_unlock(&quitMutex);
											break;
										}
									}
									else
									{
										pthread_mutex_unlock(&quitMutex);
										break;
									}
									pthread_mutex_unlock(&quitMutex);
								}
								if((fseek (filePtr,offset, SEEK_SET)) != 0)			// seek the offset in the file
								{
									printf("\nError in seeking the requested offset position in the file\n");
									pthread_exit(NULL);
								}
								i=0;
								if(fSize < 512)									// send the file data to the client
								{
									bytesRead = fread(buffer,1,fSize,filePtr);
									while(bytesRead != 0)
									{
										pthread_mutex_lock(&quitMutex);
										if(quitStat != 1)	
										{
											if((n = write(acceptSocket,&buffer[i],1)) < 0)
											{
												state = 1;
												pthread_mutex_unlock(&quitMutex);
												break;
											}	
										}
										else
										{
											pthread_mutex_unlock(&quitMutex);
											state = 1;
											break;
										}
										pthread_mutex_unlock(&quitMutex);
										bytesRead--;
										i++;
										if(state == 1)
										{
											break;
										}
									}
								}
								else
								{
									while(fSize > 0)			// for a file that is greater than 512 bytes long
									{							// this is because the max buffer size is 512 bytes for the server
										bytesRead = fread(buffer,1,512,filePtr);
										i=0;
										while(bytesRead != 0)
										{
											
											//pthread_mutex_lock(&quitMutex);
											if(quitStat != 1)	
											{
												if((n = write(acceptSocket,&buffer[i],1)) < 0)
												{	
													state = 1;
													fclose(filePtr);
													close(acceptSocket);
													//pthread_mutex_unlock(&quitMutex);
													pthread_exit(NULL);
													break;
												}	
											}
											else
											{
												//pthread_mutex_unlock(&quitMutex);
												state = 1;
												close(acceptSocket);
												fclose(filePtr);
												pthread_exit(NULL);
												break;
											}
											//pthread_mutex_unlock(&quitMutex);
											bytesRead--;
											i++;
											if(state == 1)
											{
												break;
											}
										}
									
										fSize -= i;
										if(state == 1)
											break;
									}
								}
							}
						}
						if(openFlag == 1)
						{
							fclose(filePtr);
						}
						break;
					}
		default:
				{
					prepareMsgHeader(0xFCFE,0,msgHeader,offset,delay,1);
					for(i=0;i<11;i++)
					{
						pthread_mutex_lock(&quitMutex);
						if(quitStat != 1)
						{
							if((n = write(acceptSocket,&msgHeader[i],1)) < 0)
							{
								pthread_mutex_unlock(&quitMutex);
								break;
							}
						}
						else
						{
							pthread_mutex_unlock(&quitMutex);
							break;
						}
						pthread_mutex_unlock(&quitMutex);
					}
					break;
				}
	}
}

/*
*	Function: usrSigHandler
*	This is the signal handler for SIGUSR1 signal
*/

void usrSigHandler()
{
	printf("\nHence Shutting down the Server\n");
}

/*
*	Function: parseclntReq
*	This is the function that handles the request received from the client. This is a child thread that processes the 
*	client request and then exits
*/

void *parseclntReq(void *acceptSock)
{
	int acceptSocket = *((int *)acceptSock);
	int n=0,i=0,recvdBytes = 0,headerRead = 0;
	unsigned char clientData[512] = {'\0'};
	unsigned char clientReqData[11] = {'\0'};
	uint16_t reqType = 0;
	uint32_t offset = 0,dataLength = 0;
	uint8_t delay = 0;
	unsigned char reqData[512] = {'\0'};
	unsigned char msgHeader[11] = {'\0'},clientaddress[16] = {'\0'};
	fd_set readfds;
	tv.tv_sec = 10;
    tv.tv_usec = 0;
    FD_ZERO(&readfds);
    FD_SET(acceptSocket, &readfds);
	i=0;
	while(headerRead != 11)				// Receive the header from the client
	{	
		select(acceptSocket+1, &readfds, NULL, NULL, &tv);
		if (FD_ISSET(acceptSocket, &readfds))
		{
			if((n = read(acceptSocket,&clientReqData[i],1)) < 0)
			{
				prepareMsgHeader(0xFCFE,0,msgHeader,0,0,1);
				for(i=0;i<11;i++)
				{
					pthread_mutex_lock(&quitMutex);
					if(quitStat != 1)
					{
						if((n = write(acceptSocket,&msgHeader[i],1)) < 0)
						{
							pthread_mutex_unlock(&quitMutex);
							break;
						}
					}
					else
					{
						pthread_mutex_unlock(&quitMutex);
						break;
					}
					pthread_mutex_unlock(&quitMutex);
				}
				break;
			}
			headerRead++;
			i++;
		}
		else					// if the client falis the send the request within 10 seconds then time out occurs
		{
			printf("\nServer Timed out\n");
			prepareMsgHeader(0xFCFE,0,msgHeader,offset,delay,1);
			for(i=0;i<11;i++)
			{
				pthread_mutex_lock(&quitMutex);
				if(quitStat != 1)
				{
					if((n = write(acceptSocket,&msgHeader[i],1)) < 0)
					{
						pthread_mutex_unlock(&quitMutex);
						break;
					}
				}
				else
				{
					pthread_mutex_unlock(&quitMutex);
					break;
				}
				pthread_mutex_unlock(&quitMutex);
			}
			FD_CLR(acceptSocket, &readfds);
			break;
		}
	}
	if(headerRead >= 11)
	{
		memcpy(&reqType,clientReqData,2);
		reqType = ntohs(reqType);
		memcpy(&offset,(clientReqData+2),4);
		offset = ntohl(offset);
		memcpy(&delay,(clientReqData+6),1);
		delay = ntohs(delay);
		memcpy(&dataLength,(clientReqData+7),4);
		dataLength = ntohl(dataLength);
		//clientData = (char *)malloc(dataLength+1);
		FD_ZERO(&readfds);
		FD_SET(acceptSocket, &readfds);
		tv.tv_sec = 0;
		tv.tv_usec = 100;
		int j=0,errFlag = 0;
		for(i=0;i<dataLength;i++)				// receive the request data after the client header is read
		{
			pthread_mutex_lock(&quitMutex);
			if(quitStat != 1)	
			{
				if(i >= 256)
				{
					j = j % 512;
					if((n = read(acceptSocket,&clientData[j],1)) < 0)
					{
						printf("\nError reading from socket\n");
						pthread_mutex_unlock(&quitMutex);
						pthread_exit(NULL);
					}
					j++;
					errFlag = 1;
				}
				else
				{
					if((n = read(acceptSocket,&clientData[i],1)) < 0)
					{
						printf("\nError reading from socket\n");
						pthread_mutex_unlock(&quitMutex);
						pthread_exit(NULL);
					}
					recvdBytes++;
				}
			}
			else
			{
				pthread_mutex_unlock(&quitMutex);
				break;
			}
			pthread_mutex_unlock(&quitMutex);			
		}
		if(errFlag == 1)
		{
			displayToScreen(dataLength,clientaddress,reqType,offset,delay);
			prepareMsgHeader(0xFCFE,0,msgHeader,offset,delay,1);
			for(i=0;i<11;i++)
			{
				pthread_mutex_lock(&quitMutex);
				if(quitStat != 1)
				{
					if((n = write(acceptSocket,&msgHeader[i],1)) < 0)
					{
						pthread_mutex_unlock(&quitMutex);
						break;
					}
				}
				else
				{
					pthread_mutex_unlock(&quitMutex);
					break;
				}
				pthread_mutex_unlock(&quitMutex);
			}
			if((close(acceptSocket)) == -1)
			{
				printf("\nError closing socket\n");
				pthread_exit(NULL);
			}
			pthread_exit(NULL);
		}
		memcpy(reqData,clientData,dataLength);
		reqData[dataLength] = '\0';
		if(servdisplayFlag == 1)
		{
			pthread_mutex_lock(&stdoutLock);
			strcpy(clientaddress,inet_ntoa(accept_addr.sin_addr));
			displayToScreen(dataLength,clientaddress,reqType,offset,delay);
			pthread_mutex_unlock(&stdoutLock);
		}
		processClientRequest(reqType,acceptSocket,offset,delay,reqData);
	}
	else
	{
		printf("\n\tReceived %d bytes from %s",headerRead,inet_ntoa(accept_addr.sin_addr));
	}
	if((close(acceptSocket)) == -1)
	{
		printf("\nError closing socket\n");
		pthread_exit(NULL);
	}
	pthread_exit(NULL);
}

/*
*	Function: intHandler
*	This is the interrup handler for SIGALRM and SIGINT signals
*/

void intHandler(int signum)
{
	pthread_mutex_lock(&quitMutex);
	if(signum == 2)
	{
		printf("\nUser pressed <Cntrl+C>\n");
		sigType = 1;
	}
	if(signum == 14)
	{
		printf("\nAlarm Signal Generated.\n");
	}
	quitStat = 1;						// set the quit status to be true to tell the child that its time to quit
	close(nSocket);
	pthread_mutex_unlock(&quitMutex);
}

/*
*	Register the signal handlers for various signals
*/

void registerSignalHandlers()
{
	signal(SIGALRM,intHandler);
	signal(SIGINT,intHandler);
	signal(SIGPIPE,SIG_IGN);
	signal(SIGUSR1,usrSigHandler);
}

int main(int argc, char *argv[])
{
	struct sockaddr_in serv_addr;
	int i=0,sockOptLen = sizeof(int);
	bool sockOpt = true;
	pthread_mutex_init(&quitMutex, NULL);
	readcmdline(argc, argv);
	if((nSocket = socket(AF_INET,SOCK_STREAM,0)) == -1)
	{
		printf("\nError creating socket\n");
		exit(1);
	}
	memset(&serv_addr,0,sizeof(struct sockaddr_in));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(portNum);
	if ((setsockopt(nSocket,SOL_SOCKET,SO_REUSEADDR,&sockOpt,sockOptLen)) == -1) 
	{
		printf("Error in setting socket opt");
	}
	if((bind(nSocket,(struct sockaddr *)&serv_addr,sizeof(serv_addr))) == -1)
	{
		printf("\nError binding to socket\n");
		exit(1);
	}	
	if((listen(nSocket, 5)) == -1)
	{
		printf("\nError listening to socket\n");
		exit(1);
	}
	registerSignalHandlers();
	alarm(timeout);				// set the alarm for the timeout specified in the command line parameters
	i=0;						// by defalut the timeout is 60 seconds
	while(1)
	{
		int acceptSock=0;
		socklen_t acceptlen;
		acceptlen = sizeof(accept_addr);
		acceptSock = accept(nSocket,(struct sockaddr *) &accept_addr,&acceptlen);
		int *tempSock = (int *)malloc(sizeof(int));
		*tempSock = acceptSock;
		if (*tempSock < 0) 
		{
			break;
		}
		else 
		{
			pthread_t child_thread;	
			childCount++;
			if(pthread_create(&child_thread,NULL,parseclntReq,(void *)tempSock))	//Create child thread for handling client request
			{
				printf("\nError Creating thread\n");
				continue;
			}
			childThreadArray[i] = child_thread;
			pthread_mutex_lock(&quitMutex);
			if(quitStat == 1)
			{
				pthread_mutex_unlock(&quitMutex);
				break;
			}
			else
			{
				pthread_mutex_unlock(&quitMutex);
			}
			i++;
		}
	}
	//if((close(nSocket)) == -1)
	//{
		//perror("\nError closing listening socket\n");
		//exit(1);
	//}
	for(i = 0; i < childCount ; i++)					// Signal the child threads (if any) to quit and wait for them to exit
	{
		if((pthread_kill(childThreadArray[i],SIGUSR1)) != 0)
		{
			//printf("\nChild Thread Already Exited\n");
		}
	}	
	for(i = 0; i < childCount ; i++)
	{
		(void) pthread_join(childThreadArray[i],NULL);
	}
	printf("\nServer Shutdown Successfully\n");
	exit(0);
}
