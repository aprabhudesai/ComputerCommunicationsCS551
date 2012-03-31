/*------------------------------------------------------------------------------
 *      Name: Aniket Zamwar and Abhishek Prabhudesai
 *      USC ID: 1488-5616-98
 *      Aludra/Nunki ID: zamwar
 *      Aludra/Nunki ID: prabhude
 *
 *      Project: WarmUp Project #3 - CSCI 551 Spring 2012
 *      Instructor: Bill Cheng
 *      File Description: This section of project consists of all the implementation
 *      				of project logic.
------------------------------------------------------------------------------*/

// HEADER Declarations to be used in the project

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <netdb.h>
//#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>
#include "limits.h"
//#include <errno.h>
//#include <string.h>
//#include <strings.h>
#include <string>
//#include <signal.h>
//#include <stdio.h>
//#include <openssl/md5.h>

//#define bool int
//#define false 0
//#define true 1

#define MYMESSAGE 1
#define NOTMYMESSAGE 0


using namespace std;
