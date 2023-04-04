# SPDX-License-Identifier: GPL-2.0
dtb-$(CONFIG_ARCH_BCM2835) += bcm2711-rpi-4-b.dtb

subdir-y	+= northstar2
subdir-y	+= stingray

# Enable fixups to support overlays on BCM2835 platforms
ifeq ($(CONFIG_ARCH_BCM2835),y)
	DTC_FLAGS ?= -@
endif
