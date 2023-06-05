#!/bin/bash
echo "-------------------mount rootfs ---------------"

if [ $# -ne 1 ]; then
        echo "Usage: ./mount_rootfs <board>"
	echo "Please provide the board name (vexpress, virt64, rpi4, rpi4_64, cm4_64)"
	exit 0
fi 
echo "Here: board is $1"
../../scripts/mount_cpio.sh $PWD/board/$1/rootfs.cpio


