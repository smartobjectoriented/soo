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



Frontends
=========

SO3 has a slightly different device and driver model. The main structure related to the device is the ``dev_t`` structure
which is built during the device tree parsing; as soon as a frontend is detected in the DTS, a corresponding ``dev_t`` structure
is allocated and filled with the information from the DTS. The main frontend initialization function is called the private
structure ``vdummy_priv_t`` is allocated at this time.

.. figure:: /img/SOO_architecture_v2021_2-FE_structures.png
    
   Device and driver model for a frontend within SO3


