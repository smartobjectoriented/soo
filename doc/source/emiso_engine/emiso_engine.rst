.. _emiso_engine:

############
EMISO Engine
############

.. note::

	Work in progress - the current version only contains basic info.

The EMISO Engine replaces the docker engine in the EMISO environment. It acts as
a gateway between *Portainer* agent and *SO3 containers*. The following picture
presents the communication flow between the *Portainer* Server and *SO3 Containers*.

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

The following picture shows the architecture of the EMISO engine. It is constituted
by:

* A Web Server which receives requests from the Docker API. It is a Restful HTTP
  server.
* EMISO Daemon - it handles the interaction with the SO3 Containers
* Cli interface. It offers an entry point to interact with the EMISO Daemon. It is
  used to interact with the SO3 Containers from the user-space mainly for debug
  purposes.

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

The following table provides the mapping between the Docker and SO3 states.

	==============  =============================
	Docker          SO3 container
	==============  =============================
	created         ME_state_booting
	created         ME_state_preparing
	running         ME_state_living
	paused          ME_state_suspended
	error           ME_state_migrating
	paused          ME_state_dormant
	dead            ME_state_killed
	exited          ME_state_terminated
	dead            ME_state_dead
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

*************
cli interface
*************

.. note::

	The cli interface is not implemented in current ``emiso-engine`` version

The cli interface supports the following commands:

	=====================  ==========================================
	Cmd Name               Description
	=====================  ==========================================
	image info             Return information on the available images
	image rm <IMAGE NAME>  Remove <IMAGE NAME>
	=====================  ==========================================
