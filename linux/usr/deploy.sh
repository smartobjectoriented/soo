
#!/bin/bash

SCRIPT_PATH="$( cd "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
BUILD_DIR=$SCRIPT_PATH/build

# Create and initialize the associative array
declare -A install_paths=(
    ["bin"]="fs/usr/local/bin/"
    ["root"]="fs/root/"
)

# Deploy usr apps into the agency partition (second partition)
echo Deploying usr apps into the agency partition...
cd ../../filesystem
./mount.sh 2
sudo cp -r ../linux/usr/build/deploy/* fs/root/

# Loop over installation paths
for folder in "${!install_paths[@]}"; do
    target_path=${install_paths[$folder]}

    sudo mkdir -p $target_path
    sudo cp -r $BUILD_DIR/$folder/* $target_path >/dev/null 2>&1
done

sleep 1
./umount.sh

