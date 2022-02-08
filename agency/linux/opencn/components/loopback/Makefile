
obj-y += loopback.o

CFLAGS_loopback.o += -DRTAPI

ifeq ($(ARCH),arm64)
CFLAGS_REMOVE_loopback.o = -mgeneral-regs-only
endif
