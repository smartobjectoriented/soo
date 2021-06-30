
.. _boards:

###########################
Boards specific information
###########################

This chapter is dedicated to specific information related to the boards on which
the SOO framework is deployed.

******************
Raspberry Pi 4 (b)
******************

The RPi4 has an expander for external board like Sense-HAT or others. 
Typically, UART pins are also present and can be used for console or other functionalities.

In the SOO framework, ``UART1`` is normally used for the console (avz & Linux); this uart
is designed to be driven by a 8250 UART driver while the other UARTs (UART0, UART2-5) are using
the PL011 driver. UART1 is called mini UART. ``UART0`` is also multiplexed with the Bluetooth device, so
it is necessary to preserve this UART for this purpose.

On some configuration in SOO.domotics, UART1 has been used for interfacing the LoRA module.
For this reason, there is a configuration using ``UART5`` for the console, instead of ``UART1``.

.. figure:: /img/SOO_RPi4_Pinouts_gpio.png
   :scale: 80 %
   
   Pinouts and GPIO bindings
  
 
.. figure:: /img/SOO_RPi4_Pinouts_functions.png
   :scale: 60 %
   
   RPi4 Pinouts and alternamte functions
   
   
