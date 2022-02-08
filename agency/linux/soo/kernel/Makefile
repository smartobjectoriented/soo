#
# Makefile for the linux kernel.
#
#

obj-y	+= console/ debug/ 
obj-y	+= vbus/ vbstore/

# OpenCN
ifneq ($(CONFIG_X86),y)
	obj-y	+= vbus_me/
endif

obj-$(CONFIG_ARM) += arm32/
obj-$(CONFIG_ARM64) += arm64/
obj-$(CONFIG_X86) += x86/

obj-y   += setup.o
obj-y	+= hypervisor.o
obj-y	+= evtchn.o
obj-y	+= gnttab.o
obj-y 	+= soo_guest_activity.o

# dependencies on generated files need to be listed explicitly

mrproper clean:
	@find . -name "*.o" | xargs rm -f


