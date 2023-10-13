#!/bin/bash
#
# Update the agency.json which contains the components version number
# and some metadatas useful for the upgrade process
#
# Contributors: David Truan: July 2019
#

IS_UPGRADABLE=0
INCREMENT_KERNEL=0
INCREMENT_ROOTFS=0
INCREMENT_UBOOT=0
INCREMENT_AVZ=0
INCREMENT_DEVICETREE=0


while getopts ":krudaUt:" option
do
    case $option in
        k)
            INCREMENT_KERNEL=1
            ;;
        r)
            INCREMENT_ROOTFS=1
            ;;
        u)
            INCREMENT_UBOOT=1
            ;;
        d)
            INCREMENT_DEVICETREE=1
            ;;
        a)
            INCREMENT_AVZ=1
            ;;
        U)
            IS_UPGRADABLE=1
            ;;
        t)
            AGENCY_TYPE="\"${OPTARG}\""
            ;;
        :)
            echo "Option -$OPTARG requires argument"
            exit 1
            ;;
        \?)
            echo "$OPTARG : Invalid option"
            exit 1
            ;;
    esac
done


KERNEL_VERSION=`jq '.kernel' agency.json`
UBOOT_VERSION=`jq '.uboot' agency.json`
UENV_VERSION=`jq '.uEnv' agency.json`
AVZ_VERSION=`jq '.avz' agency.json`
DTB_VERSION=`jq '.devicetree' agency.json`
ROOTFS_VERSION=`jq '.rootfs' agency.json`

AGENCY_TYPE_ORIG=`jq '.type' agency.json`

if [ -z "$AGENCY_TYPE" ]; then

    AGENCY_TYPE=$AGENCY_TYPE_ORIG
fi

if [ $INCREMENT_KERNEL -eq 1 ]; then
    echo "Incrementing kernel version from $KERNEL_VERSION to $((KERNEL_VERSION+1))"
    KERNEL_VERSION=$((KERNEL_VERSION+1))
fi
if [ $INCREMENT_UBOOT -eq 1 ]; then
    echo "Incrementing uboot version from $UBOOT_VERSION to $((UBOOT_VERSION+1))"
    UBOOT_VERSION=$((UBOOT_VERSION+1))
fi
if [ $INCREMENT_AVZ -eq 1 ]; then
    echo "Incrementing kernel version from $AVZ_VERSION to $((AVZ_VERSION+1))"
    AVZ_VERSION=$((AVZ_VERSION+1))
fi
if [ $INCREMENT_DEVICETREE -eq 1 ]; then
    echo "Incrementing kernel version from $DTB_VERSION to $((DTB_VERSION+1))"
    DTB_VERSION=$((DTB_VERSION+1))
fi
if [ $INCREMENT_ROOTFS -eq 1 ]; then
    echo "Incrementing kernel version from $ROOTFS_VERSION to $((ROOTFS_VERSION+1))"
    ROOTFS_VERSION=$((ROOTFS_VERSION+1))
fi

tmp=$(mktemp)

jq  ".kernel=${KERNEL_VERSION} | .avz=${AVZ_VERSION} | .uboot=${UBOOT_VERSION} | .devicetree=${DTB_VERSION} | .rootfs=${ROOTFS_VERSION} | .type=${AGENCY_TYPE}" agency.json > "$tmp" && mv "$tmp" agency.json
