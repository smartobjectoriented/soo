
obj-y += streamer.o

CFLAGS_streamer.o += -DRTAPI

ifeq ($(ARCH),arm64)
CFLAGS_REMOVE_streamer.o = -mgeneral-regs-only
endif