#!/bin/bash
echo "-------------------mount initrd ---------------"

if [ $# -ne 1 ]; then
        echo "Usage: ./mount_initrd <board>"
	echo "Please provide the board name (vexpress, rpi4)"
	exit 0
fi 
echo "Here: board is $1"
../../scripts/mount_cpio.sh $PWD/board/$1/initrd.cpio
