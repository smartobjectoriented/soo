#!/bin/bash

# This scripts periodically runs a hciconfig -a command to create dummy requests
# to the Bluetooth interface. If the Bluetooth interface has an issue, it will trigger
# a BUG, then a reboot of the Smart Object.

while true
do
    sleep 5
    hciconfig -a > /dev/null 2>&1
done
