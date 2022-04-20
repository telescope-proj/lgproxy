.. _installation:

Installing Dependencies
=======================

.. note::
    Install dependencies on both the host and the client side!

Build Dependencies
******************

* cmake
* make
* protobuf-c
* libfabric

Fedora and derivatives
**********************

.. code-block:: bash

    sudo dnf install -y libfabric libfabric-devel \
    protobuf-c-compiler protobuf-c-devel gcc cmake git

.. note::

    By default, some of the dependencies for Looking Glass are not available in
    the RHEL & Rocky Linux repositories.

To enable RDMA transports, you will need to install the RDMA core libraries.
This may be done using the following commands:

.. code-block:: bash

    sudo dnf install rdma-core -y

Diagnostic tools for RDMA can also be installed using:

.. code-block:: bash

    sudo dnf install perftest infiniband-diags ibstat -y

Ubuntu
******

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

All Ubuntu & Debian Versions
----------------------------

Once the above commands complete, run the following commands to install the
other dependencies:

.. code-block:: bash

    sudo apt install -y protobuf-c-compiler libprotobuf-c-dev \
    autogen libtool cmake gcc make build-essential git

To enable RDMA transports:

.. code-block:: bash

    sudo apt install rdma-core -y

Diagnostic tools for RDMA can also be installed using:

.. code-block:: bash

    sudo dnf install perftest infiniband-diags ibstat -y

Once complete, go to :doc:`building`.