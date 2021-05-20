.. _ME:

==================
Mobile Entity (ME)
==================

A Mobile Entity (ME) is the core logic of the application which can be constituted of several tasks or 
even of several processes. MEs are based on SO3 operating system.

Specy Aptitude Descriptor (SPAD)
--------------------------------

Each ME may have one or several :term:`SPAD` (Specy Aptitude Descriptor). The ``SPAD`` determines a specific
feature (or set of features).

The *SPAD* is also used to tell the agency if the ME is inclined to cooperate with other ME.

Enabling the possibility for an ME to perform *cooperation* with other ME requires to call
the following function in the main application of the ME:

.. code-block:: c

   spad_enable_cooperate();
  
  