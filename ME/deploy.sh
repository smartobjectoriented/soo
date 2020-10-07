#!/bin/bash

echo Deploying all MEs into the third partition...

cd ../agency/filesystem

./mount.sh 3

sudo rm -rf fs/*

if [ "$1" != "clean" ]; then
    if [ "$1" != "" ]; then
        ME_to_deploy="../../ME/$1/target/so3virt.itb"
        if [ -f ${ME_to_deploy} ]; then
            sudo cp -rf $ME_to_deploy fs/
            echo "$1 deployed"
        else
            echo "The ITB you want to deploy is not present."
        fi
    else
        echo "No ME specified to be deployed!"
    fi
    
fi
./umount.sh
