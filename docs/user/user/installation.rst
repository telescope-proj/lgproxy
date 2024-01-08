.. _installation:

Installing Dependencies
=======================

.. note::
    
    Ensure the dependencies are installed on both the host and client side.
    This document only covers dependencies for **LGProxy**. For Looking Glass,
    please refer to the `Looking Glass Documentation 
    <https://looking-glass.io/docs/>`_.

Build Dependencies
------------------

If you are using a distribution not covered here, e.g. Arch, please
search for the following packages in your package manager.

.. list-table::
    :header-rows: 1

    * - Package
      - Minimum Version
    * - CMake
      - 3.5 
    * - Make
      - 4.0
    * - GCC
      - 4.8.1
    * - Libfabric
      - 1.10    

Additionally, ensure you have installed the development headers for Libfabric.

You could also use Clang (v3.3+) instead of GCC, but other than compile-testing
with our CI workflow, LGProxy is not extensively tested with this configuration.

Fedora, RHEL, and derivatives
-----------------------------

Fedora, RHEL, Rocky Linux, AlmaLinux, etc.

.. note::

    If you intend to run the Looking Glass client on RHEL/Rocky Linux, some of
    the dependencies for Looking Glass may not available by default in these
    repositories.

.. code-block:: bash

    sudo dnf install -y rdma-core libfabric libfabric-devel gcc gcc-c++ cmake make git

Optional diagnostic tools for RDMA can also be installed using:

.. code-block:: bash

    sudo dnf install -y qperf infiniband-diags ibstat 

Debian, Ubuntu, and derivatives
-------------------------------

Debian, Ubuntu, Linux Mint, ElementaryOS, etc.

Run the following commands to install the core dependencies:

.. code-block:: bash

    sudo apt install -y rdma-core autogen libtool cmake gcc make build-essential git

Optional diagnostic tools for RDMA can also be installed using:

.. code-block:: bash

    sudo apt install -y qperf infiniband-diags ibstat

Depending on your distribution version, you might have to install Libfabric from
source. This applies if your Libfabric version is lower than the minimum
required version.

Versions based on **Ubuntu 20.04**, **Debian 10**, or older versions must
compile Libfabric from source.

Installing Libfabric
~~~~~~~~~~~~~~~~~~~~

Users of older distributions may have older versions of Libfabric installed
which are not compatible with LGProxy must compile Libfabric from source to use
it with LGProxy.

To check the Libfabric version in your package manager, use

.. parsed-literal::

    $ apt show libfabric1

    Package: libfabric1
    **Version: 1.11.0-3**
    ...
    Description: libfabric communication library

The package might not be known in your distro as ``libfabric1``, rather
``libfabric``.

Installing distro-provided Libfabric
************************************

Only applicable for **Ubuntu 22.04**, **Debian 11**, and later versions

.. code-block:: bash

    sudo apt install -y libfabric-bin libfabric-dev libfabric1

Compiling Libfabric from source
*******************************

Applicable for users of **Ubuntu 20.04**, **Debian 10**, or older.

Chances are that if you need to install Libfabric here, LGProxy will be the only
application that will utilize it. Therefore, this tutorial will go over the
installation of Libfabric as a regular (non-root) user and enabling only the
features required by LGProxy.

We will install Libfabric to your home directory in the folder ``libfabric``,
but you can choose any non-system directory. If you are not a technical user, we
recommend you use a dedicated directory, since it can be difficult to uninstall
libfabric in case you accidentally delete the installation files.

.. code-block:: bash

    sudo apt -y install librdmacm-dev
    git clone https://github.com/ofiwg/libfabric.git fabric
    cd fabric/
    ./autogen.sh
    ./configure --prefix=$HOME/libfabric --enable-only \
                --enable-tcp=yes --enable-rxm=yes --enable-verbs=yes

This should output that the TCP, RXM and Verbs providers are enabled, as below:

::

    ***
    *** Built-in providers: tcp rxm verbs
    *** DSO providers:
    ***

After confirming, proceed onto the installation process.

::

    make -j $(nproc)
    make install

If this process was successful, you should be able to see the following output
when you scroll up:

::

    ----------------------------------------------------------------------
    Libraries have been installed in:
       /home/username/libfabric/lib

    (some extra info here)

    See any operating system documentation about shared libraries for
    more information, such as the ld(1) and ld.so(8) manual pages.
    ----------------------------------------------------------------------

You'll then need to either add the Libfabric binary to your PATH for the current
session, or add it to your ``.bashrc`` (or equivalent) to make the change
permanent.

.. code-block:: bash
    
    export PATH=$HOME/libfabric/bin:$PATH

Check that Libfabric was found and try to run it:

::

    $ whereis fi_info
    fi_info: /home/username/libfabric/bin/fi_info

    $ fi_info --version
    fi_info: 1.21.0a1
    libfabric: 1.21.0a1
    libfabric api: 1.20


This should correspond with the version you downloaded from the Git repository
(latest).

.. note::

    You should keep the downloaded Libfabric folder (here named ``fabric``),
    since it makes the removal of Libfabric easier when needed.

.. note::
    Once complete, go to :doc:`building`.