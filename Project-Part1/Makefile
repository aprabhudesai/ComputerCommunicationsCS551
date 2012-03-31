#make file for project

CC = g++
CFLAGS = -c
WFLAG = -Wall
LFLAG = -lpthread -lsocket -lnsl -I/home/scf-22/csci551b/openssl/include -L/home/scf-22/csci551b/openssl/lib -lcrypto
DFLAG = -g
OBJS = iniparser.o uoid.o keyboardThread.o nonBeaconTimerThread.o beaconTimerThread.o checkThread.o commonFunctions.o reader.o writer.o nonBeaconDispatcher.o dispatcher.o messageHandlers.o beaconNode.o nonBeaconNode.o ntwknode.o 

sv_node: $(OBJS)
	$(CC) $(WFLAG) $(OBJS) -o sv_node $(LFLAG)
	
iniparser.o: iniparser.h ntwknode.h iniparser.cpp
	$(CC) $(CFLAGS) iniparser.cpp $(WFLAG)
	
keyboardThread.o: keyboardThread.h keyboardThread.cpp headers.h
	$(CC) $(CFLAGS) keyboardThread.cpp $(WFLAG)
	
nonBeaconTimerThread.o: nonBeaconTimerThread.cpp headers.h
	$(CC) $(CFLAGS) nonBeaconTimerThread.cpp $(WFLAG)
	
beaconTimerThread.o: beaconTimerThread.cpp headers.h
	$(CC) $(CFLAGS) beaconTimerThread.cpp $(WFLAG)
	
checkThread.o: checkThread.cpp systemDataStructures.h ntwknode.h headers.h
	$(CC) $(CFLAGS) checkThread.cpp $(WFLAG)
	
commonFunctions.o: systemDataStructures.h commonFunctions.cpp headers.h messageStructures.h
	$(CC) $(CFLAGS) commonFunctions.cpp $(WFLAG)
	
reader.o: reader.cpp systemDataStructures.h messageStructures.h headers.h
	$(CC) $(CFLAGS) reader.cpp $(WFLAG)

writer.o: writer.cpp systemDataStructures.h messageStructures.h headers.h
	$(CC) $(CFLAGS) writer.cpp $(WFLAG)

nonBeaconDispatcher.o: nonBeaconDispatcher.cpp systemDataStructures.h messageStructures.h headers.h
	$(CC) $(CFLAGS) nonBeaconDispatcher.cpp $(WFLAG)

dispatcher.o: dispatcher.cpp systemDataStructures.h messageStructures.h headers.h
	$(CC) $(CFLAGS) dispatcher.cpp $(WFLAG)

messageHandlers.o: messageStructures.h messageHandlers.cpp message.h headers.h
	$(CC) $(CFLAGS) messageHandlers.cpp $(WFLAG)
	
beaconNode.o: beaconNode.h beaconNode.cpp headers.h
	$(CC) $(CFLAGS) beaconNode.cpp $(WFLAG)
	
nonBeaconNode.o: nonBeaconNode.h nonBeaconNode.cpp headers.h
	$(CC) $(CFLAGS) nonBeaconNode.cpp $(WFLAG)
	
ntwknode.o: systemDataStructures.h iniparser.h headers.h ntwknode.cpp
	$(CC) $(CFLAGS) ntwknode.cpp $(WFLAG)

uoid.o: uoid.cpp
	$(CC) $(CFLAGS) $(WFLAG) -I/home/scf-22/csci551b/openssl/include -L/home/scf-22/csci551b/openssl/lib uoid.cpp
	
clean:
	rm -f sv_node *.o *~
