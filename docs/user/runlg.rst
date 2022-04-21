.. _runlg:

Running Looking Glass
=====================

You can run Looking Glass with the usual options; however, you will need to add
the ``-c`` and ``spice:captureOnStart=yes`` options to allow keyboard, mouse,
and audio support to work properly. If you have not configured your VM's SPICE
server to listen on external interfaces, you must do so as well. On the host:

.. image:: spice.png

.. warning::

    This exposes your VM SPICE channel at all times. A future update will open
    the SPICE channel on demand, but it has not currently been implemented.

Example:

.. code-block:: bash

    ./looking-glass-client -c 172.22.0.1 spice:captureOnStart=yes -f /dev/kvmfr0