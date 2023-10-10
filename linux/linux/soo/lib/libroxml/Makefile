obj-y += roxml_buff.o roxml_commit.o roxml_content.o 
obj-y += roxml_core.o roxml_edit.o roxml_mem.o roxml_nav.o
obj-y += roxml_parser.o roxml_xpath.o roxml_file.o

ifeq ($(ARCH),arm)

CFLAGS_roxml_xpath.o += -march=armv7-a -mfpu=neon -mfloat-abi=softfp
else ifeq ($(ARCH),arm64)

CFLAGS_REMOVE_roxml_xpath.o = -mgeneral-regs-only

endif
