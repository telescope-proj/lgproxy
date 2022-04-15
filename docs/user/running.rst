.. _running:

Running the Application
=======================

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
    This is currently a very early release of this software so when running the Looking Glass client 
    you will need to add the **-c** option to specify which host to connect to for your spice server,
    the **spice:captureOnStart=yes** option also needs to be set for mouse input to work properly.

The **sink** side is the side where your Looking Glass client will be running on. The hostname or IP address specified is the host machine
running the **source** application. By default only the hostname and port need to be spcified when running the application. The default shm
file is **/dev/shm/looking-glass** which is the same as what Looking-Glass. The shared file memory size does not need to be specified. 
The **-d** flag will delete the shm file when the application has exited to free up memory. The poll interval specifies RDMA polling interval
by defaul the polling interval will be set to 1ms but it could be set to 0 which will increase performance CPU usage will be much higher.

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

The **source** application will be running on the host machine where Looking Glass is running. Only the hostname and port need to be specified.
The hostname should be set to `0.0.0.0` if you want to accept incoming connections from all addresses. The port is the port where it will listen
for incoming connections. The shared memory file does not need to be specified it will default to **/dev/shm/looking-glass**. 