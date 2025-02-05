.. _venocean:

*******
EnOcean
*******
The *venocean* interface allows a ME to communicate with EnOcean devices. EnOcean devices have multiple applications: 

- EnOcean Sensors (Temperature, Humidity, Light, ...)
- EnOcean Switches (Light, Power, ...)
- EnOcean Actuators (Light, Relay, ...)

The figure below represent all the interactions to communicate with a EnOcean device.

.. figure:: /img/SOO_enocean_architecture.png
    :align: center


EnOcean transceiver
===================
The EnOcean transceiver supported is the `TCM515`_. The TCM515 provides a radio link between EnOcean devices and an external host connected via a UART interface.
It handles the ERP (EnOcean Radio Protocol) and embed the data in a ESP3 (EnOcean Serial Protocol) packet. `ESP3`_ is the protocol used to communicate from a host device to
the TCM515. It can be used to send and receive EnOcean radio telegrams as well as configure the TCM515 itself. 

TCM515 driver
=============
A TCM515 driver has been implemented in order to communicate with the TCM515. It uses the *serdev* framework to access the serial port. To assign a serial port to the driver 
the following dts subnode must be added to the chosen UART:

.. code-block:: c

    &uart<nr> {
        
        ...

        status = "okay";

        enocean {
            compatible = "enocean,tcm515";
            current-speed = <57600> /* default baudrate */
        };
    };

The driver allows other drivers to subscribe to it. When new data is received each subscriber will receive it. A write function to send data to an EnOcean device is also available. 
If a response is expected its possible to set a one time callback.

ESP3 packet
===========
Data structure:

+--------------+--------+-----------------+-----------+--------------------------------------------------+
| Byte         | Group  | Content         | Value hex | Description                                      |
+==============+========+=================+===========+==================================================+
| 0x00         | None   | Sync byte       | 0x55      |                                                  |
+--------------+--------+-----------------+-----------+--------------------------------------------------+
| 0x01         | Header | Data length MSB | 0xnn      |                                                  |
+--------------+--------+-----------------+-----------+--------------------------------------------------+
| 0x02         | Header | Data length LSB | 0nn       |                                                  |
+--------------+--------+-----------------+-----------+--------------------------------------------------+
| 0x03         | Header | Optional length | 0xnn      |                                                  |
+--------------+--------+-----------------+-----------+--------------------------------------------------+
| 0x04         | Header | `Packet type`_  | 0xnn      |                                                  |
+--------------+--------+-----------------+-----------+--------------------------------------------------+
| 0x05         | None   | CRC8 Header     | 0xnn      |                                                  |
+--------------+--------+-----------------+-----------+--------------------------------------------------+
| 0x06         | Data   | Data            | ...       | Contains the EnOcean payload (telegram)          |
+--------------+--------+-----------------+-----------+--------------------------------------------------+
| 0x06 + x     | Data   | Optional data   | ...       | Contains additional data. Extends the data field |
+--------------+--------+-----------------+-----------+--------------------------------------------------+
| 0x06 + x + y | None   | CRC8 data       | 0xnn      |                                                  |
+--------------+--------+-----------------+-----------+--------------------------------------------------+

The EnOcean data is contained in the data field and it's the only part of the ESP3 packet that the *venocean* backend sends to the *venocean* front end. The rest of the data is 
used by the TCM515 driver.

*venocean* backend
==================
The backend subscribes to `TCM515 driver`_ to receive EnOcean telegrams. When new data is received the backend extract the data part of the `ESP3 packet`_ and forward it to 
the venocean front end through the shared ring.

*venocean* front end
====================
The front end received the EnOcean telegram through the shared ring. An ME can get the EnOcean data by calling the *venocean_get_data()* function. This function is blocking and will
return only when new data is received.

Supported EnOcean devices
=========================
Here is a list of supported EnOcean devices. This devices have been tested and work with the TCM515 transceiver. The EnOcean telegrams are organized as shown in the `EnOcean equipment profiles`_. 

Switches
--------

`PTM210`_ frame format:

+-------------+-----------+------------+---------------------------+
| Byte        | Content   | Value hex  | Description               |
+=============+===========+============+===========================+
| 0x00        | RORG      | 0x76       | Radio telegram type RPS   |
+-------------+-----------+------------+---------------------------+
| 0x01        | Data      | 0xnn       | - 0x00 switch released    |
|             |           |            | - 0x70 switch press up    |
|             |           |            | - 0x50 switch press down  |
+-------------+-----------+------------+---------------------------+
| 0x02 - 0x05 | Sender ID | 0xnnnnnnnn | EnOcean Unique ID         |
+-------------+-----------+------------+---------------------------+
| 0x6         | Status    | 0xnn       | Status bits               |
+-------------+-----------+------------+---------------------------+

More to come ...

Future developments
===================

- BE/FE add possibility to send EnOcean telegrams from the front end.
- Implement more TCM515 commands as needed and provide an interface (sysfs ?) to interact with the TCM515.
- BE supports for multiple FEs
  
.. _TCM515: https://www.enocean.com/wp-content/uploads/downloads-produkte/en/products/enocean_modules/tcm-515/user-manual-pdf/TCM-515-User-Manual-21.pdf
.. _ESP3: https://usermanual.wiki/m/a0b4d9036aad0f4f220621c1d89bad843cbb72e96b17194c9248bb519fc3b2bc.pdf
.. _Packet type: https://usermanual.wiki/m/a0b4d9036aad0f4f220621c1d89bad843cbb72e96b17194c9248bb519fc3b2bc.pdf#%5B%7B%22num%22%3A41%2C%22gen%22%3A0%7D%2C%7B%22name%22%3A%22XYZ%22%7D%2C65%2C697%2C0%5D
.. _PTM210: https://www.enocean.com/wp-content/uploads/downloads-produkte/en/products/enocean_modules/ptm-210ptm-215/user-manual-pdf/PTM21x_User_Manual_Sep2021.pdf
.. _EnOcean equipment profiles: https://www.enocean-alliance.org/wp-content/uploads/2020/07/EnOcean-Equipment-Profiles-3-1.pdf#page=14