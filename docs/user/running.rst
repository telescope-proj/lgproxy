.. _running:

Running The Application
=======================

.. warning::

    Use of DMABUF with LGProxy (i.e., the KVMFR module) works, but has not been
    extensively tested.

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

The **sink** side is the receiver side, i.e. the computer on which you will be
running the Looking Glass client. The hostname or IP address specified should be
that of the **source** application. You may use any interface with an IPv4
address; the negotiation protocol in LibTRF will attempt to use the fastest
interface.

The shared memory file does not need to be specified - it defaults to
``/dev/shm/looking-glass``, which is the default for the Looking Glass
application. The file size does not need to be explicity specified - the system
determines this automatically unless a specific size is needed.

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

Setting the log level
*********************

The log level may be set for specific components of LGProxy:

LGProxy & LibTRF
----------------
Values
    *   1 (Trace)
    *   2 (Debug)
    *   3 (Info) (Default Value)
    *   4 (Warn)
    *   5 (Error)
    *   6 (Fatal)

Set the LGProxy log level with the environment variable: **LP_LOG_LEVEL=VALUE**

Set the LibTRF log level with the environment variable: **TRF_LOG_LEVEL=VALUE**

Libfabric
---------
Values
    * warn
    * trace
    * info
    * debug

Set the Libfabric log level with the environment variable: **FI_LOG_LEVEL=VALUE**

Once both sides are running and connected, you may start the Looking Glass
client application: :doc:`runlg`.