#!/bin/sh

SOO_CORE_MAJOR=126
DCM_MAJOR=127
ASF_MAJOR=100

echo -n "Creating SOO Agency devices..."
mkdir /dev/soo
mknod /dev/soo/core c ${SOO_CORE_MAJOR} 0
mknod /dev/soo/dcm c ${DCM_MAJOR} 0
mknod /dev/soo/asf c ${ASF_MAJOR} 0
echo "OK"

if [ -r /etc/emiso/otherapps.sh ]; then
    echo -n "Staring additional applications..."
    /root/otherapps.sh
    echo "OK"
fi
