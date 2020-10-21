#!/bin/bash

if [ "$PLATFORM" == "" ]; then
    if [ "$1" == "" ]; then
        echo "PLATFORM must be defined (vexpress, rpi3, rpi4, bpi, merida)"
        echo "You can invoke mount.sh <partition_nr> <platform>"
        exit 0
    fi
    
    PLATFORM=$1
fi

sleep 1

sudo umount fs

# Let the filesystem be synchronized
sleep 1

if [ "$PLATFORM" == "vexpress" -o "$PLATFORM" == "merida" -o "$PLATFORM" == "virt64" ]; then
    sudo losetup -D
fi
