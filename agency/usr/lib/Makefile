#
# Makefile
#

-include ../include/config/auto.conf

LIBZ4_DIR = lz4-1.9.2
LIBFROZEN_DIR = frozen
LIBUNTAR_DIR = untar

LIBS = dcm 
LIBS += frozen

ifdef CONFIG_LZ4
LIBS += lz4
endif

lib_final: $(patsubst %, lib%.a, $(LIBS))

liblz4.a: ../$(LIBZ4_DIR)/liblz4.a
	$(Q)cp ../$(LIBZ4_DIR)/lib/liblz4.a $@
../$(LIBZ4_DIR)/liblz4.a:
	$(Q)make -C ../$(LIBZ4_DIR) lib

libfrozen.a: ../$(LIBFROZEN_DIR)/libfrozen.a
	$(Q)cp ../$(LIBFROZEN_DIR)/libfrozen.a $@
../$(LIBFROZEN_DIR)/libfrozen.a:
	$(Q)$(MAKE) -C ../$(LIBFROZEN_DIR) frozen_final
	
lib%.a: ../%/built-in.o
	$(Q)$(AR) rcs$(KBUILD_ARFLAGS) $@  $<

clean:
	$(Q)rm -rf *.so
	$(Q)$(MAKE) -C ../$(LIBZ4_DIR) clean
	$(Q)$(MAKE) -C ../$(LIBFROZEN_DIR) clean
