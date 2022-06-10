#!/bin/bash

SCRIPTPATH="$( cd "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"

cd $SCRIPTPATH/../agency/filesystem

./mount.sh 3

# sudo rm -rf fs/*

if [ "$1" != "clean" ]; then
    if [ "$1" != "" ]; then

        echo Deploying all MEs into the third partition...
        if [  $# -eq 2 ]; then
            ME_to_deploy="../../ME/$1/$2.itb"
        else
            ME_to_deploy="../../ME/$1/*.itb"
        fi
        sudo cp -rf $ME_to_deploy fs/
        
        echo "$1 deployed"
    else
        echo "No ME specified to be deployed!"
    fi

else
    sudo rm -rf fs/*
    echo "The MEs in the third partition were removed"    
fi
./umount.sh
