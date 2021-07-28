
.. _transceiver:

Transceiver logical block
-------------------------

The transceiver is responsible to broadcast data packets handled by the :ref:`transcoder <transcoder>` to the neighbourhood 
in the most convenient way. It relies on a datalink protocol which is used to take into account the properties of the underlying
plugin interface. 


Winenet datalink protocol
^^^^^^^^^^^^^^^^^^^^^^^^^
*Protocol/FSM to be described...*



Parameters
^^^^^^^^^^

The following constants/values are defined and used by the datalink protocol.

Acknowledgment related constants
""""""""""""""""""""""""""""""""

+------------------------+----------------------------------------------------------------+
| Name                   | Definition                                                     |
+========================+================================================================+
| *WNET_TSPEAKER_ACK_MS* | Delay in ms to receive a ACK beacon when required              |
+------------------------+----------------------------------------------------------------+
| *WNET_RETRIES_MAX*     | Number of max retries to send a packet when no ack is received |
+------------------------+----------------------------------------------------------------+
