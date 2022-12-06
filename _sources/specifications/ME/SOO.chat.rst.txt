.. _SOO_chat:
  
SOO.chat
########

This documents aims at describing the **SOO.chat** role, use-cases and
architecture.

SOO.chat purposes
-----------------

The **SOO.chat** purposes are:

-  Being able to implement a small live chat app (like IIRC) between
   multiple Smart Objects.
-  Demonstarting the network capabilities with multiple Smart Objects.
-  Having a clean base for the ME model (migration, behaviour,
   cooperation, …).
-  Having a full tablet to Smart Object data/events exchange.
-  Implementing the *soo.gui* protocol handling in the **SOO.chat** ME.

SOO.chat usage
--------------

The **SOO.chat** ME will be in charge to handle and distribute chat
message between the Smart Objects. The user writes a message on the
tablet, then it is sent and propagated into the network to update every
other user connected to another Smart Object available in the network.
Each tablet then displays the message in it’s message history. For now
the goal is to only have one chat room where every user can write and
read the messages.

.. figure:: /img/specifications/MEs/SOO_chat_overview.png
   :align: center
   
   Overview of the a SOO.chat use-case 

Each tablet can connect to any SO supporting SOO.chat and having a SOO.chat ME residing in it.

SOO.chat architecture
---------------------

The SOO.chat architecture only consist of a ME. There is no need for a
BE in the agency, as all the message are stored in the MEs and the
communication with the tablet is done using the vuihandler.

There are two “types” of SOO.chat ME in this system: \* The migrating
ones, which transmit **new** messages into the network. \* The residing
ones, which keeps a minimal history and will collaborate with the
migrating ones.

In the end, the migrating ones are just a clone of the residing ME,
which is sent to dispatch the message through the network.

Distributing a message
~~~~~~~~~~~~~~~~~~~~~~

The messages are distributed by the The messages are stored in a small
history in each ME. Each SOO has a residing SOO.chat ME. Here is how a
message is passed to one tablet to the system:

1. The user connects the SOO-gui tablet application to a SOO.
2. The user writes a message on the tablet. The tablet notify the ME for
   each new character typed. The ME keeps the message as a temporary
   buffer.
3. The user press **Send**, which sends a button event to the connected
   Smart Object vuihandler BE.
4. The BE route the event to the SOO.chat ME.
5. The ME adds the message to its temporary history (from itself),
   marking it a *new_message* so it knows it needs to share it with
   other MEs if it encounters some.
6. The ME sends itself in the network.
7. When arriving in another SOO, the migrating ME initiate a cooperation
   with the residing SOO.chat ME. It then checks if the residing ME
   already has the message. If not, the residing ME merge it with its
   history. It then sends the update to the tablet. As a ME migrates
   when it has a new message to distribute, it should almost always be
   the case.
8. The migrating ME then continues its migration trough the network,
   until it has effectively distributed the new message to the whole
   ecosystem.

It is to note that a migrating ME only carries its new message. It does
not have a history as its only goal is to distribute its new message.

SOO.chat ME behaviour
---------------------

This chapter describes:

-  The migration behaviours

-  The collaboration behaviour

-  The app behaviour

Migration and cllbacks
~~~~~~~~~~~~~~~~~~~~~~

Every Smart Object has a residing SOO.chat ME. Once the tablet sends a
new message to our residing ME, the ME is sent to migrate in the system.
The migrating ME now needs to go trough the whole system to distribute
the message. It keeps a list of `visits` which it visited.

.. figure:: /img/specifications/MEs/SOO_chat_migration.png
   :align: center
   
   Migration flowchart 

As described in the diagram above, the ME kills itself if it migrated in
a visited SO. If it is a new SO, it continues its callback sequence and
start a cooperation with the present ME. The last part of the diagram is split in two, one path for the migrating ME (initiator), the other one for the target.


Callbacks
~~~~~~~~~

This chapter describes the callbacks behaviors.

pre_activate
^^^^^^^^^^^^
- The ME retrieve the originUID of the first agency it is deployed on (once).
- The ME check if the host is already visited.
  * If not: Add the host to the visited list
  * If yes: ask to kill ourself

cooperate
^^^^^^^^^

Most of the job is done in the ``cooperate``. It is described in the migration flowchart.
The cooperation is always the initiator (migrating ME) cooperating with an already present target ME (it can be a local or a migrating one).
In our case, four scenarios are possible:
 - No ME is present in the SO. So it means the migrating one is alone. We continue our propagation.
 - An ME is present, but it is not a SOO.chat. We continue our propagation.
 - A migrating SOO.chat is present. If we have the same message, merge the histories and kill the initiator, otherwise, don't do anything and continue our propagation.
 - A local SOO.chat is present. The local SOO.chat checks if it already has the new message, and adds it to its history if needed. 


pre_propagate
^^^^^^^^^^^^^
The pre-propagate will decide if the ME will be propagated or if it must die. It is called periodically (300ms).
It checks if any of the callbacks asked for a propagation (internal flag). If a propagation is needed, it checks if 
it is dormant. If yes, it means we need to ask to kill ourself, if not, it means the ME must be propagated.
The internal propagation flag is always resetted here, so if no other callback changes it, the ME will die as soon as it is dormant.

SOO.chat app
~~~~~~~~~~~~

The SOO.chat app is the core of the SOO.chat ME. It is able to store a
small history for the messages which  were distributed. It also
contains and execute the helpers needed to compare and merge histories. 

Messages id management
^^^^^^^^^^^^^^^^^^^^^^

Each time a new message is received from the tablet, the SOO.chat
assigns it a ``id``, incrementing it each time. It is used as a
heuristic data, which, in addition to the ME age, is needed when doing
histories merge.

A message is stored in the following structure:

+--------+------+------------------------------------------------------+
| Member | Type | Description                                          |
+========+======+======================================================+
| id     | uint | Unique message id. Incremented for each new message, |
|        | 64_t | by ME                                                |
+--------+------+------------------------------------------------------+
| orig   | uint | origin agency UID. Used to keep a trace of the       |
| in_uid | 64_t | originating SO                                       |
+--------+------+------------------------------------------------------+
| text   | char | The message text                                     |
|        | \*   |                                                      |
+--------+------+------------------------------------------------------+

History management
^^^^^^^^^^^^^^^^^^

| The residing MEs keep a dictionary of the last message from each SO,
  using the ``origin_uid`` as a key and the ``id`` as the value.
| These data are enough to correctly merge the messages when
  collaborating with a residing MEs as seen in the flowchart before.


Sending a message
^^^^^^^^^^^^^^^^^
For now, the ME receive a message from the tablet, each time a new character is typed into the textedit. 
It is done this way, to avoid having dependency and coupling between the widget. 

Here is a flowchart describing how the message temporary received and how the ME knows when to send it.


.. figure:: /img/specifications/MEs/SOO_chat_message_sending.png
   :align: center
   
   Message buffering and sending from the **chat** app. 


We can see that the ME keeps the temporary message and update it each time a new character is 
typed on the tablet `text-edit` widget. The ME knows it has to send the message when it receives the event
from the `button-send` widget. 

SOO.chat XML UI model
~~~~~~~~~~~~~~~~~~~~~

This describes the XML model and how it will interact and be used by the
tablet.

The tablet app will look like this:

.. figure:: /img/specifications/MEs/SOO_chat_tablet_mockup.png
   :align: center
   
   Tablet chat page mockup 

It is consisted of: 
 * A Label inidcating the name of the app 
 * A TextEdit used to type our message 
 * A **Send** button to send themessage 
 * A ScrollView to display the messages 
 * Two Label to use as an entry for the ScrollView.

History widget
^^^^^^^^^^^^^^

A new type of widget is to be implemented in the model for the history
scrollview. It will allow to insert the uid|message pair each time a new
message is received. It also will allow to scroll the history.

XML model
^^^^^^^^^

The following model is used to generate the tablet UI for this ME.

.. code:: xml


       <model slot_id=SLOT_ID_HERE
           <name>SOO.chat</name>
           <description>SOO.chat permet de participer à un live chat entre Smart Objects.</description>
           <layout>
               <row>
                   <col span=\"2\">
                       <text>SOO.chat app</text>
                   </col>
               </row>
               <row>
                   <col span=\"4\">
                       <scrollview for=\"msg-history\"> "messages here" </label>
                   </col>
               </row>

               <row>
                   <col span=\"3\">
                       <input id=\"text-edit\" > "your new msg here" </input>
                   </col>


                   <col span=\"1\">
                       <button id=\"button-send\" lockable=\"false\"> "Send" </button>
                   </col>
               </row>
           </layout>
       </model>    

XML messages management
~~~~~~~~~~~~~~~~~~~~~~~

Here are all the message the SOO.chat ME can send to the tablet:

1. ``chat``:

   -  ``slot_id``: Originating UID the message was sent from
   -  ``content``: The message text

.. code:: xml

    <message to="msg-history">
        <chat from="UID">The message itself </chat>
    </message>

A chat message is a message augmented with a `chat` member which embed this chat's metadata (sender and text).
It is destined to the `msg-history` widget which will create the entry from the metadata and display it.   

XML events management
~~~~~~~~~~~~~~~~~~~~~

Here are all the events the SOO.chat ME can receive and handle from the
tablet:

1. ``text-edit``:

   -  ``action``: What is this event about (onValueChanged, onClear, ...).
   -  ``text``: The message text.

.. code:: xml 

    <event from="text-edit" action="valueChanged">your new msg here</event> 


2. ``button-send``:
   -  ``action``: What is this event about (clickDown, clickUp, ...).

.. code:: xml 

    <event from="button-send" action="clickDown"/>
