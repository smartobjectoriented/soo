#!/bin/bash
echo "-------------------mount rootfs ---------------"

mkdir -p fs

DEVLOOP=$(sudo losetup --partscan --find --show ./board/so3virt/rootfs.fat)

sudo mount ${DEVLOOP}p1 fs
