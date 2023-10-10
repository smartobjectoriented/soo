
# Smart Object Oriented technology
# SOO filesystem support.

##vExpress

- An image can be created using dd, for example (16 MB): 
    dd if=/dev/zero of=flash bs=4096 count=4096
    
- Apply the following command to set up loopback device
    sudo losetup --partscan --find --show flash

- Make a FAT32 partition using fdisk

- Format as following:
    mkfs.vfat /dev/loop0p1
    

Use the ./stf script to load qemu.

Within U-boot, just make the following command to list all entries in the partition
    fatls mmc 0:1

### vexpress.gui_defconfig GUI support

So far, the xdriver_xf86-video-fbdev-0.4.4 driver is buggy and requires a new fbdev.c located in xf86-video-fbdev-0.5.0 package at the rootfs location.
Simply move fbdev.c to xdriver_xf86-video-fbdev-0.4.4/src and do "$ make xdriver_xf86-video-fbdev-rebuild in rootfs, followed by "$ make".

### Deployment of javafx/ARM
http://docs.gluonhq.com/javafxports/#anchor-1

cp armv6hf-sdk/rt/lib/ext/jfxrt.jar jdk1.8.0_201/jre/lib/ext/
cp armv6hf-sdk/rt/lib/arm/* jdk1.8.0_201/jre/lib/arm/
cp armv6hf-sdk/rt/lib/javafx.properties jdk1.8.0_201/jre/lib/
cp armv6hf-sdk/rt/lib/javafx.platform.properties jdk1.8.0_201/jre/lib/
cp armv6hf-sdk/rt/lib/jfxswt.jar jdk1.8.0_201/jre/lib/

-> Create symlinks in /usr/bin -> /opt/jdk1.8../jre/java /opt/jdk1.9../bin/javac + jar


## Primary rootfs support
The build.conf file at the agency root can have a complete root filesystem in the cpio format. Basically, such a cpio file is found in rootfs/images location. The rootfs is concatened to the final binary image which will be loaded in the smart object

## ROOTFS with initrd support and MMC main rootfs
Some boards require firmware to be loaded from the filesystem. In this case, some files can be embedded in a small initrd filesystem specific to a board.
The initrd.cpio file can be found in rootfs/board/<board> location.
To mount/unmount such file, use the ./{u}mount_initrd.sh script with the board as parameter.
