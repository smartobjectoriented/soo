
obj-y += sampler.o

CFLAGS_sampler.o += -DRTAPI

#CFLAGS_sampler.o += -mhard-float -DRTAPI
#CFLAGS_REMOVE_sampler.o = -mno-sse -mno-mmx -mno-sse2 -mno-3dnow

ifeq ($(ARCH),arm64)
CFLAGS_REMOVE_sampler.o = -mgeneral-regs-only
endif
