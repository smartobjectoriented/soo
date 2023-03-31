#!/bin/bash

# The build process between 'rpi4_64' & 'cm4' is the same
if [ "$PLATFORM" = "cm4_64" ]; then
    PLATFORM="rpi4_64"
fi
echo Deploying secondary rootfs into the second partition...
./mount_rootfs.sh ${PLATFORM}
cd ../filesystem
./mount.sh 2
sudo rm -rf fs/*
sudo cp -rf ../rootfs/fs/* fs/
./umount.sh  
cd ../rootfs
./umount_rootfs.sh ${PLATFORM}

