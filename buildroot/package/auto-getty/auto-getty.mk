################################################################################
#
# auto-getty
#
################################################################################

#
# Guidelines:
# http://buildroot.uclibc.org/downloads/manual/manual.html#generic-package-tutorial
#

# config
AUTO_GETTY_VERSION = 0.1
AUTO_GETTY_SITE = $(TOPDIR)/package/$(AUTO_GETTY_NAME)/src
AUTO_GETTY_SITE_METHOD = local

# hooks
define AUTO_GETTY_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/auto-getty $(TARGET_DIR)/usr/bin/
	#$(INSTALL) -D -m 0755 $(@D)/S91auto-getty $(TARGET_DIR)/etc/init.d/
	
endef

ifeq ($(BR2_PACKAGE_AUTO_GETTY_ROOT_LOGIN),y)
define AUTO_GETTY_ROOT_LOGIN_INSTALL
	$(INSTALL) -D -m 0755 $(@D)/auto-root-login $(TARGET_DIR)/usr/bin/
	$(INSTALL) -D -m 0755 -d $(TARGET_DIR)/etc/default
	$(INSTALL) -D -m 0644 $(@D)/autogetty $(TARGET_DIR)/etc/default/autogetty
endef
AUTO_GETTY_INSTALL_TARGET_CMDS += $(AUTO_GETTY_ROOT_LOGIN_INSTALL)
endif

$(eval $(generic-package))
