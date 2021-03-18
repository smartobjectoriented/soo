.. _user_guide:

 
User Guide
==========
   
The installation should work in any Ubuntu/Kubuntu installation superior
to ``16.10``. It is assumed that you are running an x86_64 version.

The following setup prepares to run the SOO framework in a QEMU-based
emulated environment. There is a file called **build.conf** in the
*agency/* directory which specifies the target platform. After a fresh
clone, build.conf is configured for QEMU *vExpress* platform.

Additionally, the *vExpress* target is also configured with TrustZone
support.

Pre-requisite
-------------

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
   
The OP-TEE environment requires the following python packages:

.. code:: bash

   pip3 install pycryptdome
   sudo apt install python3-pyelftools

The following packets are not mandatory, but they can be installed to
prevent annoying warnings:

.. code:: bash

   sudo apt-get install bison flex

Toolchain
~~~~~~~~~

The toolchain has been created by Linaro, with the version 2018-05. It
includes an arm-linux-gnueabihf GCC 6.4.1 compiler. For now, nothing has
been tested with a version greater than this one.

You can download the toolchain at the following address:
http://releases.linaro.org/components/toolchain/binaries/6.4-2018.05/arm-linux-gnueabihf/gcc-linaro-6.4.1-2018.05-x86_64_arm-linux-gnueabihf.tar.xz.asc

Uncompress the archive and add the *bin* subdirectory to your path.

You can check the version by running the following command:

.. code:: bash

   arm-linux-gnueabihf-gcc --version

The output should look like:

::

   Using built-in specs.
   COLLECT_GCC=/opt/gcc-linaro-6.4.1-2018.05-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-gcc
   COLLECT_LTO_WRAPPER=/opt/gcc-linaro-6.4.1-2018.05-x86_64_arm-linux-gnueabihf/bin/../libexec/gcc/arm-linux-gnueabihf/6.4.1/lto-wrapper
   Target: arm-linux-gnueabihf
   Configured with: '/home/tcwg-buildslave/workspace/tcwg-make-release/builder_arch/amd64/label/tcwg-x86_64-build/target/arm-linux-gnueabihf/snapshots/gcc.git~linaro-6.4-2018.05/configure' SHELL=/bin/bash --with-mpc=/home/tcwg-buildslave/workspace/tcwg-make-release/builder_arch/amd64/label/tcwg-x86_64-build/target/arm-linux-gnueabihf/_build/builds/destdir/x86_64-unknown-linux-gnu --with-mpfr=/home/tcwg-buildslave/workspace/tcwg-make-release/builder_arch/amd64/label/tcwg-x86_64-build/target/arm-linux-gnueabihf/_build/builds/destdir/x86_64-unknown-linux-gnu --with-gmp=/home/tcwg-buildslave/workspace/tcwg-make-release/builder_arch/amd64/label/tcwg-x86_64-build/target/arm-linux-gnueabihf/_build/builds/destdir/x86_64-unknown-linux-gnu --with-gnu-as --with-gnu-ld --disable-libmudflap --enable-lto --enable-shared --without-included-gettext --enable-nls --with-system-zlib --disable-sjlj-exceptions --enable-gnu-unique-object --enable-linker-build-id --disable-libstdcxx-pch --enable-c99 --enable-clocale=gnu --enable-libstdcxx-debug --enable-long-long --with-cloog=no --with-ppl=no --with-isl=no --disable-multilib --with-float=hard --with-fpu=vfpv3-d16 --with-mode=thumb --with-tune=cortex-a9 --with-arch=armv7-a --enable-threads=posix --enable-multiarch --enable-libstdcxx-time=yes --enable-gnu-indirect-function --with-build-sysroot=/home/tcwg-buildslave/workspace/tcwg-make-release/builder_arch/amd64/label/tcwg-x86_64-build/target/arm-linux-gnueabihf/_build/sysroots/arm-linux-gnueabihf --with-sysroot=/home/tcwg-buildslave/workspace/tcwg-make-release/builder_arch/amd64/label/tcwg-x86_64-build/target/arm-linux-gnueabihf/_build/builds/destdir/x86_64-unknown-linux-gnu/arm-linux-gnueabihf/libc --enable-checking=release --disable-bootstrap --enable-languages=c,c++,fortran,lto --build=x86_64-unknown-linux-gnu --host=x86_64-unknown-linux-gnu --target=arm-linux-gnueabihf --prefix=/home/tcwg-buildslave/workspace/tcwg-make-release/builder_arch/amd64/label/tcwg-x86_64-build/target/arm-linux-gnueabihf/_build/builds/destdir/x86_64-unknown-linux-gnu
   Thread model: posix
   gcc version 6.4.1 20180425 [linaro-6.4-2018.05 revision 7b15d0869c096fe39603ad63dc19ab7cf035eb70] (Linaro GCC 6.4-2018.05)

Basic Components
----------------

Currently, the framework contains all what is required to get a full
functional environment. It includes the QEMU emulator, ARM TrustZone
components, U-boot bootlader, etc.

QEMU
~~~~

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
----------------------------

Since the SOO agency relies on TrustZone for security concerns, it is
necessary to compile the trusted-firmware-a package as follows:

ARM Trusted firmware (trusted-firmware-a) also known as ATF
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code:: bash

   cd trusted-firmware-a
   ./build.sh

OTEE_OS (Open Trusted Execution Environment)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code:: bash

   cd optee_os
   ./build.sh

OPTEE TA (Trusted Applications)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The *optee_ta/* directory contains our trusted applications used to
cipher/uncipher the ME, discovery beacons, etc.

.. code:: bash

   cd optee_ta
   ./build.sh

U-boot
------

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


SOO Components
--------------

Agency
~~~~~~

This section presents the different components which are required to be
built in the **agency/** directory. Different configurations are possible.

Target platforms
^^^^^^^^^^^^^^^^
The file ``build.conf`` in ``agency/`` contains the ``PLATFORM`` (and eventually ``TYPE``) variables 
to select the target platform.

Possible platforms and types are:

+------------+-------------------------------------+
| Name       |                                     |
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
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

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
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

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
^^^^^^^^^^^^^^^^^^^^^^^^

In addition to the ``rootfs``, the Agency has its own applications that
can be found in ``agency/usr``. The build system of this part relies on
CMake. The build is achieved with the following script:

::

   cd agency/usr
   ./build.sh

Agency filesystem
^^^^^^^^^^^^^^^^^

Once all main Agency components have been built, they will be put in a
virtual disk image as it is possible to attach such a virtual SD-Card
storage device with QEMU). The virtual storage is created in
``filesystem/`` directory and will contain all the necessary partitions.

The creation of the virtual disk image is done as follows:

.. code:: bash

   cd agency/filesystem
   ./create_img.sh vexpress

Deployment into the storage device
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

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
~~~~~~~~~~~~~~~~~~

For a quick test, it is proposed to build and to deploy the SOO.refso3
reference Mobile Entity.

ME Build
^^^^^^^^

The main ``ME``\ directory is amazingly ``ME`` at the root. The
``ME/base`` directory contains all the source code and related files of
all mobile entities. Indeed, each ME is produced according to their
configuration file and device tree.

Basically, a ME is constituted of its kernel (based on SO3 Operating
System), a device tree and eventually a rootfs used as **ramfs** (the
rootfs is embedded in the ME image itself, hence the ITB file).

ME Kernel Build
^^^^^^^^^^^^^^^

The SO3 kernel of the SOO.refso3 ME is built with the following
commands:

.. code:: bash

   cd ME/base/so3
   make refso3_ramfs_defconfig
   make

As you can see, the build system is still based on Linux KBuild.

ME User Space Build
^^^^^^^^^^^^^^^^^^^

In this case, the ``refso3_ramfs_defconfig`` configuration means we have
a rootfs with the ME. Therefore, we can compile the ``usr/`` component
which contains basic applications (note that most applications are
issued from the SO3 gitlab repository).

.. code:: bash

   cd ME/base/usr
   make

ME Filesystem Generation and Deployment
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

As for the Agency, the ME needs a virtual storage based on FAT-32 to
store the rootfs components. Note that ``so3virt`` below refers to the
type of (target) platform of the SO3 environment (which in our case is a
generic virtual platform).

This is done as such:

.. code:: bash

   cd ME/base/rootfs
   ./create_ramfs so3virt

And of course, the deployment of *usr* contents into this storage device
(only one partition). Again, ``so3virt`` refers to the platform type
used in SO3 in this context.

.. code:: bash

   cd ME/base/usr
   ./deploy.sh so3virt

Final Deployment
^^^^^^^^^^^^^^^^

The ME ITB is produced with the following deployment script:

.. code:: bash

   cd ME/base
   ./deploy.sh SOO.refso3 refso3_ramfs

The script indicates that the resulting ``itb`` file is copied in
``SOO.refso3`` (in ``ME/``) directory.

Now, the related ``itb`` file has to be deployed in the third partition
of the (virtual) SD-card found in the Agency.

.. code:: bash

   cd agency
   ./deploy.sh -m SOO.refso3

ME Injection from the Agency
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

It’s time to test the new ME in the running environment. To do that,
simply start the framework. The agency process which is started
automatically will inspect the contents of ``/mnt/ME`` directory and
load all available ``itb`` files.
