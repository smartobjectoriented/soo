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
| *<spid>*         | Species ID in a 64-bit value encoding                       |
+------------------+-------------------------------------------------------------+
| *<spadcaps>*     | Species Aptitude Capabilities in a 64-bit value encoding    |
+------------------+-------------------------------------------------------------+
| *<me_name>*      | Name of this ME (for example SOO.outdoor)                   |
+------------------+-------------------------------------------------------------+
| *<me_shortdesc>* | Short description of this ME (must contain 1024 char. max.) |
+------------------+-------------------------------------------------------------+

The following entries are dedicated to the ME ID information.

* /soo/me/<domID>/spid
* /soo/me/<domID>/spadcaps
* /soo/me/<domID>/name
* /soo/me/<domID>/shortdesc

The :ref:`Migration Manager <migration_manager>` provides a facility to get 
the list of MEs with their ID Information in the kernel and in the user space
via an ioctl.

Species Identifier (SPID)
-------------------------

A Mobile Entity is identified by its SPID, a unique 64-bit identifier corresponding to its species
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

+-----------------------+-----------------------------------------------------------------------------+
| SPID                  | ME Description                                                              |
+=======================+=============================================================================+
|| *0x0020000000000001* || SOO.blind is devoted to the handling of a "standard" blind which provides  |
||                      || the user with features such as putting the blinds up or down, but the ME   |
||                      || can also perform synergies with SOO.outdoor for example to decide how      |
||                      || to manipulate the blinds, according to the weather condition               |
+-----------------------+-----------------------------------------------------------------------------+
|| *0x0020000000000002* || SOO.outdoor is able to collect data from any weather station. This ME      |
||                      || can be used in conjunction with other MEs which can use this kind of data. |
+-----------------------+-----------------------------------------------------------------------------+
|| *0x0020000000000003* || SOO.wagoled is able to received event from an EnOcean switch and redirect  |
||                      || the received commnand to the Wago ecosystem.                               |
+-----------------------+-----------------------------------------------------------------------------+

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

The ME may frequently migrate from one smart object to another one. Actually, a memory snapshot
of the ME to migrate is performed by the agency, which is transfered to the other smart object.
Of course, the snapshot requires the ME to be in a consistent state; that is why the ME must
suspend its frontend drivers before the snapshot is done. All frontend drivers will be resumed
right after the copy; these operations happen with efficient inter-CPU interrupt (IPI) mechanisms and
are performed very quickly; In most cases, the running ME is briefly interrupted and does not cause
any significant latency.

Hence, when a ME is injected into a smart object by the user, or coming from another smart object,
the agency initiates a sequence of callbacks function execution within the context of the ME, but
from the CPU's agency, i.e. CPU #0. The residing ME has its own callback sequence which involves
suspending (before the snapshot) and resuming (after the snapshot) the backend drivers. 
The callback sequences are therefore slightly different between the residing ME and the migrated ME.

Furthermore, in the migrating (arriving) ME, the ME has to create and initialize the ``vbstore`` entries 
related to itself as well as to all frontend drivers managed by the ME.

Finally, the newly injected ME (from a tablet/smartphone or automatically from the SD-card at the boot time)
has a dedicated callback sequence as well. 

All these callback sequences are described in the next sections.

State of a Mobile Entity
------------------------

Any ME has an internal state to manage its behaviour. The state can be changed at any time by the different callbacks.
The following functions are available to manage the ME state:

.. c:function::
   void set_ME_state(ME_state_t state)

   To set a ME in a specific state

.. c:function::
   int get_ME_state(void)

   To get the current a ME state.
 

+-----------------------+-------------------------------------------------------------------------------------------------------------+
| State                 | Description                                                                                                 |
+=======================+=============================================================================================================+
| *ME_state_booting*    | ME is currently booting...                                                                                  |
+-----------------------+-------------------------------------------------------------------------------------------------------------+
| *ME_state_preparing*  | ME is being paused during the boot process, in the case of an injection, before the frontend initialization |
+-----------------------+-------------------------------------------------------------------------------------------------------------+
| *ME_state_living*     | ME is full-functional and activated (all frontend devices are consistent)                                   |
+-----------------------+-------------------------------------------------------------------------------------------------------------+
| *ME_state_suspended*  | ME is suspended before migrating. This state is maintained for the resident ME instance                     |
+-----------------------+-------------------------------------------------------------------------------------------------------------+
| *ME_state_migrating*  | ME just arrived in SOO                                                                                      |
+-----------------------+-------------------------------------------------------------------------------------------------------------+
| *ME_state_dormant*    | ME is resident, but not living (running)                                                                    |
+-----------------------+-------------------------------------------------------------------------------------------------------------+
| *ME_state_killed*     | ME has been killed before to be resumed                                                                     |
+-----------------------+-------------------------------------------------------------------------------------------------------------+
| *ME_state_terminated* | ME has been terminated (by a force_terminate)                                                               |
+-----------------------+-------------------------------------------------------------------------------------------------------------+
| *ME_state_dead*       | ME does not exist                                                                                           |
+-----------------------+-------------------------------------------------------------------------------------------------------------+

Callback functions
------------------

There are two kinds of callback functions in a ME: ``domcalls`` and ``dc_event`` based callbacks.
Domcalls are functions which are called by the agency directly, on its dedicated CPU (CPU #0), 
in the context of the ME. Callbaks using *dc_event* are triggered from the CPU agency through an IPI
(Inter-Processor Interrupt) and the ME executes the code itself, enabling the possibility to use
its scheduler (it is not the case with *domcalls* of course).

Callback functions - *domcalls*
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

A *domcall* function is typically called by the agency and executed on the agency CPU. There is
an switch of address space to reach the memory context of the ME and to be able to access its variables.
Consequently, asynchronous activities which could require access to the ME scheduler is **strictly forbidden**.

.. c:function::
   int cb_pre_propagate(soo_domcall_arg_t *args) 

   It is called right before the migration, i.e. the snapshot of the ME. 
   ``args`` is of type ``pre_propagate_args_t`` and has a ``status`` field which
   can have the following value: ``PROPAGATE_STATUS_YES`` or ``PROPAGATE_STATUS_NO``
   indicating if the ME can be propagated or not.
   If the ME is not propagated, no further callback functions are executed.
   
.. c:function::   
   int cb_pre_activate(soo_domcall_arg_t *args) 

   Called after a migration to see if it makes sense for this ME to be resumed
   in this smart object. If not, the ME state can be set to ``ME_state_killed``
   
.. c:function::
   int cb_cooperate(soo_domcall_arg_t *args)
   
   This a very important callback function which allows the migrated ME to exchange
   information with other MEs which reside in the smart object.
   ``args`` is of type `cooperate_args_t` containing a field called ``role``
   
   The role can be ``COOPERATE_INITIATOR`` or ``COOPERATE_TARGET`` depending in 
   which ME the *cooperate()* function is executed. The first role is given to
   the migrated ME while the second role is given to the residing ME when the
   migrated ME performed a call to the *cooperate()* function in this (residing) ME.
   This mechanism clearly enables inter-ME collaboration and is useful to decide
   which ME must stay alive or be killed.  
   

Callback functions - *dc_event*
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The following callback functions are executed in the ME context on the CPU belonging to the ME. 
Asynchronous activities requiring the ME scheduler are authorized. 

.. c:function::
   int cb_pre_suspend(soo_domcall_arg_t *args)

   Called before suspending the frontend drivers.
   
.. c:function::   
   int cb_pre_resume(soo_domcall_arg_t *args)

   Called before resuming the frontend drivers

.. c:function::
   int cb_post_activate(soo_domcall_arg_t *args)
   
   This callback function is called once all frontend drivers have been resumed. It is
   the final callback function called at the end of each migration process.
      
.. c:function::
   int cb_force_terminate(void)

   Tell the ME that a *force terminate* will be performed for this ME.
   The ME state is changed during this callback and is typically 
   set to ``ME_state_terminated``
    

.. note::

   The *suspend* and *resume* callbacks are not specific to a particular ME and is a generic
   procedure to suspend and to resume frontend drivers. The code of this callbacks should **NOT** be changed.
  

Callback sequence in the injected ME
------------------------------------

| The following sequence is executed during a ME injection:
| ``pre_activate`` -> ``cooperate`` 

The ME state is set to ``ME_state_living``


Callback sequence in the residing ME
------------------------------------

| The following sequence is executed during a migration process:
| ``pre_propagate`` -> ``pre_suspend`` -> ``suspend`` (snapshot) ``resume`` -> ``post_activate`` 

The ME state is set to ``ME_state_living``


Callback sequence in the migrating ME
-------------------------------------

| The following sequence is executed during a migration process:
| ``pre_propagate`` -> ``pre_suspend`` -> ``suspend`` (snapshot & migrating) ``pre_activate`` -> ``cooperate`` -> ``resume`` -> ``post_activate``

The ME state is set to ``ME_state_living``


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







