
obj-y   += sooenv.o

obj-y	+= kernel/
obj-y 	+= drivers/
obj-y	+= core/
obj-y   += dcm/
obj-y   += iuoc/

obj-y 	+= soolink/
obj-y	+= lib/

obj-$(CONFIG_SOO_RT_APP)   += rtapp.o

CFLAGS_REMOVE_rtapp.o = -mgeneral-regs-only
