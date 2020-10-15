#!/bin/bash

# script=${BASH_SOURCE[0]}
# # Get the path of this script
# SCRIPTPATH=$(realpath $(dirname "$script"))


# $1 MUST contain the SOO.refso3 absolute path
_REFPATH=$1

mv $_REFPATH/so3/arch/arm/so3.lds.orig $_REFPATH/so3/arch/arm/so3.lds
mv $_REFPATH/so3/arch/arm/Makefile.orig $_REFPATH/so3/arch/arm/Makefile
rm $_REFPATH/so3/arch/arm/inc_upgrade.S
rm $_REFPATH/so3/arch/arm/update.bin
