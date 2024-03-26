.. _user_guide:

User Guide
##########
   
The installation should work in any Ubuntu/Kubuntu installation superior
to ``16.10``. It is assumed that you are running an x86_64 version.

The following setup prepares to run the SOO framework in a QEMU-based
emulated environment. There is a file called **build.conf** in the
*agency/* directory which specifies the target platform. After a fresh
clone, build.conf is configured for QEMU *virt32* platform.

Additionally, the *virt32* target is also configured with TrustZone
support.

Pre-requisite
*************

Additional packages
===================

We need to run some i386 executables, and we need to install some i386
libraries too.

.. code:: bash

   sudo dpkg --add-architecture i386
   sudo apt-get update
   sudo apt-get install libc6:i386 libncurses5:i386 libstdc++6:i386
   sudo apt-get install lib32z1-dev
   sudo apt-get install zlib1g:i386

Various other packages are required:

.. code:: bash

   sudo apt-get install pkg-config libgtk2.0-dev bridge-utils
   sudo apt-get install unzip bc
   sudo apt-get install elfutils u-boot-tools
   sudo apt-get install device-tree-compiler
   sudo apt-get install fdisk
   sudo apt-get install libncurses-dev
   
The OP-TEE environment requires the following python packages:

.. code:: bash

   pip3 install pycryptodome
   sudo apt install python3-pyelftools

The following packets are not mandatory, but they can be installed to
prevent annoying warnings:

.. code:: bash

   sudo apt-get install bison flex

SO3 Git submodule
=================

SO3 is used as hypervisor (AVZ) and for MEs. It is defined as a sub-module in
the main SOO repository.

To clone the submodule, do:

.. code-block:: bash

   git submodule update --init

Actually, the SO3 version for SOO includes additional files and a piece of modified
files specific to the SOO environment. A build script is used to create symbolic
links to original SO3 files as well as to SOO-related files stored in ``ME/soo/so3``.

Go to the ``ME/work`` directory and execute the ``build.sh`` in order to build
the SOO specific SO3 environment.

.. code-block: bash

   cd ME/work
   ./build.sh

It may take a while since the script create symlinks to all SO3 files.

.. warning::

   Do not modify SO3 files which are not stored in ``ME/soo/so3`` since these
   files come from the submodule. Go rather to Github SO3 page and make an issue
   if modifications concern *standalone* SO3 or simply copy the original file
   in ``ME/soo/so3`` to make changes which are SOO-specific.


Toolchain
=========

The AArch-32 (ARM 32-bit) toolchain can be installed with the following commands:

.. code-block:: shell

   $ sudo mkdir -p /opt/toolchains && cd /opt/toolchains
   # Download and extract arm-none-linux-gnueabihf toolchain (gcc v11.3.1).
   $ sudo wget https://snapshots.linaro.org/gnu-toolchain/11.3-2022.06-1/arm-linux-gnueabihf/gcc-linaro-11.3.1-2022.06-x86_64_arm-linux-gnueabihf.tar.xz
   $ sudo tar xf gcc-linaro-11.3.1-2022.06-x86_64_arm-linux-gnueabihf.tar.xz
   $ sudo rm gcc-linaro-11.3.1-2022.06-x86_64_arm-linux-gnueabihf.tar.xz
   $ sudo mv gcc-linaro-11.3.1-2022.06-x86_64_arm-linux-gnueabihf arm-linux-gnueabihf_11.3.1
   $ sudo echo 'export PATH="${PATH}:/opt/toolchains/arm-linux-gnueabihf_11.3.1/bin"' | sudo tee -a /etc/profile.d/02-toolchains.sh

For the 64-bit version (virt & RPi4), the AArch-64 (ARM 64-bit) toolchain can be installed with the following commands:

.. code-block:: shell

   $ sudo mkdir -p /opt/toolchains && cd /opt/toolchains
   # Download and extract arm-none-linux-gnueabihf toolchain (gcc v10.2).
   $ sudo wget https://developer.arm.com/-/media/Files/downloads/gnu/11.3.rel1/binrel/arm-gnu-toolchain-11.3.rel1-x86_64-aarch64-none-linux-gnu.tar.xz
   $ sudo tar xf arm-gnu-toolchain-11.3.rel1-x86_64-aarch64-none-linux-gnu.tar.xz
   $ sudo rm arm-gnu-toolchain-11.3.rel1-x86_64-aarch64-none-linux-gnu.tar.xz
   $ sudo mv arm-gnu-toolchain-11.3.rel1-x86_64-aarch64-none-linux-gnu aarch64-none-linux-gnu_11.3
   $ echo 'export PATH="${PATH}:/opt/toolchains/aarch64-none-linux-gnu_11.3/bin"' | sudo tee -a /etc/profile.d/02-toolchains.sh

Basic Components
****************

Currently, the framework contains all what is required to get a full
functional environment. It includes the QEMU emulator, ARM TrustZone
components, U-boot bootlader, etc.

QEMU
====

QEMU is in version 8.0.0. The source code is fetched and patched in order
to have framebuffer and mouse/keyboard support with the ``virt`` machine.

To fetch, patch and build QEMU, execute the following commands:

.. code:: bash

   cd qemu/
   ./fetch.sh
   ./configure --target-list=arm-softmmu,aarch64-softmmu --disable-attr --disable-werror --disable-docs
   make -j $(nproc)

It may take some time, be patient! It builds QEMU to support both virt32 and virt64 platforms as defined
in the SOO environment.

TrustZone Related Components
============================

Since the SOO agency relies on TrustZone for security concerns, it is
necessary to compile the trusted-firmware-a package as follows:

ARM Trusted firmware (trusted-firmware-a) also known as ATF
-----------------------------------------------------------

.. code:: bash

   cd trusted-firmware-a
   ./build.sh

OTEE_OS (Open Trusted Execution Environment)
============================================

.. code:: bash

   cd optee_os
   ./build.sh

OPTEE TA (Trusted Applications)
-------------------------------

The *optee_ta/* directory contains our trusted applications used to
cipher/uncipher the ME, discovery beacons, etc.

.. code:: bash

   cd optee_ta
   ./build.sh

U-boot
======

The bootloader used by SOO is **U-boot**. In the sub-directory, there
are also various environment files used by the bootloader.

From 2019, the build system of agency and MEs is strongly based upon
U-boot ITB binary files which contain all necessary components. Not only
the SOO Agency is entirely contained in an ITB file, but also the Mobile
Entities (MEs) which are produced as that.

The compilation of *U-boot* is done with the following config and
commands (from the soo directory):

.. code:: bash

   cd u-boot
   make virt32_defconfig
   make -j8

The following configurations are available:

+-----------------------+------------------------------------------+
| Name                  | Platform                                 |
+=======================+==========================================+
| *virt32_defconfig*    | Basic QEMU/virt 32-bit platform          |
+-----------------------+------------------------------------------+
| *virt64_defconfig*    | QEMU/virt 64-bit platform                |
+-----------------------+------------------------------------------+
| *rpi_4_32b_defconfig* | Raspberry Pi 4 in 32-bit mode            |
+-----------------------+------------------------------------------+
| *rpi4_64_defconfig*   | Raspberry Pi 4 in 64-bit mode            |
+-----------------------+------------------------------------------+
| *cm4_64_defconfig*    | Raspberry Pi / CM4 module in 64-bit mode |
+-----------------------+------------------------------------------+

SOO Components
**************

Agency
======

This section presents the different components which are required to be
built in the **agency/** directory. Different configurations are possible.

Target platforms
----------------
The file ``build.conf`` in the root directory contains the ``PLATFORM`` to select the target platform.

Possible platforms and types are:

+-----------+------------------------------------------+
| Name      | Platform                                 |
+===========+==========================================+
| *virt32*  | Basic QEMU/virt 32-bit platform          |
+-----------+------------------------------------------+
| *virt64*  | QEMU/virt 64-bit platform                |
+-----------+------------------------------------------+
| *rpi4*    | Raspberry Pi 4 in 32-bit mode            |
+-----------+------------------------------------------+
| *rpi4_64* | Raspberry Pi 4 in 64-bit mode            |
+-----------+------------------------------------------+
| *cm4_64*  | Raspberry Pi / CM4 module in 64-bit mode |
+-----------+------------------------------------------+


AVZ Hypervisor
--------------

Since ``avz`` is based on SO3, it will be compiled from the source available
in ``so3/so3`` thanks to the ``./build.sh`` script availabe in *avz* directory.

Building avz first requires to prepare the configuration as the following example for the *virt64* platform:

.. code-block:: bash

   ~$ cd avz
   ~/avz$ ./build.sh virt64_avz_pv_soo_defconfig
   `/avz$ ./build.sh

In this example, the hypervisor is configured to support paravirtualized SOO enabled guest.
   
Executing the script without argument leads to a full build of avz.

To clean the avz directory properly, the ``-c`` option is availabe:

.. code-block:: bash

   ~/avz$ ./build.sh -c
   
   
Linux kernel
------------

To build the Linux kernel of the Agency, move to the kernel root directory ``linux/linux``.

Using ``make`` is the simplest way to build the kernel after configuring adequatly, as example
for the *virt64* platform:

.. note::

   The ``-j20`` option assumes you can use 20 CPU cores to make the build with parallel execution.
   
.. code-block:: bash

   ~$ cd linux/linux
   ~/linux/linux$ make virt64_defconfig
   ~/linux/linux$ make -j20
   

Main root filesystem (**rootfs**)
---------------------------------

In the code below, you have to replace ``MYARCH`` with the selected architecture. 
All available configurations (\*_defconfig) are placed in
the ``configs/`` directory.

-  If the chosen architecture is ``virt32``, *MYARCH* should be *virt32*.
-  If the chosen architecture is ``Raspberry Pi 4``: *MYARCH* should be *rpi4* .
-  etc.

The following commands first retrieve all packages in a first step, then it compiles everything. 
It may take quite a long time… Be patient!

From the agency’s directory:

.. code:: bash

   cd linux/rootfs
   make MYARCH_defconfig
   make source
   make

Initial ramfs (initrd) filesystem
---------------------------------

In the agency, there is an ``initrd`` filesystem which is embedded in
the *ITB* image file. In order to access the content of this *initrd*, 
a script in ``agency/rootfs`` is available. For example, to access
the content of the *virt32* board:

.. code:: bash

   cd linux/rootfs
   ./mount_initrd.sh virt32
   cd fs

Unmounting the filesystem is done with:

.. code:: bash
   
   cd linux/rootfs
   ./umount_initrd.sh virt32

Agency user applications
------------------------

In addition to the ``rootfs``, the Agency has its own applications that
can be found in ``linux/usr``. The build system of this part relies on
CMake. The build is achieved with the following script:

::

   cd linux/usr
   ./build.sh

Agency filesystem
-----------------

Once all main Agency components have been built, they will be put in a
virtual disk image as it is possible to attach such a virtual SD-Card
storage device with QEMU). The virtual storage is created in
``filesystem/`` directory and will contain all the necessary partitions.

The creation of the virtual disk image is done as follows:

.. code:: bash

   cd filesystem
   ./create_img.sh virt32

Deployment into the storage device
----------------------------------

Finally, the deployment of all Agency components (including the
bootloader in some configurations) is achieved with the following script
(option ``-a`` for all) located at the root directory:

.. code:: bash

   ./deploy.sh -a

The script has different options (try simply ``./deploy.sh`` to get all
options).

Yeahhh!… Now it is time to make a try by launching the SOO Agency with
the following script, in the ``root/`` directory.

.. code:: bash

   ./st

The script will launch QEMU with the correct options and the Agency
should start with the AVZ hypervisor and the Linux environment. You
should get a prompt entitled:

.. code:: bash

   `agency ~ #`

Mobile Entity (ME)
==================

For a quick test, it is proposed to build and to deploy the SOO.refso3
reference Mobile Entity.

ME Build
--------

The main ``ME``\ directory is amazingly ``ME`` at the root. However,
the source code is located in ``so3/`` directory since it is based on
this operating system.

Basically, a ME is constituted of its kernel (based on SO3 Operating
System), a device tree and eventually a rootfs used as **ramfs** (the
rootfs is embedded in the ME image itself, hence the ITB file).

ME build script
---------------

The ME can be build using the build.sh script found in ``ME`` directory.
This script takes 3 arguments 2 of which a mandatory and an optional one.

.. code:: bash
   
   ./build.sh

   Build ME
   Usage: ./build.sh -OPTIONS <ME_NAME> [OPTIONAL_CONFIG]
   OPTIONS:
   -k    build kernel only
   -u    build user apps only
   -ku   build kernel and apps

   Clean ME
   Usage: ./build.sh -c <ME_NAME> <OTPIONAL_CONFIG>

   ME_NAME can be one of the following:
   - SOO.agency
   - SOO.blind
   - SOO.ledctrl
   - SOO.net
   - SOO.outdoor
   - SOO.refso3
   - SOO.wagoled

   OPTIONAL_CONFIG can be one of the following:
   - refso3_ramfs

   Examples:
   ./build.sh -k SOO.refso3
   ./build.sh -ku SOO.refso3 refso3_ramfs

- The OPTIONS argument allow you to build just the kernel ``-k``, just the user space ``-u`` or both ``-ku``. The ``-c`` option clean up the build. 
- The ME_NAME argument allow you to select which ME you want too build. You must use the SOO.<ME_NAME> syntax.
- The OPTIONAL_CONFIG allow you to select a specific config for an ME if more than one exist. See `Examples` above.

The build output `<ME_NAME>.itb` will be put inside the ``ME/SOO.<ME_NAME>`` folder which will be created if it doesn't already exists.
   
Final Deployment
----------------

To deploy the newly built ME in the third partition of (virtual) SD-card use the deploy.sh script found 
in the root folder.

.. code:: bash
   
   ./deploy.sh -m SOO.<ME_NAME>


ME Injection from the Agency
----------------------------

It’s time to test the new ME in the running environment. To do that,
simply start the framework. The agency process which is started
automatically will inspect the contents of ``/mnt/ME`` directory and
load all available ``itb`` files.
