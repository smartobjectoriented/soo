#!/bin/bash

script=${BASH_SOURCE[0]}
# Get the path of this script
_SCRIPTPATH=$(realpath $(dirname "$script"))

# $1 MUST contain the SOO.refso3 absolute path
_REFPATH=$1

mv $_REFPATH/so3/arch/arm/so3.lds $_REFPATH/so3/arch/arm/so3.lds.orig
mv $_REFPATH/so3/arch/arm/Makefile $_REFPATH/so3/arch/arm/Makefile.orig
cp $_SCRIPTPATH/bootfiles/* $_REFPATH/so3/arch/arm/
