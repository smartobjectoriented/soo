################################################################################
#
# python-sense-hat
#
################################################################################

PYTHON_SENSE_HAT_VERSION = 2.5.0
PYTHON_SENSE_HAT_SITE = https://github.com/astro-pi/python-sense-hat/archive
PYTHON_SENSE_HAT_SOURCE =  v$(PYTHON_SENSE_HAT_VERSION).tar.gz

PYTHON_SENSE_HAT_SETUP_TYPE = setuptools
#SENSE_HAT_TARGET_DIR = $(TARGET_DIR)/usr/lib/python3.8/site-packages
PYTHON_SENSE_HAT_LICENSE = Raspberry Pi Foundation
PYTHON_SENSE_HAT_LICENSE_FILES = LICENSE.txt

#PYTHON_SENSE_HAT_BUILDDIR = $(@D)/Linux/python
#PYTHON_SENSE_HAT_BUILDDIR_OUT = $(@D)/Linux/python/build/lib.linux-x86_64-3.8

# define PYTHON_SENSE_HAT_EXTRACT_CMDS
# 	$(UNZIP) -d $(@D) $(PYTHON_SENSE_HAT_DL_DIR)/$(PYTHON_SENSE_HAT_SOURCE)
# 	mv $(@D)/python-sense-hat-master/* $(@D)
# 	$(RM) -r $(@D)/python-sense-hat-master
# endef
 
#define SENSE_HAT_RENAME
#	mv $(SENSE_HAT_TARGET_DIR)/RTIMU.cpython-38-arm-linux-gnueabihf.so $(SENSE_HAT_TARGET_DIR)/RTIMU.so
#endef

#PYTHON_SENSE_HAT_POST_INSTALL_TARGET_HOOKS += SENSE_HAT_RENAME

$(eval $(python-package))
