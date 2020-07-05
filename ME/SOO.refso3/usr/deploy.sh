#!/bin/bash

# Deploy usr apps into the first partition
echo Deploying usr apps into the first partition...
cd ../rootfs
./mount_rootfs.sh
sudo rm -rf fs/*
sudo cp -r ../usr/out/* fs/
sudo cp -r ../usr/resources/* fs/
sudo ./umount_rootfs.sh
