.. _portainer:

#########
Portainer
#########

*Portainer CE* (Community Edition) is an open-source container management tool
that simplifies the deployment, management, and monitoring of Docker containers
and containerized applications. It provides a user-friendly web-based interface
that allows users to interact with Docker and manage containers, images, networks,
and volumes without needing to use complex command-line tools.

In the SOO framework, `Portainer Server CE` is used as the Container Orchestration
Use User Interface (COUI).

************
Installation
************

Portainer Server runs as lightweight Docker containers on a Docker engine. It means
docker must be installed on the Host PC.

* Creation of a volume that Portainer Server use to store it database:

.. code-block:: shell

    docker volume create portainer_data

* Download and install Portainer Server container:

.. code-block:: shell

    $ docker run -d -p 8000:8000 -p 9443:9443 \
       --name portainer \
       --restart=always \
       -v /var/run/docker.sock:/var/run/docker.sock \
       -v portainer_data:/data \
       portainer/portainer-ce:latest

``docker ps`` command can be used to check if the Portainer server is running

To log-in, open a web browser and to to:

.. code-block:: shell

    https://localhost:9443

*************
Initial setup
*************

Once the Portainer Server has been deployed, and you have navigated to the instance's
URL, you are ready for the initial setup.

First time connected to the Portainer server; the first user has to be created.
This first user will be an administrator. The username defaults to admin and the
password must be at least 12 characters long.

Once the admin user has been created, the Environment Wizard will automatically
launch. The wizard will help get you started with Portainer.

The installation process automatically detects your local environment and sets it
up for you. If you want to add additional environments to manage with this Portainer
instance, click Add Environments. Otherwise, click Get Started to start using
Portainer!

*********************
Create an environment
*********************

In short, an environment in Portainer represents a SOO mobile entity.

* Select "environment"
* Click "+ Add environment"
* Select "Docker Standalone" --> click "Start Wizard"
    * Select "API"
    * Provide a name to the environment
    * Set the API + port (default port is 2375)
    * no TLS

**********************
Create a SO3 container
**********************

* Select the environment on which to create the container
* Select "Containers" --> click "+ Add container"
    * Give a name at the container
    * Provide the image name in image field (``/mnt/ME/<CONTAINER NAME>.itb)``
    * Click on "Advanced mode" to select the "Simple mode"
    * Disable "Always pull the image" button
    * Click on "Deploy the container"


***************
EMISO Container
***************

A simple container, called ``emiso_64`` has been created. It can be used as an
example of of SO3 container. It simply prints a message/log each second.

* Compilation:

.. code-block:: shell

    $ cd <SOO HOME>/ME
    $ ./build.sh -k SOO.emiso_64

* Deployment:

.. code-block:: shell

    ./deploy.sh -m SOO.emiso_64
