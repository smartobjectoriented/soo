obj-y += ctype.o logfile.o strings.o strtox.o strtod.o vfprintf.o

ifeq ($(ARCH),arm)

CFLAGS_strtod.o += -march=armv7-a -mfpu=neon -mfloat-abi=softfp
CFLAGS_vfprintf.o += -march=armv7-a -mfpu=neon -mfloat-abi=softfp

obj-y += libm/

else ifeq ($(ARCH),x86)

CFLAGS_strtox.o += -mhard-float  
CFLAGS_REMOVE_strtox.o = -mno-sse -mno-mmx -mno-sse2 -mno-3dnow

CFLAGS_strtod.o += -mhard-float  
CFLAGS_REMOVE_strtod.o = -mno-sse -mno-mmx -mno-sse2 -mno-3dnow

CFLAGS_vfprintf.o += -mhard-float  
CFLAGS_REMOVE_vfprintf.o = -mno-sse -mno-mmx -mno-sse2 -mno-3dnow

else ifeq ($(ARCH),arm64)

CFLAGS_REMOVE_strtox.o = -mgeneral-regs-only
CFLAGS_REMOVE_strtod.o = -mgeneral-regs-only
CFLAGS_REMOVE_vfprintf.o = -mgeneral-regs-only

obj-y += libm/

endif
