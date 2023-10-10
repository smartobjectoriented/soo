#!/bin/bash

function usage {
  echo "$0 [OPTIONS]"
  echo "  -c        Clean"
  echo "  -d        Debug build"
  echo "  -v        Verbose"
  echo "  -s        Single core"
  echo "  -h        Print this help"
}

function install_file_root {
  [ -f $1 ] && echo "Installing $1" && cp $1 build/deploy
}

function install_directory_root {
  [ -d $1 ] && echo "Installing $1" && cp -R $1 build/deploy
}

function install_file_directory {
  [ -f $1 ] && echo "Installing $1 into $2" && mkdir -p build/deploy/$2 && cp $1 build/deploy/$2
}

clean=n
debug=n
verbose=n
singlecore=n

while getopts cdhvs option
  do
    case "${option}"
      in
        c) clean=y;;
        d) debug=y;;
	    v) verbose=y;;
        s) singlecore=y;;
        h) usage && exit 1;;
    esac
  done

SCRIPT=$(readlink -f $0)
SCRIPTPATH=`dirname $SCRIPT`

if [ $clean == y ]; then
  echo "Cleaning $SCRIPTPATH/build"
  rm -rf $SCRIPTPATH/build
  make -C module clean
  exit
fi

if [ $debug == y ]; then
  build_type="Debug"
else
  build_type="Release"
fi

echo "Starting $build_type build"
mkdir -p $SCRIPTPATH/build

cd $SCRIPTPATH/build
cmake -DCMAKE_BUILD_TYPE=$build_type -DCMAKE_TOOLCHAIN_FILE=../rootfs/host/share/buildroot/toolchainfile.cmake ..
if [ $singlecore == y ]; then
    NRPROC=1
else
    NRPROC=$((`cat /proc/cpuinfo | awk '/^processor/{print $3}' | wc -l` + 1))
fi
if [ $verbose == y ]; then
	make VERBOSE=1 -j1
else
	make -j$NRPROC
fi
cd -

# Check for modules

make -C module

mkdir -p build/deploy/

# Agency Core
install_file_root build/core/agency

# ASF test program
install_file_directory build/apps/asf_ta_tst/asf_ta_tst asf_ta_tst

# Other apps
install_file_root build/apps/injector
install_file_root build/apps/saveme
install_file_root build/apps/restoreme
install_file_root build/apps/shutdownme
install_file_root build/apps/melist
install_file_root build/apps/blacklist_soo

# Wago app
install_file_root build/wago_client/wago-client

# Mqtt app
install_file_root build/mqtt_iuoc/mqtt-client

# And modules if any
cp module/*.ko build/deploy 2>/dev/null




