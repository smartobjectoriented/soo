
SOOlink subsystem
=================

.. toctree::
   :maxdepth: 5
   :hidden:
   
   transcoder
   transceiver
   plugins

SOO network simulation and topologies
-------------------------------------

With the :ref:`simulation plugin <simulation_plugin>`, it is possible to have several emulated (independent)
smart objects running in a unique QEMU instance. It is possible to define whatever network topology, i.e. it is 
possible to define the *light of sight* between each smart object and thus create a specific neighbourhood.

To evaluate the efficiency in disseminating the MEs in the neighbourhood taking into account
hidden smart objects, we can stream buffers constantly between SOOs and to count how many buffers
are received from each smart object.

Results shown on the figure below have been performed with the following configuration:

* QEMU/vexpress emulated environment
* Buffer of 16 KB
* From boot up to ~10s after getting the agency prompt
* The number corresponds to the quantity of buffer (of 16 KB) received from the other side

.. figure:: /img/SOO_architecture_v2021_2-Topologies_6_SOO.png
    
   Dissemination of buffers with 6 smart objects (direct neighbourhood and hidden nodes)
   
The next figure shows the same experiments with a linear topology composed of 4 SOOs.

.. figure:: /img/SOO_architecture_v2021_2-Topologies_4_SOO.png
    
   Topology with 4 SOOs in a simple linear light of sight

And another topology with 8 SOOs leading to several conflicts and retries on no-ack

.. figure:: /img/SOO_architecture_v2021_2-Topologies_8_SOO.png
    
   Topology with 8 SOOs in a simple linear light of sight

As we can see with these results, the dissemination is globally well spread between the different smart objects
even with some hidden nodes. It is very promising regarding high network scalability of the ecosystem.





   
   



   