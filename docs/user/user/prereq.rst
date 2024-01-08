.. _prereq:

Prerequisites
=============

The use of RDMA capable network adapters is strongly recommended. In case you
have fast NICs that are incapable of RDMA, the use of :ref:`software RDMA
<swrdma>` is strongly preferred over the fallback TCP transport.

In any case, **the locked memory limit must be increased**. The default limit is
extremely low (usually a few KB or MB). You can check the amount of locked
memory your security policy allows with the ulimit command as follows:

.. code-block:: 

    $ ulimit -l

    unlimited

If there is not enough locked memory, LGProxy may fail to start with the error
"cannot allocate memory", even though you may have enough memory on your system.

As a general rule, the locked memory limit should be about double that of your
shared memory region size. In some cases more or less is required; you will have
to test this on your own system, or remove the locked memory limit entirely.

Edit the file ``/etc/security/limits.conf``, and add the overrides for your user
or group to the bottom of the file. An example is below:

.. code-block::

    user soft memlock 1048576
    user hard memlock 1048576

Change ``user`` to your username (you can also use a group name), and
``1048576`` to a value that works on your system. You may also change it to
``unlimited``.