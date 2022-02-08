obj-y += opencn.o

obj-$(CONFIG_OPENCN_ETHERCAT_MASTER) += master/
obj-$(CONFIG_OPENCN_BECKHOFF_APP) += beckhoff_app.o

obj-y += lib/

obj-$(CONFIG_OPENCN_COMPANION) += companion/

obj-y += rtapi/ hal/

ifneq ($(CONFIG_OPENCN_COMPANION),y)
	obj-y += components/
endif

ifeq ($(ARCH),x86)
	obj-y += backend/
	obj-y += frontend/
	obj-y += soo/
endif
