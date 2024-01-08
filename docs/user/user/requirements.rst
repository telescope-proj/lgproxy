.. _requirements:

System Requirements
===================

The use of fast network adapters is a **hard requirement**. As a general rule,
this means 10Gbit/s capable adapters or higher must be used on both ends of the
connection. In addition, hardware RDMA capability on your network adapter is
strongly recommended. In case you have fast adapters that are incapable of RDMA,
the use of :ref:`software RDMA <swrdma>` is preferable over the fallback TCP
transport.

When using RDMA, **the locked memory limit must be increased**. The default
limit is extremely low (usually a few KB or MB). You can check the amount of
locked memory your security policy allows by:

.. code-block:: 

    $ ulimit -l

    unlimited

If there is not enough locked memory, LGProxy may fail to start with the error
"no memory available", even though you may have enough memory on your system.

While LGProxy does not **require** any special hardware, the use of RDMA capable
hardware is recommended. Additionally, system and network configuration may be
required for optimal performance. In particular, the locked memory limit must be
increased when using RDMA as a transport.

Check 

Configuration steps, as well as a listing of RDMA capable hardware, may be found
in the `LibTRF documentation
<https://telescope-proj.github.io/libtrf/index.html>`_.

Recommended Specifications
~~~~~~~~~~~~~~~~~~~~~~~~~~

The requirements depend on whether you are using DMABUF and whether the
transport type is accelerated. The below table gives the recommended maximum
bandwidth utilization for each class of NIC with :ref:`performance
optimizations <perf_optimization>`. 

.. list-table:: 
    :header-rows: 1

    * - NIC Class
      - Hardware RDMA 
      - Software RDMA
      - TCP
    * - Enterprise RDMA
      - 100 Gbit/s
      - 25 Gbit/s
      - 10 Gbit/s
    * - Enterprise non-RDMA
      - 
      - 10 Gbit/s
      - 10 Gbit/s
    * - Consumer mid-range
      -
      - 5 Gbit/s
      - 3 Gbit/s
    * - Consumer low-end
      -
      - 2 Gbit/s
      - 1 Gbit/s    

Some examples of each NIC class:

- See :ref:`this table <rdma_vendors>` for RDMA NIC vendors.
- Enterprise non-RDMA NICs: Broadcom BCM57404 or Intel X540 series.
- Consumer mid-range NICs: Aquantia AQC10x series.
- Consumer low-end NICs: All Realtek NICs and USB NICs.

Client
------

Best Case - DMABUF + RDMA
^^^^^^^^^^^^^^^^^^^^^^^^^

- CPU: At least a modern dual core processor is recommended.
- GPU: Follow Looking Glass recommendations.
- RAM: 4 GB
- NIC: RDMA capable NIC - link speed depends on your bandwidth requirements.

Worst Case - No DMABUF + TCP
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- CPU: One core per 200 Mpix/s pixel rate + two cores for Looking Glass
- GPU: Follow Looking Glass recommendations.
- RAM: 4 GB
- NIC: NIC with decent non-RDMA hardware offload support

.. warning:: 

    We only recommend the use of TCP as a last resort, e.g. when you do not have
    root access to the system you want to run LGProxy on. You can run RDMA over
    non-RDMA NICs using software emulation which performs better than TCP, see
    :ref:`here <swrdma>`.   

Host
----

Uncompressed, using RDMA (best case)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

There are no additional recommendations aside from your NIC having enough
bandwidth and supporting RDMA, if your PC is already capable of running Looking
Glass.

Compressed, using TCP (worst case)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

One additional core should be available to the VM guest, for real-time
compression. The recommendation for one additional core per 200 Mpix/s also
applies.

Bandwidth Calculator
~~~~~~~~~~~~~~~~~~~~

You may use the below bandwidth calculator to determine bandwidth requirements
for your chosen video streaming quality. 

.. raw:: html
    :file: ../_static/test.html
