#!/bin/bash

OPTEE_TA_DIR="$(dirname "$(readlink -f $0)")"
TA_BUILD_DIR=${OPTEE_TA_DIR}/build

# Checking if TA build folder exists - Not all platform supports ASF/Optee
if [ -d ${TA_BUILD_DIR} ]; then
	echo Deploying Trused Application into the agency partition in /root/ta ...
	cd ../agency/filesystem
	./mount.sh 2
	sudo mkdir -p fs/root/ta
	sudo cp ${TA_BUILD_DIR}/* fs/root/ta
	./umount.sh
fi
