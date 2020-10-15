#!/bin/bash

#########################################################################
#                           ME build script
#                  
# This script is in charge to build the MEs present in this directory.
# It needs the ME name as its parameter.
# It uses the SOO.refso3 as its compilation folder.
#
# All ME have to specify some files for it to work properly:
#   -config_overlay: ME specific config definition.
#   -apps/: Folder containing ME specific applications, like the me.c.
#   -apps/include/: Folder containing the headers for the ME apps.
#   -callbacks.c: Callbacks file defining the ME specific callbacks.
#
# Every ME can define a pre/post_build.sh script to cover the non
# general build case.
#
# Usage:
#   ./build <ME_TYPE> [OPTIONS]
#
# Version:
#       1.0 DTN 07.10.2020
#########################################################################

script=${BASH_SOURCE[0]}
# Get the path of this script
SCRIPTPATH=$(realpath $(dirname "$script"))

REFPATH=$SCRIPTPATH/SOO.refso3/

function usage {
  echo "$0 <ME_TYPE> [OPTIONS]"
  echo "  -c        Clean"
  echo "  -h        Print this help"
}


# Get the valid MEs using ls over the folders
VALID_MES=$(ls -1d */ | cut -c 1- | rev | cut -c 2- | rev | sort)

let valid_arg=0

if [ -z $1 ]; then
    echo "You need to provide a ME name. These are the possibilites"
    echo "$VALID_MES"
    exit -1
else
    while read -r line; do
        if [ "$1" == "$line" ]; then
            valid_arg=1
        fi
    done <<< "$VALID_MES"
fi

if [ $valid_arg -eq 0 ]; then
    echo "This ME type is unknown."
    exit -1
fi
# The ME is valid. Here we shift the argument to allow getopts to
# start its parsing at $2. 
TYPE=$1; shift

ME_SPECIFIC_DIR=${SCRIPTPATH}/${TYPE}/ME_specific

clean=n
build_itb=n

while getopts chi option
  do
    case "${option}"
      in
        c) clean=y;;
        h) usage && exit 1;;
        i) build_itb=y;;
    esac
  done

cd $REFPATH/so3

# Create links to this ME apps/ and callbacks.c files
# symlink to the apps folder
ln -sf ${ME_SPECIFIC_DIR}/apps apps
if [ -d "${ME_SPECIFIC_DIR}/apps/include" ]; then
    cp -r ${ME_SPECIFIC_DIR}/apps/include/* include/apps
fi

# Treat the clean before copying everything so we exit right after. But
# we still need to symlink apps so Makefile doesn't complain.
if [ "$clean" == "y" ]; then
    echo "Cleaning ME..."
    make clean
    rm -rf apps
    rm ${SCRIPTPATH}/${TYPE}/so3.bin 2> /dev/null
    rm ${SCRIPTPATH}/${TYPE}/so3virt.dtb 2> /dev/null
    rm ${SCRIPTPATH}/${TYPE}/target/so3virt.itb 2> /dev/null
    exit 0
fi

cd soo
ln -sf ${ME_SPECIFIC_DIR}/callbacks.c callbacks.c

# Create the full defconfig file by concatening the base one with our ME specific
cd ${REFPATH}/so3/configs
cat so3virt_defconfig ${ME_SPECIFIC_DIR}/config_overlay > "${TYPE}_defconfig"
cp ${ME_SPECIFIC_DIR}/${TYPE}_overlay.dts ${REFPATH}/so3/arch/arm/boot/dts/

# Execute the pre-build script if the ME has one
# it allows for some ME to tune/add/modify other files than the general case
if [ -f ${ME_SPECIFIC_DIR}/pre_build.sh ]; then
    source ${ME_SPECIFIC_DIR}/pre_build.sh $REFPATH
fi

# Make SO3 using the TYPE (refso3 here).
cd ${REFPATH}/so3

echo "Now building the ME $TYPE..."
ME_TYPE=$TYPE make ${TYPE}_defconfig
ME_TYPE=$TYPE make

# Copy the resulting so3.bin in the ME folder
cp so3.bin ${SCRIPTPATH}/${TYPE}/
cp so3 ${SCRIPTPATH}/${TYPE}/so3.elf
cp arch/arm/boot/dts/so3virt.dtb ${SCRIPTPATH}/${TYPE}/

if [ "$build_itb" == "y" ]; then
    echo "Generating ITB..."
    cd ${SCRIPTPATH}/${TYPE}
    source deploy.sh
    cd ${REFPATH}/so3
fi

# Execute the pre-build script if the ME has one
# it allows for some ME to tune/add/modify other files than the general case
if [ -f ${ME_SPECIFIC_DIR}/post_build.sh ]; then
    source ${ME_SPECIFIC_DIR}/post_build.sh $REFPATH
fi

# Cleaning in the soo directory
rm -rf apps
rm soo/callbacks.c
rm include/apps/*
rm configs/${TYPE}_defconfig
rm arch/arm/boot/dts/${TYPE}_overlay.dts
