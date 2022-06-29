#!/bin/bash

SCRIPTPATH="$( cd "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"

ME_to_deploy="${SCRIPTPATH}/${1}/*.itb"

cd ../agency/filesystem

./mount.sh 3

if [ "$1" != "clean" ]; then
    if [ "$1" != "" ]; then

        echo Deploying all MEs into the third partition...
        sudo cp -rf $ME_to_deploy fs/ 
        echo "$1 deployed"
    else
        echo "No ME specified to be deployed!"
    fi

else
    sudo rm fs/* 2>/dev/null
    echo "The MEs in the third partition were removed"    
fi
./umount.sh
