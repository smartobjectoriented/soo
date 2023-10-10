#
# Simple Makefile to compile the LZ4 lib and the build_upgrade tool
#

CC=gcc
 
TARGET=build_upgrade

SRC 		= $(TARGET).c
OBJS		= $(SRC:.c=.o)

LZ4_PATH = lib/lz4-1.9.2
LZ4_LIB= $(LZ4_PATH)/lib

INCLUDE = -I. -I$(LZ4_LIB)

all: $(LZ4_LIB)/liblz4.a $(TARGET)

# flags for hardenering & libraries
CFLAGS += $(INCLUDE) -g -pedantic -Wall -Wno-long-long -Wno-variadic-macros #-Wextra -Wcast-align -Wcast-qual -Wmissing-declarations -Wsign-conversion -Wswitch-default -Wundef -Werror -Wno-unused 

LDFLAGS = -lpthread -lc -L$(LZ4_LIB) -llz4

$(TARGET): $(OBJS) 
	$(CC)  $^ -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS)  -c $^

$(LZ4_LIB)/liblz4.a:
	make -C $(LZ4_PATH) lib

clean:
	$(MAKE) -C $(LZ4_PATH) clean
	rm -f build_upgrade
	rm -f *.o *.bin
