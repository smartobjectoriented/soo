#!/bin/bash

if [ $# -ne 1 ]; then
        echo "Usage: ./umount_ramfs <board>"
	echo "Please provide the board name (vexpress, rpi4, virt64, rpi4_64, so3virt)"
	exit 0
fi

echo "Here: board is $1"
echo "-------------------umount ramfs ---------------"

sudo umount fs
sudo losetup -D
sudo rm -rf fs


