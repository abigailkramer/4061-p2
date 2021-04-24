
CFLAGS = -Wall -g
CC	   = gcc $(CFLAGS)

all : blather

bl_server.o : bl_server.c server_funcs.c simpio.c util.c blather.h
	$(CC) -c bl_server.c

bl_client.o : bl_client.c simpio.c util.c blather.h
	$(CC) -c bl_client.c

server_funcs.o : server_funcs.c blather.h
	$(CC) -c $<

simpio.o : simpio.c blather.h
	$(CC) -c $<

util.o : util.c blather.h
	$(CC) -c $<

#clean: 
#	rm -f commando *.o

include test_Makefile