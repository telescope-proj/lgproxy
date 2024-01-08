.. _perf_optimization:

Performance Optimizations
=========================

There are several steps you can take to improve networking performance,
regardless of whether you are using hardware RDMA, software RDMA, or TCP.

.. warning:: 

    | Please make sure that LGProxy is actually functioning before performing
      these optimizations. They will break networking on your system if not done
      correctly, and you might not be able to tell whether LGProxy or your
      system configuration is broken. 
    
    | To begin, please read the manuals for your networking hardware, including
      network adapters and switches, to determine the highest common MTU values.
      Issues can manifest as random losses in connectivity or no connectivity at
      all to certain machines. We recommend tracking the changes you made so you
      can revert them later if necessary.

Jumbo Frames
------------

All transport types benefit from jumbo frames. Increase the MTU to the highest
possible value that your NIC and intermediate hardware (e.g. network switches)
supports. If you use NetworkManager, you can make this persistent in the network
configuration (shown here is ``nmtui``):

.. parsed-literal::

    ┌───────────────────────────┤ Edit Connection ├───────────────────────────┐
    │                                                                         │ 
    │         Profile name Network Connection______________________           │ 
    │               Device 12:34:56:AB:CD:EF (enp5s0f0np0)_________           │ 
    │                                                                         │ 
    │ ╤ ETHERNET                                                    <Hide>    │ 
    │ │ Cloned MAC address ________________________________________           │ 
    │ │                **MTU 9000______ bytes**                                   │ 
    │ └                                                                       │ 
    │ ╤ 802.1X SECURITY                                             <Hide>    │ 
    │ │ [ ] Enable 802.1X security                                            │ 
    │ └                                                                       │ 
    │                                                                         │ 
    │ ╤ IPv4 CONFIGURATION <Manual>                                 <Hide>    │ 
    │ │          Addresses 10.91.0.20/24____________ <Remove>                 │ 
    │ │                    <Add...>                                           │ 
    │ │            Gateway _________________________                          │ 
    │ │        DNS servers <Add...>                                           │ 
    │ │     Search domains <Add...>                                           │ 
    │ │                                                                       │ 
    │ │            Routing (No custom routes) <Edit...>                       │ 
    │ │ [ ] Never use this network for default route                          │ 
    │ │ [ ] Ignore automatically obtained routes                              │ 
    │ │ [ ] Ignore automatically obtained DNS parameters                      │ 
    │ │                                                                       │ 
    │ │ [X] Require IPv4 addressing for this connection                       │ 
    │ └                                                                       │ 
    │                                                                         │ 
    │ ═ IPv6 CONFIGURATION <Disabled>                               <Show>    │ 
    │                                                                         │ 
    │ [X] Automatically connect                                               │ 
    │ [X] Available to all users                                              │ 
    │                                                                         │ 
    │                                                           <Cancel> <OK> │ 
    │                                                                         │ 
    └─────────────────────────────────────────────────────────────────────────┘ 
                                                                                        


Or you can set it temporarily:

::

    sudo ip link set interface_name mtu value

Replace ``interface_name`` and ``value`` with the interface name and MTU values
respectively.

Check whether the MTU increase is active with ``ip link``:

.. parsed-literal::

    $ ip link show interface_name
    1 ....
    2 ....
    3: interface_name: <BROADCAST,MULTICAST,UP,LOWER_UP> **mtu 9000** qdisc mq state UP mode DEFAULT group default qlen 1000
        link/ether 12:34:56:AB:CD:EF brd ff:ff:ff:ff:ff:ff

Again, replace ``interface_name`` with the actual name of your interface.

For InfiniBand and RoCE, there are only a few MTUs which are actually used on
the wire: 256, 512, 1024, 2048, and 4096 bytes. You can try 2152 and 4200 bytes
for MTUs as a starting point- and confirm whether the larger MTUs are being used
through the ``ibv_devinfo`` command, which shows the maximum MTU of your NIC as
well as the actual MTU used for RDMA traffic, which will be lower than the
currently set MTU due to overhead.

.. code-block:: 

    $ ibv_devinfo
    
    hca_id: rocep4s0f0
        transport:                      InfiniBand (0)
        fw_ver:                         8.59.1.0
        node_guid:                      (guid redacted)
        sys_image_guid:                 (guid redacted)
        vendor_id:                      0x1077
        vendor_part_id:                 5718
        hw_ver:                         0x1
        phys_port_cnt:                  1
                port:   1
                        state:                  PORT_ACTIVE (4)
                        max_mtu:                4096 (5)
                        active_mtu:             4096 (5)
                        sm_lid:                 0
                        port_lid:               0
                        port_lmc:               0x00
                        link_layer:             Ethernet

Hugepages
---------

LGProxy can take advantage of hugepages for its own internal message buffers, as
well as IVSHMEM registration on the **sink** side (to a limited degree). On most
systems this is already configured correctly, check by running the following
command: 

.. code-block:: 

    $ cat /sys/kernel/mm/transparent_hugepage/enabled
    
    always [madvise] never

``madvise`` is the ideal option here, since ``always`` may break some
applications. If you are using hugepages and RDMA together, you must set the
environment variable ``RDMAV_HUGEPAGES_SAFE=1`` to allow for hugepages to be
used in RDMA memory regions.

IOMMU Configuration
-------------------

.. note:: 

    Disabling full IOMMU on the host can increase the risk of certain DMA
    attacks. Consider, with regard to your threat model, whether this is
    acceptable for you.

RDMA NICs benefit from higher performance and lower latency if the IOMMU is
configured in pass-through mode instead of on the whole system. You will, of
course, still be able to use virtual machines with GPU passthrough when this
option is enabled.

Certain NICs, especially older models, might not work at all with the IOMMU
enabled. It might be worth configuring this option if you experience failures
with memory registration and RDMA data transfer that are not exhibited in TCP
mode.

Since different systems use different bootloaders, the exact sequence is not
specified here. However, wherever you currently put IOMMU kernel options, you
can add ``iommu=pt`` to use IOMMU in pass-through mode. 

For GRUB, this is usually added to the ``GRUB_CMDLINE_LINUX`` line in
``/etc/default/grub``:

.. parsed-literal::

    GRUB_CMDLINE_LINUX="rhgb amd_iommu=on **iommu=pt**"
