
include $(PWD)/../build.conf


KDIR = $(PWD)/../linux

ifeq ($(PLATFORM),$(filter $(PLATFORM),rpi4))
obj-m := senseled.o
endif

ifeq ($(PLATFORM),$(filter $(PLATFORM),vexpress))
obj-m := modtry.o
endif


module:
	@make -C $(KDIR) modules_prepare
	@make -C $(KDIR) M=$(PWD)/module modules
	
clean:
	@make -C $(KDIR) M=$(PWD)/module clean

