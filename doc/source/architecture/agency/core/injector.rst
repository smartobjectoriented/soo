
.. _injector:

ME Injector
-----------

The **ME Injector** functionality allows a ME to be injected into a smart object. At the bootstrap of SOO, the Agency looks
at a dedicated partition of the SD-CARD to retrieve some ME ITBs.

An ME is packed into a U-boot ITB file which is parsed by the AVZ hypervisor when loading into the RAM.


Deployment in dedicated storage partition
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

This is the standard method used in the Smart Objects. The entities (MEs) are stored in a dedicated
partition (*/mnt/ME* mount point).

The Agency core automatically injects all MEs that are in this partition by invoking ``ME_inject()`` which 
query the hypervisor to parse the ITB buffer.

Deployment via a remote GUI
^^^^^^^^^^^^^^^^^^^^^^^^^^^

A user can upload a ME from a tablet (or smartphone). The ME ITB is stored in the Android device and is transferred
via Bluetooth. The ``vuihandler`` backend is involved in the retrieval of the ITB buffer and invokes the injector
to load the ME.


Memory management during the injection
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The injector is responsible to allocate the memory necessary to store the ITB buffer (image) which represents the ME itself.

When the ME is stored in the local SD-card storage, the agency core application allocates a user space area.
When the ME is retrieved from an external device using Bluetooth, the *vuihandler* backend performs an in-kernel memory
allocation in the *vmalloc'd* area of the Linux kernel.

These kind of memory allocation results in spare allocation of pages and implies updates of the page tables within the LInux kernel.

However, the hypervisor needs to switch to its own address space in order to get access to the ME memory area in the RAM. 
In this MMU configuration, the ITB buffer is not accessible anymore. For this reason, a copy of the ME ITB into the AVZ heap
is done.

.. figure:: /img/20220316_4_SOO_architecture-Injector.drawio.png 
   :align: center
   

As depicted on the figure above, the ME ITB may have been loaded into the agency user space or in the *vmalloc'd* area
of the Linux kernel.

We assume that 8-MB AVZ heap is sufficient to host the ITB ME (< 2 MB in most cases).

If the ITB should become larger, it is still possible to compress (and enhance AVZ with a uncompressor invoked
at loading time). Wouldn't be still not enough, a temporary fixmap mapping combined with *get_free_pages* should be envisaged
to have the ME ITB accessible from the AVZ user space area.

However, in the 64-bit version, this will not be an issue anymore because the memory zone reserved to the hypervisor will be much larger.

