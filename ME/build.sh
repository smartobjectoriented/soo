#!/bin/bash

# This script is used to configure, build and generate MEs. 

ME_PATH="$( cd "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
SO3_PATH=$ME_PATH/../so3

RED='\033[0;31m'
NC='\033[0m' # No Color

usage() {
  echo "Build ME"
  echo "Usage: $0 -OPTIONS <ME_NAME> [OPTIONAL_CONFIG]"
  echo "OPTIONS:"
  echo "  -k    build kernel only"
  echo "  -u    build user apps only"
  echo "  -ku   build kernel and apps"
  echo ""
  echo "Clean ME"
  echo "Usage: $0 -c <ME_NAME> <OTPIONAL_CONFIG>"
  echo ""
  echo "ME_NAME can be one of the following:"
  echo "  - SOO.agency"
  echo "  - SOO.chat"
  echo "  - SOO.blind"
  echo "  - SOO.ledctrl"
  echo "  - SOO.outdoor"
  echo "  - SOO.refso3"
  echo "  - SOO.wagoled"
  echo "  - SOO.iuoc"
  echo ""
  echo "OPTIONAL_CONFIG can be one of the following:"
  echo "  - refso3_ramfs"
  echo ""
  echo "Examples:"
  echo "$0 -k SOO.refso3"
  echo "$0 -ku SOO.refso3 refso3_ramfs"
}

while getopts "ku:c:" opt; do
  case "$opt" in
    c)
      build_clean=y
      ;;
    k)
      build_kernel=y
      ;;
    u)
      build_user=y
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
fi

ME_FOLDER=$2
# Check optional config
if [ $# -eq 3 ]; then
  ME_NAME=$3
else
  # Separate SOO. from ME name
  IFS='.'
  read -a strarr <<< ${ME_FOLDER}
  ME_NAME=${strarr[1]}
  IFS=''
fi

echo $ME_NAME

ME_DEFCONFIG=$ME_NAME"_defconfig"
ME_ITS=$ME_NAME".its"

if [ "$build_clean" == "y" ]; then
  cd $SO3_PATH/so3
  make clean

  echo "Removing $ME_NAME.itb from target"
  rm -f ../target/$ME_NAME.itb

  echo "Removing all ITB images in SOO.$ME_NAME"
  rm -f $ME_PATH/SOO.$ME_NAME/*.itb

  exit 1
fi

if [ "$build_kernel" == "y" ]; then
  # === Configuration ===
  echo "=== Configuring build for SOO.$ME_NAME ==="
  cd $SO3_PATH/so3/configs
          
  # Check if defconfig exists
  if [ ! -f $ME_DEFCONFIG ]; then
    echo -e "${RED}Error: $SO3_PATH/so3/configs/$ME_DEFCONFIG not found${NC}"
      exit
  fi

  cd $SO3_PATH/target

  # Check if its exists
  if [ ! -f $ME_ITS ]; then
    echo "${RED}Error: $SO3_PATH/target/$ME_ITS not found${NC}"
    exit
  fi

  cd $SO3_PATH/so3
  make $ME_DEFCONFIG
  cd $ME_PATH

  echo ""

  # === Build ===
  echo "=== Compiling SOO.$ME_NAME ==="

  cd $SO3_PATH/so3
  make || exit 1
  cd $ME_PATH

  echo "=== Compilation done ==="
  echo ""
fi

if [ "$build_user" == "y" ]; then
  if [ ! -f $SO3_PATH/rootfs/rootfs.fat ]; then
    cd $SO3_PATH/rootfs
    ./create_ramfs.sh so3virt
  fi  
  cd $SO3_PATH/usr
  ./build.sh
  ./deploy.sh
fi
  
# === Generate itb ===

# Create folder for ME if it doesn't already exist
cd $ME_PATH
mkdir -p $ME_FOLDER && cd $ME_FOLDER
if [ ! -f README ]; then
  touch README
  echo "This directory contains the ITB of the SOO.${ME_NAME} ME." > README
fi
cd $SO3_PATH

echo "Deploying the ME into its itb file..."
cd target
./mkuboot.sh $ME_NAME

echo "Copying the ITB image $ME_NAME.itb in the target SOO.$ME_NAME directory"
cp $ME_NAME.itb $ME_PATH/$ME_FOLDER/
