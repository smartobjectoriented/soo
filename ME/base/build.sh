#!/usr/bin/bash

# This script is used to configure, build and genenrate the SOO.wagoled ME. 
# To configure the ME use "-t" option, to compile it use the "-b" option and 
# finally to generate the itb use the "-d" option

SCRIPTPATH="$( cd "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
BASE_PATH=$SCRIPTPATH
SO3_PATH=$BASE_PATH/so3

help()
{
    echo "Build script"
    echo 
    echo "Usage: build.sh [OPTION]"
    echo "Options: "
    echo "b     build"
    echo "c     clean"
    echo "d     generate itb"
    echo "h     Prints this help"
    echo "t     configure"
}

while read var; do
if [ "$var" != "" ]; then
  export $(echo $var | sed -e 's/ //g' -e /^$/d -e 's/://g' -e /^#/d)
fi
done < build.conf

ME_NAME=${ME}
ME_DEFCONFIG="$ME_NAME"_defconfig
ME_FOLDER=SOO.$ME_NAME

# Create folder for ME if it doesn't already exist
cd $BASE_PATH/../
mkdir -p $ME_FOLDER && cd $ME_FOLDER
mkdir -p target && cd target
touch README

configure()
{
    echo "=== Configuring build for SOO.$ME_NAME ==="
    cd $SO3_PATH
    make $ME_DEFCONFIG
    cd $SCRIPTPATH
}

build()
{
    cd $SO3_PATH
    make
    cd $SCRIPTPATH
}

clean()
{
    cd $SO3_PATH
    make clean
    cd $SCRIPTPATH
}

generate_itb()
{
    cd $BASE_PATH
    ./deploy.sh -m $ME_FOLDER $ME_NAME
    cd $SCRIPTPATH

}

if [ $# -eq 0 ];
then
    echo "Error: missing argument" 
    help
    exit 0
else

while getopts ":bcdht" option; do
    case $option in
        b)
            build
            exit;;
        c)
            clean
            exit;;
        d)
            generate_itb
            exit;;
        h)
            help
            exit;;
        t)
            configure 
            exit;;
        \?)
            echo "Error: invalid option"
            help
            exit;;
    esac
done
fi
