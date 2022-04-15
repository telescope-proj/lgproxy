.. _installation:


Installating Dependencies
=========================

.. note::
    Install dependencies on both the host and the client side.

Build Dependencies
******************

* cmake
* make
* protobuf-c
* libfabric

Fedora/RHEL/Rocky Linux
***********************

.. code-block:: bash

    sudo dnf install -y libfabric libfabric-devel \
    protobuf-c-compiler protobuf-c-devel gcc cmake git

Ubuntu
******

Ubuntu 20.04 or older
---------------------

.. note::
    If you are using Ubuntu 20.04 or lower you need to compile `Libfabric <https://github.com/ofiwg/libfabric>`_ from source. The version in the package manager is too old.

.. code-block:: bash

    sudo apt install -y protobuf-c-compiler libprotobuf-c-dev \
    autogen libtool cmake gcc make build-essential git

**Compiling Libfabric from souce**

.. code-block:: bash

    git clone https://github.com/ofiwg/libfabric.git
    ./autogen.sh
    ./configure
    make -j $(nproc)
    sudo make install


Ubuntu 21.04 or newer
---------------------
.. code-block:: bash

    sudo apt install -y libfabric-bin libfabric-dev \
    libfabric1 protobuf-c-compiler libprotobuf-c-dev






