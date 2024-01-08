.. _faq:

Frequently Asked Questions
==========================

For whom is LGProxy intended / Why LGProxy?
-------------------------------------------

LGProxy is intended as a proof of concept for uncompressed video over local
networks. Users who understand the benefits and limitations of Looking Glass
over using traditional solutions such as SPICE, RDP, and GameStream can use
LGProxy to realize these same benefits over an adequately fast network
connection.

We'd like to stress that LGProxy is not a good fit for regular users in most
cases. It can be considered as an alternative to wiring fixed-function, costly,
point-to-point bundled HDMI, USB, and Thunderbolt cables over long distances.
Instead, existing optical or copper Ethernet installations can be used to bring
uncompressed remote desktop connections from arbitrary network connection points
in the home or office to another: from the living room to the bedroom; from the
server room to the office, with an experience indistinguishable from sitting in
front of the machine.

We decided to use Looking Glass to realize this concept because it already has a
strong focus on performance and latency, and places uncompressed frames directly
in system memory, which is simple for our RDMA subsystem to send over the
network.

If you are interested in the technical details or contributing to LGProxy, you
can refer to our technical documentation for more information.

Does LGProxy work on slower networks?
-------------------------------------

Some users have tried running LGProxy over slow gigabit networks with TCP.
Please note that this is not a supported configuration, and it is physically
impossible to get good performance over these kinds of networks with
uncompressed video: on a gigabit network, expect about 15 FPS at 1080p.

For these configurations, please use applications like Sunshine/Moonlight or
Parsec instead. They may have compression artifacts, but this is unavoidable
when sending video over low-bandwidth networks.

What does "Invalid KVMFR Version" mean?
---------------------------------------

LGProxy communicates with Looking Glass through the Looking Glass KVMFR
protocol. This protocol changes from time to time, and so your version of
LGProxy and/or Looking Glass might not be updated to a version where both KVMFR
versions match.

To get the supported KVMFR version and Looking Glass version that your
installation of LGProxy was built against, simply run ``./client --help`` or
``./server --help``. The top lines will display relevant information.

::

       Feature Support       Git
               Version       Version
                     |       |
        LGProxy v0.3.0-ec6635b (client)
        TCM 0.4.6; Looking Glass B6-226-g8cd002f1; KVMFR 20
                |                               |         |
        Telescope    The version of Looking Glass         The underlying
       Connection        your LGProxy version was         Looking Glass
          Manager       built against, and should         protocol version
          Version                         support

Currently, LGProxy does not have a stable API. All of these values should
therefore match on both sides of the connection.

Why is RDMA so important for LGProxy?
-------------------------------------

RDMA works in a fundamentally different way compared to TCP. All data transfers,
checksums, etc. are handled in hardware and userspace, which provides a 10-100x
reduction in latency and 5-10x increase in throughput with zero CPU usage.
Because Looking Glass, and by extension LGProxy, transmits uncompressed frames
at several gigabits per second, the OS TCP stack and/or CPU can simply become
overwhelmed by the amount of data movement. RDMA easily keeps up with this
traffic with nearly no CPU involvement. The high CPU usage you might see when
using LGProxy with RDMA is not actually due to data transfer overhead, rather
checking that data transfers have actually completed.

Can you add feature X?
----------------------

Feedback and feature requests are always appreciated. However, whether they can be
added depends on the complexity of implementation. Please note that there is
currently only one maintainer for LGProxy, and it is a free software product;
not all requested features will be considered or added.

Can LGProxy run on bare metal?
------------------------------

While possible, there are currently no plans for LGProxy to run on bare metal.
The time investment needed for this would be quite large. However, it is
certainly possible, and contributions that add such features are welcome.

The major limiting factors are the poor support and documentation in Windows for
RDMA libraries, as well as the heavy modifications that would have to be made to
Looking Glass and LGProxy to operate on Windows as well as use RDMA instead of
shared memory as a frame transport medium.

Do you have a Discord, Mastodon, etc. for this project?
-------------------------------------------------------

This implies moderation effort - something that I am not keen on spending time
on. If the user count is higher, and moderators are willing to moderate, this
might be reconsidered in the future.

Bug reports and feature requests can be conducted on the GitHub repository for
LGProxy. An additional benefit is that such discussions are public, organized,
easily searchable, and show clear status information unlike other platforms.