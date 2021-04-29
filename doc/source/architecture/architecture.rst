.. _architecture:

################
SOO Architecture
################

.. toctree::
   :maxdepth: 5
   :hidden:
   
   soolink/soolink
   
==============================
Overall framework architecture
==============================

The next figure shows an overview of the execution environment inside a smart object 

.. figure:: /img/SOO_Architecture_overview.png
   :scale: 50 %
    
   Overview of the SOO framework in a smart object

The ``Agency`` has two components: the ``AVZ Hypervisor`` which is a type-1 hypervisor and
the Linux domain. The Agency constitutes the resident part of the smart object, i.e. it does not
migrate itself between smart objects. 

The ``Mobile Entity`` (ME) is a complete environment based on the SO3 operating system. 
The ME automatically migrates from a smart object to another smart object which is present
in its neighborhood. The ME has the application logic which interacts with the hardware by
means of interactions with the Agency, through virtualized interfaces. Furthermore, one
of major fharacteristics of SOO is that all resident MEs can interact each with other according
when necessary.

As shown on this picture, several MEs can reside (and run) at a certain time. The limit of MEs will depend
on the available RAM reserved for this purpose.


The Big Picture
---------------

The next figure is another view of the general architecture. The ME embeds the SO3 kernel and may also have a 
user space; in this case, applications such a shell can run and make usage of libc (or other libraries) functionalities.

.. figure:: /img/SOO_Architecture_general.png
   :scale: 50 %

   General architecture with resident and migrating parts

AVZ Hypervisor is responsible to start the Agency when the smart object is switched on as well as taking care
about loading, hosting and managing the migration of all mobile entities which may travel over the time.


======
Agency
======

The agency is the whole software which is resident within a smart object. It is composed of the ``avz`` hypervisor
and ``Linux`` as the main domain. Actually, the domain is divided in ``domain #0`` which is the non realtime domain
of the agency, and ``domain #1`` which is a hard realtime domain running independently of the Linux scheduler.
The hard realtime domain is a highly modified version of the Xenomai/Cobalt kernel with its RTDM API.

Subsystems and Functional Blocks
--------------------------------

.. figure:: /img/SOO_Architecture_general_detailed.png
   :scale: 50 %

   SOO Subsystems and functional blocks

.. _ME:

==================
Mobile Entity (ME)
==================

A Mobile Entity (ME) is the core logic of the application which can be constituted of several tasks or 
even of several processes. MEs are based on SO3 operating system.

Specy Aptitude Descriptor (SPAD)
--------------------------------

Each ME may have one or several :term:`SPAD` (Specy Aptitude Descriptor). The ``SPAD`` determines a specific
feature (or set of features).

The *SPAD* is also used to tell the agency if the ME is inclined to cooperate with other ME.

Enabling the possibility for an ME to perform *cooperation* with other ME requires to call
the following function in the main application of the ME:

.. code-block:: c

   spad_enable_cooperate();
  
  

