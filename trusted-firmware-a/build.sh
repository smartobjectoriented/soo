
#!/bin/bash

 
 
while read var; do
if [ "$var" != "" ]; then
  export $(echo $var | sed -e 's/ //g' -e /^$/d -e 's/://g' -e /^#/d)
fi
done < ../agency/build.conf

if [ "$PLATFORM" == "merida" ]; then
    echo "Building ATF for running on MERIDA"
    make  CROSS_COMPILE=aarch64-linux-gnu- PLAT=sun50i_a64 DEBUG=1 SPD=opteed bl31 -j8
fi

if [ "$PLATFORM" == "vexpress" ]; then
    echo "Building ATF for running on vExpress Qemu"
    make  PLAT=qemu DEBUG=1 -j8
fi
