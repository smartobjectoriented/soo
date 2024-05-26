
include $(PWD)/../../build.conf


KDIR = $(PWD)/../linux

ifeq ($(PLATFORM),$(filter $(PLATFORM),rpi4 rpi4_64))
obj-m := sensehat.o senseled.o
sensehat-objs += sensehat_mod.o rpisense.o vvext.o 
endif

ifeq ($(PLATFORM),$(filter $(PLATFORM),virt32 virt64 cm4_64))
obj-m := modtry.o
endif


module:
	@make -C $(KDIR) modules_prepare
	@make -C $(KDIR) M=$(PWD)/module modules
	
clean:
	@make -C $(KDIR) M=$(PWD)/module clean

