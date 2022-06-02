#!/bin/bash

if [ "$PLATFORM" == "" ]; then
    if [ "$1" == "" ]; then
        echo "PLATFORM must be defined (vexpress, virt64, rpi4, rpi4_64)"
        echo "You can invoke umount.sh <platform>"
        exit 0
    fi
    
    PLATFORM=$1
fi

sleep 2

sudo umount fs

# Let the filesystem be synchronized
sleep 2

if [ "$PLATFORM" == "vexpress" -o "$PLATFORM" == "virt64" ]; then
    sudo losetup -D
fi
