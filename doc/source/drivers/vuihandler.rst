.. _vuihandler:

vuihandler driver
-----------------


Introduction
============

*must be updated...*

The vUIHandler interface allows a ME to communicate with a Bluetooth device like a tablet or a smartphone. 
It relies on the RFCOMM protocol and uses the TTY serial port to transmit the data to the BT adapter.

Associated dev capabilities:

+---------------------+-------------------------+---------------------------------------------------+
| Devcaps class       | Devcaps                 |                                                   |
+=====================+=========================+===================================================+
| *DEVCAPS_CLASS_COM* | *DEVCAP_COMM_UIHANDLER* | Enabled with a tablet UI application is connected |
+---------------------+-------------------------+---------------------------------------------------+

Only one dev capability in the list below can be set at a time



Features and characteristics
============================

* vUIHandler allows the bi-directional communication using the RFCOMM protocol over the Bluetooth interface, providing a PAN service over BT, between a tablet (or smartphone) and a Smart Object.
* vUIHandler is a non-realtime driver as no realtime constraints are required.
* At most one vUIHandler frontend per ME providing one TX+RX channel.
* The remote application targets one ME by setting the target ME SPID in the packet it sends. vUIHandler is responsible of the dispatching among the MEs.
* If several MEs have an access to the vUIHandler interface, there is no priority control among the MEs. In addition:
   - Any TX request coming from any ME will be processed without distinction.
   - Any RX request will be forwarded to the ME targeted by the incoming packet.
* Maximal packet size: see Section 3.5.
* vUIHandler can also be used without targeting any ME. As it is the main entry point for any Bluetooth transfer, it is also used to inject a ME using Bluetooth. In that case, the data are further retrieved by the Injector, or any Agency module needing BT access.


TBD
***
* The vUIHandler must act as a subscription, to which diverse clients could connect. The clients have a unique id which is used to route the incoming data.
* The only process done by the vUIHandler, other than routing, is the direct forwarding to the ME. A special module could also be in charge to dispatch data to the MEs, to offload even more the processing outside vUIHandler.


Functional description
======================

General architecture
********************
.. figure:: /img/SOO_drivers_vuihandler_architecture.png
   :align: center
   
   Architecture and interfaces of the vuihandler 
   

The vuihandler interface is split in two parts:
* A backend: used to access SOOlink and the RFCOMM driver. Resides in the agency.
* A frontend: used to send/receive the ME application data to/from the backend.

The data transfers uses the following structures:
* There are two shared rings (circular buffers) per ME:
   - tx_ring is dedicated to the TX path, from the ME to the remote device.
   - rx_ring is dedicated to the RX path, from the remote device to the ME.
* There are two shared buffers per ME to store the outgoing and incoming packets:
   - One shared buffer is dedicated to the TX path.
   - One shared buffer is dedicated to the RX path. 
* There is a TX buffer used to queue TX packets in the backend. It is needed as we only have one TX thread and multiple TX sources (agency or MEs). It is implemented as a limited circular buffer.
* Every packet, TX or RX goes through SOOlink before/after being transmitted/received.

SOOlink paths
^^^^^^^^^^^^^
.. figure:: /img/SOO_drivers_vuihandler_soolink_path.png
   :align: center
   
   Diagram sequence of the RX path from the RFCOMM driver to the vuihandler 

TODO: do the same for tx

Userspace Bluetooth server
**************************
The BT interface consist of a thread running in the agency user space application called during the post-boot init (*S51agency*), using the `btmanager.sh` script. It opens a RFCOMM socket in raw (TTY) mode. The linux rfcomm module is patched to forward the data to SOOlink instead of the classical path.  

In order to prepare the BT adapter, the `btmanager.sh` script does the configuration using the BlueZ tools suite (in perenthesis). This is a high level description of this process:

* Attach an address to the BT controller and load it firmware (`hciattach`)
* Launch the BT daemon  (`bluetoothd`)
* Configure the controller to avoid having to ask for a PIN during the pairing (`bt-agent`)
* Configure the channel in Serial-Port mode so it uses the TTY interfac (`sdptool`)
* Launch a RFCOMM server which opens a socket and listen to the incoming connections (`rfcomm`) 



Frontend
********
The frontend (FE) is directly used by the client, which is the ME application that wants to communicate with the remote device.

Data structures
^^^^^^^^^^^^^^^
The frontend handles its data using these structures:

* `vuihandler_t`: Stores the data used to communicate with the BE (rings, buffers, evtchn, ...) 
* `vuihandler_priv_t`: Private driver data. Wrapper around `vuihandler_t`, which also stores data used to monitor and handle the state of the FE.

Init
^^^^
Allocate the private data structure. Initialize the FE boilerplate.

Probe
^^^^^
The pages dedicated to the rings and the shared buffers are allocated. The *pfns* are saved in *vbstore*. The ring IRQ handlers are registered.


Connected
^^^^^^^^^
The frontend enters in connected state when the following conditions are met:

* The shared rings are allocated.
* The shared buffers are allocated.
* The event channels for the rings are ready.

When connected it does the following:

* Notify the BE via virq so it can process any pending request
* Start the TX thread


Reconfiguring
^^^^^^^^^^^^^
Does the same as probe.

Closed
^^^^^^
The inter-domain event channels are un-bound and closed. The shared rings are cleared. The shared buffers are cleared.

Suspend
^^^^^^^
Does nothing at the moment.

Resume
^^^^^^
Does nothing at the moment.

Backend
*******
The backend (BE) is in the agency.


Data structures
^^^^^^^^^^^^^^^
The backend handles its data and the corresponding FE(s) data using these structures:

* `vuihandler_drv_priv_t`: Private BE structure. Only allocated once per BE. Maintains the BE specific data (completions, rfcomm_pid, ...)
* `vuihandler_t`: Stores everything related to a specific FE (rings, evtchn, shared_buffer, ...)
* `vuihandler_priv_t`: Wrapper around the `vuihandler_t` structure. This is the structure registered as private data to the `vbus_device` representing out frontend.
* `list_head`: A list to store every `vbus_device` corresponding to the FEs.
* `vdrvback_t`: Generic backend descriptor, which specifies the callbacks used by the BE. It also stores the `vuihandler_drv_priv_t` private BE structure as its data. 


Init
^^^^
It does the following:

* Tells `Device Access` to enable the dev capability `DEVCAP_COMM_UIHANDLER` in class `DEVCAPS_CLASS_COM`. 
* Initializes the TX buffers used afterward.
* Register the threads (RX and TX) to the `sooenv` in order to start them when SOOlink is ready.
* Initialize the backend boilerplate.

probe
^^^^^
Called each time a FE connect to the BE.

It does the following:

* Allocate a structure to maintain the FE state and members.
* Assign the previously allocated structure to the `vbus_device` corresponding to the FE as private data.
* Register the `vbus_device` in its internal list to be able to handle multiple frontends.

remove
^^^^^^
Called when a frontend is removed.

It does the opposite of `probe`.


resume
^^^^^^
Called when a FE is resumed.

Does nothing at the moment.


suspend
^^^^^^^
Called when a FE is suspended.

Does nothing at the moment.


connected
^^^^^^^^^
Called when a FE is connected.

Does nothing at the moment.


reconfigured
^^^^^^^^^^^^
Called when a FE is reconfigured.

It does the following:

* Allocate and initialize the rings used by the reconfigured FE.
* Bind the event channels (evtchn) to their corresponding virq callbacks.


close
^^^^^
Called when a FE is closed.

It does the following:

* Deallocates and deinitializes the rings used by the closed FE.
* Unmap and unbind the event channels.


FE to BE communication
**********************
This describes a typical TX communication. It is used when an ME application wants to send data to the tablet.

.. figure:: /img/SOO_drivers_vuihandler_FE_send.png
   :align: center
   
   Sequence diagram showing the FE sending a TX packet to the tablet


The diagram above is a bit simplified as it doesn't fully show the layers between the FE and the BE. You can refer the the TBD document for more information about this layer.
It still shows the basic concept to send a packet from the ME to the tablet. As every sending/receiving`are asynchronous, the `vuihandler_send_fn` (FE) and the `tx_task_fn` (BE) are running as threads and are completed once the data are ready to be sent.


External interfaces
*******************
This section describes the interfaces from the BE point of view.

TBD