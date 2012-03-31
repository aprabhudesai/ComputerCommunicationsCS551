
## This is the Makefile for Warmup project 1
## It compiles the client and server codes

CC = gcc
CFLAGS = -I/home/scf-22/csci551b/openssl/include
FLAGS1 = -lsocket -lnsl
FLAGS3 = -lcrypto
WFLAG = -Wall
LIBS = -L/home/scf-22/csci551b/openssl/lib
OBJ1 = client.o messageFunctions.o
OBJ2 = server.o messageFunctions.o
	
all: client server
	
client: $(OBJ1)
	$(CC) $(CFLAGS) -o client client.o messageFunctions.o $(LIBS) $(FLAGS1) $(FLAGS3) $(WFLAG)
	
server:$(OBJ2)
	$(CC) -o server server.o messageFunctions.o $(FLAGS1) $(WFLAG)
	
clean:
	rm -f client server *.o
