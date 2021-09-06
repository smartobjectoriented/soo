#!/bin/sh
iw reg set CH
iw wlan0 set type ibss
ifconfig wlan0 up
iw wlan0 ibss join soo-wifi 5180
iwconfig wlan0 essid "soo-wifi"
