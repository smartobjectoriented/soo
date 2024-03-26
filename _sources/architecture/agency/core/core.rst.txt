Core subsystem
==============

.. toctree::
   :maxdepth: 5
   :hidden:
   
   migration_manager
   device_access
   me_access
   injector
   
The general architecture of the agency core subsystem is depicted on the figure below.
It shows the various functional blocks of this subsystem.

.. figure:: /img/SOO_architecture_v2021_2-Agency_core.png
   :align: center
   
   Agency core subsystem architecture

The **Migration Manager** functional block is in charge to manage the migration cycle at a certain frequency. 
All mobile entities (MEs) are queried through specific callbacks and if they are ready to be migrated, 
the Migration Manager is starting the callback sequence.

Additionally, the Migration Manager provides an API to retrieve information about present MEs such as
their ``SPID`` (64-bit Specy ID), name and short description.

The **Device Access** functional block is decomposed in several functions like ``Storage`` which deals 
with the access to the local storage, an internal eMMC flash like the one present in smart object. 
The ``Device`` function is used to query the availability and other information about the local peripherals. 
To do that, this function interacts with the :ref:`Virtualized Interfaces <virt_interfaces>` subsystem. 

Finally, the ``Identity``  function manages the agencyUID which is proper to the Smart Object. 
It takes care of the location and security aspects related to the **agencyUID** identifier.



