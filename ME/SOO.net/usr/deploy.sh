#!/bin/bash

# Deploy usr apps into the first partition
echo Deploying usr apps into the first partition...
cd ../rootfs
./mount_ramfs.sh so3virt
sudo rm -rf fs/*
sudo cp -r ../usr/out/* fs/
sudo ./umount_ramfs.sh so3virt
