
#matlab_gen_objs:=$(notdir $(patsubst %.c,%.o,$(wildcard source/$(src)/matlab_gen/*.c)))
obj-y += feedopt.o feedopt_fp.o

# -D__bool_true_false_are_defined is for tmwtypes.h, it prevents it from defining true and false
ccflags-y += -I. -Iinclude/opencn \
	 -DRTAPI -Iopencn/rtapi  -D_POSIX_C_SOURCE=199309L

# _POSIX_C_SOURCE must be at least: 199309L

ifeq ($(ARCH),arm)

	CFLAGS_feedopt_fp.o += -march=armv7-a -mfpu=neon -mfloat-abi=softfp
	
else ifeq ($(ARCH),x86)

	ccflags-y += -mhard-float

	CFLAGS_REMOVE := -mno-sse -mno-mmx -mno-sse2 -mno-3dnow -Wframe-larger-than=2048
	CFLAGS_REMOVE_feedopt.o = $(CFLAGS_REMOVE)
	
else ifeq ($(ARCH),arm64)

	CFLAGS_feedopt.o += -mno-outline-atomics 
	CFLAGS_feedopt_fp.o += -mno-outline-atomics 
	
	CFLAGS_REMOVE_feedopt.o = -mgeneral-regs-only 
	CFLAGS_REMOVE_feedopt_fp.o = -mgeneral-regs-only
	
endif
