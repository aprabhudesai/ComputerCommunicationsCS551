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

#include "headers.h"
#include "message.h"
#include "ntwknode.h"

extern unsigned char *GetUOID(char *,char *,unsigned char *uoid_buf,int);


struct message{
	uint8_t messType;   	// message type
	unsigned char uoid[21];	// UOID of Message
	//string uoidKey;		// UOID Key
	uint8_t ttl;			// ttl for each message
	char resv;				// reserved byte
	uint32_t dataLength;	// Message Data Length Field

	char *messageHeader;    //Message Header Character Stream Pointer
	char *message;			//MEssage Character Stream
	void *messageStruct; 	//Generic pointer to point to any type of below structures

	int myMessage;
	// Key to access Connection Details Map
	/*
	 * This is NULL if node is the creater of the message
	 * else it is the key corresponding to the node from where message is received.
	 */
	char* connectedNeighborsMapKey;

	char howMessageFormed;  // Received/Forwarded/Sent Types
							// use r/f/s for these entries

	 struct timeval logTime;
	 time_t timestamp;

	 int sendStatus;
	//bool isResponse;

	/*char *responseHeader;
	struct messageHeader* responseHeader;
	char *messageResponse;
	void *messageResponseStruct;*/

	// (hostname + port) for identification - key for map
	// boolean myMessage; //true if my request, false if forwarded request
	// timestamp
	// count for forwarded message

};


// JOIN Message Structure
struct joinMessage{
	uint32_t hostLocation;
	uint16_t hostPort;
	char hostName[256];
	uint32_t dataLength;
};

//joinResponse Message Structure
struct joinRespMessage{
	unsigned char uoid[21];
	uint32_t distance;
	uint16_t hostPort;
	char hostName[256];
	uint32_t dataLength;

	//string uoidKey;
};

//hello Message Structure
struct helloMessage{
	uint16_t hostPort;
	char hostName[256];
	uint32_t dataLength;	
};

//keep alive Message Structure
//*Note: Not Used or needed as Keep Alive has no message body
struct keepAliveMessage{
	uint32_t dataLength;	
	//empty
};

//notify Message Structure
struct notifyMessage{
	//0: unknown
	//1: user shutdown
	//2: unexpected kill signal received
	//3: self-restart
	uint8_t errorCode; 
	uint32_t dataLength;
};

//check Message Structure
//*Note: Not Used or needed as check Message has no message body
struct checkMessage{
	//emptyMessage
	uint32_t dataLength;
};

//Check Response Message Structure
struct checkRespMessage{
	unsigned char checkMsgHeaderUOID[21];
	uint32_t dataLength;
};

// Status Message Structure
struct statusMessage{
	uint8_t statusType; // 0x01: Neighbors Information
			    // 0x02: Files Information should be sent as reply
	uint32_t dataLength;
};

// Store details for files
// received in Status Response Message
struct fileMetadata{
	// Need to be implemented for Second part of Project

};

// Status Response Message Structure
struct statusRespMessage{
	unsigned char statusMsgHeaderUOID[21];

	uint16_t hostInfoLength; // length has to be 2 bytes for port + strlen of hostName
	uint16_t hostPort;		// host port of node sending the status response
	char hostName[256];		// hostname of node sending the status response

	/* ------------- */
	// no of Data entiries -> for personal programming logic use
	int dataRecordCount;		// count for number of entries response has by the reply
	uint32_t dataLength;
	uint8_t statusType; // store for local Logic implementation
	/* ------------- */

	/* Neighbor Info */
	uint16_t neighHostPorts[100];
	char neighHostNames[100][256];
	uint32_t dataRecordSize[100];

	/* File Info */
	/* For Part 2 */
	struct fileMetadata *fileMetadataObj;


};


void setMessageTime(struct message *);

struct message* createMessageHeaderStruct();
struct joinMessage* createjoinMessageStruct();
struct joinRespMessage* createjoinRespMessageStruct();
struct helloMessage* createhelloMessageStruct();
struct notifyMessage* createnotifyMessageStruct();
struct checkRespMessage* createcheckRespMessageStruct();
struct statusMessage* statusMessageStruct();
struct statusRespMessage* statusRespMessageStruct();

char* createHeader(struct message*);
struct message* parseHeader(char *);
bool createMessageBuffer(struct message *);
bool parseMessage(struct message *);

void displayUOID(unsigned char *);

void printHeader(struct message*);
void printJoinMessage(struct joinMessage* ptr);
void printjoinRespMessage(struct joinRespMessage*);
void printHelloMessage(struct helloMessage *);
void printcheckRespMessage(struct checkRespMessage*);
void printNotifyMessage(struct notifyMessage*);
void printstatusMessage(struct statusMessage*);
void printstatusRespMessage(struct statusRespMessage*);

/*
 *  CREATE MESSAGE FUNCTIONS DECLARATIONS
 */

struct checkRespMessage* createCheckResp(unsigned char *);
struct notifyMessage* createNotify(uint8_t);
struct helloMessage* createHello(char*,uint16_t);
struct joinMessage* createJoin(uint32_t ,uint16_t ,char *);
struct joinRespMessage *createJoinRespMessage(struct joinMessage*, struct message *,struct NodeInfo *);
struct message* CreateMessageHeader(uint8_t /*messType*/,uint8_t /*ttl*/,uint32_t /*dataLength*/,
			char */*messageHeader*/,char */*message*/,void */*messageStruct*/,
			char* /*temp_connectedNeighborsMapKey*/,int /* My Message Or NotMyMessage*/,struct NodeInfo * /*NodeInfo pointer*/);

struct statusMessage* createStatus(uint8_t);
struct statusRespMessage* createStatusResp(struct NodeInfo*,struct message*);
struct statusRespMessage * createSelfStatus(struct NodeInfo *ndInfoTemp,uint8_t statusType);

//Functions to create Message Structures from received Message body character streams
/*
 * @Parameters: character stream pointer + length of the stream to be parsed.
 */
struct checkRespMessage* parseCheckRespMessage(char *,uint32_t);
struct notifyMessage* parseNotifyMessage(char *,uint32_t);
struct helloMessage* parseHelloMessage(char *,uint32_t);
struct joinRespMessage* parseJoinRespMessage(char*,uint32_t);
struct joinMessage* parseJoinMessage(char *,uint32_t);
struct statusMessage* parseStatusMessage(char *buffer,uint32_t);
struct statusRespMessage* parseStatusRespMessage(char *,uint32_t, uint8_t);

/*
 * Pass the structure which has to be converted to the character stream
 */
char *createJoinMessage(struct joinMessage *);
char *createJoinRespMessage(struct joinRespMessage *);
char* createHelloMessage(struct helloMessage* );
char* createCheckRespMessage(struct checkRespMessage *);
char* createCheckMessage(struct checkMessage *);
char* createNotifyMessage(struct notifyMessage *);
char* createKeepAliveMessage(struct keepAliveMessage *);
char* createStatusMessage(struct statusMessage *);
char* createStatusRespMessage(struct statusRespMessage *);


// FUTURE WORK
/* Part 2
struct searchMessage{
	If the search type is 1, the next field contains an exact file name. If the search type is 2, the next field contains an exact SHA1 hash value (not hexstring-encoded). If the search type is 3, the next field contains a list of keywords (separated by space characters).
	uint8_t searchType;
	char *query;
	uint32_t dataLength;
};

struct searchRespMessage{
	uint32_t dataLength;	
};

struct getMessage{
	uint32_t dataLength;
};

struct getRespMessage{
	uint32_t dataLength;
};

struct storeMessage{
	uint32_t dataLength;
};

struct deleteMessage{
	uint32_t dataLength;
};

*/

/*char *GetUOID(char *node_inst_id,char *obj_type,char *uoid_buf,int uoid_buf_sz)
{
	static unsigned long seq_no=(unsigned long)1;
	char sha1_buf[SHA_DIGEST_LENGTH], str_buf[104];
	sprintf(str_buf, "%s_%s_%1ld",
	node_inst_id, obj_type, (long)seq_no++);
	SHA1(str_buf, strlen(str_buf), sha1_buf);
	memset(uoid_buf, 0, uoid_buf_sz);
	memcpy(uoid_buf, sha1_buf,
	min(uoid_buf_sz,sizeof(sha1_buf)));
	return uoid_buf;
}*/
