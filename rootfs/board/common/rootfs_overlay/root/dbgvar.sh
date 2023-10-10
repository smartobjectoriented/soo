#!/bin/bash

DBGVAR_PATH=/sys/devices/system/soo/soo0/dbgvar

if [[ -n $1 ]]
then
    echo $1 > ${DBGVAR_PATH}
else
    cat ${DBGVAR_PATH}
fi
