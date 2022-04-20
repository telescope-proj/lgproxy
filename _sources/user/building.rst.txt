.. _building:

Building The Application
========================

.. note::
    Ensure all the dependencies are installed for your operating system. Head
    to :doc:`installation` for more details.

.. code-block:: bash

    git clone https://github.com/telescope-proj/lgproxy.git
    cd lgproxy
    cmake .
    make -j $(nproc)

The client application will be built into **sink_build** and the server side
application will be in **source_build**.

Once complete, go to :doc:`running`.