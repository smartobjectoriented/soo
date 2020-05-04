#
# Makefile
#

hello_world_ta_final: build/hello_world_ta_tst

KBUILD_CFLAGS	+=  -Wall -Wno-long-long -Wno-variadic-macros
KBUILD_AFLAGS	+=

%.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS)

build/hello_world_ta_tst: hello_world_ta_tst.o
	$(Q)$(CC) $(LDFLAGS) $<  -o $(PWD)/$@

clean:
	@rm -f *.o *.a
