
#!/bin/bash

# Deploy usr apps into the agency partition (second partition)
echo Deploying usr apps into the agency partition...
cd ../filesystem
./mount.sh 2
sudo cp ../usr/build/* fs/root/
./umount.sh

