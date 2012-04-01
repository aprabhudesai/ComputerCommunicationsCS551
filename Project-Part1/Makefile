#make file for project

CC = g++
CFLAGS = -c
WFLAG = -Wall
LFLAG = -lpthread -lsocket -lnsl -I/home/scf-22/csci551b/openssl/include -L/home/scf-22/csci551b/openssl/lib -lcrypto
DFLAG = -g
OBJS = iniparser.o uoid.o keyboardThread.o nonBeaconTimerThread.o beaconTimerThread.o checkThread.o commonFunctions.o reader.o writer.o nonBeaconDispatcher.o dispatcher.o messageHandlers.o beaconNode.o nonBeaconNode.o ntwknode.o 

sv_node: $(OBJS)
	$(CC) $(WFLAG) $(DFLAG) $(OBJS) -o sv_node $(LFLAG)
	
iniparser.o: iniparser.h ntwknode.h iniparser.cpp
	$(CC) $(CFLAGS) $(DFLAG) iniparser.cpp $(WFLAG)
	
keyboardThread.o: keyboardThread.h keyboardThread.cpp headers.h
	$(CC) $(CFLAGS) $(DFLAG) keyboardThread.cpp $(WFLAG)
	
nonBeaconTimerThread.o: nonBeaconTimerThread.cpp headers.h
	$(CC) $(CFLAGS) $(DFLAG) nonBeaconTimerThread.cpp $(WFLAG)
	
beaconTimerThread.o: beaconTimerThread.cpp headers.h
	$(CC) $(CFLAGS) $(DFLAG) beaconTimerThread.cpp $(WFLAG)
	
checkThread.o: checkThread.cpp systemDataStructures.h ntwknode.h headers.h
	$(CC) $(CFLAGS) $(DFLAG) checkThread.cpp $(WFLAG)
	
commonFunctions.o: systemDataStructures.h commonFunctions.cpp headers.h messageStructures.h
	$(CC) $(CFLAGS) $(DFLAG) commonFunctions.cpp $(WFLAG)
	
reader.o: reader.cpp systemDataStructures.h messageStructures.h headers.h
	$(CC) $(CFLAGS) $(DFLAG) reader.cpp $(WFLAG)

writer.o: writer.cpp systemDataStructures.h messageStructures.h headers.h
	$(CC) $(CFLAGS) $(DFLAG) writer.cpp $(WFLAG)

nonBeaconDispatcher.o: nonBeaconDispatcher.cpp systemDataStructures.h messageStructures.h headers.h
	$(CC) $(CFLAGS) $(DFLAG) nonBeaconDispatcher.cpp $(WFLAG)

dispatcher.o: dispatcher.cpp systemDataStructures.h messageStructures.h headers.h
	$(CC) $(CFLAGS) $(DFLAG) dispatcher.cpp $(WFLAG)

messageHandlers.o: messageStructures.h messageHandlers.cpp message.h headers.h
	$(CC) $(CFLAGS) $(DFLAG) messageHandlers.cpp $(WFLAG)
	
beaconNode.o: beaconNode.h beaconNode.cpp headers.h
	$(CC) $(CFLAGS) $(DFLAG) beaconNode.cpp $(WFLAG)
	
nonBeaconNode.o: nonBeaconNode.h nonBeaconNode.cpp headers.h
	$(CC) $(CFLAGS) $(DFLAG) nonBeaconNode.cpp $(WFLAG)
	
ntwknode.o: systemDataStructures.h iniparser.h headers.h ntwknode.cpp
	$(CC) $(CFLAGS) $(DFLAG) ntwknode.cpp $(WFLAG)

uoid.o: uoid.cpp
	$(CC) $(CFLAGS) $(DFLAG) $(WFLAG) -I/home/scf-22/csci551b/openssl/include -L/home/scf-22/csci551b/openssl/lib uoid.cpp
	
clean:
	rm -f sv_node *.o *~
