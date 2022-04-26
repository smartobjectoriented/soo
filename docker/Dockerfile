FROM    ubuntu:20.04

        # Setup timezone (needed by some packages on installation).
RUN     ln -snf /usr/share/zoneinfo/Europe/Zurich /etc/localtime && \
        echo Europe/Zurich > /etc/timezone &&   \
        # Setup frontend as non-interactive.
        echo 'debconf debconf/frontend select Noninteractive' | debconf-set-selections &&   \
        # Add i386 architecture.
        dpkg --add-architecture i386 && \
        # Install required packages.
        apt-get update && apt-get install -y --no-install-recommends bc bison bridge-utils ca-certificates cpio device-tree-compiler  \
        elfutils file flex gcc git g++ lib32z1-dev libc6:i386 libgtk2.0-dev libssl-dev libstdc++6:i386 make nano    \
        openssh-server patch pkg-config python3 python3-crypto python3-pyelftools python3-sphinx python3-sphinxcontrib.plantuml python3-sphinx-rtd-theme  \ 
        rsync sudo wget unzip u-boot-tools xz-utils zlib1g:i386 &&  \ 
        rm -rf /var/lib/apt/lists/* &&  \
        # Create toolchains directory.
        mkdir -p /opt/toolchains && cd /opt/toolchains &&   \
        #Download and extract arm-none-linux-gnueabihf toolchain (gcc v9.2.1).
        wget -q https://developer.arm.com/-/media/Files/downloads/gnu-a/9.2-2019.12/binrel/gcc-arm-9.2-2019.12-x86_64-arm-none-linux-gnueabihf.tar.xz &&  \
        tar -xf gcc-arm-9.2-2019.12-x86_64-arm-none-linux-gnueabihf.tar.xz &&  \
        rm gcc-arm-9.2-2019.12-x86_64-arm-none-linux-gnueabihf.tar.xz &&  \
        mv gcc-arm-9.2-2019.12-x86_64-arm-none-linux-gnueabihf arm-none-linux-gnueabihf_9.2.1 &&  \
        echo 'export PATH="${PATH}:/opt/toolchains/arm-none-linux-gnueabihf_9.2.1/bin"' | tee -a /etc/profile.d/02-toolchains.sh &&  \
        # Download and extract aarch64-linux-gnu toolchain (gcc v6.4.1).
        wget -q https://developer.arm.com/-/media/Files/downloads/gnu-a/10.2-2020.11/binrel/gcc-arm-10.2-2020.11-x86_64-aarch64-none-linux-gnu.tar.xz &&  \
        tar -xf gcc-arm-10.2-2020.11-x86_64-aarch64-none-linux-gnu.tar.xz &&  \
        rm gcc-arm-10.2-2020.11-x86_64-aarch64-none-linux-gnu.tar.xz &&  \
        mv gcc-arm-10.2-2020.11-x86_64-aarch64-none-linux-gnu aarch64-none-linux-gnu_10.2 &&  \
        echo 'export PATH="${PATH}:/opt/toolchains/aarch64-none-linux-gnu_10.2/bin"' | tee -a /etc/profile.d/02-toolchains.sh &&  \
        # Apply system and user configuration.
        update-alternatives --install /usr/bin/python python /usr/bin/python3 1 &&\
        mkdir -p /var/run/sshd && \
        useradd -ms /bin/bash reds -G sudo &&\
        echo 'reds:reds' | chpasswd &&\
        echo '%sudo ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers

USER    reds

EXPOSE  22

CMD     ["sudo", "/usr/sbin/sshd", "-D"]
