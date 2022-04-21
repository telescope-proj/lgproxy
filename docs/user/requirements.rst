.. _requirements:

System Requirements
===================

While LGProxy does not **require** any special hardware, the use of RDMA capable
hardware is recommended. Additionally, system and network configuration may be
required for optimal performance. In particular, the locked memory limit must be
increased when using RDMA as a transport.

Configuration steps, as well as a listing of RDMA capable hardware, may be found
in the `LibTRF documentation
<https://telescope-proj.github.io/libtrf/index.html>`_.

Recommended Specifications
~~~~~~~~~~~~~~~~~~~~~~~~~~

The requirements depend on whether you are using DMABUF and whether the
transport type is accelerated.

Client
------

Using DMABUF & RDMA (best case)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- CPU: At least a modern dual core processor is recommended.
- GPU: Follow Looking Glass recommendations. NVIDIA GPUs do not support DMABUF with
  Looking Glass.
- RAM: 4 GB
- NIC: RDMA capable NIC - link speed depends on your bandwidth requirements.

No DMABUF & TCP (worst case)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- CPU: One core per 200 Mpix/s pixel rate + two cores for Looking Glass
- GPU: Follow Looking Glass recommendations.
- RAM: 4 GB
- NIC: NIC with decent non-RDMA hardware offload support, i.e. Intel NICs.

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

.. note::

    While LGProxy runs over gigabit networks, this is not an ideal
    configuration. It must be run at low resolutions/framerates or with
    real-time texture compression support enabled. Use of real-time texture
    compression currently requires our custom `fork of Looking Glass
    <https://github.com/telescope-proj/LookingGlass/tree/client-dxtc>`_.

Bandwidth Calculator
~~~~~~~~~~~~~~~~~~~~

You may use the below bandwidth calculator to determine bandwidth requirements
for your chosen video streaming quality. 

.. note::

    There is additional transmission overhead that varies based on several
    factors, such as the active transport type, MTU, and network load. As a
    safety margin, you should add around 10% to the result as overhead.

.. raw:: html
    :file: ../_static/test.html
