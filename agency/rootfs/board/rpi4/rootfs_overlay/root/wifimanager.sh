#!/bin/bash

DEFAULT_CHANNEL=36
DEFAULT_SSID="soo-wifi"

if [[ -n ${WIFI_SSID} ]]
then
    SSID=${WIFI_SSID}
else
    SSID=${DEFAULT_SSID}
fi

if [[ -n ${WIFI_CHANNEL} ]]
then
    CHANNEL=${WIFI_CHANNEL}
else
    CHANNEL={DEFAULT_CHANNEL}
fi

iw reg set CH
iwconfig wlan0 mode ad-hoc
iwconfig wlan0 essid ${SSID}
iwconfig wlan0 channel ${DEFAULT_CHANNEL}
sleep 2
ifconfig wlan0 up
sleep 1

