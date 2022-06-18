################################################################################
#
# clp
#
################################################################################

# OpenCN
#CLP_VERSION = 1.17.6

CLP_VERSION = 1.17.3
CLP_SOURCE = $(CLP_VERSION).tar.gz
CLP_SITE = https://github.com/coin-or/Clp/archive/releases
CLP_DEPENDENCIES = coinutils host-pkgconf openblas lapack glpk
CLP_INSTALL_STAGING = YES
CLP_LICENSE = Eclipse Public License - v 1.0
CLP_LICENSE_FILES = LICENSE
CLP_CONF_OPTS = --with-blas=openblas --with-blas-lib=-lopenblas

$(eval $(autotools-package))
