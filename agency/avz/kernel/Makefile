#
# Makefile for the linux kernel.
#

# Object file lists.

obj-y	:= entry-armv.o
obj-y	+= entry-common.o
obj-y	+= hypercalls.o
obj-y	+= setup-arch.o
obj-y	+= traps.o
obj-y	+= compat.o
obj-y	+= smpboot.o
obj-y	+= vfp.o
obj-y	+= console.o

obj-$(CONFIG_PSCI) += psci_smp.o
obj-$(CONFIG_PSCI) += smccc-call.o

obj-y	+= debug.o

obj-y += backtrace.o
obj-y += domctl.o 
obj-y += agency_build.o ME_build.o
obj-y += domain.o
obj-y += irq.o 
obj-y += physdev.o
obj-y += setup.o
obj-y += smpboot.o
obj-y += smp.o 
obj-y += time.o
obj-y += dom_mig_transfer.o dom_mig_restore.o
obj-y += soo_activity.o

obj-y += hyp-stub.o

obj-y += bitmap.o
obj-y += cpu.o
obj-y += domctl.o
obj-y += event_channel.o
obj-y += keyhandler.o
obj-y += lib.o
 
obj-y += sched_flip.o sched_rt.o
obj-y += schedule.o
obj-y += softirq.o
obj-y += spinlock.o
obj-y += string.o

obj-y += timer.o
obj-y += vsprintf.o

obj-y += head.o
