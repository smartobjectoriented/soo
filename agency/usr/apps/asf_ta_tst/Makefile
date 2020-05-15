#
# Makefile
#

asf_ta_final: build/asf_ta_tst

KBUILD_CFLAGS	+=  -Wall -Wno-long-long -Wno-variadic-macros
KBUILD_AFLAGS	+=

%.o: %.c
	$(CC) -o $@ -I../../include -c $< $(CFLAGS)

build/asf_ta_tst: asf_ta_tst.o
	$(Q)$(CC) $(LDFLAGS) $<  -o $(PWD)/$@

clean:
	@rm -f *.o *.a
