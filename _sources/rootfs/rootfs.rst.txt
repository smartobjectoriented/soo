
.. _rootfs:

############################
User applications and rootfs
############################

.. toctree::
   :maxdepth: 5
   :hidden:
   
   agency_usr

In the SOO environment and on the *agency* side, there are actually two ``rootfs`` (*root filesystem*).

The first one is known as ``initrd`` and contains a minimal *rootfs* which is stored in *RAM*. It is
basically used to load firmware, basic drivers and to prepare switching to the secondary *rootfs*
generally stored in a separate partition of the SD-card.

The second one is the main (secondary) *rootfs* which contains everything required at the user space level.

The *rootfs* is built by means of ``buildroot`` which can be easily customised according to needs.

In addtion, user space applications can also be compiled and deployed separately.
These :ref:`applications <agency_usr>` are stored in the ``agency/usr/`` directory.



   
