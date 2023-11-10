.. _docker:

######
Docker
######

The *Docker* engine has to be integrated in the ``EMISO`` environment.

***********
Integration
***********

**Kernel**:

The script ``check-config.sh`` serves the purpose of generating the list of the
kernel modules required to enable Docker support.

* Usage: ``check-config.sh <DEFCONFIG FILE>``

It is located in ``<EMISO HOME>/docker/`` folder.

**rootfs**

The following options have to be enabled to add docker tool and engine in the rootfs:

.. code-block:: shell

	BR2_PACKAGE_DOCKER_CLI
	BR2_PACKAGE_DOCKER_CLI_STATIC
	BR2_PACKAGE_DOCKER_ENGINE
	BR2_PACKAGE_DOCKER_PROXY

*****************
Images Management
*****************

*Docker* images are typically obtained by utilizing the docker pull command. It
relies on an active internet connection. However, an internet connection cannot
be guaranteed. To overcome this limitation, the *docker* images are embedded in
the target rootfs (path: ``/root/docker_images/``)

Image integration
=================

The image should be placed in one of the following directories:

* <EMISO HOME>/agency/usr/docker_images/{aarch32, aarch64}

The following commands shows how to retrieve a *docker* image and place it at the
correct place in `emiso` repo

.. code-block:: shell

	$ docker pull <IMAGE>
	$ docker save -o <IMAGE NAME>.docker <IMAGE NAME>
	$ zip <IMAGE NAME>.zip <IMAGE NAME>.docker
	$ mv <IMAGE NAME>.zip <EMISO HOME>/agency/usr/docker_images/{aarch32, aarch64}

.. warning::

	The `docker pull` command will pull the image for the host platform (x86 for
	an x86 PC for example)

Here are the commands to load an image in the target:

1. Extract the image: ``unzip docker_images/<IMAGE NAME>.zip``
2. Load the image:  ``docker load -i <IMAGE NAME>.docker``
3. (optional) remove the image file: ``rm <IMAGE NAME>.docker``

**************
Basic commands
**************

* List of available images: ``docker image ls``
* Remove an images: ``docker image rm <IMAGE ID>``
* List of running containers: ``docker container ls``
* List of all containers: ``docker container ls -a``
* Start a container: ``docker container start <IMAGE NAME>``
* Stop a container: ``docker container stop <IMAGE NAME>``
* Remove the stopped containers: ``docker container prune``
