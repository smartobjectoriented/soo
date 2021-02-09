#!/bin/bash

if [ "$1" == "" ]; then
  echo "Usage: $0 <ME_NAME>"
  echo ""
  echo "Here, ME name is without the SOO. prefix"
  echo ""
  echo "- refso3"
  
  exit 1
fi

echo Deploying the ME into its itb file...
cd target
./mkuboot.sh $1
echo Copying the ITB image in the target SOO.$1 directory
cp $1.itb ../../SOO.$1/target/





    


