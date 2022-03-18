.. _architecture:

################
SOO Architecture
################
  
.. toctree::
   :maxdepth: 5
   :hidden:
   
   overall
   agency/agency
   ME/ME
   virt_interfaces/virt_interfaces
   

SOO (Smart Object Oriented) Architecture is divided into two main areas: ``Agency`` and ``Mobile Entity (ME)``.

The agency consists in the software components which reside in a smart object (SOO). It is mainly composed of
an hypervisor called ``AVZ`` *Agency VirtualiZer* and the ``Linux`` domain.

The Mobile Entity is the execution environment which is able to migrate from one SOO to another.
So far, an ME is made of the SO3 operating system which runs kernel tasks and/or user space processes, depending
on its configuration (most MEs can probably achieved by means of kernel tasks).

A smart object can host several MEs; currently the max number is defined to 5 MEs, but it could be much more since
the ME is relatively a small and compact environment.

