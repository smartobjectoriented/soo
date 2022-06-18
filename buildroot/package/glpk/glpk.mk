################################################################################
#
# glpk
#
################################################################################

GLPK_VERSION = 4.65
GLPK_SOURCE = glpk-$(GLPK_VERSION).tar.gz
GLPK_SITE = ftp://ftp.mirror.nl/pub/mirror/gnu/glpk
GLPK_DEPENDENCIES = host-pkgconf 
GLPK_INSTALL_STAGING = YES
GLPK_LICENSE = GNU General Public License - v 3.0
GLPK_LICENSE_FILES = LICENSE
#GLPK_CONF_OPTS = --with-blas=openblas --with-blas-lib=-lopenblas

$(eval $(autotools-package))
