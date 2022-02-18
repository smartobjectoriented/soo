# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the Atmel network device drivers.
#
macb-y	:= macb_main.o

ifeq ($(CONFIG_MACB_USE_HWSTAMP_ETHERCAT),y)
macb-y	+= macb_ptp.o
endif

obj-$(CONFIG_MACB_ETHERCAT) += macb.o
obj-$(CONFIG_MACB_PCI_ETHERCAT) += macb_pci.o
