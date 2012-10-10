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

	unsigned char fileName[256];
	uint32_t fileSize;
	unsigned char SHA1val[21]; // 20 byte SHA of File
	unsigned char nonce[21];
	unsigned char keywordsWithSpace[2048];
	unsigned char keywords[50][250];
	int keyWordCount;
    unsigned char bitVector[128];
    unsigned char fileType[5];

    unsigned char fileId[21];
    unsigned char password[21];
    char fileWithPath[256];

    uint32_t metaSize; // = File Name + 4 of file Size + 20 SHA1 + 20 nonce +
    						// keywords length including space + 128 bit Vector + file ID 20 + 12 bytes for all <CR> and <LF>
    // DO NOT Include keywordCount, fileType size in this size
    //*** Include <CR><LF> for all entries in the meta Size (there are 6 <CR><LF> in total = 12 bytes)
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
	struct fileMetadata *fileMetadataObj[100];

};


/* -------------------------------------------------- PART 2 -------------------------------------- */

struct searchMessage{
	// If the search type is 1, the next field contains an exact file name.
	// If the search type is 2, the next field contains an exact SHA1 hash value (not hexstring-encoded).
	// If the search type is 3, the next field contains a list of keywords (separated by space characters).

	uint8_t searchType;
	unsigned char query[256];
	uint32_t dataLength;
};

struct searchRespMessage{
	uint32_t dataLength;

	unsigned char searchUOID[21];

	int respCount;

	uint32_t nextlength[100];

	//unsigned char respFileID[100][20];
	struct fileMetadata *respMetadata[100];
};

struct getMessage{
	uint32_t dataLength;

	unsigned char fileName[256];
	unsigned char fileUOID[21];
	unsigned char fileSHA[21];
};

struct getRespMessage{
	uint32_t dataLength;

	unsigned char getUOID[21];

	uint32_t metaLength;
	uint32_t nonFileLength;
	struct fileMetadata *respMetadata;

	FILE *fp;
	//unsigned char *fileData;
};

struct storeMessage{
	uint32_t dataLength;

	uint32_t metaLength;
	uint32_t nonFileLength;
	struct fileMetadata *respMetadata;
	FILE *fp;
	//unsigned char fileData[256];

};

struct deleteMessage{

	uint32_t dataLength;
	//FileName=foo
	 // SHA1=63de...
	  // Nonce=fcca...
	   // Password=bac9...

	unsigned char fileName[256];
	unsigned char fileSHA[21];
	unsigned char nonce[21];
	unsigned char password[21];

	//unsigned char fileSpec[1024];
};

/* ======================================================================================== */




void setMessageTime(struct message *);

struct message* createMessageHeaderStruct();
struct joinMessage* createjoinMessageStruct();
struct joinRespMessage* createjoinRespMessageStruct();
struct helloMessage* createhelloMessageStruct();
struct notifyMessage* createnotifyMessageStruct();
struct checkRespMessage* createcheckRespMessageStruct();
struct statusMessage* statusMessageStruct();
struct statusRespMessage* statusRespMessageStruct();
struct storeMessage * createStoreMessageStruct();
struct getRespMessage* createGetRespMessageStruct();
struct getMessage * createGetMessageStruct();
struct searchRespMessage * createSearchRespMessageStruct();
struct searchMessage* searchMessageStruct();
struct deleteMessage * createDeleteMessageStruct();

struct fileMetadata* createMetadataStruct();

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
void printSearchMessage(struct searchMessage *);

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
struct storeMessage * createStore(struct fileMetadata *);
struct getRespMessage* createGetResp(struct fileMetadata *,struct message *);
struct getMessage *createGet(struct fileMetadata *,unsigned char *);
struct searchRespMessage *createSearchResp(struct fileMetadata * [], int, struct message *);
struct searchMessage* createSearch(unsigned char *, int);
struct deleteMessage *createDelete(unsigned char *);

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
struct getMessage *parseGetMessage(char *, uint32_t);
struct searchRespMessage* parseSearchRespMessage(char *,uint32_t);
struct searchMessage *parseSearchMessage(char *,uint32_t);
struct storeMessage * parseStoreMessage(char *,uint32_t);
struct getRespMessage * parseGetRespMessage(char *,uint32_t);
struct deleteMessage * parseDeleteMessage(char *, uint32_t);

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
char * createStoreMessage(struct storeMessage *);
char * createGetRespMessage(struct getRespMessage *);
char* createGetMessage(struct getMessage *);
char * createSearchRespMessage(struct searchRespMessage *);
char * createSearchMessage(struct searchMessage *);
char * createDeleteMessage(struct deleteMessage *);

// NEW FUNCTIONS ========================

bool handleStoreCommand(unsigned char *, uint8_t, unsigned char *);
struct fileMetadata* createMetadataAndFiles(unsigned char *,unsigned char *,int,unsigned char *, uint32_t);
void parseKeywords(struct fileMetadata*);
void CreateBitVector(struct fileMetadata *);

