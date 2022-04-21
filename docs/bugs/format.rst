.. _format:

Bug Report Format
=================

.. warning::

    Do not submit Looking Glass issues that only occur when LGProxy is used to
    the Looking Glass GitHub.

If you have discovered a LGProxy bug, please rerun the program with the
following options prefixed: 

.. code-block:: text

    LP_LOG_LEVEL=1 TRF_LOG_LEVEL=1 FI_LOG_LEVEL=debug

Please go through this checklist before submitting bug reports:

- ☐ Have you followed the LibTRF Network Optimization Steps?
- ☐ Do the Looking Glass versions on both sides match?
- ☐ Do the LGProxy and LibTRF versions on both sides match?
- ☐ Is this bug not in the :doc:`knownbugs` section?

RDMA Transport Specific

- ☐ Have you increased the amount of locked memory available?
- ☐ Does running an ``ib_read_bw`` test pass on both sides?
- ☐ Do both sides support the same RDMA protocol?

Please cut and fill in the following information, either inline or by attaching
text files:

.. code-block:: text

    LGProxy Version:

    NIC Models:
    NIC Driver Types (in-kernel or proprietary):
    
    (RDMA Specific) Output of `ibstat` command below:


    Output of either the `ip a` or `ifconfig` command below:


    Source Application Log Dump:


    Sink Application Log Dump:


    Looking Glass Host Log:


    Looking Glass Client Log:

Then, please create an issue and describe the behaviour on our `GitHub page
<https://github.com/telescope-proj/lgproxy/issues>`_.