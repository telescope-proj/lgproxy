.. _rdma_vendors:

RDMA NIC Vendor List
====================

These NICs are known to have RDMA support, and/or they have been tested with
LGProxy in the past. Most network adapters do not require special software or
proprietary drivers beyond what is provided in the kernel (including
NVIDIA/Mellanox NICs). Proprietary packages like Mellanox OFED designed for HPC
users exists, but these are not required for LGProxy.

.. warning::

    RoCEv1, RoCEv2, iWARP, and InfiniBand are incompatible with each other. For
    NICs which support both RoCEv1 and v2 it may be necessary to manually
    specify the RoCE version used or update the firmware to a version which
    supports both simultaneously. 

.. csv-table:: 
    :file: ../_static/nic_vendor.csv
    :header-rows: 1

\* For the ConnectX-3 Pro, RoCEv2 is only enabled on the proprietary Mellanox
OFED driver, which is out of support and has issues with newer kernel versions.
Check the following `link
<https://docs.nvidia.com/networking/display/mlnxenv496060lts/rdma+over+converged+ethernet+(roce)>`_.
