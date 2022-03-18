#!/usr/bin/bash

SCRIPTPATH="$( cd "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
BASE_PATH=$SCRIPTPATH/../base
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

configure()
{
    cd $SO3_PATH
    make wagoled_defconfig
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
    ./deploy.sh -m SOO.wagoled wagoled
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
