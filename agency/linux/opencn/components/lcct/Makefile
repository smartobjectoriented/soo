
obj-y += lcct.o lcct_rt.o lcct_home.o lcct_jog.o lcct_stream.o lcct_gcode.o

ccflags-y += -DRTAPI

ifeq ($(ARCH),arm)

	CFLAGS_lcct_rt.o += -march=armv7-a -mfpu=neon -mfloat-abi=softfp
	CFLAGS_lcct_jog.o += -march=armv7-a -mfpu=neon -mfloat-abi=softfp
	CFLAGS_lcct_stream.o += -march=armv7-a -mfpu=neon -mfloat-abi=softfp
	CFLAGS_lcct_gcode.o += -march=armv7-a -mfpu=neon -mfloat-abi=softfp

else ifeq ($(ARCH),x86)

	ccflags-y += -mhard-float

	CFLAGS_REMOVE_lcct.o = -mno-sse -mno-mmx -mno-sse2 -mno-3dnow
	CFLAGS_REMOVE_lcct_rt.o = -mno-sse -mno-mmx -mno-sse2 -mno-3dnow
	CFLAGS_REMOVE_lcct_home.o = -mno-sse -mno-mmx -mno-sse2 -mno-3dnow
	CFLAGS_REMOVE_lcct_jog.o = -mno-sse -mno-mmx -mno-sse2 -mno-3dnow
	CFLAGS_REMOVE_lcct_stream.o = -mno-sse -mno-mmx -mno-sse2 -mno-3dnow
	CFLAGS_REMOVE_lcct_gcode.o = -mno-sse -mno-mmx -mno-sse2 -mno-3dnow

else ifeq ($(ARCH),arm64)

	CFLAGS_REMOVE_lcct.o = -mgeneral-regs-only
	CFLAGS_REMOVE_lcct_rt.o = -mgeneral-regs-only
	CFLAGS_REMOVE_lcct_home.o = -mgeneral-regs-only
	CFLAGS_REMOVE_lcct_jog.o = -mgeneral-regs-only
	CFLAGS_REMOVE_lcct_stream.o = -mgeneral-regs-only
	CFLAGS_REMOVE_lcct_gcode.o = -mgeneral-regs-only
	
endif
