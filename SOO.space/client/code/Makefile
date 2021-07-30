SHELL := /bin/bash

TARGET = main
FILE_SIZE = 1024 # 1KB

SRV_FILE=send_data.bin
CLIENT_FILE=rcv_data.bin
LIBS = -lpthread
CC = gcc
CFLAGS = -g -Wall -Werror -DFILE_SIZE=$(FILE_SIZE) -DSRV_FILE=\"$(SRV_FILE)\" -DCLIENT_FILE=\"$(CLIENT_FILE)\" 

.PHONY: default all clean

default: $(TARGET)
all: $(TARGET) run verify

OBJECTS = $(patsubst %.c, %.o, $(wildcard *.c))
HEADERS = $(wildcard *.h)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) $(LIBS) -o $@

$(SRV_FILE):
	dd if=/dev/urandom of=$(SRV_FILE) bs=4096 count=$(FILE_SIZE) iflag=count_bytes

run: $(TARGET) $(SRV_FILE)
	./$(TARGET)

verify:
	cat $(SRV_FILE) | cmp $(CLIENT_FILE) || true

clean:
	rm -f *.o
	rm -f $(TARGET)
	rm -f $(SRV_FILE)
	rm -f $(CLIENT_FILE)
