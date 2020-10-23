
#!/bin/bash

usage()
{
  echo "Usage: $0 [OPTIONS] <ME_NAME>"
  echo ""
  echo "Where OPTIONS are:"
  echo "  -a    Deploy all"
  echo "  -b    Deploy boot (kernel, U-boot, etc.)"
  echo "  -r    Deploy rootfs (secondary)"
  echo "  -u    Deploy usr apps"
  echo "  -m    Deploy MEs according to the deploy script in ME partition."
  echo "  -t    Deploy Trused Application"
  echo "  -c    Remove all MEs from the third partition."
  echo "  -f    Deploy via fastboot transfer and flash on eMMC."
  echo "  -g    Deploy via fastboot and boot directly."
  echo "  -e    Deploy via sunxi FEL mode."
  echo ""
  echo "ME_NAME is used with -a or -m and correspond to the <ME_NAME> in the ME path to be deployed."
  echo "Examples are:"
  echo "  SOO.blind"
  echo "  SOO.refso3"
  exit 1
}

while getopts "abcerumfgt" o; do
  case "$o" in
    a)
      deploy_rootfs=y
      deploy_boot=y
      deploy_usr=y
      deploy_me=y
      deploy_ta=y
      ;;
    b)
      deploy_boot=y
      ;;
    c)
      clean_me=y
      ;;
    r)
      deploy_rootfs=y
      ;;
    u)
      deploy_usr=y
      ;;
    m)
      deploy_me=y
      ;;
    t)
      deploy_ta=y
      ;;
    f)
      deploy_transfer=y
      ;;
    e)
      deploy_fel=y
      ;;
    g)
      deploy_fastboot=y
      ;;
    *)
      usage
      ;;
  esac
done

if [ $OPTIND -eq 1 ]; then usage; fi

while read var; do
if [ "$var" != "" ]; then
  export $(echo $var | sed -e 's/ //g' -e /^$/d -e 's/://g' -e /^#/d)
fi
done < build.conf

if [ "$TYPE" != "" ]; then
  PLATFORM_TYPE=${PLATFORM}_${TYPE}
else
  PLATFORM_TYPE=${PLATFORM}
fi

export PLATFORM_TYPE

# We now have ${PLATFORM} which names the platform base
# and ${PLATFORM_TYPE} to be used when the type is required.
# Note that ${PLATFORM_TYPE} can be equal to ${PLATFORM} if no type is specified.

if [ "$PLATFORM" != "vexpress" -a "$PLATFORM" != "merida" ]; then
    echo "Specify the device name of MMC (ex: sdb or mmcblk0 or other...)"
    read devname
    export devname="$devname"
fi

if [ "$deploy_boot" == "y" ]; then
    # Deploy files into the boot partition (first partition)
    echo Deploying boot files into the first partition...

    cd target
    ./mkuboot.sh ${PLATFORM}/${PLATFORM_TYPE}
    cd ../filesystem
    ./mount.sh 1
    sudo rm -rf fs/*
    sudo cp ../target/${PLATFORM}/${PLATFORM_TYPE}.itb fs/${PLATFORM}.itb
    sudo cp ../upgrade/agency.json fs/
    sudo cp ../upgrade/root_flag fs/
    sudo cp ../../u-boot/uEnv.d/uEnv_${PLATFORM}.txt fs/uEnv.txt

    if [ "$PLATFORM" == "vexpress" ]; then
	# Nothing else ...
        ./umount.sh
        cd ..
    fi

    if [ "$PLATFORM" == "rpi3" ]; then
        sudo cp -r ../../bsp/rpi3/* fs/
        sudo cp ../../u-boot/u-boot.bin fs/kernel.img
        ./umount.sh
        cd ..
    fi

    if [ "$PLATFORM" == "merida" ]; then
         ./umount.sh

        # Deploy SPL in the image
        echo Deploying SPL ...
        dd if=../../u-boot/spl/sunxi-spl.bin of=sdcard.img.${PLATFORM} bs=1k seek=8 conv=notrunc

        # ATF bl31.bin
        echo Deploying ATF bl31.bin
        dd if=../../trusted-firmware-a/build/sun50i_a64/debug/bl31.bin of=sdcard.img.${PLATFORM} bs=512 seek=10000 conv=notrunc

        # OP-TEE OS
        echo Deploying OP-TEE OS
        dd if=../../optee_os/out/arm-plat-sunxi/core/tee-pager_v2.bin of=sdcard.img.${PLATFORM} bs=512 seek=10100 conv=notrunc

        # U-boot uEnv.txt
        echo Deploying uEnv.txt for U-boot environment
        dd if=../../u-boot/uEnv.d/uEnv_merida.txt of=sdcard.img.${PLATFORM} bs=512 seek=11000 conv=notrunc

        # U-Boot
        echo Deploying u-boot with its dtb
        dd if=../../u-boot/u-boot-dtb.bin of=sdcard.img.${PLATFORM} bs=512 seek=11001 conv=notrunc

        cd ..
    fi

    if [ "$PLATFORM" == "rpi4" ]; then
        sudo cp -r ../../bsp/rpi4/* fs/
        sudo cp ../../u-boot/u-boot.bin fs/kernel7.img
        ./umount.sh
        cd ..
    fi
fi

if [ "$deploy_rootfs" == "y" ]; then
    # Deploy of the rootfs (second partition)
    cd rootfs
    ./deploy.sh
    cd ..
fi

if [ "$deploy_usr" == "y" ]; then

    # Deploy the usr apps related to the agency
    cd usr
    ./deploy.sh
    cd ..
fi

if [ "$deploy_me" == "y" ]; then

    # Deploy the usr apps related to the agency
    cd ../ME
    ./deploy.sh $2
    cd ../agency
fi

if [ "$deploy_ta" == "y" ]; then

    # Deploy the TA related to the agency
    cd ../optee_ta
    ./deploy.sh
    cd -
fi


if [ "$clean_me" == "y" ]; then

    # Deploy the usr apps related to the agency
    cd ../ME
    ./deploy.sh clean
    cd ../agency
fi

if [ "$deploy_transfer" == "y" ]; then

    # Deploy on the board using fastboot (Merida use case)
     ../bsp/merida/fastboot/build/fastboot/fastboot flash agency filesystem/sdcard.img.${PLATFORM}
     ../bsp/merida/fastboot/build/fastboot/fastboot continue

fi

if [ "$deploy_fel" == "y" ]; then

    echo "Deploying U-boot SPL..."
    sudo ../bsp/merida/sunxi-tools/sunxi-fel -v spl ../u-boot/spl/sunxi-spl.bin

    echo "Deploying Trusted Firmware"
    sudo ../bsp/merida/sunxi-tools/sunxi-fel -p write 0x44000 ../trusted-firmware-a/build/sun50i_a64/debug/bl31.bin

    echo "Deploying OPTEE"
    sudo ../bsp/merida/sunxi-tools/sunxi-fel -p write 0x40000000 ../optee_os/out/arm-plat-sunxi/core/tee-pager_v2.bin

    echo "Deploying U-boot"
    sudo ../bsp/merida/sunxi-tools/sunxi-fel -p write 0x4a000000 ../u-boot/u-boot-dtb.bin

    echo "Deploying uEnv.txt"
    sudo ../bsp/merida/sunxi-tools/sunxi-fel -p write 0x4b000000 ../u-boot/uEnv.d/uEnv_merida.txt

    #echo "Deploying Kernel"
   # sudo ../bsp/merida/sunxi-tools/sunxi-fel -p write 0x52000000 target/${PLATFORM}/${PLATFORM_TYPE}

    echo "Starting U-boot..."
    sudo ../bsp/merida/sunxi-tools/sunxi-fel exe 0x4a000000

    echo "Done."

fi

if [ "$deploy_fastboot" == "y" ]; then

    # Deploy on the board using fastboot (Merida use case)
    ../bsp/merida/fastboot/build/fastboot/fastboot boot target/${PLATFORM}/${PLATFORM_TYPE}.itb

fi
