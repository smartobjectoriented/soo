#!/bin/bash

while read var; do
if [ "$var" != "" ]; then
  export $(echo $var | sed -e 's/ //g' -e /^$/d -e 's/://g' -e /^#/d)
fi
done < ../build.conf

echo Deploying transient content
./mount_initrd.sh ${PLATFORM}
cp -r transient/* initrd/transient/
./umount_initrd.sh ${PLATFORM}

echo Done...
