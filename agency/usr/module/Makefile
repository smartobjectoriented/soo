

KDIR = ../../linux

obj-m := senseled.o

module:
	@make -C $(KDIR) M=$(PWD)/module modules
	
clean:
	@make -C $(KDIR) M=$(PWD)/module clean

