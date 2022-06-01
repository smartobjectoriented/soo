################################################################################
#
# python-sense-hat
#
################################################################################

PYTHON_SENSE_EMU_VERSION =
PYTHON_SENSE_EMU_SOURCE = master.zip
PYTHON_SENSE_EMU_SITE = https://github.com/astro-pi/python-sense-emu/archive
PYTHON_SENSE_EMU_SETUP_TYPE = setuptools
PYTHON_SENSE_EMU_LICENSE = Raspberry Pi Foundation
PYTHON_SENSE_EMU_LICENSE_FILES = LICENSE.txt

define PYTHON_SENSE_EMU_EXTRACT_CMDS
	$(UNZIP) -d $(@D) $(PYTHON_SENSE_EMU_DL_DIR)/$(PYTHON_SENSE_EMU_SOURCE)
	mv $(@D)/python-sense-emu-master/* $(@D)
	$(RM) -r $(@D)/python-sense-emu-master
endef
 
$(eval $(python-package))
