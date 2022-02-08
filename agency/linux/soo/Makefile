
obj-y   += sooenv.o

obj-y	+= kernel/
obj-y 	+= drivers/
obj-y	+= core/

ifneq ($(CONFIG_X86),y)
	obj-y 	+= soolink/
endif

obj-$(CONFIG_SOO_RT_APP)   += rtapp.o

CFLAGS_REMOVE_rtapp.o = -mgeneral-regs-only
