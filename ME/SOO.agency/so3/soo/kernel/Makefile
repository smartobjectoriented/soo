#
# Makefile for the linux kernel.
#
#

obj-y	+= hypervisor.o
obj-y	+= evtchn.o
obj-y	+= gnttab.o
obj-y	+= vbus/ vbstore/ soo/ console/ debug/ 

# dependencies on generated files need to be listed explicitly

mrproper clean:
	@find . -name "*.o" | xargs rm -f


