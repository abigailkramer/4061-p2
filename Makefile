CFLAGS = -Wall -g
CC	   = gcc $(CFLAGS)

all : bl_server bl_client bl_showlog

bl_server.o : bl_server.c blather.h
	$(CC) -c bl_server.c

bl_client.o : bl_client.c blather.h
	$(CC) -c bl_client.c

bl_showlog.o : bl_showlog.c blather.h
	$(CC) -c bl_showlog.c

server_funcs.o : server_funcs.c blather.h
	$(CC) -c $<

simpio.o : simpio.c blather.h
	$(CC) -c $<

util.o : util.c blather.h
	$(CC) -c $<

bl_server : bl_server.o server_funcs.o simpio.o util.o
	$(CC) -o bl_server bl_server.o server_funcs.o simpio.o util.o -lpthread

bl_client : bl_client.o simpio.o util.o
	$(CC) -o bl_client bl_client.o simpio.o util.o -lpthread

bl_showlog : bl_showlog.o util.o
	$(CC) -o bl_showlog bl_showlog.o util.o

clean:
	rm -f bl_server bl_client bl_showlog *.fifo *.log *.o

include test_Makefile
