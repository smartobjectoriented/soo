.. _sooblind:

SOO.blind
=========
This document aims at describing the SOO.Blind role, use-cases and architecture.

SOO.blind propose
=================


SOO.blind architecture
======================

.. image:: /img/specifications/MEs/SOO.blind.drawio.png


SOO.blind Migration
===================
Every smart object having either a KNX BE or a EnOcean BE has a residing SOO.blind. If a new EnOcean
event occurs (switch pressed) the ME will migrate in search of its counter part residing on a 
smart object having a KNX BE.

The migration diagram below shows this behavior.

.. uml:: 
    @startuml

    [*] --> cb_pre_propagate
    cb_pre_propagate --> cb_pre_suspend 
    cb_pre_suspend : Is there new Enocean data ?
    cb_pre_suspend --> cb_suspend_1 : Yes
    cb_suspend_1 : Snapshot and Migrate

    cb_pre_suspend --> cb_suspend_2 : No
    cb_suspend_2 : Don't propagate and resume normal activities

    @enduml

.. uml::
    @startuml

    [*] --> pre_activate
    pre_activate : Is there another SOO.blind ?
    pre_activate --> cooperate_1 : Yes
    cooperate_1 : Exchange data with residing ME
    cooperate_1 --> resume_1
    resume_1 --> post_activate_1
    post_activate_1 : ME kill itself

    pre_activate --> cooperate_2 : No
    cooperate_2 --> resume_2
    resume_2 : Is there a KNX or Enocean BE ?
    resume_2 --> post_activate_2 : Yes
    post_activate_2 : ME take up residence
    resume_2 --> post_activate_3 : No
    post_activate_3 : Continue migration

    @enduml