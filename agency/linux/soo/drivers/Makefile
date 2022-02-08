#
# Makefile for SOO general drivers.
#

obj-y += vdevback.o
obj-y += vdevfront.o

obj-$(CONFIG_VDUMMY_BACKEND) += vdummyback/ 
obj-$(CONFIG_VUART_BACKEND) += vuartback/
obj-$(CONFIG_VUIHANDLER_BACKEND) += vuihandlerback/
obj-$(CONFIG_VINPUT_BACKEND)	+= vinputback/
obj-$(CONFIG_VLEDS_BACKEND)		+= vledsback/
obj-$(CONFIG_VFB_BACKEND)  += vfbback/
obj-$(CONFIG_VNET_BACKEND)  += vnetback/

obj-$(CONFIG_VUART_FRONTEND) += vuartfront/
obj-$(CONFIG_VDUMMY_FRONTEND) += vdummyfront/
