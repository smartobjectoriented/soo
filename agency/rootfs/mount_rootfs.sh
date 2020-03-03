#!/bin/bash
echo "-------------------mount rootfs ---------------"

if [ $# -ne 1 ]; then
        echo "Usage: ./mount_rootfs <board>"
	echo "Please provide the board name (vexpress, rpi4)"
	exit 0
fi 
echo "Here: board is $1"
../../tools/mount_cpio.sh $PWD/board/$1/rootfs.cpio


