.. _swrdma:

Software RDMA Configuration
===========================

Many non-datacenter NICs do not support RDMA offload. Instead of resorting to
TCP, you can use the software RDMA support provided in the Linux RDMA stack. The
performance of this solution lies somewhere in between TCP and real hardware
RDMA support.

You can also use this solution if only one side of the connection supports
hardware RDMA, or both sides' RDMA protocols are incompatible. Software
implementations exist for the RoCEv2 and iWARP protocols, and you only need to
use software support on the side that does not support hardware RDMA.

You will need root access to configure the RXE kernel module. Without it, your
only option is TCP.

The same system tweaks as hardware RDMA (e.g. locked memory) are necessary, and
the following steps also assume you have already installed the required packages
for LGProxy.

RXE Kernel Driver Configuration
-------------------------------

Determine which link you would like to use software RDMA support on. Open a
terminal and type:

.. code-block:: 

    ip link

This should produce output such as:

.. code-block:: 

    1: lo: <LOOPBACK,UP,LOWER_UP> mtu 65536 qdisc noqueue state UNKNOWN mode DEFAULT group default qlen 1000
    link/loopback 00:00:00:00:00:00 brd 00:00:00:00:00:00
    2: enp5s0: <NO-CARRIER,BROADCAST,MULTICAST,UP> mtu 1500 qdisc mq state DOWN mode DEFAULT group default qlen 1000
        link/ether (address redacted) brd ff:ff:ff:ff:ff:ff
    3: enp4s0f0: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 9000 qdisc mq state UP mode DEFAULT group default qlen 1000
        link/ether (address redacted) brd ff:ff:ff:ff:ff:ff
    4: enp4s0f1: <NO-CARRIER,BROADCAST,MULTICAST,UP> mtu 1500 qdisc mq state DOWN mode DEFAULT group default qlen 1000
        link/ether (address redacted) brd ff:ff:ff:ff:ff:ff'

In this example, we would like to use the interface ``enp4s0f0``. Choose the
correct interface on your system and substitute ``enp4s0f0`` for your network
interface in the following command.

Enable software RDMA as follows:

.. code-block:: 

    sudo rdma link add rxe0 type rxe netdev enp4s0f0
    rdma link

To use the iWARP protocol instead of RoCEv2, replace ``rxe0`` and ``rxe`` with
``siw0`` and ``siw``. Note that since iWARP uses the TCP protocol, performance
may be lower than that of RoCEv2.

If the interface was created correctly, you should see the following output:

.. code-block:: 

    link rxe0/1 state ACTIVE physical_state LINK_UP netdev enp4s0f0

When running LGProxy with software RDMA, use the IP address of the link you
created the RXE interface on. For example:

.. code-block:: 

    ip addr show enp4s0f0

.. code-block:: 

    3: enp4s0f0: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 9000 qdisc mq state UP group default qlen 1000
    link/ether (address redacted) brd ff:ff:ff:ff:ff:ff
    inet 192.168.1.234/24 brd 192.168.1.255 scope global dynamic noprefixroute enp4s0f0
       valid_lft 1234sec preferred_lft 1234sec

In this case, use the IP address ``192.168.1.234`` as the NIC address argument.