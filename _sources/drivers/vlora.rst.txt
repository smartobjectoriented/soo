.. _vlora:

****
LoRa
****
The *vlora* interface allows a ME to communicate with LoRa devices.

.. warning:: Not yet implemented

LoRa transceiver
================
The LoRa transceiver supported is the `RN2483`_ mounted on a `LoRa click board`_. The RN2483 provides a 
UART interface to send and receive LoRa messages. It can be configured using commands sent directly to the serial port.
A user space applications for test purposes and a firmware updater are available in a separate repository `click-boards`_. 

RN2483 driver
=============
A RN2483 driver has been implemented to communicate with the RN2483 directly from the Linux kernel.
It uses the *serdev* framework to access the serial port. To assign a serial port to the driver the 
following device tree subnode must be added to the chose UART:

.. code-block:: c

    &uart<nr> {

        ...

        status = "okay";

        lora {
            compatible = "lora,rn2483";
            current-speed = <57600> /* default baudrate */
        };
    };

The driver allows other drivers to subscribe to it. When new data is received each subscriber will receive it.
A *send data* function is available to send data through LoRa.

RN2483 commands and LoRa string data
====================================
The commands supported by RN2483 are available in the `RN2483 LoRa User's Guide`_. The table below shows only 
the currently implemented commands:         

+-----------------+-------------+----------------------------------------------------------------------+
| Command string  | Arguments   | Description                                                          |
+=================+=============+======================================================================+
| *sys reset*     | None        | Reset and restart the RN2483 module                                  |
+-----------------+-------------+----------------------------------------------------------------------+
| *sys get ver*   | None        | Returns information related to hardware platform                     |
+-----------------+-------------+----------------------------------------------------------------------+
| *mac pause*     | None        | Pause LoRaWan stack functionality for <args> ms                      |
+-----------------+-------------+----------------------------------------------------------------------+
| *radio tx*      | data string | String of hexadecimal values to be transmitter                       |
+-----------------+-------------+----------------------------------------------------------------------+
| *radio rx*      | 0           | Enable continuos reception mode. It will wait until data is received |
+-----------------+-------------+----------------------------------------------------------------------+
| *radio stoprx*  | None        | Stop continuos reception mode. For example to send data.             |
+-----------------+-------------+----------------------------------------------------------------------+
| *radio set wdt* | 0 - 2^32    | Update the timeout length in ms. Set to 0 to disable                 |
+-----------------+-------------+----------------------------------------------------------------------+

.. note:: In order to correctly setup the device for non LoRaWAN transmission you must first set 
    send the command *mac pause*, then disable the watchdog *radio set wdt 0* and finally the *radio tx* or
    *radio tx* commands can be used. 

When the driver is probed it performs all necessary action and sets itself in continuos reception mode. 
If the *send data* function is called the reception mode will be stopped, the data will be sent and the 
reception is reenabled.


.. _RN2483: https://www.microchip.com/en-us/product/RN2483
.. _Lora click board: https://www.mikroe.com/lr-click
.. _click-boards: https://gitlab.com/smartobject/bsp/click-boards/-/tree/main/lora_click
.. _RN2483 LoRa User's Guide: https://ww1.microchip.com/downloads/en/DeviceDoc/RN2483-LoRa-Technology-Module-Command-Reference-User-Guide-DS40001784G.pdf#page=15
