.. _agency:

******
Agency
******


.. toctree::
   :maxdepth: 5
   :hidden:
   
   core/core
   soolink/soolink
   
 
The agency is the whole software which is resident within a smart object. It is composed of the ``avz`` hypervisor
and ``Linux`` as the main domain. Actually, the domain is divided in ``domain #0`` which is the non realtime domain
of the agency, and ``domain #1`` which is a hard realtime domain running independently of the Linux scheduler.
The hard realtime domain is a highly modified version of the Xenomai/Cobalt kernel with its RTDM API.

The figure below shows the Agency subsystems and functional blocks.

.. figure:: /img/SOO_Architecture_general_detailed.png
   :scale: 50 %

   SOO Subsystems and functional blocks

================
VBus and VBstore
================

*to be completed*

VBstore
-------

.. warning::
   Do not forget to free the memory allocated at the return of ``vbus_read()``.

