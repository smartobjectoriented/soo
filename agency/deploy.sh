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
  echo ""
  echo "ME_NAME is used with -a or -m and correspond to the <ME_NAME> in the ME path to be deployed."
  echo "Examples are:"
  echo "  SOO.blind"
  echo "  SOO.refSO3"
  exit 1
}

while getopts "abrumtc" o; do
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

# We now have ${PLATFORM} which names the platform base.

if [ "$PLATFORM" != "vexpress" -a "$PLATFORM" != "virt64" ]; then
    echo "Specify the device name of MMC (ex: sdb or mmcblk0 or other...)"
    read devname
    export devname="$devname"
fi

if [ "$deploy_boot" == "y" ]; then
    # Deploy files into the boot partition (first partition)
    echo Deploying boot files into the first partition...

    cd target
    ./mkuboot.sh ${PLATFORM}
    cd ../filesystem
    ./mount.sh 1
    sudo rm -rf fs/*
    sudo cp ../target/${PLATFORM}.itb fs/${PLATFORM}.itb
    sudo cp ../upgrade/agency.json fs/
    sudo cp ../upgrade/root_flag fs/
    sudo cp ../../u-boot/uEnv.d/uEnv_${PLATFORM}.txt fs/uEnv.txt

    if [ "$PLATFORM" == "vexpress" -o "$PLATFORM" == "virt64" ]; then
	# Nothing else ...
        ./umount.sh
        cd ..
    fi

    if [ "$PLATFORM" == "rpi4" ]; then
        sudo cp -r ../../bsp/rpi4/* fs/
        sudo cp ../../u-boot/u-boot.bin fs/kernel7.img
        ./umount.sh
        cd ..
    fi
    
    if [ "$PLATFORM" == "rpi4_64" ]; then
        sudo cp -r ../../bsp/rpi4/* fs/
        sudo cp ../../u-boot/u-boot.bin fs/kernel8.img
        ./umount.sh
        cd ..
    fi

    if [ "$PLATFORM" == "cm4_64" ]; then
        sudo cp -r ../../bsp/rpi4/* fs/
        sudo cp ../../u-boot/u-boot.bin fs/kernel8.img
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
