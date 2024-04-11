
.. _me_access:

ME access logical block
-----------------------

The ME access logical block provides facilities to acess the ME descriptor, state and all information
related to MEs.

ME ID
^^^^^

The Migration Manager provides an API that can be used to retrieve the following information related to
each present mobile entity in the current smart object:

* SPID
* Short name (like "SOO.blind")
* Short description (a maximum 1024-byte free text)

The following fonction can be used either in the kernel or in the user space via
the ``AGENCY_IOCTL_GET_ME_ID_ARRAY`` ioctl syscall.

.. c:function:: 
   void get_ME_id_array(ME_id_t *ME_id_array)

.. note::
   The caller must allocate an array of <ME_id_t> elements. The number of elements
   corresponds to ``MAX_ME_DOMAINS``.
   
   
XML formatted message for ME ID
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The following function can be used to retrieve a well-formated XML message which will
be compatible with the table UI application.

.. c:function
   void xml_prepare_id_array(char *buffer, ME_id_t *ME_id_array)
  
The caller must allocate a buffer with a sufficient size. Only ME in state different than
``ME_state_dead`` will appear in the generated XML message.

   


 
