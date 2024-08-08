.. _emiso_engine:

############
EMISO Engine
############

The EMISO Engine replaces the docker engine in the `EMISO` environment. It supports
a subset of the Docker APIs set.
The following picture presents the communication flow between the *Portainer* Server
and *SO3 Containers*.

.. figure:: pictures/EMISO-message_flow.png
	:name: _fig-Communication flow
	:alt: Communication flow
	:align: center

	Communication flow

**Legends**

	(1) The Portainer Server communicates directly with the EMISO Engine running
	    on the Smart Object. The communication is done via HTTP or HTTPS/TLS using
	    a RESTful API
	(2) EMISO Engine provides an interface to control the SO3 containers.

The application can be called as following:

.. code-block:: shell

	$ emiso_engine [-s] [-i]

Where

* (optional) ``-s``: Start the webserver in secure mode (HTTPS/TLS)
* (optional) ``-i``: Interactive mode - start the cli interface instead of the
  webserver.

.. note::

	The `interactive` (``cli``) mode is not implemented yet

.. note::

	Due to some `limitation <https://github.com/portainer/portainer/issues/8011>`_
	with `Portainer` Server, the Secure mode is not supported

*******
Service
*******

A ``emiso`` service has been added to help control the engine. Currently this
service only starts the ``emiso-engine``.

Usage:

* Control

.. code-block:: shell

	systemctl {start,stop,status,restart} emiso.service

* Retrieve logs

.. code-block:: shell

	journalctl -fu emiso.service

************
Architecture
************

The following picture depicts the architecture of the EMISO engine. It is constituted
by:

.. figure:: pictures/EMISI-engine_architecture.png
	:name: _fig-engine_architecture
	:alt: Engine Architecture
	:align: center

	Engine Architecture

The different blocks of the engine are:

* A web server compliant with a subset of the Docker APIs.
* EMISO Daemon handles the interactions with the SO3 Containers
* The cli interface offers an entry point to interact with the EMISO Daemon. It
  is used to directly interact with the SO3 Containers, bypassing the webserver.
  It can be used for debugging purposes, for example.

.. note::

	The cli interface is not implemented in current ``emiso-engine`` version

******
Daemon
******

The EMISO engine *Daemon* provides an interface to interact with the SO3 containers.
SO3 Container are SOO Mobile Entity (ME). ME has been instrumented to be controlled
by the daemon.

The following table provides the mapping between the Docker and SO3 elements. The
Docker elements which are not present in the table - like volumes, networks, … -
are not handled by the SO3 containers.

	==============  =============================
	Docker          SO3 container
	==============  =============================
	Dockerfile      SO3 Container sources
	Image           SO3 itb file
	Container       SO3 “injected” container
	==============  =============================

The EMISO Engine daemon provides supports the following features:

* Retrieving status/info about the SO3 Images/Containers
* SO3 Container deployment/injection/creation
* SO3 Container start/stop/restart
* SO3 Container pause/unpause
* SO3 Container termination / kill
* SO3 Container update with a new container version

SO3 Images
==========

An SO3 Container image consists in a SO3 “itb” file. These images are stored in
``/mnt/ME/`` SD card partition.

SO3 Container - Creation
========================

The creation of an SO3 container consists in a SO3 injection. At the end of the
injection process, the container state is “booting”.

SO3 Container - Start
=====================

Starting a SO3 container is similar to the Migration finalization process in SOO
framework.

It is expected that the SO3 container state is “booting” before starting it. The
final state is “living”.

SO3 Container - Stop
====================

In Docker, the container stop command consists in sending the ``SIGTERM``, and
after a grace period, ``SIGKILL``. It is a “gentle” container kill procedure. Once
a container has been stopped, it is possible to restart the container by calling
the “start” command.

To provide the same behaviors, the SO3 container stop command performs the following
tasks:

* Force terminate the ME
* Re-Inject the ME

The Container is then ready to be started!

SO3 Container – Pause / Unpause
===============================

The SO3 container pause / unpause consists in a ME migration ``init`` and finalization
commands respectively.

SO3 Container - Logs
====================

SO3 Containers have to provide a method to retrieve their logs through Docker APIs.
This improvement involves enhancing the VUART backend driver. When a print is made
from a SO3 Container, the message is sent to the Linux kernel via the VUART
backend/frontend drives. Then, the backend driver prints these messages by directly interfacing with the UART driver.

In this update, the logged messages are now also stored in dedicated log files.
Each container has its own file. The file path for these logs is as follows:

* File path: ``/var/log/soo/me_<ME_slotID>.log``

The following image shows an overview of this log's mechanism.

.. figure:: pictures/emiso_engine_logs_flow.png
	:name: _fig-emiso_engine_logs_flow
	:alt: EMISO engine logs flow
	:align: center

	EMISO engine logs flow

The behaviors is implemented this way:
* **SO3 Container**: The ``logs`` function has been added to SO3 containers. This
function adds ``[ME:<SLOT ID>]`` prefix to the messages.
* **linux**: syslog-ng has been configured to store the messages with this prefix
in the logs files.

.. note::

	All the ``me_<ME_slotID>.log`` files are deleted at boot time

*************
cli interface
*************

.. note::

	The cli interface is not implemented in current ``emiso-engine`` version
