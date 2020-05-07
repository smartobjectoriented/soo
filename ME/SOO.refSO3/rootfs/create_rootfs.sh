#!/bin/bash

mkdir -p board/so3virt

#create image first
dd if=/dev/zero of=board/so3virt/rootfs.fat bs=1024 count=2048
DEVLOOP=$(sudo losetup --partscan --find --show board/so3virt/rootfs.fat)

#create the partition this way
(echo o; echo n; echo p; echo; echo; echo; echo; echo; echo t; echo; echo c; echo w) | sudo fdisk $DEVLOOP;

sudo mkfs.vfat ${DEVLOOP}p1
sudo losetup -d $DEVLOOP
