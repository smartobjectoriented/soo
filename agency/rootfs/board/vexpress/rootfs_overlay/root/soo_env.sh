
# This script is sourced by initd script

export SOO_NAME="SOO-vexpress"

echo ${SOO_NAME} > /sys/devices/system/soo/soo0/soo_name
