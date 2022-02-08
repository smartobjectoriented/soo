
obj-y += opencn_rtapi.o

ccflags-y += -Iopencn/rtapi -D RTAPI

ifeq ($(ARCH),x86)

CFLAGS_opencn_rtapi.o += -mhard-float
CFLAGS_REMOVE_opencn_rtapi.o = -mno-sse -mno-mmx -mno-sse2 -mno-3dnow

else ifeq ($(ARCH),arm64)

CFLAGS_REMOVE_opencn_rtapi.o = -mgeneral-regs-only

endif
