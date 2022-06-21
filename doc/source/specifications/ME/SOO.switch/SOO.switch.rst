.. _sooswitch:

**********
SOO.switch
**********
This page aims at describing the **SOO.Switch** role, use-cases and architecture.

SOO.switch purpose
==================
The purpose of **SOO.Switch** is to gather switches events, like a the pressure or the release, 
and to transport them through the **SOO** ecosystem. A **SOO.switch** can be bound to one or 
multiple other MEs that will react on the switch event. For example a **SOO.blind** could perform
an action if it cooperate with a switch.

SOO.switch general architecture
===============================
There are multiple types of switches all using different kind of communication protocols like EnOcean,
KNX, Z-Wave, etc... The SOO.switch ME exist to abstract the switch event from the protocol. The 
image below shows the general architecture of the ME. At the moment only **EnOcean PTM210** and 
**KNX GTL2TW** switches are supported but many more are to come.

.. image:: /img/specifications/MEs/SOO.switch.drawio.png

SOO.switch migration pattern
============================

The diagram below shows the behavior that triggers **SOO.switch** to migrate.

.. uml:: 
    @startuml

    start
    if (Is there a switch event?) then (yes)
        :Snapshot and migrate;
        kill
    else (no)
        if (ME state dormant?) then (yes)
            :Kill ME;
            kill
        else (no)
            :Don't migrate and resume normal activities;
            kill
        endif
    endif

    @enduml


The diagram below shows the behavior of **SOO.switch** when it arrives on a new smart object. 

.. uml::
    @startuml

    start
    if (Is this a new host?) then (yes)
        :Add to host list;
        if (Am I alone?) then (yes)
            :Continue migration;
            kill
        else (no)
            if (Can I cooperate with it ?) then (yes)
                :Exchange data and set ME dormant;
                kill
            else (no)
                :Continue migration;
                kill
            endif
        endif        
    else (no) 
        :Kill ME;
        kill
    endif
    @enduml

SOO.switch shared data
======================

The **SOO.switch** shared structure contains the following members that can be used during the
cooperation. Not all of them apply to each type of hardware switch.

+-----------+--------------------------------------------------+--------------------------------------------------------+
| Member    | Description                                      | Value                                                  |
+===========+==================================================+========================================================+
| pos       | The button position on the switch                | enum { LEFT_UP, LEFT_DOWN, RIGHT_UP, RIGHT_DOWN, NONE} |
+-----------+--------------------------------------------------+--------------------------------------------------------+
| press     | How the switch have been pressed                 | enum { SHORT, LONG, NONE }                             |
+-----------+--------------------------------------------------+--------------------------------------------------------+
| status    | The status of the switch                         | enum { ON, OFF, NONE }                                 |
+-----------+--------------------------------------------------+--------------------------------------------------------+
| type      | The switch physical model                        | enum { PTM210, GTL2TW, ..., NONE }                     |
+-----------+--------------------------------------------------+--------------------------------------------------------+
| originUID | ID of the smartobjet from which the ME is coming | uint64                                                 |
+-----------+--------------------------------------------------+--------------------------------------------------------+
| timestamp | Unique value assigned at migration               | uint64                                                 |
+-----------+--------------------------------------------------+--------------------------------------------------------+
| delivered | The cooperation has already happened             | boolean                                                |
+-----------+--------------------------------------------------+--------------------------------------------------------+
