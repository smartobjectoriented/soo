#!/bin/bash

BT_TTY="/dev/ttyAMA1"

# The MAC address is generated using some agency UID bytes
AGENCYUID_FILE="/sys/devices/system/soo/soo0/agencyUID"
BDADDR=`cat ${AGENCYUID_FILE} | cut -c1-17`

hciattach ${BT_TTY} bcm43xx 460800 noflow - ${BDADDR}

DEFAULT_HCI_NAME="soo-bt"

if [[ -n ${BT_NAME} ]]
then
    HCI_NAME=${BT_NAME}
else
    HCI_NAME=${DEFAULT_HCI_NAME}
fi

/usr/libexec/bluetooth/bluetoothd --compat &
hciconfig hci0 up
# Secure Simple Pairing Mode
hciconfig hci0 sspmode 1
# Discoverable
hciconfig hci0 piscan
# Class Networking
hciconfig hci0 class 001101
hciconfig hci0 name ${HCI_NAME}
# Serial Port
sdptool add --channel=1 SP

# Launch the agent with NoInputNoOutput so we don't have to enter a PIN 
bt-agent -c NoInputNoOutput -d


# This opens a rfcomm socket which will be hooked by vuihandler
while true
do
    # Kill RFCOMM instance if any
    killall -9 rfcomm > /dev/null 2>&1
    rfcomm release all
    # Watch for RFCOMM connections
    rfcomm -r watch 1
done




