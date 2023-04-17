#!/bin/bash

QEMU_AUDIO_DRV="none"
GDB_PORT_BASE=1234
USR_OPTION=$1

N_QEMU_INSTANCES=`ps -A | grep qemu-system | wc -l`

launch_qemu() {
    QEMU_MAC_ADDR="$(printf 'DE:AD:BE:EF:%02X:%02X\n' $((N_QEMU_INSTANCES)) $((N_QEMU_INSTANCES)))"

    GDB_PORT=$((${GDB_PORT_BASE} + ${N_QEMU_INSTANCES}))

    echo -e "\033[01;36mMAC addr: " ${QEMU_MAC_ADDR} "\033[0;37m"
    echo -e "\033[01;36mGDB port: " ${GDB_PORT} "\033[0;37m"

    while read var; do
        if [ "$var" != "" ]; then
                export $(echo $var | sed -e 's/ //g' -e /^$/d -e 's/://g' -e /^#/d)
        fi      
    done < agency/build.conf

    if [ "$PLATFORM" == "virt64" ]; then

    sudo qemu-system-aarch64 $@ ${USR_OPTION} \
  	-smp 4 \
	-serial mon:stdio  \
	-M virt,gic-version=2 -cpu cortex-a72  \
	-device virtio-blk-device,drive=hd0 \
	-drive if=none,file=agency/filesystem/sdcard.img.virt64,id=hd0,format=raw,file.locking=off \
	-m 1024 \
	-kernel u-boot/u-boot \
	-nographic \
	-netdev tap,id=n1,script=scripts/qemu-ifup.sh,downscript=scripts/qemu-ifdown.sh \
	-device virtio-net-device,netdev=n1,mac=${QEMU_MAC_ADDR} \
        -gdb tcp::${GDB_PORT}

    else    

    sudo qemu-system-arm $@ ${USR_OPTION} \
  	-smp 4 \
	-serial mon:stdio  \
	-M vexpress-a15,secure=on -cpu cortex-a15 \
	-m 1024 \
	-bios bl1.bin \
	-semihosting-config enable,target=native \
	-sd agency/filesystem/sdcard.img.vexpress \
	-net tap,script=scripts/qemu-ifup.sh,downscript=scripts/qemu-ifdown.sh -net nic,macaddr=${QEMU_MAC_ADDR} \
	-nographic \
	-gdb tcp::${GDB_PORT}
        
    fi
    
    QEMU_RESULT=$?
    
#    if [[ ${QEMU_RESULT} -ne 0 ]]
#    then
#        echo -e "\033[01;31mPort already used! Trying the next one...\033[0;37m"
#    
#        N_QEMU_INSTANCES=$((${N_QEMU_INSTANCES} + 1))
#        launch_qemu
#    fi
}

launch_qemu
