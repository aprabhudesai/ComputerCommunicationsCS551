/* ------------------------------------------------------- */
int main(){

	char *mHeaderStream;
	struct message* mHeader;
	mHeader = createMessageHeaderStruct();
	char *buffer;
	/* ==========CREATE HEADER PARSE HEADER CHECK============ */
	mHeader -> messType = 0xFC;
	strcpy(mHeader -> uoid,"abcdefghijklmnopqrst");
	mHeader -> ttl = 5;		
	mHeader -> resv = '0';			
	mHeader -> dataLength = 32;
	
	mHeaderStream = createHeader(mHeader);

	printf("\nHeader Creation Done Done\n");

	struct message* dummy;

	dummy = parseHeader(mHeaderStream);

	
	printf("Message Type \'%02x\'\n",dummy -> messType);
	printf("UOID \'%s\'\n",dummy -> uoid);
	printf("TTL \'%d\'\n",dummy -> ttl);
	printf("Data Length \'%d\'\n",dummy -> dataLength);
	
	/* ============CREATE HEADER PARSE HEADER CHECK=========== */
	printf("\n=============JOIN MESSAGE CHECK====================\n");
	
	struct joinMessage* joinMessageObj = (struct joinMessage*)malloc(sizeof(struct joinMessage));

	joinMessageObj -> hostLocation = 123;  // 4byte
	joinMessageObj -> hostPort = 123;   // 2 byte
	strcpy(joinMessageObj -> hostName,"google.com"); // 10 byte
	joinMessageObj -> dataLength = 16; 
	
	buffer = createJoinMessage(joinMessageObj);

	free(joinMessageObj);
	joinMessageObj = NULL;
	joinMessageObj = parseJoinMessage(buffer,16);

	printf("Host Location \'%d\'\n",joinMessageObj -> hostLocation);
	printf("Host Port \'%d\'\n",joinMessageObj -> hostPort);
	printf("Host Name \'%s\'\n",joinMessageObj -> hostName);

	free(buffer);
	buffer = NULL;
	free(joinMessageObj);
	printf("\n==============JOIN RESPONSE MESSAGE CHECK===================\n");

	struct joinRespMessage* joinRespMessageObj = createjoinRespMessageStruct();
	strcpy(joinRespMessageObj -> uoid,"abcdefghijklmnopqrst");  //21 bytes
	joinRespMessageObj -> distance = 123;  // 4 bytes
	joinRespMessageObj -> hostPort = 321;			// 2 bytes
	strcpy(joinRespMessageObj -> hostName,"google.com"); // 10 bytes
	joinRespMessageObj -> dataLength = 37;

	buffer = createJoinRespMessage(joinRespMessageObj);

	free(joinRespMessageObj);
	joinRespMessageObj = NULL;

	joinRespMessageObj = parseJoinRespMessage(buffer,37);
	printf("UOID \'%s\'\n",joinRespMessageObj -> uoid);
	printf("Host Location Distance \'%d\'\n",joinRespMessageObj -> distance);
	printf("Host Port \'%d\'\n",joinRespMessageObj -> hostPort);
	printf("Host Name \'%s\'\n",joinRespMessageObj -> hostName);

	free(buffer);
	free(joinRespMessageObj);
	buffer = NULL;
	printf("\n==============HELLO MESSAGE CHECK===================\n");

	struct helloMessage* helloMessageObj = createhelloMessageStruct();
	helloMessageObj -> hostPort = 6556; //2 bytes
	strcpy(helloMessageObj -> hostName,"google.com"); //10 bytes
	helloMessageObj -> dataLength = 12;

	buffer = createHelloMessage(helloMessageObj);
	free(helloMessageObj);
	helloMessageObj = NULL;
	helloMessageObj = parseHelloMessage(buffer,12);
	printf("Host Port \'%d\'\n",helloMessageObj -> hostPort);
	printf("Host Name \'%s\'\n",helloMessageObj -> hostName);
	free(buffer);
	buffer = NULL;
	free(helloMessageObj);
	helloMessageObj = NULL;

	printf("\n==============Keep Alive MESSAGE CHECK===================\n");
	printf("It has no body\n\n");
	
	printf("\n==============NOTIFY MESSAGE CHECK===================\n");
	struct notifyMessage* notifyMessageObj = createnotifyMessageStruct();
	notifyMessageObj -> errorCode = 3;
	notifyMessageObj -> dataLength = 1;

	buffer = createNotifyMessage(notifyMessageObj);
	free(notifyMessageObj);
	notifyMessageObj = NULL;

	notifyMessageObj = parseNotifyMessage(buffer, 1);
	printf("\nError Status \'%d\'\n",notifyMessageObj -> errorCode);

	printf("\n==============CHECK MESSAGE CHECK===================\n");
	printf("It has no body\n\n");

	printf("\n============CHECK RESPONSE MESSAGE CHECK===================\n");
	struct checkRespMessage* checkRespMessageObj = createcheckRespMessageStruct();
	strcpy(checkRespMessageObj -> checkMsgHeaderUOID, "abcdefghijklmnopqrst");
	
	buffer = createCheckRespMessage(checkRespMessageObj);
	free(checkRespMessageObj);
	checkRespMessageObj = NULL;

	checkRespMessageObj = parseCheckRespMessage(buffer, 20);

	printf("\nCheck Response UOID \'%s\'\n",checkRespMessageObj -> checkMsgHeaderUOID);

	printf("\n++++++++++++++++++++All Check done++++++++++++++++++++++++++\n");
	
}

