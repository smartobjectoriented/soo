
include $(PWD)/../build.conf


KDIR = ../../linux

ifeq ($(PLATFORM),$(filter $(PLATFORM),rpi4))
#obj-m := senseled.o
endif

ifeq ($(PLATFORM),$(filter $(PLATFORM),vexpress))
obj-m := modtry.o
endif


module:
	@make -C $(KDIR) M=$(PWD)/module modules
	
clean:
	@make -C $(KDIR) M=$(PWD)/module clean

