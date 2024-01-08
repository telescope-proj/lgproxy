.. _knownbugs:

Known Bugs
==========

- When the client connects and there has been no motion on the screen, LGProxy
  will sometimes fail to display the display contents unless there is a display
  update.
- When ``spice:captureOnStart=yes`` is not specified, the Looking Glass client
  may sometimes fail to display the mouse cursor.
- Use of FSR with raw texture formats sometimes fails when in DMABUF import
  mode.