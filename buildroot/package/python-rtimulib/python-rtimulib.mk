################################################################################
#
# python-rtimulib
#
################################################################################

PYTHON_RTIMULIB_VERSION =
PYTHON_RTIMULIB_SOURCE = master.zip
PYTHON_RTIMULIB_SITE = https://github.com/RTIMULib/RTIMULib2/archive
PYTHON_RTIMULIB_SETUP_TYPE = distutils
RTIMULIB_TARGET_DIR = $(TARGET_DIR)/usr/lib/python3.10/site-packages
PYTHON_RTIMULIB_LICENSE = Specific licence
PYTHON_RTIMULIB_LICENSE_FILES = LICENSE

PYTHON_RTIMULIB_BUILDDIR = $(@D)/Linux/python
PYTHON_RTIMULIB_BUILDDIR_OUT = $(@D)/Linux/python/build/lib.linux-x86_64-3.8

define PYTHON_RTIMULIB_EXTRACT_CMDS
	$(UNZIP) -d $(@D) $(PYTHON_RTIMULIB_DL_DIR)/$(PYTHON_RTIMULIB_SOURCE)
	mv $(@D)/RTIMULib2-master/* $(@D)
	$(RM) -r $(@D)/RTMIMULib2-master
endef
 
define RTIMULIB_RENAME
	mv $(RTIMULIB_TARGET_DIR)/RTIMU.cpython-310*.so $(RTIMULIB_TARGET_DIR)/RTIMU.so
endef

PYTHON_RTIMULIB_POST_INSTALL_TARGET_HOOKS += RTIMULIB_RENAME

$(eval $(python-package))
