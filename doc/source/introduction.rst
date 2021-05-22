.. _introduction:

Introduction
============

Smart Object Oriented (SOO) technology is the result of many years of research and development 
in the field of embedded systems including hardware and software topics such as ARM microcontrollers, 
System-on-Module integration, operating systems and software virtualization. All this work has been and is being realized
at `REDS <REDS_>`__ (Reconfigurable Embedded Digital Systems) Institute from `HEIG-VD <HEIG-VD_>`__.

The SOO framework has been released publicly on `Gitlab <SOO_gitlab_>`__ in March 2020 under the **GPLv2 Licence**. 

People are encouraged to visit our `dedicated forum <SOO_discourse_>`__ and to contribute.

History
-------

In early 2014, `Prof. Daniel Rossier <DRE_>`__ introduced the notion of ``Smart Object`` in the context of SOO technology based on
the paradigm of self-migration of running in-execution (live) environments.
 
The idea is to address numerous technical challenges which are common to connected devices such as the deployment of 
new applications and their maintenance in embedded systems, wireless connectivity in preferably very scalable networks, 
highly secure execution environments, low power consumption and cost-effective hardware, etc. but above all, 
simple mechanisms to establish dynamic online synergies between applications themselves, in other words between execution 
environments which are visiting smart objects.

The SOO technology strongly relies on various low-level operating system concepts and embedded virtualization technics which 
play a fundamental role in this framework. In this context, Linux and XEN have been a source of inspiration at many levels.

Prof. Rossier's team was among the first people who tried to port XEN hypervisor on an ARM embedded system; at the same time,
Samsung also started such a work with an emphasis on security aspects (they run XEN on Freescale iMX21 boards). 
REDS published in 2012 its `EmbeddedXEN framework <_EmbeddedXEN>`__ including results from a `joint collaboration with Logitech 
<EMBX_Logitech_>`__ which consisted to deploy two OS on an HTC smartphone, an Android OS and the SqueezeBox Touch operating 
system as the second OS, both OS sharing the same hardware resources. 
This work was presented in San Diego during the `XenSummit'12 <EMBX_xensummit_>`__. A demonstration on `YouTube <EMBX_demo_>`__ and 
a `white paper <EMBX_whitepaper_>`__ are also available.

One major evolution of this framework was to integrate the migration paradigm. It gave birth to the SOO technology in early 2014 and
it was then possible to have a full Linux running environment migrating from one device to another in a constant way.
The migrating environment - aka *ME - Mobile Entity* - is now based on SO3, a lightweight operating system fully developed in the
REDS Institute. 

As we can see, embedded virtualization on ARM devices still remains a hot research topic inside REDS, with the perspective
to promote and to develop fully decentralized and automous systems with a major concern on wireless communication and security.

Further details can also be found in this `blog dedicated to SOO <SOO_blog_>`__.


Main Conceptual Components
--------------------------

The two essential components of the framework are the ``Agency`` and the ``Mobile Entity``.

The :ref:`Agency <agency>` (agency) is the part of software which resides permanently within a smart object device while 
the :ref:`Mobile Entity <ME>` (ME) is the part of software which migrates from one smart object (SOO) to another SOO.


.. _REDS: http://www.reds.ch
.. _HEIG-VD: http://www.heig-vd.ch
.. _SOO_blog: https://blog.reds.ch/?p=1020
.. _SOO_gitlab: https://gitlab.com/smartobject/soo
.. _SOO_discourse: https://discourse.heig-vd.ch/c/soo
.. _DRE: https://reds.heig-vd.ch/en/team/details/daniel.rossier
.. _EmbeddedXEN: https://sourceforge.net/projects/embeddedxen
.. _EMBX_Logitech: https://wiki.slimdevices.com/index.php/EmbeddedXEN.html
.. _EMBX_xensummit: https://fr.slideshare.net/xen_com_mgr/dealing-with-hardware-heterogeneity-using-embeddedxen-a-virtualization-framework-tailored-to-arm-based-embedded-systems
.. _EMBX_demo: https://www.youtube.com/watch?v=ErLZQE5ZI7U&feature%3B=player_embedded
.. _EMBX_whitepaper: https://en.wikipedia.org/wiki/File:EmbeddedXEN_publication_final.pdf






