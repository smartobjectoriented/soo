#!/bin/bash

SOO_CONF_FILE="/etc/soo/soo.conf"

if [[ -f ${SOO_CONF_FILE} ]]
then
    source ${SOO_CONF_FILE}
fi

if [[ -z ${SOO_NAME} ]]
then
    SOO_NAME="soo"
fi

DEVACCESS_SOO_NAME_FILE="/sys/devices/system/soo/soo0/soo_name"

echo ${SOO_NAME} > ${DEVACCESS_SOO_NAME_FILE}
