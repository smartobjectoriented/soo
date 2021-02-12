
=============================================
Configuration for AArch64 64-bit architecture
=============================================

This chapter is devoted to the AArch64 64-bit configuration.

QEMU target
-----------

The new platform in the emulated environment is called ``virt64`` (instead of ``vexpress``)

The following target is required at the configuration:

.. code:: bash

   cd qemu
   ./configure --target-list=aarch64-softmmu --disable-attr --disable-docs
   make -j8

(The other options may remain unchanged)

(Virtual) SD-card Storage
-------------------------

The virtual storage file can be generated with the following command:

.. code:: bash
   
   ./create_img.sh virt64

U-boot
------

Of course, U-boot must be compiled with the aarch64-linux-gnu toolchain.
Edit the Makefile and change the ``CROSS_COMPILE`` variable.

U-boot can be configured (and rebuilt) as follows:

.. code:: bash
   
   make qemu_arm64_defconfig
   make -j8

Agency
------

.. warning::

   Do not forget to modify file ``build.conf`` in the *agency/* directory in order to have
   the correct platform.
   
Linux
~~~~~

Again, the Makefile needs to be adpated accordingly, with the correct *CROSS_COMPILE* value, i.e. ``aarch64-linux-gnu``
Furthermore, the **ARCH** variable must be set to ``arm64`` (which corresponds to the subdirectory in *arm/* directory.

- The default configuration to be used is  ``virt64_defconfig``

^^^^^^^^^^^^^^^^^^^^^^^^
Address Space Management
^^^^^^^^^^^^^^^^^^^^^^^^

Linux address space: 

Virtual addresses are 48-bit

- User space:     0x0000000000000000 - 0x0000800000000000
- Kernel space:   0xffff800000000000 - 0xffffffffffffffff

Constants:

- TEXT_OFFSET = 0x800000

~~~~~~~~~
Bootstrap
~~~~~~~~~

- U-boot prepare the image header as expected by Linux:

.. code-block:: c

   struct Image_header {
      uint32_t code0;      /* Executable code */
      uint32_t code1;      /* Executable code */
      uint64_t text_offset;   /* Image load offset, LE */
      uint64_t image_size; /* Effective Image size, LE */
      uint64_t flags;      /* Kernel flags, LE */
      uint64_t res2;    /* reserved */
      uint64_t res3;    /* reserved */
      uint64_t res4;    /* reserved */
      uint32_t magic;      /* Magic number */
      uint32_t res5;
   };

- Start physical address: 0x40080000  (determined by U-boot probably)
 


