# This script is sourced by initd script

export WIFI_SSID="soo-domotics"
export WIFI_CHANNEL=40
export WIFI_BANDWIDTH=""
export SOO_NAME="rpi3"
export BT_NAME="soo-rpi3"

echo ${SOO_NAME} > /sys/devices/system/soo/soo0/soo_name
echo ${BT_NAME} > /etc/hostname