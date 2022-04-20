.. _running:

Running The Application
=======================

.. warning::

    When using RDMA transports, please ensure you have configured your network
    and system correctly, else you may experience poor performance. Information
    on recommended configuration steps may be found in the `LibTRF documentation
    <https://telescope-proj.github.io/libtrf/index.html>`_.

.. warning::
    Currently, the proxy has very poor performance with DMABUF. Use of /dev/shm
    over /dev/kvmfr is advised, but this may change in a future update.

Sink
****

**Usage Guide**

.. code-block:: text

    Looking Glass Proxy (LGProxy)
    Copyright (c) 2022 Telescope Project Developers
    Matthew McMullin (@matthewjmc), Tim Dettmar (@beanfacts)

    Documentation: https://telescope-proj.github.io/lgproxy
    Documentation also contains licenses for third party libraries
    used by this project

    Options:
    -h  Hostname or IP address to connect to
    -p  Port or service name to connect to
    -f  Shared memory or KVMFR file to use
    -s  Size of the shared memory file - not required unless
        the file has not been created
    -d  Delete the shared memory file on exit
    -r  Polling interval in milliseconds

.. note::
    This is currently a very early release of this software. When running the
    Looking Glass client, you will need to add the ``-c`` option to specify
    which host to connect to for your SPICE server, the 
    ``spice:captureOnStart=yes`` option also needs to be set for mouse input
    to work properly.

The **sink** side is the receiver side, i.e. the computer on which you will be
running the Looking Glass client. The hostname or IP address specified should be
that of the **source** application. You may use any interface with an IPv4
address; the negotiation protocol in LibTRF will attempt to use the fastest
interface.

The shared memory file does not need to be specified - it defaults to
/dev/shm/looking-glass, which is the default for the Looking Glass application.
The file size does not need to be explicity specified - the system determines
this automatically unless a specific size is needed.

The polling interval specifies how often the transport library polls for
incoming data. By default, the interval is 1ms, but may be set to 0 to use busy
waiting or a higher interval for lower framerates and slower mice.

.. warning::

    Reducing the polling interval to 0 will pin at least two cores at 100%
    usage!

Source
******

**Usage Guide**

.. code-block:: text

    Looking Glass Proxy (LGProxy)
    Copyright (c) 2022 Telescope Project Developers
    Matthew McMullin (@matthewjmc), Tim Dettmar (@beanfacts)

    Documentation: https://telescope-proj.github.io/lgproxy
    Documentation also contains licenses for third party libraries
    used by this project

    Options:
    -h  Hostname or IP address to listen on
    -p  Port or service name to listen on
    -f  Shared memory or KVMFR file to use
    -s  Size of the shared memory file

The source application runs on the host machine containing the VM running
Looking Glass. Only the hostname and port need to be specified. To listen on all
interfaces, use `0.0.0.0`. Specify an unused port between 10240 and 65535. If
the shared memory file is not specified, it defaults to
``dev/shm/looking-glass``.