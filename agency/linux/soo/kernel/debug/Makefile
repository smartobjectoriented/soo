
obj-y += gpio.o time.o dbgvar.o meminfo.o logbool.o dbglib.o dbgvar.o
obj-y	+= bandwidth.o 
obj-y	+= debug.o

obj-$(CONFIG_ARM) += bandwidth_neon.o

ifeq ($(CONFIG_ARM),y)
CFLAGS_bandwidth_neon.o		:= -mfpu=neon -mfloat-abi=softfp -ffreestanding
endif
