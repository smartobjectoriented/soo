
obj-y	+= kernel/
obj-y 	+= drivers/
 
obj-$(CONFIG_VIUOC_BACKEND)   += iuoc/

obj-y	+= lib/

obj-$(CONFIG_SOO_RT_APP)   += rtapp.o

CFLAGS_REMOVE_rtapp.o = -mgeneral-regs-only
