.. _ME:

******************   
Mobile Entity (ME)
******************


A Mobile Entity (ME) is the core logic of the application which can be constituted of several tasks or 
even of several processes. MEs are based on SO3 operating system.

ME ID Information
=================

All information related to the identity of the ME (SPID, name, short desc, etc.) is primarly 
found in its ME device tree (DT).

Device tree properties bindings and entries in vbstore
------------------------------------------------------

The following properties are used in a ME dts.

+------------------+-------------------------------------------------------------+
| Property         | Description                                                 |
+==================+=============================================================+
| *<spid>*         | Species ID in a 128-bit value encoding                      |
+------------------+-------------------------------------------------------------+
| *<me_name>*      | Name of this ME (for example SOO.outdoor)                   |
+------------------+-------------------------------------------------------------+
| *<me_shortdesc>* | Short description of this ME (must contain 1024 char. max.) |
+------------------+-------------------------------------------------------------+

The following entries are dedicated to the ME ID information.

* /soo/me/<domID>/spid
* /soo/me/<domID>/name
* /soo/me/<domID>/shortdesc

The :ref:`Migration Manager <migration_manager>` provides a facility to get 
the list of MEs with their ID Information in the kernel and in the user space
via an ioctl.

Species Identifier (SPID)
-------------------------

A Mobile Entity is identified by its SPID, a unique 128-bit identifier corresponding to its species
(for example, "SOO.blind" has a unique SPID that is associated).
Several ME of the same SPIDs are de facto in various smart objects, possibly in the same smart object
until one instance decide to kill the other one(s).

The following SPID are currently assigned:

Basic mobile entities
^^^^^^^^^^^^^^^^^^^^^

+----------------------+-----------------------------------------------------------------------------+
| SPID                 | ME Description                                                              |
+======================+=============================================================================+
| *0x0010000000000001* | SOO.refso3 used as reference ME. A new ME can be based on this model.       |
|                      | There are mainly two configurations (hence two dts) with and without ramfs. |
+----------------------+-----------------------------------------------------------------------------+
| *0x0010000000000002* | SOO.ledctrl is the ME running in SOO.ledctrl smart object which is          |
|                      | a Raspberry Pi 4 enhanced with the Sense HAT extension. This ME             |
|                      | basically uses the led matrix and joystick. It is mainly used               |
|                      | for demonstration purposes.                                                 |
+----------------------+-----------------------------------------------------------------------------+


Mobile entities devoted to SOO.domotics family
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

+----------------------+----------------------------------------------------------------------------+
| SPID                 | ME Description                                                             |
+======================+============================================================================+
| *0x0020000000000001* | SOO.blind is devoted to the handling of a "standard" blind which provides  |
|                      | the user with features such as putting the blinds up or down, but the ME   |
|                      | can also perform synergies with SOO.outdoor for example to decide how      |
|                      | to manipulate the blinds, according to the weather condition               |
+----------------------+----------------------------------------------------------------------------+
| *0x0020000000000002* | SOO.outdoor is able to collect data from any weather station. This ME      |
|                      | can be used in conjunction with other MEs which can use this kind of data. |
+----------------------+----------------------------------------------------------------------------+

System and housekeeping mobile entities
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

+----------------------+------------------------------------------------------------------------------------------+
| SPID                 | ME Description                                                                           |
+======================+==========================================================================================+
| *0x0030000000000001* | SOO.agency enables the upgrade of agency image within a smart object. The ME             |
|                      | has an embedded of the new version of agency. All smart objects can be updated.          |
+----------------------+------------------------------------------------------------------------------------------+
| *0x0030000000000002* | SOO.net can transport networking data packet from a SOO.net smart object to any          |
|                      | other smart objects. The ME can then collect data packet to forward them to the network. |
+----------------------+------------------------------------------------------------------------------------------+


Species Aptitude Descriptor (SPAD)
----------------------------------

Each ME may have one or several :term:`SPAD` (Species Aptitude Descriptor). The ``SPAD`` determines a specific
feature (or set of features).

The *SPAD* is also used to tell the agency if the ME is inclined to cooperate with other ME.

Enabling the possibility for an ME to perform *cooperation* with other ME requires to call
the following function in the main application of the ME:

.. c:function:: 
   void spad_enable_cooperate(void)

   
Lifecycle of a Mobile Entity
============================

When a ME is injected into a smart object by the user or simply arrives from another smart object,
the agency initiates a sequence of callback function execution within the context of the ME, but
from the CPU's agency (CPU #0 normally).

The ME will then prepare the initialization of ``vbstore`` entries related to itself as well as to
the frontend drivers which are managed by the ME.

ME Interactions with the User Interface application
===================================================

The ME can manage XML messages and events in order to interact with a GUI running
on the tablet. The following helpers are very helpful to this purpose. The messages/events
are forwarded to the vuihandler frontend.


Message handling
----------------

This function prepare a XML message based on its ID and value:

.. c:function:: 
   void xml_prepare_message(char *buffer, char *id, char *value)

The buffer is allocated by the caller and will contain the XML formatted message.
  

Event handling
--------------

.. c:function::
   void xml_parse_event(char *buffer, char *id, char *action)

The event message (pointed by *buffer*) contains a specific action with an associated ID. These fields can be retrieved
with this function. The caller must allocate the memory.







