#!/bin/bash

# Create the agency rootfs and the ME partition.

BLKDEV=/dev/mmcblk1

if [[ -b ${BLKDEV}p2 && -b ${BLKDEV}p3 ]]
then

    # Unmount the partition 2 if already mounted
    mountpoint -q /mnt/mmc2
    mmc2_mountpoint=$?
    if [[ ${mmc2_mountpoint} -eq 0 ]]
    then
        umount /mnt/mmc2
    fi
    
    # Format the agency partition. It should not be journalized.
    echo "Formatting partition 2..."
    yes | mkfs.ext4 ${BLKDEV}p2
    echo "OK"

    # Unmount the partition 3 if already mounted
    mountpoint -q /mnt/mmc3
    mmc2_mountpoint=$?
    if [[ ${mmc3_mountpoint} -eq 0 ]]
    then
        umount /mnt/mmc3
    fi

    # Format the ME partition. It should not be journalized.
    echo "Formatting partition 3..."
    yes | mkfs.ext4 ${BLKDEV}p3
    echo "OK"
    
else

    echo "Creating partitions..."

    # Partition 1: offset 8MB, size 64MB
    # Partition 2: offset 72MB, size 1024MB
    # Partition 3: offset 1GB+72MB, size 1024MB

    ( echo n; echo p; echo 2; echo 147456; echo +1024M; echo n; echo p; echo 3; echo 2244608; echo +1024M; echo w ) | fdisk ${BLKDEV}

    fdisk -l ${BLKDEV}
    
    echo "OK"
    
fi
