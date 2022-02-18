obj-y += strtox.o strtod.o 
obj-y += libroxml/

ifeq ($(ARCH),arm)

CFLAGS_strtod.o += -march=armv7-a -mfpu=neon -mfloat-abi=softfp
CFLAGS_vfprintf.o += -march=armv7-a -mfpu=neon -mfloat-abi=softfp

else ifeq ($(ARCH),arm64)

CFLAGS_REMOVE_strtox.o = -mgeneral-regs-only
CFLAGS_REMOVE_strtod.o = -mgeneral-regs-only
CFLAGS_REMOVE_vfprintf.o = -mgeneral-regs-only

endif
