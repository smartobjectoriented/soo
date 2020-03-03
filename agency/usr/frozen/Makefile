frozen_final: libfrozen.a

libfrozen.a: frozen.o
	$(AR) rcs $@ $^
	
%.o: %.c
	$(CC) -c $^ -o $@

PHONY += FORCE

FORCE:

clean:
	@rm -f *.o *.a