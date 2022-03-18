.. _virt_interfaces:

*****************************************
Virtualized interfaces (backend/frontend)
*****************************************

The SOO framework has various virtualized interfaces which can be used between the ME and the agency.
On the agency side, these are ``backends`` (BE) drivers and can manage several links to MEs which have the 
corresponding ``frontends`` (FE) drivers. Basically, the backend manage one *link* for each ME and each FE.


Backends
========

Linux has a well-defined device and driver model in which the basic structures are ``struct device`` and ``struct device_driver``.
On top of that, the ``struct vbus_device`` structure defined in *vbus* is used to refer a specific backend device and the
``struct vbus_driver`` refers to a specific backend driver.

As usual in Linux, the association between device and driver is done with a matching mechanism between strings which refer
to a device and to a driver. When a matching between device an driver is done, the structures have the references to each
other for this spcific binding.

.. figure:: /img/SOO_architecture_v2021_2-BE_structures.png
  
   Device and driver model for a backend within Linux

In Linux, and in the underlying SOO vbus mechanism which is inspired from the XEN hypervisor (however quite different), 
a ``vbus_device`` contains the ``device`` structure.

Each backend has a private structure which has reference to the *vbus* definition associated to the device; it contains
the ring(s) and other fields required for managing the activities related to the interactions with the frontend.

There is also a private structure attached to the ``vbus_driver`` associated to a device. The following helpers can 
be used to handle this private structure:

To attach a private structure to a *vbus driver*

.. c:function:: 
   void *vdrv_get_priv(struct vbus_driver *vdrv) 

To get the private structure attached to a *vbus_driver*

.. c:function::
   void *vdrv_get_priv(struct vbus_driver *vdrv) 

Finally, a helper to get the private structure attached to *vbus_driver* from a particular *vbus_device*

.. c:function::
   void *vdrv_get_vdevpriv(struct vbus_device *vdev)
   
Defining a private structure attached to a *vbus_driver* can be done in the *init* function of the driver.

VBstore and storage of properties
---------------------------------

Each backend driver must have a basic entry in ``vbstore`` in order to process properties managed by the frontend.
The entries are pre-defined in the file ``soo/kernel/vbstore/vbstorage.c``.

Example of such a entry is:

.. code-block:: c

   np = of_find_compatible_node(NULL, NULL, "vdummy,backend");
   if (of_device_is_available(np))
      vbs_store_mkdir("/backend/vdummy");

As we can see, the entry is created only if the backend is enabled in the *device tree*.

Frontends
=========

SO3 has a slightly different device and driver model. The main structure related to the device is the ``dev_t`` structure
which is built during the device tree parsing; as soon as a frontend is detected in the DTS, a corresponding ``dev_t`` structure
is allocated and filled with the information from the DTS. The main frontend initialization function is called the private
structure ``vdummy_priv_t`` is allocated at this time.

.. figure:: /img/SOO_architecture_v2021_2-FE_structures.png
    
   Device and driver model for a frontend within SO3

VBstore on ME side
------------------

Each frontend driver must create its own basic entry in ``vbstore``. Properties such as its *state* will be then added so that
a ``watch`` can be attached and trigger a function execution when the state changes.

The entries are created in the file ``soo/kernel/vbstore/vbstore_me.c``.

Example of such an entry is :

.. code-block:: c

   fdt_node = fdt_find_compatible_node(__fdt_addr, "vdummy,frontend");
   if (fdt_device_is_available(__fdt_addr, fdt_node)) {
      DBG("%s: removing vdummy from vbstore...\n", __func__);
      vbstore_dev_remove(ME_domID(), "vdummy");
   }

Again, we check the device tree (in SO3) to see if the frontend is enabled or not.



