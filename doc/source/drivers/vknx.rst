.. _vknx:

****
VKNX
****
The *vknx* interface allow a ME to communicate with devices found on a KNX bus. The figure below 
shows all the interactions to communicate with a KNX device.

.. image:: /img/KNX_BE_FE.drawio.png

KNX hardware
============
To interact with the KNX bus the supported device is the `Kberry838`_. This device implement
a `BAOS protocol`_ server which abstract the KNX group addresses into datapoints. The datapoints 
and group addresses linkage can be done in ETS. An example on how to create a ETS project using 
the Kberry838 can be found *here*.

KBerry838 driver
================
A KBerry838 driver has been implemented in order to communicate with the Kberry838. It uses 
the *serdev* framework to access the serial port. To assign a serial port to the driver 
the following dts subnode must be added to the chosen UART:

.. code-block:: c

    &uart<nr> {
        
        ...

        status = "okay";

        enocean {
            compatible = "knx,kberry838";
            current-speed = <19200> /* default baudrate */
        };
    };

The driver allows other drivers to subscribe to it. When new data is received each subscriber 
will receive it. The BAOS requests implemented are shown in the list below:

    -  Get_Server_Item
    -  Get_Datapoint_Value
    -  Get_Datapoint_Description
    -  Set_Datapoint_value

*vknx* backend
==============
The backend subscribes to `Kberry838 driver`_ to receive KNX values. When new data 
is received the backend extract the data and forward it to the vknx front end through 
the shared ring. The backend forward requests coming from the frontend to the KNX bus 
using the functions available through the *baos_client* interface.

*vknx* frontend
===============
The front end received the BAOS datapoints through the shared ring. An ME can get the 
KNX data by calling the *vknx_get_data()* function. This function is blocking and will
return only when new data is received. The fronted can request data to knx bus. The vknx 
fronted offers some *get/set* datapoints functions.

.. _KBerry838: https://weinzierl.de/en/products/knx-baos-modul-838/
.. _BAOS protocol: https://weinzierl.de/images/download/development/830/knxbaos_protocol_v2.pdf