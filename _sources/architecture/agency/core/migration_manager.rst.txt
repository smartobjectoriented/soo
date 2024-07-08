
.. _migration_manager:

Migration manager logical block
-------------------------------

The Migration Manager deals with the automatic propagation of the residing MEs within a Smart Object. 
This subsystem does not require realtime capabilities and therefore runs on CPU #0 (non-realtime CPU).


Migration Sequence
^^^^^^^^^^^^^^^^^^

**DomainID, ME slot ID**


There are several ways to identify a ME in a smart object. Two identifiers are used and have different
semantics: the **domID** and the (ME) **slotID**. 

The domain refers to the complete environment running either Linux or SO3.

There are actually two types of agency: the non-RT (standard) Agency running Linux on CPU #0 which is the main
Linux environment running most system and SOO applications. The RT Agency is running (the same) Linux on CPU #1
but refers to the hard realtime extension integrated in Linux. The RT extension is based on a revisited
Xenomai/Cobalt integration where there is absolutely **no** interaction with the Linux scheduler. It is aimed at
running critical tasks using the local environment (interfaces) with hard time constraints. 

And finally, the ME domains wich run SO3 operating system (with or without rootfs).

The identifiers are:

* ``DomainID`` which refers to the domain: the first domain is considered as **domID #0** and corresponds to
  the non-RT agency domain. The **domID #1** is attached to the RT agency. In the memory, the two domains
  occupied the same area. For this reason, both domID 0 and domID 1 will use the same *memslotID*.
  Hence, the first ME will use **domID #2**, the second ME **domID #3**, etc.

* ``ME slot ID`` refers to the memslot used to store the domain (or the hypervisor). Typically,
  **slotID #0** for storing the hypervisor, them **slotID #1** is used for *domain #0* **and**
  *domain #1* (the same Linux environment actually). The **slotID #2** corresponds to the ME with **domID #2**,
  and thus regarding MEs, *slotID* is identical to *domID*.



 
