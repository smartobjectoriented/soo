
#!/bin/bash

 
 
while read var; do
if [ "$var" != "" ]; then
  export $(echo $var | sed -e 's/ //g' -e /^$/d -e 's/://g' -e /^#/d)
fi
done < ../agency/build.conf

if [ "$PLATFORM" == "merida" ]; then
    echo "Building OP-TEE for running on MERIDA"
    make  CROSS_COMPILE64=aarch64-linux-gnu- PLATFORM=sunxi-sun50i_a64
fi

if [ "$PLATFORM" == "vexpress" ]; then
    echo "Building OP-TEE for running on vExpress qemu"
    make -j8
fi
