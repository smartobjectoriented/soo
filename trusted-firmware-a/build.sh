
#!/bin/bash

 
 
while read var; do
if [ "$var" != "" ]; then
  export $(echo $var | sed -e 's/ //g' -e /^$/d -e 's/://g' -e /^#/d)
fi
done < ../build.conf

if [ "$PLATFORM" == "virt64" ]; then
    echo "Building ATF for running on QEMU/virt64"
    make  CROSS_COMPILE=aarch64-none-linux-gnu- PLAT=qemu DEBUG=1 SPD=opteed bl31 -j8
fi

if [ "$PLATFORM" == "vexpress" ]; then
    echo "Building ATF for running on vExpress Qemu"
    make  PLAT=qemu DEBUG=1 -j8
fi
