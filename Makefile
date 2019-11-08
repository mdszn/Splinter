CC=gcc
CFLAGS=-g -Wall -pthread

all: main shell

main: driver.c server.c splinter.c connectioninfo.c
	$(CC) $(CFLAGS) -o splinter driver.c server.c splinter.c connectioninfo.c client.c thread_read.c

shell: splintersh.c
	gcc -Wall splintersh.c -o shell

clean:
	rm -f splinter shell
