.. _logging:

Logging facility
================

This section will describe the new *soo_log* facility which provides an simple and efficient mechanism
to output log messages according to filters.

Example of usage:

.. code-block:: c

   soo_log("[soo:soolink:discovery] A log message...\n");
   
   

Enabling ``initcall`` logging
=============================

To enable logging of initcall traces, the following assignment has to be set
in the DT *bootargs* property:

.. code-block:: c

   initcall_debug=1  ignore_loglevel


*to be completed*