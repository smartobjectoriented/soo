#!/bin/bash

UPGRADE_IMAGE_NAME="update.bin"

./build_upgrade "$@"
cp $UPGRADE_IMAGE_NAME ../../ME/SOO.agency/
