#!/bin/bash

BT_TTY="/dev/ttyAMA0"

# The MAC address is generated using some agency UID bytes
AGENCYUID_FILE="/sys/devices/system/soo/soo0/agencyUID"
BDADDR=`cat ${AGENCYUID_FILE} | cut -c0-17`

hciattach ${BT_TTY} bcm2035 3000000 flow - ${BDADDR}

DEFAULT_HCI_NAME="soo-bt"

if [[ -n ${BT_NAME} ]]
then
    HCI_NAME=${BT_NAME}
else
    HCI_NAME=${DEFAULT_HCI_NAME}
fi

/usr/libexec/bluetooth/bluetoothd &
hciconfig hci0 up
hciconfig hci0 name ${HCI_NAME}
# Discoverable
hciconfig hci0 piscan
# Class Networking
# Secure Simple Pairing Mode
hciconfig hci0 sspmode 1
# Class Networking
hciconfig hci0 class 020300





