#
# Makefile
#

core_final: build/agency

#
# Ensure linker flags are correct


KBUILD_CFLAGS	+=  -Wall -Wno-long-long -Wno-variadic-macros #-Wextra -Wcast-align -Wcast-qual -Wmissing-declarations -Wsign-conversion -Wswitch-default -Wundef -Werror -W no-unused 

KBUILD_AFLAGS	+=  

obj-y += core.o injector.o leds.o migration/ upgrader/ device_access/

LIBARCHIVE_LIBS = -lz -lexpat -larchive

BR_LIBS_PATH= ../../rootfs/host/arm-buildroot-linux-gnueabihf/sysroot/usr/lib/

LIBZ=libz.so
LIBARCHIVE=libarchive.so
LIBEXPAT=libexpat.so
LIBNETTLE=libnettle.so
LIBLZ4=liblz4.so

build/agency: built-in.o
	$(Q)echo Generating agency application
	$(Q)cp $(BR_LIBS_PATH)$(LIBZ) $(BR_LIBS_PATH)$(LIBARCHIVE) $(BR_LIBS_PATH)$(LIBNETTLE) $(BR_LIBS_PATH)$(LIBEXPAT) $(BR_LIBS_PATH)$(LIBLZ4) $(PWD)/lib/
	$(Q)$(CC) $(LDFLAGS) $(PWD)/core/$< -lpthread -lc -ldcm -llz4 -lnettle -lfrozen $(LIBARCHIVE_LIBS) -o $(PWD)/$@
