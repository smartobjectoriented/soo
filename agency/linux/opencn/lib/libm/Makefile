
# obj-y += k_sin.o s_sin.o
obj-y += s_cos.o k_cos.o k_sin.o  e_rem_pio2.o k_rem_pio2.o s_scalbn.o s_floor.o

ifeq ($(ARCH),arm)

CFLAGS_s_cos.o += -march=armv7-a -mfpu=neon -mfloat-abi=softfp
CFLAGS_k_cos.o += -march=armv7-a -mfpu=neon -mfloat-abi=softfp
CFLAGS_k_sin.o += -march=armv7-a -mfpu=neon -mfloat-abi=softfp
CFLAGS_k_rem_pio2.o += -march=armv7-a -mfpu=neon -mfloat-abi=softfp
CFLAGS_e_rem_pio2.o += -march=armv7-a -mfpu=neon -mfloat-abi=softfp
CFLAGS_s_scalbn.o += -march=armv7-a -mfpu=neon -mfloat-abi=softfp
CFLAGS_s_floor.o += -march=armv7-a -mfpu=neon -mfloat-abi=softfp

else ifeq ($(ARCH),arm64)

obj-y += frexp.o

CFLAGS_REMOVE_s_cos.o += -mgeneral-regs-only
CFLAGS_REMOVE_k_cos.o += -mgeneral-regs-only
CFLAGS_REMOVE_k_sin.o += -mgeneral-regs-only
CFLAGS_REMOVE_k_rem_pio2.o += -mgeneral-regs-only
CFLAGS_REMOVE_e_rem_pio2.o += -mgeneral-regs-only
CFLAGS_REMOVE_s_scalbn.o += -mgeneral-regs-only
CFLAGS_REMOVE_s_floor.o += -mgeneral-regs-only
CFLAGS_REMOVE_frexp.o += -mgeneral-regs-only

endif
