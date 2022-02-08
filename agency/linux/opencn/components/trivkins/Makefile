
obj-y += trivkins.o

CFLAGS_trivkins.o += -DRTAPI

ifeq ($(ARCH),arm64)
CFLAGS_REMOVE_trivkins.o = -mgeneral-regs-only
endif
