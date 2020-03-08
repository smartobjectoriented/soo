#!/bin/bash
echo "-------------------umount rootfs ---------------"

if [ $# -ne 1 ]; then
        echo "Usage: ./umount_rootfs <board>"
	echo "Please provide the board name (vexpress, rpi4)"
	exit 0
fi 
echo "Here: board is $1"
../../scripts/umount_cpio.sh $PWD/board/$1/rootfs.cpio


