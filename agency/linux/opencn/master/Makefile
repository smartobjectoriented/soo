

# obj-$(CONFIG_OPENCN_ETHERCAT_MASTER) += ec_master.o

obj-$(CONFIG_OPENCN_ETHERCAT_MASTER) += \
	coe_emerg_ring.o \
	datagram.o \
	datagram_pair.o \
	device.o \
	domain.o \
	fmmu_config.o \
	foe_request.o \
	fsm_change.o \
	fsm_coe.o \
	fsm_foe.o \
	fsm_master.o \
	fsm_pdo.o \
	fsm_pdo_entry.o \
	fsm_sii.o \
	fsm_slave.o \
	fsm_slave_config.o \
	fsm_slave_scan.o \
	fsm_soe.o \
	mailbox.o \
	master.o \
	pdo.o \
	pdo_entry.o \
	pdo_list.o \
	reg_request.o \
	sdo.o \
	sdo_entry.o \
	sdo_request.o \
	slave.o \
	slave_config.o \
	soe_errors.o \
	soe_request.o \
	sync.o \
	sync_config.o \
	voe_handler.o \
	module.o \
	cdev.o \
	ioctl.o

#	utils.o

# removed file from original ec_master src tree
# rtdm-ioctl.o: ioctl interfaces
# rtdm.o

# ifeq (@ENABLE_EOE@,1)
# ec_master-objs += ethernet.o
# endif

# ifeq (@ENABLE_DEBUG_IF@,1)
# ec_master-objs += debug.o
# endif

# CFLAGS_rtdm.o := -I@include/xenomai/include
# CFLAGS_rtdm-ioctl.o := -DEC_IOCTL_RTDM

CFLAGS_rtdm-ioctl.o := -DEC_IOCTL_RTDM
# EXTRA_CFLAGS += -DEC_IOCTL_RTDM

# REV := $(shell if test -s $(src)/../revision; then \
# 		cat $(src)/../revision; \
# 	else \
# 		hg id -i $(src)/.. 2>/dev/null || echo "unknown"; \
# 	fi)

# CFLAGS_module.o := -DREV=$(REV)

#------------------------------------------------------------------------------
