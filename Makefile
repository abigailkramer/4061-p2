
CFLAGS = -Wall -g
CC	   = gcc $(CFLAGS)

all : bl_server bl_client bl_showlog

bl_server : bl_server.c
	$(CC) -o $@ $^ server_funcs.c simpio.c util.c -lpthread

bl_client : bl_client.c
	$(CC) -o $@ $^ simpio.c util.c -lpthread

bl_showlog : bl_showlog.c
	$(CC) -o $@ $^ util.c -lpthread

clean: 
	rm -f bl_server bl_client bl_showlog *.fifo *.log

include test_Makefile