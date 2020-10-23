FROM    ubuntu:20.04

        # Setup timezone (needed by some packages on installation).
RUN     ln -snf /usr/share/zoneinfo/Europe/Zurich /etc/localtime && \
        echo Europe/Zurich > /etc/timezone &&   \
        # Setup frontend as non-interactive.
        echo 'debconf debconf/frontend select Noninteractive' | debconf-set-selections &&   \
        # Add i386 architecture.
        dpkg --add-architecture i386 && \
        # Install required packages.
        apt-get update && apt-get install -y --no-install-recommends bc bison bridge-utils ca-certificates cpio \
        elfutils file flex gcc git g++ lib32z1-dev libc6:i386 libgtk2.0-dev libssl-dev libstdc++6:i386 make nano    \
        openssh-server patch pkg-config python3 python3-crypto python3-pyelftools rsync sudo wget unzip u-boot-tools    \
        xz-utils zlib1g:i386 &&  \
        rm -rf /var/lib/apt/lists/* &&  \
        # Create toolchains directory.
        mkdir -p /opt/toolchains && cd /opt/toolchains &&   \
        # Download and extract arm-linux-gnueabihf toolchain (gcc v6.4.1).
        wget -q https://releases.linaro.org/components/toolchain/binaries/6.4-2018.05/arm-linux-gnueabihf/gcc-linaro-6.4.1-2018.05-x86_64_arm-linux-gnueabihf.tar.xz &&    \
        tar -xf gcc-linaro-6.4.1-2018.05-x86_64_arm-linux-gnueabihf.tar.xz &&   \
        rm gcc-linaro-6.4.1-2018.05-x86_64_arm-linux-gnueabihf.tar.xz &&    \
        mv gcc-linaro-6.4.1-2018.05-x86_64_arm-linux-gnueabihf arm-linux-gnueabihf_6.4.1 && \
        echo 'export PATH="${PATH}:/opt/toolchains/arm-linux-gnueabihf_6.4.1/bin"' > /etc/profile.d/02-toolchains.sh && \
        # Download and extract aarch64-linux-gnu toolchain (gcc v6.4.1).
        wget -q https://releases.linaro.org/components/toolchain/binaries/6.4-2018.05/aarch64-linux-gnu/gcc-linaro-6.4.1-2018.05-x86_64_aarch64-linux-gnu.tar.xz &&    \
        tar -xf gcc-linaro-6.4.1-2018.05-x86_64_aarch64-linux-gnu.tar.xz && \
        rm gcc-linaro-6.4.1-2018.05-x86_64_aarch64-linux-gnu.tar.xz &&  \
        mv gcc-linaro-6.4.1-2018.05-x86_64_aarch64-linux-gnu aarch64-linux-gnu_6.4.1 && \
        echo 'export PATH="${PATH}:/opt/toolchains/aarch64-linux-gnu_6.4.1/bin"' >> /etc/profile.d/02-toolchains.sh &&  \
        # Apply system and user configuration.
        update-alternatives --install /usr/bin/python python /usr/bin/python3 1 &&  \
        mkdir -p /var/run/sshd &&   \
        useradd -ms /bin/bash reds -G sudo &&   \
        echo 'reds:reds' | chpasswd &&  \
        echo '%sudo ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers

USER    reds

EXPOSE  22

CMD     ["sudo", "/usr/sbin/sshd", "-D"]
