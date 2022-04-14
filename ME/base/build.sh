#!/usr/bin/bash

# This script is used to configure, build and genenrate the SOO.wagoled ME. 
# To configure the ME use "-t" option, to compile it use the "-b" option and 
# finally to generate the itb use the "-d" option

SCRIPTPATH="$( cd "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
BASE_PATH=$SCRIPTPATH
SO3_PATH=$BASE_PATH/so3

RED='\033[0;31m'
NC='\033[0m' # No Color

usage() {
  echo "Build ME"
  echo "Usage: $0 -m <ME_NAME>"
  echo ""
  echo "Clean ME"
  echo "Usage: $0 -c <ME_NAME>"
  echo ""
  echo "ME_NAME can be one of the following"
  echo "  - SOO.agency"
  echo "  - SOO.blind"
  echo "  - SOO.ledctrl"
  echo "  - SOO.net"
  echo "  - SOO.outdoor"
  echo "  - SOO.refso3"
  echo "  - SOO.refso3_ramfs"
  echo "  - SOO.wagoled"
  echo ""
}

while getopts "m:c:" opt; do
  case "$opt" in
    c)
      build_clean=y
      ;;
    m)
      build_me=y
      ;;
    :)
      usage
      exit
      ;;
    *)
      usage
      exit
      ;;
  esac
done

if [ $OPTIND -eq 1 ]; then 
    usage
    exit 
else
    # Separate SOO. from ME name
    IFS='.'
    read -a strarr <<< ${2}
    ME_NAME=${strarr[1]}
    IFS=''

    ME_DEFCONFIG=$ME_NAME"_defconfig"
    ME_DTS=$ME_NAME".dts"
    ME_ITS=$ME_NAME".its"
fi

if [ "$build_clean" == "y" ]; then
    cd $SO3_PATH
    make clean
    cd $BASE_PATH

    echo "Removing $ME_NAME.itb from $BASE_PATH/target"
    rm -f target/$ME_NAME.itb

    echo "Removing all ITB images in SOO.$ME_NAME"
    rm -f $BASE_PATH/../SOO.$ME_NAME/target/*.itb
fi

if [ "$build_me" == "y" ]; then
    # === Configuration ===
    echo "=== Configuring build for SOO.$ME_NAME ==="
    cd $SO3_PATH/configs
        
    # Check if defconfig exists
    if [ ! -f $ME_DEFCONFIG ]; then
        echo -e "${RED}Error: $SO3_PATH/configs/$ME_DEFCONFIG not found${NC}"
        echo ""
        usage
        exit 0;
    fi
    cd $BASE_PATH

    # Check if dts exists
    cd $SO3_PATH/dts
    if [ ! -f $ME_DTS ]; then
        echo "${RED}Error: $SO3_PATH/dts/$ME_DTS not found${NC}"
        exit 0;
    fi

    # Check if its exists
    cd $BASE_PATH/target
    if [ ! -f $ME_ITS ]; then
        echo "${RED}Error: $BASE_PATH/target/$ME_ITS not found${NC}"
        exit 1;
    fi

    cd $SO3_PATH
    make $ME_DEFCONFIG
    cd $BASE_PATH

    echo ""

    # === Build ===
    echo "=== Compiling SOO.$ME_NAME ==="

    cd $SO3_PATH
    make || exit 1
    cd $BASE_PATH

    echo "=== Compilation done ==="
    echo ""

    # === Generate itb ===

    # Create folder for ME if it doesn't already exist
    cd $BASE_PATH/../
    mkdir -p SOO.$ME_NAME && cd SOO.$ME_NAME
    mkdir -p target && cd target
    if [ ! -f README ]; then
        touch README
        echo "This directory contains the ITB of the SOO.${ME_NAME} ME." > README
    fi
    cd $BASE_PATH

    echo "Deploying the ME into its itb file..."
    cd target
    ./mkuboot.sh $ME_NAME

    echo "Copying the ITB image $ME_NAME.itb in the target SOO.$ME_NAME directory"
    cp $ME_NAME.itb $BASE_PATH/../SOO.$ME_NAME/target/
fi