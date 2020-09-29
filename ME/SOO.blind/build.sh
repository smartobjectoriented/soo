#!/bin/bash

TYPE="blind"

echo "Now building the ME $TYPE..."

function usage {
  echo "$0 [OPTIONS]"
  echo "  -c        Clean"
  echo "  -h        Print this help"
}


clean=n

while getopts ch option
  do
    case "${option}"
      in
        c) clean=y;;
        h) usage && exit 1;;
    esac
  done


# Create links to this ME apps/ and callbacks.c files
cd ../so3
# symlink to the apps folder
ln -sf ../SOO.${TYPE}/apps/ apps

if [ -d "../SOO.${TYPE}/apps/include" ]; then
    cp -r ../SOO.${TYPE}/apps/include/* include/apps
fi

cd soo
ln -sf ../../SOO.${TYPE}/callbacks.c callbacks.c

# Create the full defconfig file by concatening the bas one with our ME specific
cd ../configs
cat so3virt_defconfig ../../SOO.${TYPE}/config_overlay > "SOO.${TYPE}_defconfig"


# Make SO3 using the TYPE (blind here).
cd ..
ME_TYPE=$TYPE make clean
if [ $clean == n ]; then
    ME_TYPE=$TYPE make SOO.${TYPE}_defconfig
    ME_TYPE=$TYPE make
fi

cp so3.bin ../SOO.${TYPE}/

# Cleaning in the soo directory
rm -rf apps
rm soo/callbacks.c
rm configs/SOO.$TYPE_defconfig
