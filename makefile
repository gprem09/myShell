CC = gcc
CFLAGS = -Wall

all: cshell

cshell: cshell.o
	$(CC) $(CFLAGS) cshell.o -o cshell

cshell.o: cshell.c
	$(CC) $(CFLAGS) -c cshell.c -o cshell.o

clean:
	rm -f cshell cshell.o