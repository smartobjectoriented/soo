#!/bin/bash

SYS_DIR="/sys/devices/virtual/bluetooth/hci0"

# Wait for the hci0 BT device to be created
while true
do
    if [[ -d ${SYS_DIR} ]]
    then
        break
    fi
    
    sleep 1
done

DEFAULT_HCI_NAME="soo-bt"

SOO_CONF_FILE="/etc/soo/soo.conf"

if [[ -f ${SOO_CONF_FILE} ]]
then
    source ${SOO_CONF_FILE}
fi

if [[ -n ${BT_NAME} ]]
then
    HCI_NAME=${BT_NAME}
else
    HCI_NAME=${DEFAULT_HCI_NAME}
fi

/usr/sbin/bluetoothd &

hciconfig hci0 up
# Secure Simple Pairing Mode
hciconfig hci0 sspmode 1
# Discoverable
hciconfig hci0 piscan
hciconfig hci0 class 001101
hciconfig hci0 name ${HCI_NAME}
# Serial Port
sdptool add SP

# Launch a dumy hciconfig pinger to detect Bluetooth freezes
/root/hciping.sh &

while true
do
    # Kill RFCOMM instance if any
    killall -9 rfcomm > /dev/null 2>&1
    rfcomm release all
    # Watch for RFCOMM connections
    rfcomm -r watch 1
done
