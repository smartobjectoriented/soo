
.. _transceiver:

Transceiver logical block
-------------------------

The transceiver is responsible to broadcast data packets handled by the :ref:`transcoder <transcoder>` to the neighbourhood 
in the most convenient way. It relies on a datalink protocol which is used to take into account the properties of the underlying
plugin interface. 


Winenet datalink protocol
^^^^^^^^^^^^^^^^^^^^^^^^^

Winenet is the datalink protocol used with WLan (Wifi) transmission. Its main purpose is to allow smart objects to send
their MEs without interfering with each other. In this perspective, Winenet gives the hand to each smart object one after
the other after transmitting a certain number of packets contained in a frame. Each frame is sent to each smart object giving
the impression that packets are broadcast. We called this approach *Unibroad* (combining Unicast and broadcast).

Compared to another datalink protocol based on routing mechanisms, Winenet is much simpler since it is based on the neighbourhood.
Each smart object (SOO) is periodically broadcasting a *Discovery* beacon and can be recognized by all SOOs which are in the light of
sight.

Each SOO has various information related to its state and the neighbourhood:

*  ``randnr`` is a random number which is determined during the *PING* procedure and will be used in case of simultaneous actions with 
   a same beacon. The greater value has the advantage and the SOO with the lower value will abort the transaction.
*  ``paired speaker`` is the *agencyUID* of the SOO which is designated as speaker. The SOO is listening at this specific speaker which
   transmits MEs.
*  ``valid`` is set to true during the *PING* procedure.

Beacons
"""""""

The following beacons are used in Winenet. The beacon has a ``cause`` field which may contain some details related to the beacon.

+---------------------+------------------------------------------------------------------+----------------------+
| Name                | Definition                                                       | Cause                |
+=====================+==================================================================+======================+
| *PING*              | Used to consider a neighbour as *valid*                          | REQUEST / RESPONSE   |
+---------------------+------------------------------------------------------------------+----------------------+
| *QUERY*             | Used to exchange the SOO internal state                          | REQUEST / RESPONSE   |
+---------------------+------------------------------------------------------------------+----------------------+
| *ACKNOWLEDGMENT*    | When a beacon or send of frames require an acknowledgment        | OK / TIMEOUT / ABORT |
+---------------------+------------------------------------------------------------------+----------------------+
| *GO SPEAKER*        | When a speaker wants another neighbour to get speaker            | (empty)              |
+---------------------+------------------------------------------------------------------+----------------------+
| *BROADCAST SPEAKER* | Ask the neighbours to get paired with us (before sending frames) | (empty) / abort      |
|                     | If the cause is *abort*, the SOO gets unpaired.                  |                      |
+---------------------+------------------------------------------------------------------+----------------------+

Only *GO SPEAKER* and *BROADCAST SPEAKER* beacons require an acknowledgment.

There are four main states: ``INIT``, ``IDLE``, ``SPEAKER`` and ``LISTENER``.

State *INIT* is used at the beginning until the *Discovery* protocol insert the smart object in the neighbourhood.

.. note::
   Each smart object is also considered as *neighbour*, however it remains with the ``valid`` attribute to false.

INIT state
""""""""""
Then, the state *IDLE* is a temporary state until a first neighbour is discovered. 

When a neighbour appears, a *PING* procedure is automatically triggered as follows: the SOO with the lower ``agencyUID`` sends
a ``PING REQUEST`` to the other. Then, the other SOO will answer with a ``PING RESPONSE``.

.. note::
   Everytime a beacon is sent out to another SOO, its internal state is sent with the beacon. The internal state is made of
   the ``paired speaker`` and a ``randnr`` random number.

LISTENER state
""""""""""""""
The SOO with at least one neighbour is put to the *LISTENER* state after triggering a ``QUERY`` procedure.
A ``QUERY REQUEST`` beacon is sent to the first neighbour. The SOO will be able to determine if it has to
switch to the *SPEAKER* state at the receival of ``QUERY RESPONSE`` beacon.

The *QUERY* procedure is also used as a consolidation mechanism to make sure the speaker is still a *true* speaker,
and if it is not the case to re-activate a speaker in any case.

Depending on the value of *paired speaker*, the SOO can answer to ``GO SPEAKER`` or ``BROADCAST SPEAKER`` beacon.


SPEAKER state
"""""""""""""
This state enables the transmission of a pending buffer (typically a ME).
The buffer comes from the *transcoder* layer which actually splits the buffer in ``packets``. Winenet is buffering
a certain number of packets to have a ``frame``. The frame (with multiple packets) is sent out to each SOO and requires
to be acknowledged.


Parameters
^^^^^^^^^^

The following constants/values are defined and used by the datalink protocol.

Acknowledgment related constants
""""""""""""""""""""""""""""""""

+----------------------------+----------------------------------------------------------------+
| Name                       | Definition                                                     |
+============================+================================================================+
| *WNET_ACK_TIMEOUT_MS*      | Delay in ms to receive a ACK beacon when required              |
+----------------------------+----------------------------------------------------------------+
| *WNET_RETRIES_MAX*         | Number of max retries to send a packet when no ack is received |
+----------------------------+----------------------------------------------------------------+
| *WNET_LISTENER_TIMEOUT_MS* | Delay in ms during which a beacon must be received             |
+----------------------------+----------------------------------------------------------------+
