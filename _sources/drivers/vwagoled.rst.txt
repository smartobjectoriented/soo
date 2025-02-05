.. _vwagoled:

********
vwagoled
********

The *vwagoled* interface allows a ME to interact with WAGO PFC200 and its bus components. The figure below shows the interactions
to communicate with the Wago ecosystem.

.. figure:: /img/SOO_wagoled_architecture.png
    :align: center

*vwagoled* backend
==================
The backend receives instructions from the shared ring and communicate with the Wago-client through 
the *sysfs* interface.

*vwagoled* front end
====================
The fronted provides a function to send a command to apply to the LEDs. This function will create a
new ring request and send it to the backend. 

Wago client application
=======================
The Wago client application interacts with the *vwagoled* backend through *sysfs*. The entries provided
by the backend are the following:

- vwagoled_notify (read-only)
- vwagoled_led_on (read-only)
- vwagoled_led_off (read-only)
- vwagoled_get_topology (read/write)
- vwagoled_get_status (read/write)

The application performs a blocking *read()* on *vwagoled_notify*. Once new data arrives *vwagoled_notify*
will return a string containing which attribute to read next. The other attributes correspond to a specific action
like turn on or off the LEDs. When performing a *read()* on one of this the IDs of the LEDs to perform the action 
on is returned. The Wago application then send the corresponding HTTP request to the wago-server application running on the PFC200.

Wago server application
=======================
The Wago server application implements a REST server which provides a REST API that allows to access
the devices connected on the PFC200 KBus. In this specific case the REST API provides access to a 
DALI Master Module (Wago 753-647) use to control the led connected to the DALI bus. The routes provided
by the REST API are listed below:

+---------------------------------+-------------+----------------------------------+-----------------------------------------------------------+
| Route (URL)                     | HTTP method | Arguments                        | Description                                               |
+---------------------------------+-------------+----------------------------------+-----------------------------------------------------------+
| <ip>:<port>/dali/topology       | GET         | None                             | Return a list (json) of the devices found on the DALI bus |
+---------------------------------+-------------+----------------------------------+-----------------------------------------------------------+
| <ip>:<port>/dali/led?id=        | GET         | Comma separated LED ids or "all" | Return the current status of the LEDs                     |
+---------------------------------+-------------+----------------------------------+-----------------------------------------------------------+
| <ip>:<port>/dali/led/on?id=     | POST        | Comma separated LED ids or "all" | Turn on the LEDs                                          |
+---------------------------------+-------------+----------------------------------+-----------------------------------------------------------+
| <ip>:<port>/dali/led/off?id=    | POST        | Comma separated LED ids or "all" | Turn off the LEDs                                         |
+---------------------------------+-------------+----------------------------------+-----------------------------------------------------------+
| <ip>:<port>/dali/led/toggle?id= | POST        | Comma separated LED ids or "all" | Toggle the LEDs (inverse current status)                  |
+---------------------------------+-------------+----------------------------------+-----------------------------------------------------------+

Future developments
===================
- BE support for multiple FEs