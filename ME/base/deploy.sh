#!/bin/bash

usage() {
  echo "Usage: $0 -m <ME_NAME> <ITB_FILE>"
  echo ""
  echo "ME_NAME can be one of the following:"
  echo "  - SOO.refso3"
  echo "  - SOO.blind"
  echo "  - SOO.outdoor"
  echo "  - SOO.net"
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

while getopts "mc" o; do
  case "$o" in
    c)
      deploy_clean=y
	  ;;
    m)
      deploy_me=y
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
        rm -f ../$2/target/*
fi

if [ "$deploy_me" == "y" ]; then
        echo Deploying the ME into its itb file...
        cd target
        ./mkuboot.sh $3

        echo Copying the ITB image $2.itb in the $2 directory
        cp $3.itb ../../$2/
fi





    


