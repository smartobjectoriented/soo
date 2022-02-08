
obj-y += hal_lib.o halcmd.o

ccflags-y += -I include -Iopencn/rtapi -DRTAPI

ifeq ($(ARCH),x86)

CFLAGS_halcmd.o += -mhard-float 
CFLAGS_REMOVE_halcmd.o = -mno-sse -mno-mmx -mno-sse2 -mno-3dnow

else ifeq ($(ARCH),arm64)

CFLAGS_REMOVE_hal_lib.o = -mgeneral-regs-only
CFLAGS_REMOVE_halcmd.o = -mgeneral-regs-only

endif
