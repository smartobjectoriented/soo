.. _user_guide:

##########
User Guide
##########
   
The installation should work in any Ubuntu/Kubuntu installation superior
to ``16.10``. It is assumed that you are running an x86_64 version.

The following setup prepares to run the SOO framework in a QEMU-based
emulated environment. There is a file called **build.conf** in the
*agency/* directory which specifies the target platform. After a fresh
clone, build.conf is configured for QEMU *vExpress* platform.

Additionally, the *vExpress* target is also configured with TrustZone
support.

*************
Pre-requisite
*************

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
   $ sudo wget https://developer.arm.com/-/media/Files/downloads/gnu-a/10.2-2020.11/binrel/gcc-arm-10.2-2020.11-x86_64-aarch64-none-linux-gnu.tar.xz
   $ sudo tar xf gcc-arm-10.2-2020.11-x86_64-aarch64-none-linux-gnu.tar.xz
   $ sudo rm gcc-arm-10.2-2020.11-x86_64-aarch64-none-linux-gnu.tar.xz
   $ sudo mv gcc-arm-10.2-2020.11-x86_64-aarch64-none-linux-gnu aarch64-none-linux-gnu_10.2
   $ echo 'export PATH="${PATH}:/opt/toolchains/aarch64-none-linux-gnu_10.2/bin"' | sudo tee -a /etc/profile.d/02-toolchains.sh

****************
Basic Components
****************

Currently, the framework contains all what is required to get a full
functional environment. It includes the QEMU emulator, ARM TrustZone
components, U-boot bootlader, etc.

QEMU
====

Currently QEMU is version *5.2* QEMU requires the additional package to
be installed:

.. code:: bash

   pip3 install ninja

From the root of the repository: (the configuration for qemu is
available in README.soo)

.. code:: bash

   cd qemu
   ./configure --target-list=arm-softmmu --disable-attr --disable-docs
   make -j8

It may take some time, be patient!

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
--------------------------------------------

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

******
U-boot
******

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
   make vexpress_defconfig
   make -j8

The following configurations are available:

+-----------------------+-------------------------------------+
| Name                  | Platform                            |
+=======================+=====================================+
| *vexpress_defconfig*  | Basic QEMU/vExpress 32-bit platform |
+-----------------------+-------------------------------------+
| *virt64_defconfig*    | QEMU/virt 64-bit platform           |
+-----------------------+-------------------------------------+
| *rpi_4_32b_defconfig* | Raspberry Pi 4 in 32-bit mode       |
+-----------------------+-------------------------------------+
| *rpi4_64_defconfig*   | Raspberry Pi 4 in 64-bit mode       |
+-----------------------+-------------------------------------+

(The last one is a custom configuration and is to be used as replacemenent
of rpi_4_defconfig)

**************
SOO Components
**************

Agency
======

This section presents the different components which are required to be
built in the **agency/** directory. Different configurations are possible.

Target platforms
----------------
The file ``build.conf`` in ``agency/`` contains the ``PLATFORM`` (and eventually ``TYPE``) variables 
to select the target platform.

Possible platforms and types are:

+------------+-------------------------------------+
| Name       | Platform                            |
+============+=====================================+
| *vexpress* | Basic QEMU/vExpress 32-bit platform |
+------------+-------------------------------------+
| *virt64*   | QEMU/virt 64-bit platform           |
+------------+-------------------------------------+
| *rpi4*     | Raspberry Pi 4 in 32-bit mode       |
+------------+-------------------------------------+
| *rpi4_64*  | Raspberry Pi 4 in 64-bit mode       |
+------------+-------------------------------------+

If *vexpress* is selected, it is (still) necessary to add a TYPE. Only, ``tz`` type
is supported.

.. note::

   The ``TYPE`` variable is useless and will be removed soon.

Main root filesystem (**rootfs**)
---------------------------------

In the code below, you have to replace ``MYARCH`` with the selected architecture. 
All available configurations (\*_defconfig) are placed in
the ``configs/`` directory.

-  If the chosen architecture is ``vexpress``, *MYARCH* should be *vexpress*.
-  If the chosen architecture is ``Raspberry Pi 4``: *MYARCH* should be *rpi4* .
-  etc.

The following commands first retrieve all packages in a first step, then it compiles everything. 
It may take quite a long time… Be patient!

From the agency’s directory:

.. code:: bash

   cd rootfs
   make MYARCH_defconfig
   make source
   make

The build of the agency including **AVZ** and **Linux** is
done by doing simply a make in the ``agency/`` root directory.

.. code:: bash

   cd agency
   make

Initial ramfs (initrd) filesystem
---------------------------------

In the agency, there is an ``initrd`` filesystem which is embedded in
the *ITB* image file. In order to access the content of this *initrd*, 
a script in ``agency/rootfs`` is available. For example, to access
the content of the *vexpress* board:

.. code:: bash

   cd rootfs
   ./mount_initrd.sh vexpress
   cd fs

Unmounting the filesystem is done with:

.. code:: bash
   
   cd rootfs
   ./umount_initrd.sh vexpress

Agency user applications
------------------------

In addition to the ``rootfs``, the Agency has its own applications that
can be found in ``agency/usr``. The build system of this part relies on
CMake. The build is achieved with the following script:

::

   cd agency/usr
   ./build.sh

Agency filesystem
-----------------

Once all main Agency components have been built, they will be put in a
virtual disk image as it is possible to attach such a virtual SD-Card
storage device with QEMU). The virtual storage is created in
``filesystem/`` directory and will contain all the necessary partitions.

The creation of the virtual disk image is done as follows:

.. code:: bash

   cd agency/filesystem
   ./create_img.sh vexpress

Deployment into the storage device
----------------------------------

Finally, the deployment of all Agency components (including the
bootloader in some configurations) is achieved with the following script
(option ``-a`` for all)

.. code:: bash

   cd agency
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

The main ``ME``\ directory is amazingly ``ME`` at the root. The
``ME/base`` directory contains all the source code and related files of
all mobile entities. Indeed, each ME is produced according to their
configuration file and device tree.

Basically, a ME is constituted of its kernel (based on SO3 Operating
System), a device tree and eventually a rootfs used as **ramfs** (the
rootfs is embedded in the ME image itself, hence the ITB file).

ME build script
---------------

The ME can be build using the build.sh script found in ``ME/base`` directory.
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

To deploy the newly built ME in the third partition of (virtual) SD-card use the deploy.sh script found in the``agency/`` folder.

.. code:: bash
   
   cd agency
   ./deploy.sh -m SOO.<ME_NAME>


ME Injection from the Agency
----------------------------

It’s time to test the new ME in the running environment. To do that,
simply start the framework. The agency process which is started
automatically will inspect the contents of ``/mnt/ME`` directory and
load all available ``itb`` files.
