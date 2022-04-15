.. _building:

Building the application
========================

.. note::
    Ensure all the dependencies are installed for your operating system.
    Look at :doc:`installation` for more details.


.. code-block:: bash

    git clone https://github.com/telescope-proj/lgproxy.git
    cd lgproxy
    cmake . && make -j $(nproc)

The client application will be built into **sink_build** and the server side application will be in **source_build**. 
After that you can copy your souce binary over to your host.

