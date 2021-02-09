#
# Makefile for the linux kernel.
#
#

obj-y	+= console/ debug/ 
obj-y	+= vbus/ vbstore/

obj-y   += setup.o
obj-y	+= hypervisor.o
obj-y	+= evtchn.o
obj-y	+= gnttab.o
obj-y 	+= soo_guest_activity.o
obj-y 	+= soo_profile.o
obj-y	+= domcall.o

# dependencies on generated files need to be listed explicitly

mrproper clean:
	@find . -name "*.o" | xargs rm -f


