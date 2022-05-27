#
# Makefile for the linux kernel.
#

# Object file lists.

obj-y	+= console.o

obj-$(CONFIG_PSCI) += psci_smp.o

obj-y += domctl.o 
obj-y += agency_build.o 
obj-y += ME_build.o
obj-y += domain.o
obj-y += physdev.o
obj-y += setup.o
obj-y += smp.o 
obj-y += time.o

obj-y += migration.o injector.o
obj-y += soo_activity.o

obj-y += bitmap.o
obj-y += domctl.o
obj-y += event_channel.o
obj-y += keyhandler.o
 
obj-y += sched_flip.o 
obj-y += schedule.o
obj-y += softirq.o
obj-y += spinlock.o

obj-y += timer.o

