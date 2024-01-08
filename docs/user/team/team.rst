.. _team:

The Telescope Project was our undergraduate research project exploring a novel
use for RDMA transports. It was developed by the Telescope Project team.

Telescope Project Team
======================

**Tim Dettmar** `beanfacts <https://github.com/beanfacts>`_

Project Leader

Created and maintains the entire Telescope Project, previously responsible for
the RDMA communication subsystem.

**Matthew McMullin** `matthewjmc <https://github.com/matthewjmc>`_

Developer of LGProxy

Developed LGProxy, a high-performance remote desktop proxy designed for
the Looking Glass application. LGProxy translates Looking Glass calls into
network operations via the Telescope RDMA communication layer.

**Vorachat Somsuay** `ailucky <https://github.com/ailucky>`_

Looking Glass Integration

Integrated support for compressed texture formats directly into the
Looking Glass client application, as a proof of concept of RDMA video transfer
over lower-bandwidth networks.

**Kittidech Ditsuwan** `poiler22 <https://github.com/poiler22>`_

Looking Glass Integration

Created interfaces between Looking Glass and texture manipulation subroutines,
such as channel swapping and texture compression, for bandwidth reduction
purposes.

Acknowledgements
----------------

During our research, we were provided access to the Apex HPC Cluster for running
and testing the limits of Telescope's RDMA capabilities. We would like to thank
Dr. Akkarit Sangpetch for mentoring and giving us access to the cluster, which
made our project possible.

We would also like to thank the Looking Glass development team for explaining
the details of their frame transfer system, as well as providing the software as
an open-source project. LGProxy would not be possible without it.

Finally, thank you, the users, for providing support and feedback for the
project. Your input makes LGProxy a better project.