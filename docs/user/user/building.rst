.. _building:

.. include:: ../subs.rst

Building The Application
========================

.. note::
    Ensure all the dependencies are installed for your operating system. Head
    to :doc:`installation` for more details.

You will first need to clone the Git repository:

.. parsed-literal::

    git clone \ |GITURL|\ 

    cd lgproxy

This repository contains the client and server applications.  

Server
------

The **server** application should be installed on the machine hosting the VM
where Looking Glass is installed. To install the server application:

::

    cd server
    mkdir build
    cd build
    cmake ..
    make -j $(nproc)

Successful compilation will produce an executable file: ``server``.

Client
------

The **client** should be installed on the machine which will be used to run the
Looking Glass client. To install the client application:

::

    cd client
    mkdir build
    cd build
    cmake ..
    make -j $(nproc)

Successful compilation will produce an executable file: ``client``.

Once complete, go to :doc:`running`.