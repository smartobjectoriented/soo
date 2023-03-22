#
# Smart Object Oriented technology (SOO.tech)
#

###############################################################################
# This file should not be modified, all configuraton is in environment.conf
###############################################################################
SHELL   := /bin/bash

DOMAINS_MAKEFLAGS := -rR --no-print-directory

.PHONY: all clean FORCE

###############################################################################
# Import configuration
###############################################################################
-include build.conf       # Load the compilation parameters

ifneq ("$(TYPE)", "")
  PLATFORM_TYPE=${PLATFORM}_${TYPE}
else
  PLATFORM_TYPE=${PLATFORM}
endif

# Receipts to generate the right configuration (defconfig) for Linux and AVZ
LINUX_CONF :=

ifeq ("$(PLATFORM)", "vexpress")
  LINUX_CONF := vexpress_defconfig
  AVZ_CONF := $(PLATFORM_TYPE)_defconfig
endif

ifeq ("$(PLATFORM)", "merida")
  LINUX_CONF := $(PLATFORM_TYPE)_defconfig
  AVZ_CONF := merida_defconfig
endif

ifeq ("$(PLATFORM)", "rpi4")
  LINUX_CONF := $(PLATFORM_TYPE)_defconfig
  AVZ_CONF := rpi4_defconfig
endif

ifeq ("$(PLATFORM)", "rpi4_64")
  LINUX_CONF := $(PLATFORM_TYPE)_defconfig
  AVZ_CONF := rpi4_64_defconfig
endif

ifeq ("$(PLATFORM)", "cm4_64")
  LINUX_CONF := $(PLATFORM_TYPE)_defconfig
  AVZ_CONF := cm4_64_defconfig
endif

ifeq ("$(PLATFORM)", "virt64")
  LINUX_CONF := $(PLATFORM)_defconfig
  AVZ_CONF := $(PLATFORM)_defconfig
endif

ifeq ("$(PLATFORM)", "")
  PHONY += help
  error:
	echo "Wrong configuration..."
endif

all: agency_domain agency_avz

###############################################################################
# Configuration done, targets which actually do the work 
###############################################################################

agency_avz:  FORCE
	@platform=$$(echo "$*" | cut -d. -f1); \
	echo "---------------------------------------------[ BUILDING AVZ hypervisor ]---"; \
	make -C avz $(AVZ_CONF)
	make -C avz -j8; \
	echo "----------------------------------------[ BUILDING AVZ DONE ]---"; 

agency_domain:
	@platform=$$(echo "$*" | cut -d. -f1); \
	echo "---------------------------------------------[ BUILDING linux ]---"; \
	make -C linux $(LINUX_CONF)
	make -C linux $(DOMAINS_MAKEFLAGS) Image -j8; \
	make -C linux $(DOMAINS_MAKEFLAGS) dtbs; \
	echo "----------------------------------------[ BUILDING linux DONE ]---"; 



