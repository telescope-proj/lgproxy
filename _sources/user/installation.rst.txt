.. _installation:

Installing Dependencies
=======================

.. note::
    
    Ensure the dependencies are installed on both the host and client side.
    This document only covers dependencies for **LGProxy**. For Looking Glass,
    please refer to the `Looking Glass Documentation 
    <https://looking-glass.io/docs/>`_.

Build Dependencies
******************

If you are using a distribution not covered here, e.g. Arch, please
search for the following packages in your package manager, **including the
development headers!**

* CMake
* Make
* GCC
* Protobuf-c
* Libfabric

Fedora and derivatives
**********************

.. code-block:: bash

    sudo dnf install -y libfabric libfabric-devel \
    protobuf-c-compiler protobuf-c-devel gcc cmake git

.. note::

    If you intend to run the Looking Glass client on RHEL/Rocky Linux, some of
    the dependencies for Looking Glass may not available by default in these
    repositories.

To enable RDMA transports, you will need to install the RDMA core libraries.
This may be done using the following commands:

.. code-block:: bash

    sudo dnf install -y rdma-core 

Diagnostic tools for RDMA can also be installed using:

.. code-block:: bash

    sudo dnf install -y perftest infiniband-diags ibstat 

Ubuntu & Debian
***************

All Versions
------------

Run the following commands to install the core dependencies:

.. code-block:: bash

    sudo apt install -y protobuf-c-compiler libprotobuf-c-dev \
    autogen libtool cmake gcc make build-essential git

To enable RDMA transports:

.. code-block:: bash

    sudo apt install -y rdma-core

Diagnostic tools for RDMA can also be installed using:

.. code-block:: bash

    sudo apt install -y perftest infiniband-diags ibstat


Ubuntu 20.04, Debian 10 and older
---------------------------------

.. note::
    If you are using Ubuntu 20.04 or lower, you will need to compile 
    `Libfabric <https://github.com/ofiwg/libfabric>`_ from source. 
    The version in the package manager is over three years out of date.

Compile Libfabric from source using the following commands:

.. code-block:: bash

    git clone https://github.com/ofiwg/libfabric.git
    cd libfabric/
    ./autogen.sh
    ./configure
    make -j $(nproc)
    sudo make install

.. note::

    Do not delete the ``libfabric/`` folder. You will need this to uninstall
    Libfabric from your system.

Ubuntu 21.10, Debian 11 and newer
---------------------------------

A supported version of Libfabric may be installed from the package manager
directly.

.. code-block:: bash

    sudo apt install -y libfabric-bin libfabric-dev libfabric1


.. note::
    Once complete, go to :doc:`building`.