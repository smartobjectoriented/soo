
obj-y += \
	lcec.o \
	lcec_conf.o \
	lcec_cycle_task.o \
 	lcec_el1252.o \
	lcec_el2252.o \
	lcec_tsd80e.o \
	lcec_ax5100.o \
	lcec_ax5200.o \
	lcec_companion.o \
	lcec_el7411.o \
	lcec_el1xxx.o \
	lcec_el2xxx.o

ifeq ($(ARCH),arm)

# ccflags-y += -DRTAPI -march=armv7-a -mfpu=neon -mfloat-abi=softfp

CFLAGS_lcec_cycle_task.o += -march=armv7-a -mfpu=neon -mfloat-abi=softfp
CFLAGS_lcec_el1252.o     += -march=armv7-a -mfpu=neon -mfloat-abi=softfp
CFLAGS_lcec_el2252.o     += -march=armv7-a -mfpu=neon -mfloat-abi=softfp
CFLAGS_lcec_tsd80e.o     += -march=armv7-a -mfpu=neon -mfloat-abi=softfp
CFLAGS_lcec_ax5100.o     += -march=armv7-a -mfpu=neon -mfloat-abi=softfp
CFLAGS_lcec_ax5200.o     += -march=armv7-a -mfpu=neon -mfloat-abi=softfp
CFLAGS_lcec_el7411.o     += -march=armv7-a -mfpu=neon -mfloat-abi=softfp
CFLAGS_lcec_companion.o  += -march=armv7-a -mfpu=neon -mfloat-abi=softfp
CFLAGS_lcec_el1xxx.o     += -march=armv7-a -mfpu=neon -mfloat-abi=softfp
CFLAGS_lcec_el2xxx.o     += -march=armv7-a -mfpu=neon -mfloat-abi=softfp

else ifeq ($(ARCH),x86)

ccflags-y += -DRTAPI -mhard-float

CFLAGS_REMOVE_lcec.o = -mno-sse -mno-mmx -mno-sse2 -mno-3dnow
CFLAGS_REMOVE_lcec_conf.o = -mno-sse -mno-mmx -mno-sse2 -mno-3dnow
CFLAGS_REMOVE_lcec_el1252.o = -mno-sse -mno-mmx -mno-sse2 -mno-3dnow
CFLAGS_REMOVE_lcec_el2252.o = -mno-sse -mno-mmx -mno-sse2 -mno-3dnow
CFLAGS_REMOVE_lcec_tsd80e.o = -mno-sse -mno-mmx -mno-sse2 -mno-3dnow
CFLAGS_REMOVE_lcec_ax5100.o = -mno-sse -mno-mmx -mno-sse2 -mno-3dnow
CFLAGS_REMOVE_lcec_ax5200.o = -mno-sse -mno-mmx -mno-sse2 -mno-3dnow
CFLAGS_REMOVE_lcec_el7411.o = -mno-sse -mno-mmx -mno-sse2 -mno-3dnow
CFLAGS_REMOVE_lcec_companion.o = -mno-sse -mno-mmx -mno-sse2 -mno-3dnow
CFLAGS_REMOVE_lcec_el1xxx.o = -mno-sse -mno-mmx -mno-sse2 -mno-3dnow
CFLAGS_REMOVE_lcec_el2xxx.o = -mno-sse -mno-mmx -mno-sse2 -mno-3dnow

else ifeq ($(ARCH),arm64)

CFLAGS_REMOVE_lcec.o = -mgeneral-regs-only
CFLAGS_REMOVE_lcec_cycle_task.o = -mgeneral-regs-only
CFLAGS_REMOVE_lcec_conf.o = -mgeneral-regs-only
CFLAGS_REMOVE_lcec_el1252.o = -mgeneral-regs-only
CFLAGS_REMOVE_lcec_el2252.o = -mgeneral-regs-only
CFLAGS_REMOVE_lcec_tsd80e.o = -mgeneral-regs-only
CFLAGS_REMOVE_lcec_ax5100.o = -mgeneral-regs-only
CFLAGS_REMOVE_lcec_ax5200.o = -mgeneral-regs-only
CFLAGS_REMOVE_lcec_el7411.o = -mgeneral-regs-only
CFLAGS_REMOVE_lcec_companion.o = -mgeneral-regs-only
CFLAGS_REMOVE_lcec_el1xxx.o = -mgeneral-regs-only
CFLAGS_REMOVE_lcec_el2xxx.o = -mgeneral-regs-only

endif
