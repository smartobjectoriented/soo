#!/bin/bash

echo "------------------- deploy usr apps in so3  ---------------"

echo Deploying user apps into the ramfs partition

cd ../rootfs
./mount.sh
sudo cp -r ../usr/out/* fs
sudo cp -r ../usr/build/deploy/* fs
./umount.sh

