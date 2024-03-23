#!/bin/bash

usage() {
  echo "Usage: $0 [-d <ME_NAME>] [-m <ME_NAME> <ITB_FILE>] [-c <ME_NAME>]"
  echo ""
  echo ""
  echo "Where OPTIONS are:"
  echo "  -d    Deploy into the filesystem"
  echo "  -m    Deploy the ME into its ITB and copy in the target dir"
  echo "  -c    Delete all ITB files in the ME target dir"
  echo "  -r    Remove all MEs from the filesystem"
  echo ""
  echo "ME_NAME can be one of the following:"
  echo "  - SOO.refso3"
  echo "  - SOO.chat"
  echo "  - SOO.blind"
  echo "  - SOO.outdoor"
  echo "  - SOO.agency"
  echo "  - SOO.wagoled"
  echo "  - SOO.iuoc"
  echo ""
  echo "The <ITB_FILE> (without extension) depends on the selected .its file available in the target/ directory."
  echo ""
  echo "Here is the list of the target/ directory:"
  echo ""

  ls target/
  
  echo ""
  echo "To clean all MEs in the current target directory, just do $0 -c <ME_NAME>"
  echo ""
  
  exit 1
}

while getopts "cdmr" o; do
  case "$o" in
    c)
      deploy_clean=y
	  ;;
    d)
      deploy_me_fs=y
      ;;
    m)
      deploy_me=y
	  ;;
	r)
      deploy_rm_fs=y
	  ;;
    *)
      usage
      ;;
  esac
done

if [ $OPTIND -eq 1 ]; then usage; fi

# Execute first this target if required
if [ "$deploy_clean" == "y" ]; then

        echo "Removing all ITB images in $2"
        rm -f $2/*
fi

if [ "$deploy_me_fs" == "y" ]; then

        echo "Deploying all ITBs belonging to $2 into the filesystem..."
        SCRIPTPATH="$( cd "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"

        ME_to_deploy="${SCRIPTPATH}/${2}/*.itb"
        cd ../filesystem

        ./mount.sh 3

        if [ "$2" != "" ]; then

          echo Deploying all MEs into the third partition...
          sudo cp -rf $ME_to_deploy fs/
          echo "$2 deployed"
        else
          echo "No ME specified to be deployed!"
        fi

        ./umount.sh
fi

if [ "$deploy_me" == "y" ]; then
        echo Deploying the ME into its itb file...

        cd work/so3/target
        ./mkuboot.sh $3

        echo Copying the ITB image $2.itb in the $2 directory
        cp $3.itb ../../ME/$2/
fi

if [ "$deploy_rm_fs" == "y" ]; then
        echo Erasing all ITBs belong to the ME $2...

        SCRIPTPATH="$( cd "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
        cd ../filesystem
        ./mount.sh 3

        sudo rm fs/* 2>/dev/null
        echo "The MEs in the third partition were removed"

        ./umount.sh
fi
