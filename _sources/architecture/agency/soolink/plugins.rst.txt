.. _plugins:

Plugins management
------------------

Plugins are used to access physical network interfaces in order to send and to receive data packets.
It constitutes the OSI layer #1 which is the physical layer under the datalink.


.. _simulation_plugin:

Simulation plugin
^^^^^^^^^^^^^^^^^

The simulation plugin enables the instantiation of several SOO environments which run in a unique
QEMU instance. It is basically used in emulation and for debugging/testing/assessment purposes.

With this plugin, it is possible to have several emulated (independent) smart objects running in 
a unique QEMU instance. It is possible to define whatever network topology, i.e. it is 
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
   :align: center
   
   Dissemination of buffers with 6 smart objects (direct neighbourhood and hidden nodes)
   
The next figure shows the same experiments with a linear topology composed of 4 SOOs.

.. figure:: /img/SOO_architecture_v2021_2-Topologies_4_SOO.png
   :align: center
    
   Topology with 4 SOOs in a simple linear light of sight

And another topology with 8 SOOs leading to several conflicts and retries on no-ack

.. figure:: /img/SOO_architecture_v2021_2-Topologies_8_SOO.png
   :align: center
    
   Topology with 8 SOOs in a simple linear light of sight

As we can see with these results, the dissemination is globally well spread between the different smart objects
even with some hidden nodes. It is very promising regarding high network scalability of the ecosystem.


WLAN plugin
^^^^^^^^^^^

The WLAN plugin is the main plugin used for the wireless Wifi network used for the migration 
of MEs.

Raspberry Pi 4 WIFI capabilities
""""""""""""""""""""""""""""""""

.. code:: bash

   soo-rpi4 ~ # iw phy0 info
   Wiphy phy0
      max # scan SSIDs: 10
      max scan IEs length: 2048 bytes
      max # sched scan SSIDs: 16
      max # match sets: 16
      max # scan plans: 1
      max scan plan interval: 508
      max scan plan iterations: 0
      Retry short limit: 7
      Retry long limit: 4
      Coverage class: 0 (up to 0m)
      Device supports roaming.
      Device supports T-DLS.
      Supported Ciphers:
         * WEP40 (00-0f-ac:1)
         * WEP104 (00-0f-ac:5)
         * TKIP (00-0f-ac:2)
         * CCMP-128 (00-0f-ac:4)
         * CMAC (00-0f-ac:6)
      Available Antennas: TX 0 RX 0
      Supported interface modes:
          * IBSS
          * managed
          * AP
          * P2P-client
          * P2P-GO
          * P2P-device
      Band 1:
         Capabilities: 0x1022
            HT20/HT40
            Static SM Power Save
            RX HT20 SGI
            No RX STBC
            Max AMSDU length: 3839 bytes
            DSSS/CCK HT40
         Maximum RX AMPDU length 65535 bytes (exponent: 0x003)
         Minimum RX AMPDU time spacing: 16 usec (0x07)
         HT TX/RX MCS rate indexes supported: 0-7
         Bitrates (non-HT):
            * 1.0 Mbps
            * 2.0 Mbps (short preamble supported)
            * 5.5 Mbps (short preamble supported)
            * 11.0 Mbps (short preamble supported)
            * 6.0 Mbps
            * 9.0 Mbps
            * 12.0 Mbps
            * 18.0 Mbps
            * 24.0 Mbps
            * 36.0 Mbps
            * 48.0 Mbps
            * 54.0 Mbps
         Frequencies:
            * 2412 MHz [1] (20.0 dBm)
            * 2417 MHz [2] (20.0 dBm)
            * 2422 MHz [3] (20.0 dBm)
            * 2427 MHz [4] (20.0 dBm)
            * 2432 MHz [5] (20.0 dBm)
            * 2437 MHz [6] (20.0 dBm)
            * 2442 MHz [7] (20.0 dBm)
            * 2447 MHz [8] (20.0 dBm)
            * 2452 MHz [9] (20.0 dBm)
            * 2457 MHz [10] (20.0 dBm)
            * 2462 MHz [11] (20.0 dBm)
            * 2467 MHz [12] (20.0 dBm)
            * 2472 MHz [13] (20.0 dBm)
            * 2484 MHz [14] (disabled)
      Band 2:
         Capabilities: 0x1062
            HT20/HT40
            Static SM Power Save
            RX HT20 SGI
            RX HT40 SGI
            No RX STBC
            Max AMSDU length: 3839 bytes
            DSSS/CCK HT40
         Maximum RX AMPDU length 65535 bytes (exponent: 0x003)
         Minimum RX AMPDU time spacing: 16 usec (0x07)
         HT TX/RX MCS rate indexes supported: 0-7
         VHT Capabilities (0x00001020):
            Max MPDU length: 3895
            Supported Channel Width: neither 160 nor 80+80
            short GI (80 MHz)
            SU Beamformee
         VHT RX MCS set:
            1 streams: MCS 0-9
            2 streams: not supported
            3 streams: not supported
            4 streams: not supported
            5 streams: not supported
            6 streams: not supported
            7 streams: not supported
            8 streams: not supported
         VHT RX highest supported: 0 Mbps
         VHT TX MCS set:
            1 streams: MCS 0-9
            2 streams: not supported
            3 streams: not supported
            4 streams: not supported
            5 streams: not supported
            6 streams: not supported
            7 streams: not supported
            8 streams: not supported
         VHT TX highest supported: 0 Mbps
         Bitrates (non-HT):
            * 6.0 Mbps
            * 9.0 Mbps
            * 12.0 Mbps
            * 18.0 Mbps
            * 24.0 Mbps
            * 36.0 Mbps
            * 48.0 Mbps
            * 54.0 Mbps
         Frequencies:
            * 5170 MHz [34] (disabled)
            * 5180 MHz [36] (20.0 dBm)
            * 5190 MHz [38] (20.0 dBm)
            * 5200 MHz [40] (20.0 dBm)
            * 5210 MHz [42] (20.0 dBm)
            * 5220 MHz [44] (20.0 dBm)
            * 5230 MHz [46] (20.0 dBm)
            * 5240 MHz [48] (20.0 dBm)
            * 5260 MHz [52] (20.0 dBm) (radar detection)
            * 5280 MHz [56] (20.0 dBm) (radar detection)
            * 5300 MHz [60] (20.0 dBm) (radar detection)
            * 5320 MHz [64] (20.0 dBm) (radar detection)
            * 5500 MHz [100] (20.0 dBm) (radar detection)
            * 5520 MHz [104] (20.0 dBm) (radar detection)
            * 5540 MHz [108] (20.0 dBm) (radar detection)
            * 5560 MHz [112] (20.0 dBm) (radar detection)
            * 5580 MHz [116] (20.0 dBm) (radar detection)
            * 5600 MHz [120] (20.0 dBm) (radar detection)
            * 5620 MHz [124] (20.0 dBm) (radar detection)
            * 5640 MHz [128] (20.0 dBm) (radar detection)
            * 5660 MHz [132] (20.0 dBm) (radar detection)
            * 5680 MHz [136] (20.0 dBm) (radar detection)
            * 5700 MHz [140] (20.0 dBm) (radar detection)
            * 5720 MHz [144] (disabled)
            * 5745 MHz [149] (disabled)
            * 5765 MHz [153] (disabled)
            * 5785 MHz [157] (disabled)
            * 5805 MHz [161] (disabled)
            * 5825 MHz [165] (disabled)
      Supported commands:
          * new_interface
          * set_interface
          * new_key
          * start_ap
          * join_ibss
          * set_pmksa
          * del_pmksa
          * flush_pmksa
          * remain_on_channel
          * frame
          * set_wiphy_netns
          * set_channel
          * tdls_oper
          * start_sched_scan
          * start_p2p_device
          * connect
          * disconnect
          * crit_protocol_start
          * crit_protocol_stop
          * update_connect_params
      Supported TX frame types:
          * managed: 0x00 0x10 0x20 0x30 0x40 0x50 0x60 0x70 0x80 0x90 0xa0 0xb0 0xc0 0xd0 0xe0 0xf0
          * AP: 0x00 0x10 0x20 0x30 0x40 0x50 0x60 0x70 0x80 0x90 0xa0 0xb0 0xc0 0xd0 0xe0 0xf0
          * P2P-client: 0x00 0x10 0x20 0x30 0x40 0x50 0x60 0x70 0x80 0x90 0xa0 0xb0 0xc0 0xd0 0xe0 0xf0
          * P2P-GO: 0x00 0x10 0x20 0x30 0x40 0x50 0x60 0x70 0x80 0x90 0xa0 0xb0 0xc0 0xd0 0xe0 0xf0
          * P2P-device: 0x00 0x10 0x20 0x30 0x40 0x50 0x60 0x70 0x80 0x90 0xa0 0xb0 0xc0 0xd0 0xe0 0xf0
      Supported RX frame types:
          * managed: 0x40 0xd0
          * AP: 0x00 0x20 0x40 0xa0 0xb0 0xc0 0xd0
          * P2P-client: 0x40 0xd0
          * P2P-GO: 0x00 0x20 0x40 0xa0 0xb0 0xc0 0xd0
          * P2P-device: 0x40 0xd0
      software interface modes (can always be added):
      valid interface combinations:
          * #{ managed } <= 1, #{ P2P-device } <= 1, #{ P2P-client, P2P-GO } <= 1,
            total <= 3, #channels <= 2
          * #{ managed } <= 1, #{ AP } <= 1, #{ P2P-client } <= 1, #{ P2P-device } <= 1,
            total <= 4, #channels <= 1
      Device supports scan flush.
      Device supports randomizing MAC-addr in sched scans.
      Supported extended features:
         * [ 4WAY_HANDSHAKE_STA_PSK ]: 4-way handshake with PSK in station mode
         * [ 4WAY_HANDSHAKE_STA_1X ]: 4-way handshake with 802.1X in station mode
         * [ DFS_OFFLOAD ]: DFS offload
      
   
