
.. _device_access:

Device access logical block
---------------------------

The **Device Access** logical block is dedicated to the management of the local devices including 
the available storage medium, the identify management and information related to the available virtualized interfaces.

Furthermore, the notion of **devcaps** is also supported.

The agency holds a table of devcaps (device capabilities).

A device capability is a 32-bit number.

DEVCAPS are organized in classes and attributes. For each devcaps, the 8 first higher bits (MSB) are the
class number while attributes are encoded in the 24 lower bits (LSB).

A devcap class represents a global functionality while devcap attributes are the *real* devcaps belongig to a specific class.

Further details will come soon.
