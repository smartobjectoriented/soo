#!/bin/bash

# Source directory containing files to link from
source_dir="${PWD}/../so3"

# Destination directory where symlinks will be created
destination_dir="${PWD}/so3"

# Change to the script's directory to resolve relative paths
cd "$(dirname "$0")" || exit

# Create destination directory if it doesn't exist
mkdir -p "$destination_dir"

# Loop through each file in the source directory
find "$source_dir" -type f -exec bash -c '
for file do
    # Get relative path of the file
    relative_path="${file#$1}"

    # Ensure destination directory structure exists
    mkdir -p "$2/${relative_path%/*}"

    # Check if the file already exists in the destination directory
    if [ ! -e "$2/$relative_path" ]; then
        # Create symlink
        ln -s "$file" "$2/$relative_path"
    fi
done
' bash "$source_dir" "$destination_dir" {} +

