################################################################################
#
# python-rtimulib
#
################################################################################

PYTHON_RTIMULIB_VERSION = 7.2.1
PYTHON_RTIMULIB_SOURCE = V$(PYTHON_RTIMULIB_VERSION).zip
PYTHON_RTIMULIB_SITE = https://github.com/RPi-Distro/RTIMULib/archive/refs/tags
PYTHON_RTIMULIB_SETUP_TYPE = distutils
RTIMULIB_TARGET_DIR = $(TARGET_DIR)/usr/lib/python$(PYTHON3_VERSION_MAJOR)/site-packages
PYTHON_RTIMULIB_LICENSE = Specific licence
PYTHON_RTIMULIB_LICENSE_FILES = LICENSE

PYTHON_RTIMULIB_BUILDDIR = $(@D)/Linux/python

define PYTHON_RTIMULIB_EXTRACT_CMDS
	$(UNZIP) -d $(@D) $(PYTHON_RTIMULIB_DL_DIR)/$(PYTHON_RTIMULIB_SOURCE)
	mv $(@D)/RTIMULib-$(PYTHON_RTIMULIB_VERSION)/* $(@D)
	$(RM) -r $(@D)/RTMIMULib-$(PYTHON_RTIMULIB_VERSION)
endef
 
define RTIMULIB_RENAME
	mv $(RTIMULIB_TARGET_DIR)/RTIMU.cpython-* $(RTIMULIB_TARGET_DIR)/RTIMU.so
endef

PYTHON_RTIMULIB_POST_INSTALL_TARGET_HOOKS += RTIMULIB_RENAME

$(eval $(python-package))
