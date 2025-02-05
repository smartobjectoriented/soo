#
# Makefile for the linux kernel.
#
#
obj-y += setup.o
obj-y += sysfs.o
obj-y += hypervisor.o
obj-y += evtchn.o
obj-y += gnttab.o
obj-y += soo_guest_activity.o

obj-y += core.o crc32.o 

obj-y += injector.o
obj-y += me_access.o

obj-y += console/ debug/  
obj-y += vbus/ vbstore/

obj-$(CONFIG_SOO_CORE_ASF) += asf/

obj-$(CONFIG_ARM) += arm32/
obj-$(CONFIG_ARM64) += arm64/
obj-$(CONFIG_X86) += x86/

obj-$(CONFIG_SOO_CORE_ASF) += asf/
 
# dependencies on generated files need to be listed explicitly

mrproper clean:
	@find . -name "*.o" | xargs rm -f


