FROM    ubuntu:20.04

ENV     TZ=Europe/Zurich

RUN     ln -snf /usr/share/zoneinfo/${TZ} /etc/localtime && \
        echo ${TZ} > /etc/timezone &&   \
        echo 'debconf debconf/frontend select Noninteractive' | debconf-set-selections &&   \
        dpkg --add-architecture i386 && \
        apt-get update && apt-get install -y --no-install-recommends libc6:i386 libstdc++6:i386 lib32z1-dev \
            zlib1g:i386 pkg-config libgtk2.0-dev bridge-utils unzip bc elfutils u-boot-tools bison flex \
            openssh-server wget ca-certificates sudo xz-utils make gcc libssl-dev file g++ patch cpio rsync &&  \
        rm -rf /var/lib/apt/lists/* &&  \
        mkdir -p /opt/toolchains && cd /opt/toolchains &&   \
        wget https://releases.linaro.org/components/toolchain/binaries/6.4-2018.05/arm-linux-gnueabihf/gcc-linaro-6.4.1-2018.05-x86_64_arm-linux-gnueabihf.tar.xz &&    \
        tar -xf gcc-linaro-6.4.1-2018.05-x86_64_arm-linux-gnueabihf.tar.xz &&   \
        rm gcc-linaro-6.4.1-2018.05-x86_64_arm-linux-gnueabihf.tar.xz &&    \
        mv gcc-linaro-6.4.1-2018.05-x86_64_arm-linux-gnueabihf arm-linux-gnueabihf_6.4.1 && \
        mkdir -p /var/run/sshd &&   \
        useradd -ms /bin/bash reds -G sudo &&   \
        echo 'reds:reds' | chpasswd &&  \
        echo '%sudo ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers &&  \
        echo 'export PATH="${PATH}:/opt/toolchains/arm-linux-gnueabihf_6.4.1/bin"' > /etc/profile.d/02-toolchains.sh

USER    reds

EXPOSE  22

CMD     ["sudo", "/usr/sbin/sshd", "-D"]

