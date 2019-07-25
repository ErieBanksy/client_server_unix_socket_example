CC=g++
CFLAGS=-c -Wall
LIBS= -lpthread

all: client_server

client_server: client_server.o
	$(CC) $^ $(LIBS) -o client_server

client_server.o: client_server.cpp
	$(CC) $(CFLAGS) client_server.cpp

clean:
	rm -rf *.o client_server
