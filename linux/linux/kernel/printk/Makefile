# SPDX-License-Identifier: GPL-2.0-only
obj-y	= printk.o
obj-$(CONFIG_PRINTK)	+= printk_safe.o
obj-$(CONFIG_A11Y_BRAILLE_CONSOLE)	+= braille.o
obj-$(CONFIG_PRINTK)	+= printk_ringbuffer.o

ifneq ($(CONFIG_SOO),y)
obj-$(CONFIG_LINUXVIRT) += lprintk.o
endif
