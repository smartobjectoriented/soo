.. _demo_blind_leds:

****************
Demo Lahoco/Wago
****************

Introduction
============

The goal of this demo is to show the multi-protocol capabilities of the SOO-mpV2 boards. It is assumed that we are working with the 64b configurations with all boards.


Architecture
============
.. figure:: /img/demos/SOO_demos_blind_leds.png
    :align: center
    :scale: 50 %

Hardware needed
===============

What you need:

 * 2x SOO-mpV2
 * 1x RPi4
 * 1x Wagoled panel with RJ45 cable
 * 1x KNX BAOS server with a KNX switch attached
 * 1x KNX blind 
 * 1x Enocean switch
 

SOO-mpv2 KNX
============

This SOO-mpv2 is used to communicate with the KNX BAOS server through the kberry module. It will receive events from the KNX switch and send messages to the KNX blind.

The ME to deploy are:
 * SOO.blind_64: Used to drive the blind when receiving SOO.switch_enocean_64 ME
 * SOO.switch_knx_64: Used to transmit the KNX switch event 

These linux configs must be activated:
 * CONFIG_KBERRY838
 * CONFIG_VKNX_BACKEND  

Setup
*****
Power-on the KNX BAOS server before powering the SOO.SOO-mpV2. It is needed to ensure the kberry module is correctly powered and can communicate with the server and the CM4.
And you are good to go!


SOO-mpv2 EnOcean
================

This SOO-mpv2 is used to receive EnOcean frames coming from the EnOcean witch. 

The ME to deploy are:
 * SOO.switch_enocean_64: Used to transmit the EnOcean switch event 

These linux configs must be activated:
 * CONFIG_TCM515
 * CONFIG_VENOCEAN_BACKEND  

Setup
*****
Nothing special, power the SOO-mpV2 and you are good to go.


RPi4 Wagoled
============

This RPi4 is used to communicate with the Wago automaton using RJ45.

The ME to deploy are:
 * SOO.wagoled_64: Used to drive the wagoled panel from the SOO.switch_knx_64 ME event. 

These linux configs must be activated:
 * CONFIG_VWAGOLED_BACKEND  

Setup
*****
Power-on the wagoled panel **must** be powered-on before the RPi4. Turn on the switch and wait for all the leds to turn OFF. Once it is done, it means the automaton has configured its server and you can now correctly scan the bus and endpoints.
Power-on the RPi4 and you are good to go.

