/*
*	client.c: This is the client code. The client sends 3 types of requests to the server
*	1. ADR - to get address of a host
*	2. FSZ - to get file size of a specific file on the server
*	3. GET - to get the data from a particular file on the server. The client then calculates
*			 an md5 on the data received.
*/

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <openssl/md5.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include "messageFunctions.h"
#include "client.h"

/*
*	Function: parsecmdline
*	Parses the command line arguments provided by the user. If they are invalid then
*	it does not accept them and throws an error message.
*/

void parsecmdline(int argc, char *argv[])
{
	int loc = 2,i = 0,j=0;
	char tempStr[50],tempPort[7],tempDelay[5],tempOffset[10];
	char *token = NULL;
	int noOfArgs = argc - 4,cnt=2;
	if(argc < 4)
	{
		printf("\nInvalid syntax for client. Required syntax is 'client {adr|fsz|get} [-d delay] [-o offset] [-m] hostname:port string'\n");
		exit(1);
	}
	if(strcmp(argv[1],"adr") != 0 && strcmp(argv[1],"fsz") != 0 && strcmp(argv[1],"get") != 0)	//Check if the argument is arg/ fsz/ get
	{																							// if not then throw an error message
		printf("\nInvalid syntax for client. Please use [adr|fsz|get] as the 1st argument\n");
		exit(1);
	}
	else
	{
		if(strcmp(argv[1],"adr") == 0)
			reqType = 1;
		else if(strcmp(argv[1],"fsz") == 0)
			reqType = 2;
		else if(strcmp(argv[1],"get") == 0)
			reqType = 3;
		while(noOfArgs != 0)
		{
			if(strcmp(argv[cnt],"-d") == 0)			// if the -d flag is specified then check if the delay value is valid or not
			{
				i=0;
				strcpy(tempDelay,argv[cnt+1]);
				while(tempDelay[i] != '\0')
				{
					if(tempDelay[i] < 48 || tempDelay[i] > 57)
					{
						printf("Specified delay is not in correct format\n");
						exit(1);
					}
					i++;
				}
				delay = atoi(argv[cnt+1]);
				int delayInt = (int) delay;
				if(delayInt < 0 || delayInt > 255)
				{
					printf("\nThe delay value specified is incorrect\n");
					exit(1);
				}
				else
				{
					loc+=2;
					noOfArgs-=2;
					cnt+=2;
				}
			}	
			else if(strcmp(argv[cnt],"-o") == 0)	// if -o flag is specified then check if the offset is a numeric value and is valid
			{
				if(strlen(argv[cnt+1]) > 10)
				{
					printf("\nThe offset value is out of bounds\n");
					exit(1);
				}
				else
				{
					i=0;
					strcpy(tempOffset,argv[cnt+1]);
					while(tempOffset[i] != '\0')
					{
						if(tempOffset[i] < 48 || tempOffset[i] > 57)
						{
							printf("\nSpecified offset is not in correct format\n");
							exit(1);
						}
						i++;
					}
					offset = atoi(argv[cnt+1]);
					
					if(offset < 0)
					{
						printf("\nThe offset value specified is incorrect\n");
						exit(1);
					}
					else
					{
						loc+=2;
						noOfArgs-=2;
						cnt+=2;
					}
				}
			}
			else if(strcmp(argv[cnt],"-m") == 0)	//if -m flag is specified then set the display flag to true
			{										// this tells the client to display the data it receives from the server
				clntdisplayFlag = 1;
				noOfArgs-=1;
				loc++;
				cnt+=1;
			}
			else
			{
				printf("\nWrong Arguments passed\n");
				exit(1);
			}
		}
		snprintf(tempStr,strlen(argv[loc]) + 1,"%s",argv[loc]);
		token = strchr(tempStr,':');
		if(token == NULL)						// check if the hostname:port number argument is valid or not
		{
			printf("\nIncorrect argument specified. Please use 'hostname:port'.\n");
			exit(1);
		}
		i = 0;
		while(tempStr[i] != ':')
		{
			hostName[i] = tempStr[i];
			i++;
		}
		hostName[i] = '\0';
		i++;
		j=0;
		while(tempStr[i] != '\0')
		{
			tempPort[j] = tempStr[i];
			if(tempPort[j] < 48 || tempPort[j] > 57)
			{
				printf("\nSpecified port number is not in correct format\n");
				exit(1);
			}
			i++;
			j++;
		}
		tempPort[j] = '\0';
		portNum = atoi(tempPort);
		if(portNum < 1024)							// check if the port specified is not a well known port
		{
			printf("\nYou have specified a well known port. Please use a dfferent port > 1024\n");
			exit(1);
		}
		loc++;
		if(strlen(argv[loc]) < 256)					// check if the last argument is not > 256 bytes in size
		{
			reqData = (char *) malloc(strlen(argv[loc]));
			snprintf(reqData,strlen(argv[loc])+1,"%s",argv[loc]);
		}
		else
		{
			printf("\nThe requested data length exceeds the maximum message size\n");
			exit(1);
		}
	}
}

/*
* 	Function: createRequest
*	Creates a request header depending upon the type of service needed (adr/ fsz/ get)
*/

void createRequest()
{
	uint16_t tempreqType;
	if(reqType == 1)
	{
		tempreqType = 0xFE10;
	}
	else if(reqType == 2)
	{
		tempreqType = 0xFE20;
	}
	else if(reqType == 3)
	{
		tempreqType = 0xFE30;
	}
	prepareMsgHeader(tempreqType,strlen(reqData),sendMsg,offset,delay,0);
}

/*
*	Function: processServerResponse
*	Processes the response obtained from the server and if the display flag is set then displays the data received
*/

void processServerResponse()
{
	int n = 0;
	if(respType == 0xFE11)
	{
		if(clntdisplayFlag == 1)
		{
			displayToScreen(respdataLength,serveraddress,respType,respoffset,respdelay);
		}
		printf("\n\tADDR = %s\n",dataField);
	}
	else if(respType == 0xFE12)
	{
		if(clntdisplayFlag == 1)
		{
			displayToScreen(respdataLength,serveraddress,respType,respoffset,respdelay);
		}
		printf("\n\tADDR request for '%s' failed.\n",reqData);
	}
	else if(respType == 0xFE21)
	{
		if(clntdisplayFlag == 1)
		{
			displayToScreen(respdataLength,serveraddress,respType,respoffset,respdelay);
		}
		printf("\n\tFILESIZE = %s\n",dataField);
	}
	else if(respType == 0xFE22)
	{
		if(clntdisplayFlag == 1)
		{
			displayToScreen(respdataLength,serveraddress,respType,respoffset,respdelay);
		}
		printf("\n\tFILESIZE request for '%s' failed.\n",reqData);
	}
	else if(respType == 0xFE31)
	{
		if(dataRecvd < respdataLength)
		{
			printf("\n\tReceived %d bytes from %s",(dataRecvd + 11),serveraddress);
		}
		else
		{
			if(clntdisplayFlag == 1)
				displayToScreen(dataRecvd,serveraddress,respType,respoffset,respdelay);
			printf("\n\tFILESIZE = %d, MD5:",dataRecvd);
			for(n=0; n<MD5_DIGEST_LENGTH; n++)
				printf("%02x", md5CheckSum[n]);
		}
	}
	else if(respType == 0xFE32)
	{
		if(clntdisplayFlag == 1)
			displayToScreen(respdataLength,serveraddress,respType,respoffset,respdelay);
		printf("\n\tGET request for '%s' failed.\n",reqData);
	}
}

/*
*	Function: sendrecvdata
*	Send/ Received data to/ from server after the header and the body of the message is constructed
*/

void sendrecvdata()
{
	struct sockaddr_in serv_addr;
	int status=0, n=0, i=0, state=0,headerRead = 0;
	bool sockOpt = true;
	MD5_CTX md5_context;
	int sockOptLen = sizeof(int);
	if((nSocket = socket(AF_INET,SOCK_STREAM,0)) == -1)
	{
		printf("\nError creating socket\n");
		exit(1);
	}
	if ((setsockopt(nSocket,SOL_SOCKET,SO_REUSEADDR,&sockOpt,sockOptLen)) == -1) 
	{
		perror("Error in setting socket opt");
		exit(1);
	}
	memset(&serv_addr,0,sizeof(struct sockaddr_in));
	server = gethostbyname(hostName);				
	if (server == NULL) 
	{
        fprintf(stderr,"ERROR, no such host\n");
        exit(1);
    }
	serv_addr.sin_family = AF_INET;
	if (server != NULL) 
	{
		memcpy((char*)(&serv_addr.sin_addr.s_addr), (char *)server->h_addr,server->h_length);
	}
	else 
	{
		/* hostname is probably an IP address already */
		serv_addr.sin_addr.s_addr = inet_addr(hostName);
	}
	memcpy((char *)server->h_addr,(char *)&serv_addr.sin_addr.s_addr,server->h_length);
	serv_addr.sin_port = htons(portNum);
	status = connect(nSocket,(struct sockaddr *)&serv_addr,sizeof(serv_addr));			// connect to the host and port specified in the command line arguments
	if (status < 0) 
	{
		printf("\nError connecting to server\n");
		exit(0);
	} 
	else 
	{
		for(i=0;i<(strlen(reqData)+11);i++)
		{
			n = write(nSocket,&sendMsg[i],1);				// write the request data to the socket one byte at a time
			
			if(n < 0)
			{
				switch(errno)
				{
					case 0:
							state = 1;
							break;
					default:
							printf("\nDefault Error condition\n");
				}
				if(state == 1)
					break;
			}
		}
	}
	i = 0;
	while(headerRead != 11)
	{
		if((n = read(nSocket,&servRespData[i],1)) < 1)				//read the response header received from the server
		{
			printf("\nError in header received from server\n");
			break;
		}
		i++;
		headerRead++;
	}
	memcpy((char *) &addr_of_server,*server->h_addr_list++, sizeof(addr_of_server));
	strcpy(serveraddress,inet_ntoa(addr_of_server));
	if(headerRead >= 11)											// if the header size is 11 bytes then the client has received the entire header
	{
		memcpy(&respType,servRespData,2);
		respType = ntohs(respType);
		memcpy(&respoffset,(servRespData+2),4);
		respoffset = ntohl(respoffset);
		memcpy(&respdelay,(servRespData+6),1);
		respdelay = ntohs(respdelay);
		memcpy(&respdataLength,(servRespData+7),4);
		respdataLength = ntohl(respdataLength);
		if(respType == 0xfe31)								// if the request type is GET, only then calculate md5 on it
		{
			char ch;
			MD5_Init(&md5_context);
			for(i=0;i<respdataLength;i++)
			{
				if((n = read(nSocket,&ch,1)) < 1)
				{
					printf("\nError receiving data from server\n");
					break;
				}
				MD5_Update(&md5_context,&ch,1);
				dataRecvd++;
			}
			MD5_Final(md5CheckSum,&md5_context);
		}
		else
		{
			memset(dataField,0,512);
			if(respdataLength > 512)		// if the response is greater than the available buffer size then just print the data to the screen as it is invalid
			{
				displayToScreen(respdataLength,serveraddress,respType,respoffset,respdelay);
				char ch1;
				if(respType == 0xFE11)
					printf("\n\tADDR = ");
				else if(respType == 0xFE21)
						printf("\n\tFILESIZE = ");
				else if(respType == 0xFE31)
						printf("\n\tFILESIZE = ");
				for(i=0;i<respdataLength;i++)
				{
					if((n = read(nSocket,&ch1,1)) < 1)
					{
						printf("\nError receiving data from server\n");
						break;
					}
					printf("%c",ch1);
					dataRecvd++;
				}
				if((close(nSocket) == -1))
				{
					perror("\nError closing the socket\n");
				}
				printf("\nResponse received is Incorrect");
				printf("\n");
				exit(1);
			}
			for(i=0;i<respdataLength;i++)		// if the response is not greater than buffer size then store the result
			{
				if((n = read(nSocket,&dataField[i],1)) < 1)
				{
					printf("\nError receiving data from server\n");
					break;
				}
				dataRecvd++;
			}
			dataField[strlen(dataField)] = '\0';
		}
		processServerResponse();
	}
	else 
	{
		if(clntdisplayFlag == 1)
			printf("\nReceived %d bytes from %s\n",headerRead,inet_ntoa(addr_of_server));
	}
	if((close(nSocket) == -1))
	{
		perror("\nError closing the socket\n");
		exit(1);
	}
}

/*
*	Function: sigintHandler
*	This is the signal handler for SIGINT. It closes the socket used to connect to the server.
*/

void sigintHandler()
{	
	if(clntdisplayFlag == 1)
		printf("\nReceived %d bytes from %s\n",(dataRecvd + 11),inet_ntoa(addr_of_server));
	if((close(nSocket) == -1))
	{
		perror("\nError closing the socket\n");
	}
	exit(1);
}

int main(int argc, char *argv[])
{
	signal(SIGPIPE,SIG_IGN);
	signal(SIGINT,sigintHandler);
	parsecmdline(argc,argv);
	createRequest();
	sendrecvdata();
	printf("\n");
	exit(0);
}
