#!/bin/bash

if [ $# -ne 1 ]; then
	echo "Please provide the board name (vexpress, rpi3, rpi4, merida, bpi)"
	exit 0
fi 

# Partition layout on the sdcard:
# - Partition #1: 32 MB (u-boot, kernel, etc.)
# - Partition #2: 70 MB (agency rootfs 1)
# - Partition #3: 100 MB (MEs)
# - Partition #4: 70 MB (agency rootfs 2)

if [ "$1" == "vexpress" -o "$1" == "merida" ]; then
    #create image first
    echo Creating sdcard.img.$1 ... 
    
    if [ "$1" == "vexpress" -o "$1" == "rpi3" -o "$1" == "rpi4" ]; then
        dd_size=1G
    else
        dd_size=400M
    fi
    dd if=/dev/zero of=sdcard.img.$1 bs="$dd_size" count=1
    devname=$(sudo losetup --partscan --find --show sdcard.img.$1)

    # Keep device name only without /dev/
    devname=${devname#"/dev/"}
fi

if [ "$1" == "rpi3" -o "$1" == "bpi" -o "$1" == "rpi4" ]; then
    echo "Specify the MMC device you want to deploy on (ex: sdb or mmcblk0 or other...)" 
    read devname
fi


if [ "$1" == "merida" ]; then
	#create the partition layout this way
    	(echo o; echo n; echo p; echo; echo 16384; echo +32M; echo t; echo e; echo a; echo n; echo p; echo; echo 81920; echo +100M; echo n; echo p; echo; echo 286720; echo +100M; echo n; echo p; echo 491520; echo +100M; echo w)   | sudo fdisk /dev/"$devname";
fi

if [ "$1" == "vexpress" -o "$1" == "rpi3" -o "$1" == "bpi" -o "$1" == "rpi4" ]; then
#create the partition layout this way
    (echo o; echo n; echo p; echo; echo; echo +64M; echo t; echo c; echo n; echo p; echo; echo; echo +400M; echo n; echo p; echo; echo; echo +100M; echo n; echo p; echo; echo; echo; echo w)   | sudo fdisk /dev/"$devname";
fi

# Give a chance to the real SD-card to be sync'd
sleep 1s

if [[ "$devname" = *[0-9] ]]; then
    export devname="${devname}p"
fi

sudo mkfs.vfat /dev/"$devname"1
sudo mkfs.ext4 /dev/"$devname"2
sudo mkfs.ext4 /dev/"$devname"3
sudo mkfs.ext4 /dev/"$devname"4
