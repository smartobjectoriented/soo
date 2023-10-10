#!/bin/bash

echo Deploying secondary rootfs into the second partition...
./mount_rootfs.sh ${PLATFORM}
cd ../../filesystem
./mount.sh 2
sudo rm -rf fs/*
sudo cp -rf ../linux/rootfs/fs/* fs/
./umount.sh  
cd ../linux/rootfs
./umount_rootfs.sh ${PLATFORM}

