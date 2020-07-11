#!/bin/bash

echo "Deploying usr into SO3 rootfs (for ramdev)"
cd usr ; ./deploy.sh so3virt_ramfs; cd ..


echo Deploying the ME into its itb file...
cd target
./mkuboot.sh so3virt_ramfs
mv so3virt_ramfs.itb so3virt.itb




    


