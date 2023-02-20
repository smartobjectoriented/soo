

ifeq ($(CONFIG_SOO_REFSO3),y)

ifeq ($(CONFIG_RAMDEV),y)
dtb-y += refso3_ramfs.dtb
else
	dtb-y += refso3.dtb
endif

endif

dtb-$(CONFIG_SOO_OUTDOOR) += outdoor.dtb
dtb-$(CONFIG_SOO_BLIND) += blind.dtb
dtb-$(CONFIG_SOO_AGENCY) += agency.dtb
dtb-$(CONFIG_SOO_NET) += net.dtb
dtb-$(CONFIG_SOO_LEDCTRL) += ledctrl.dtb
dtb-$(CONFIG_SOO_CHAT) += chat.dtb
dtb-$(CONFIG_SOO_WAGOLED) += wagoled.dtb
dtb-$(CONFIG_SOO_SWITCH_KNX) += switch_knx.dtb
dtb-$(CONFIG_SOO_SWITCH_ENOCEAN) += switch_enocean.dtb
dtb-$(CONFIG_SOO_IUOC) += iuoc.dtb

