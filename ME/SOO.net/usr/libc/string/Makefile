
-include $(USR_DIR)/Makefile.common

LIB = strcpy strcmp strncmp strlen memcpy strncpy memset memcmp strcat strchrnul strnlen memchr memmove
LIB += stpncpy stpcpy strchr strrchr memrchr strtok strspn strcspn strstr

all: $(patsubst %,%.o,$(LIB))

clean:
	rm -rf *.o

	
