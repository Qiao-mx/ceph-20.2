.. _opt_n:

-n workdir

  Runtime directory for the shared memory, compiled VCLs etc.

  In performance critical applications, this directory should be on a RAM backed filesystem.

  When running multiple vinyld instances, separate directories need to be used.

  The default is taken from the ``VINYL_DEFAULT_N`` environment variable.

  Relative paths will be appended to ``/var/run``.

  If neither ``VINYL_DEFAULT_N`` nor ``-n`` are present, the value is ``/var/run/vinyld``.

  Note: These defaults may be distribution specific.

