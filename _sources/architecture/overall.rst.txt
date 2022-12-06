.. _overall:

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

Descriptors and identity management
-----------------------------------

The agency and the ME have their own attributes which are stored in descriptors and in 
the :ref:`vbstore database <virt_interfaces>`.

The following picture shows how the agency and ME descriptors are managed in the framework.

.. figure:: /img/20220316_4_SOO_architecture-AVZ_and_domains.drawio.png
   
   Agency descriptor, ME descriptor and ME ID information
   
There are actually three kinds of structure to be differenciated as shown in this figure: the Agency descriptor,
the ME descriptor and the ME identity information. The two descriptors contain basic ID information and technical
data such as pfn, ME size, etc. and are mainly used by the (*Linux*) domain and *avz* hypervisor.

Agency and ME descriptors are stored in their own ``shared info page``; on the agency side, this page is shared 
between the domain and the hypervisor, and on the ME side, the page is shared between the ME and the hypervisor.

Agency descriptor
^^^^^^^^^^^^^^^^^
The agency descriptor is very simple since currently, only the ``agencyUID`` is stored.
In the agency, ``agencyUID`` can be obtained with ``current_soo->agencyUID``

The ``agencyUID`` is the unique 64-bit identifier which identify a smart object (SOO). It is merely
related to the hardware.

ME descriptor
^^^^^^^^^^^^^
The ME descriptor is accessible in the ME through its *shared info* page.
On the agency side, the ME descriptor is access via the following function:

.. c:function::
   void get_ME_desc(unsigned int slotID, ME_desc_t *ME_desc)

   The function executes an hypercall to retrieve the ME descriptor (allocated by the caller).
 
ME ID information
^^^^^^^^^^^^^^^^^

At the injection of a ME or at its arrival in a smart object, some *vbstore* entries are populated
with various information such as the SPID (ME identifier), name and short description. 

Typically, if a backend driver or any other functional block that needs to retrieve some general information
about the ME will use the ``ME_id_t`` structure which contain some common attributes than the ME descriptor (SPID, *spadcaps*, etc.)
and additional information like name, short description, etc. 

.. c:function::
   bool get_ME_id(uint32_t slotID, ME_id_t *ME_id)
   
   This function queries *vbstore* to get various information such as SPID, *spadcaps*, name, short description, etc.
   
   
   
