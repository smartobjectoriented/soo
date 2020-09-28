#! /bin/bash

# Here, you can do something after build of rootfs is done but
# before the final images (i.e. rootfs.tar) are generated.
# See post_image.sh script for the latter.
# Note: you should have access to most BR variables here.

rm -f target/etc/init.d/S01logging
rm -f target/etc/init.d/S01klogd

