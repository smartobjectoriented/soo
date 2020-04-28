#
# Makefile
#

asf_final: build/asf_usr

KBUILD_CFLAGS	+=  -Wall -Wno-long-long -Wno-variadic-macros
KBUILD_AFLAGS	+=

%.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS)

build/asf_usr: asf_usr.o
	$(Q)$(CC) $(LDFLAGS) $<  -o $(PWD)/$@

clean:
	@rm -f *.o *.a
