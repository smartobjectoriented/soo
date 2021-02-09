#!/bin/bash

QEMU_AUDIO_DRV="none"
GDB_PORT_BASE=1234
USR_OPTION=$1

N_QEMU_INSTANCES=`ps -A | grep qemu-system-arm | wc -l`

launch_qemu() {
    QEMU_MAC_ADDR="$(printf 'DE:AD:BE:EF:%02X:%02X\n' $((N_QEMU_INSTANCES)) $((N_QEMU_INSTANCES)))"

    GDB_PORT=$((${GDB_PORT_BASE} + ${N_QEMU_INSTANCES}))

    echo -e "\033[01;36mMAC addr: " ${QEMU_MAC_ADDR} "\033[0;37m"
    echo -e "\033[01;36mGDB port: " ${GDB_PORT} "\033[0;37m"

    sudo qemu/build/arm-softmmu/qemu-system-arm $@ ${USR_OPTION} \
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
