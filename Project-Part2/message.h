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

// Messages to be used in the Project

#define HEADER_LENGTH 27

#define JOIN		0xFC
#define RJOIN		0xFB
#define HELLO		0xFA
#define KEEPALIVE	0xF8
#define NOTIFY		0xF7
#define CHECK		0xF6
#define RCHECK		0xF5
#define STATUS		0xAB
#define RSTATUS		0xAC

#define SEARCH		0xEC
#define RSEARCH		0xEB
#define DELETE		0xBC
#define STORE		0xCC
#define GET			0xDC
#define RGET		0xDB


/* ---------------------------------------------*/
// join Message 	  - 	0xFC
// join Response Message  -	0xFB
// hello Massage	  -		0xFA
// keep Alive Message     -	0xF8
// notify Message	  -		0xF7
// check Message	  -		0xF6
// check Response Message -	0xF5

// #to be done
// status response message-	0xAB
// status message	  -		0xAC

//Part 2
// Search Message	  -		0xEC
// Search Response Message-	0xEB
// delete Message	  - 	0xBC
// store message	  - 	0xCC
// get message		  -		0xDC
// get response message	  -	0xDB

/* ---------------------------------------------- */

