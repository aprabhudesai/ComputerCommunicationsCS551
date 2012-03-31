
## This is the Makefile for Warmup project 2
## It compiles the centralserver, arrivalprocess and server codes

CC = g++
CFLAGS = -c
LFLAGS = -lpthread
WFLAG = -Wall
OBJS = centralserver.o server.o arrivalprocess.o
	
mm2: $(OBJS)
	$(CC) $(LFLAGS) $(WFLAG) $(OBJS) -o mm2
	
server.o: centralserver.h centralserver.cpp server.cpp
	$(CC) $(CFLAGS) server.cpp $(WFLAG)

arrivalprocess.o: centralserver.h centralserver.cpp arrivalprocess.cpp
	$(CC) $(CFLAGS) arrivalprocess.cpp $(WFLAG)
	
centralserver.o: centralserver.h centralserver.cpp server.cpp arrivalprocess.cpp
	$(CC) $(CFLAGS) centralserver.cpp $(WFLAG)
	
clean:
	rm -f mm2 *.o *~
