#!/bin/bash

echo Deploying Trused Application into the agency partition in /root/ta ...
cd ../agency/filesystem
./mount.sh 2
sudo mkdir -p fs/root/ta
sudo cp ../../optee_ta//build/* fs/root/ta
./umount.sh
