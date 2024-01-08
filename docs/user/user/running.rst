.. _running:

Running The Application
=======================

.. warning::

    If you have transparent hugepages **always enabled** (not the default on
    most systems), LGProxy will not function correctly. Setting the environment
    variable ``RDMAV_HUGEPAGES_SAFE=1`` might fix the issue, but we do not
    support this configuration since it can have knock-on effects.

.. note::

    The use of KVMFR DMABUF devices (``/dev/kvmfr*``) is recommended on both
    sides for performance reasons.

Server
------

The server application runs on the machine with the running VM.

Available Options
~~~~~~~~~~~~~~~~~

.. list-table::
    :header-rows: 1

    * - Short
      - Long
      - Type
      - Required
      - Example
    * - ``-n``
      - ``--nic``
      - String
      - Required
      - ``--nic 192.168.1.100``

The address of the network adapter to use for communication.

.. list-table::

    * - ``-p``
      - ``--port``
      - Integer
      - Required
      - ``--port 20000``

The port number to listen for incoming connections on.

.. list-table::

    * - ``-m``
      - ``--mem``
      - String
      - Optional
      - ``--mem /dev/kvmfr0``

The address of the shared memory region to use. If not specified, defaults to
``/dev/shm/looking-glass``. Another Looking Glass host must not be using this
shared memory region!

.. list-table::

    * - ``-i``
      - ``--interval``
      - Integer + Unit
      - Optional
      - ``--interval 1m``

The interval between polling for updates. A lower polling interval decreases
latency at the expense of CPU usage. The polling interval should be faster than
the update rate of your mouse and monitor (ideally >2x). A 1000Hz mouse sends
updates every 1ms, so the polling interval should be 500 microseconds (``-i
500u``). By default, this is set to 0 (uses 100% of two CPU cores).

You should consider using ``-i 0`` if you have the spare CPU available, since it
is the only way to avoid context switching, provides the highest frame time
consistency, and minimizes latency. This is also preferable to setting the
polling interval below 100 microseconds to avoid context switching overhead.

The supported units are: ``u`` (microseconds), ``m`` (milliseconds), ``s``
(seconds), or no unit (milliseconds).

.. list-table::

    * - ``-t``
      - ``--timeout``
      - String
      - Optional
      - ``--timeout 5s``

The connection timeout. A higher timeout makes reconnecting slower.

.. list-table::

    * - ``-h``
      - ``--help``
      - Flag
      - Optional
      - ``--interval 1m``

Display the program's built in help.

Client
------

The client application runs on the receiving machine: the one which will run the
Looking Glass client.

Available Options
~~~~~~~~~~~~~~~~~

.. list-table::
    :header-rows: 1

    * - Short
      - Long
      - Type
      - Required
      - Example
    * - ``-s``
      - ``--server``
      - String
      - Required
      - ``--server 192.168.1.200``

The address of the server to connect to.

.. list-table::

    * - ``-p``
      - ``--port``
      - Integer
      - Required
      - ``--port 20000``

The port number of the server.

.. list-table::

    * - ``-n``
      - ``--nic``
      - String
      - Required
      - ``--nic 192.168.1.100``

The address of the network adapter to use for communication.

.. list-table::

    * - ``-m``
      - ``--mem``
      - String
      - Optional
      - ``--mem /dev/kvmfr0``

The address of the shared memory region to use. If not specified, defaults to
``/dev/shm/looking-glass``. Another Looking Glass host must not be using this
shared memory region!

.. list-table::

    * - ``-l``
      - ``--mem_size``
      - Integer + Unit
      - Optional
      - ``--mem_size 32M``

The size of the shared memory region. This parameter is only used to create the
shared memory region if it does not exist at runtime, and is ignored otherwise.
Without this parameter set, the program will exit if the memory region was not
created beforehand.

The supported units are (case-insensitive): ``M`` (MB), ``K`` (KB), ``G`` (GB),
or no unit (bytes).

.. list-table::

    * - ``-i``
      - ``--interval``
      - Integer + Unit
      - Optional
      - ``--interval 1m``

The interval between polling for updates. A lower polling interval decreases
latency at the expense of CPU usage. The polling interval should be faster than
the update rate of your mouse and monitor (ideally >2x). A 1000Hz mouse sends
updates every 1ms, so the polling interval should be 500 microseconds (``-i
500u``). By default, this is set to 0 (uses 100% of one CPU core).

You should consider using ``-i 0`` if you have the spare CPU available, since it
is the only way to avoid context switching, provides the highest frame time
consistency, and minimizes latency. This is also preferable to setting the
polling interval below 100 microseconds to avoid context switching overhead.

The supported units are: ``u`` (microseconds), ``m`` (milliseconds), ``s``
(seconds), or no unit (milliseconds).

.. list-table::

    * - ``-t``
      - ``--timeout``
      - String
      - Optional
      - ``--timeout 5s``

The connection timeout. A higher timeout makes reconnecting slower.

.. list-table::

    * - ``-h``
      - ``--help``
      - Flag
      - Optional
      - ``--interval 1m``

Display the program's built in help.

Logging
-------

The log level may be set individually for LGProxy and its supporting components,
TCM and Libfabric.

.. list-table::
    :header-rows: 1

    * - 
      - LGProxy
      - TCM
      - Libfabric
    * - Environment Variable
      - ``LP_LOG_LEVEL``
      - ``TCM_LOG_LEVEL``
      - ``FI_LOG_LEVEL``

To set all log levels at the same time, the environment variable
``LP_ALL_LOG_LEVEL`` may be set. This variable takes precedence over the other
set environment variables. Note that this will produce extremely verbose output,
especially at the trace level!

The available logging levels are (from least to most verbose):

.. list-table::
    :header-rows: 1

    * - Level
      - Value
      - LGProxy
      - TCM
      - Libfabric
    * - Disabled
      - ``off``
      - ✓
      - ✓
      - Default
    * - Fatal
      - ``fatal``
      - ✓
      - Default
      - 
    * - Error
      - ``error``
      - ✓
      - ✓
      -
    * - Warning
      - ``warn``
      - Default
      - ✓ 
      - ✓
    * - Info
      - ``info``
      - ✓
      - ✓
      - ✓
    * - Debug
      - ``debug``
      - ✓
      - ✓
      - ✓
    * - Trace
      - ``trace``
      - ✓
      - ✓
      - ~

Trace logs are only available for Libfabric if was compiled from source in debug
mode (i.e., not if you used the package manager or our instructions to install
it).

Once both sides are running and connected, you may start the Looking Glass
client application: :doc:`runlg`.