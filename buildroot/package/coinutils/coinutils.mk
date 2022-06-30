################################################################################
#
# coinutils
#
################################################################################

COINUTILS_VERSION = 2.11.2
COINUTILS_SOURCE = $(COINUTILS_VERSION).tar.gz
COINUTILS_SITE = https://github.com/coin-or/CoinUtils/archive/releases
COINUTILS_LICENSE = Eclipse Public License - v 1.0
COINUTILS_LICENSE_FILES = LICENSE
COINUTILS_INSTALL_STAGING = YES

$(eval $(autotools-package))
