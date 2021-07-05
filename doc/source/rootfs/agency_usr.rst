
.. _agency_usr:

******************************
Agency user applications (usr)
******************************

In addition to the contents defined in the *rootfs*, additional applications can be built and deployed in the
``agency/usr/`` directory. Such applications are specific to the agency and do not belong to any external packages.

Development of modules and deployment
=====================================

Kernel modules can also be compiled in the ``usr/module/`` directory according to the platform as defined 
in the ``agency/build.conf`` file.

The modules are automatically deployed in the ``root/`` home directory of the target ``rootfs``.
The ``insmod`` application can then be used from the *agency shell* in order to load the module
into the kernel.

A module can be helpful for testing purposes, for example to test kernel functionalities.


