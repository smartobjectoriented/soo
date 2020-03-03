#!/bin/bash

DEFAULT_CHANNEL=36
DEFAULT_SSID="soo-wifi"

SOO_CONF_FILE="/etc/soo/soo.conf"

if [[ -f ${SOO_CONF_FILE} ]]
then
    source ${SOO_CONF_FILE}
fi

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

ifconfig wlan1 up
# background opreation : driver nl80211 on wlan1 with config file to load here : /etc/soo/wlan1.conf 
wpa_supplicant -D nl80211 -i wlan1 -c /etc/soo/wlan1.conf -B
# request ipv4 address on server
udhcpc -i wlan1
