CC = gcc
CFLAGS = -Wall -Wextra -pthread -g
all: server client
server: server.o graph.o
	$(CC) $(CFLAGS) -o server server.o graph.o

server.o: server.c common.h graph.h
	$(CC) $(CFLAGS) -c server.c
client: client.o graph.o
	$(CC) $(CFLAGS) -o client client.o graph.o

client.o: client.c common.h graph.h
	$(CC) $(CFLAGS) -c client.c
graph.o: graph.c graph.h
	$(CC) $(CFLAGS) -c graph.c
clean:
	rm -f *.o server client

.PHONY: all clean