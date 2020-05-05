
#
# Makefile
#

apps_final: apps_build

# CONFIG variables are not exported, so we need to re-read auto.conf if we want to use them
-include ../include/config/auto.conf

apps_build:
# Smart Object specific apps
ifeq ($(CONFIG_ASF),y)
	make -C asf_usr
endif

ifeq ($(CONFIG_ASF_TA_TST),y)
	make -C asf_ta_tst
endif

clean:
	$(Q)echo "Cleaning apps completed"
	$(Q)make -C asf_usr clean
	$(Q)make -C hello_world_ta_tst clean
