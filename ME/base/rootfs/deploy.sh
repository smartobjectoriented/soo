#!/bin/bash

echo "Deploying the usr apps into the ramfs filesystem..."
./mount_ramfs.sh $1
sudo rm -rf fs/*
sudo cp -rf ../usr/out/* fs/
./umount_ramfs.sh $1
