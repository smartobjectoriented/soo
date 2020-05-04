
# This script is sourced by initd script

export WIFI_SSID="soo-domotics"
export WIFI_CHANNEL=36
export WIFI_BANDWIDTH=""
export SOO_NAME="soo"
export BT_NAME="soo-${SOO_NAME}"

echo ${SOO_NAME} > /sys/devices/system/soo/soo0/soo_name
echo ${BT_NAME} > /etc/hostname
