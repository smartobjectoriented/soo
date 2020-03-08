################################################################################
#
# libsndfile
#
################################################################################

LIBSNDFILE_VERSION = 1.0.28
LIBSNDFILE_SITE = http://www.mega-nerd.com/libsndfile/files
LIBSNDFILE_INSTALL_STAGING = YES
LIBSNDFILE_LICENSE = LGPL-2.1+
LIBSNDFILE_LICENSE_FILES = COPYING

# 0001-double64_init-Check-psf-sf.channels-against-upper-bo.patch
LIBSNDFILE_IGNORE_CVES += CVE-2017-14634
# 0002-Check-MAX_CHANNELS-in-sndfile-deinterleave.patch
LIBSNDFILE_IGNORE_CVES += CVE-2018-13139 CVE-2018-19432
# 0003-a-ulaw-fix-multiple-buffer-overflows-432.patch
LIBSNDFILE_IGNORE_CVES += \
	CVE-2017-14245 CVE-2017-14246 CVE-2017-17456 CVE-2017-17457 \
	CVE-2018-19661 CVE-2018-19662
# disputed, https://github.com/erikd/libsndfile/issues/398
LIBSNDFILE_IGNORE_CVES += CVE-2018-13419

LIBSNDFILE_CONF_OPTS = \
	--disable-sqlite \
	--disable-alsa \
	--disable-external-libs \
	--disable-full-suite

$(eval $(autotools-package))
