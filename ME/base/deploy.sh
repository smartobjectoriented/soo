#!/bin/bash

echo Deploying the ME into its itb file...
cd target
./mkuboot.sh $1
cp $1.itb ../../SOO.$1/target/





    


