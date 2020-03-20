################################################################################
#
# apt
#
################################################################################

APT_VERSION = 
APT_SITE = https://salsa.debian.org/apt-team/apt/-/archive/master
APT_SOURCE = apt-master.tar.gz
#LIBCUE_LICENSE = GPL-2.0, BSD-2-Clause (rem.c)
#LIBCUE_LICENSE_FILES = LICENSE
#LIBCUE_DEPENDENCIES = host-bison host-flex flex
#LIBCUE_INSTALL_STAGING = YES

$(eval $(cmake-package))
