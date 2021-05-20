#!/bin/bash

OPTEE_OS_PATH="../../optee_os"

usage()
{
	echo "Usage: $0 [OPTIONS]"
	echo ""
	echo "Where OPTIONS are:"
	echo "  -c    Make clean"
	echo "  -h    show help"
	exit 1
}

while getopts "c" o; do
	case "$o" in
		c)
		  make_clean=y
		  ;;
		h)
		  usage
		  ;;
		*)
      	  usage
      ;;
	esac
done


# Read 'build.conf'
while read var; do
if [ "$var" != "" ]; then
  export $(echo $var | sed -e 's/ //g' -e /^$/d -e 's/://g' -e /^#/d)
fi
done < ../agency/build.conf

if [ "$PLATFORM" == "vexpress" ]; then
	echo "Building Trusted Applications for platform vExpress qemu"
	CROSS_COMPILE=arm-none-linux-gnueabihf-
	PLATFORM=vexpress
	TA_DEV_KIT_DIR=$OPTEE_OS_PATH/out/arm-plat-vexpress/export-ta_arm32
fi

if [ "$PLATFORM" == "merida" ]; then
	echo "Building Trusted Applications for platform on MERIDA"
	CROSS_COMPILE=aarch64-none-linux-gnu-
	PLATFORM=sunxi-sun50i_a64
	TA_DEV_KIT_DIR=$OPTEE_OS_PATH/out/arm-plat-sunxi/export-ta_arm64/
fi

if [ "$make_clean" == "y" ]; then
	clean="clean"
	cd build
	rm *
	cd -
else
	clean=""
fi

mkdir -p build

for d in */ ; do
	if [ "$d" == "build/" ]; then
		continue
	fi
	echo "Building '$d' TA "

	make -C $d $clean CROSS_COMPILE=$CROSS_COMPILE \
			              PLATFORM=$PLATFORM \
			              TA_DEV_KIT_DIR=$TA_DEV_KIT_DIR

	if [ "$make_clean" != "y" ]; then
		cp $d*.ta build/
	fi
done
