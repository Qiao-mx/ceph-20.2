.. _ref_cli_auth:

auth <response>
~~~~~~~~~~~~~~~
  Authenticate.

.. _ref_cli_backend_list:

backend.list [-j] [-p] [<backend_pattern>]
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  List backends.


  ``-p`` also shows probe status.

  ``-j`` specifies JSON output.

  Unless ``-j`` is specified for JSON output,  the output format is five columns of dynamic width,  separated by white space with the fields:

  * Backend name

  * Admin: How health state is determined:

    * ``healthy``: Set ``healthy`` through ``backend.set_health``.

    * ``sick``: Set ``sick`` through ``backend.set_health``.

    * ``probe``: Health state determined by a probe or some other
      dynamic mechanism.

    * ``deleted``: Backend has been deleted, but not yet cleaned
      up.

    Admin has precedence over Health

  * Probe ``X/Y``: *X* out of *Y* checks have succeeded

    *X* and *Y* are backend specific and may represent probe checks,
    other backends or any other metric.

    If there is no probe or the director does not provide details on
    probe check results, ``0/0`` is output.

  * Health: Probe health state

    * ``healthy``

    * ``sick``

    If there is no probe, ``healthy`` is output.
  * Last change: Timestamp when the health state last changed.

  The health state reported here is generic. A backend's health may also depend on the context it is being used in (e.g. the object's hash), so the actual health state as visible from VCL (e.g. using ``std.healthy()``) may differ.

  For ``-j``, the object members should be self explanatory,
  matching the fields described above. ``probe_message`` has the
  format ``[X, Y, "state"]`` as described above for Probe. JSON
  Probe details (``-j -p`` arguments) are director specific.

.. _ref_cli_backend_set_health:

backend.set_health <backend_pattern> [auto|healthy|sick]
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Set health status of backend(s) matching <backend_pattern>.

  * With ``auto``, the health status is determined by a probe
    or some other dynamic mechanism, if any
  * ``healthy`` sets the backend as usable
  * ``sick`` sets the backend as unusable


.. _ref_cli_ban:

ban <field> <operator> <arg> [&& <field> <oper> <arg> ...]
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Mark obsolete all objects where all the conditions match.

  See :ref:`std.ban()` for details

.. _ref_cli_ban_list:

ban.list [-j]
~~~~~~~~~~~~~
  List the active bans.

  Unless ``-j`` is specified for JSON output,  the output format is:

  * Time the ban was issued.

  * Objects referencing this ban.

  * ``C`` if ban is completed = no further testing against it.

  * if ``lurker`` debugging is enabled:

    * ``R`` for req.* tests

    * ``O`` for obj.* tests

    * Pointer to ban object

  * Ban specification

  Durations of ban specifications get normalized, for example "7d" gets changed into "1w".

.. _ref_cli_banner:

banner
~~~~~~
  Print welcome banner.

.. _ref_cli_help:

help [-j|<command>]
~~~~~~~~~~~~~~~~~~~
  Show command/protocol help.

  ``-j`` specifies JSON output.

.. _ref_cli_panic_clear:

panic.clear [-z]
~~~~~~~~~~~~~~~~
  Clear the last panic, if any, -z will clear related vinylstat counter(s)

.. _ref_cli_panic_show:

panic.show [-j]
~~~~~~~~~~~~~~~
  Return the last panic, if any.

  ``-j`` specifies JSON output -- the panic message is returned as an unstructured JSON string.

.. _ref_cli_param_reset:

param.reset [-j] <param>
~~~~~~~~~~~~~~~~~~~~~~~~
  Reset parameter to default value.

  The JSON output is the same as ``param.show -j <param>`` and contains the updated value as it would be represented by a subsequent execution of ``param.show``.



.. _ref_cli_param_set:

param.set [-j] <param> <value>
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Set parameter value.

  The JSON output is the same as ``param.show -j <param>`` and contains the updated value as it would be represented by a subsequent execution of ``param.show``.

  This can be useful to later verify that a parameter value didn't change and to use the value from the JSON output to reset the parameter to the desired value.

.. _ref_cli_param_show:

param.show [-l|-j] [<param>|changed]
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Show parameters and their values.

  The long form with ``-l`` shows additional information, including documentation and minimum, maximum and default values, if defined for the parameter. JSON output is specified with ``-j``, in which the information for the long form is included; only one of ``-l`` or ``-j`` is permitted. If a parameter is specified with ``<param>``, show only that parameter. If ``changed`` is specified, show only those parameters whose values differ from their defaults.

.. _ref_cli_pid:

pid [-j]
~~~~~~~~
  Show the pid of the master process, and the worker if it's running.

  ``-j`` specifies JSON output.

.. _ref_cli_ping:

ping [-j] [<timestamp>]
~~~~~~~~~~~~~~~~~~~~~~~
  Keep connection alive.

  The response is formatted as JSON if ``-j`` is specified.

.. _ref_cli_quit:

quit
~~~~
  Close connection.

.. _ref_cli_start:

start
~~~~~
  Start the Vinyl Cache process.

.. _ref_cli_status:

status [-j]
~~~~~~~~~~~
  Check status of Vinyl Cache process.

  ``-j`` specifies JSON output.

.. _ref_cli_stop:

stop
~~~~
  Stop the Vinyl Cache process.

.. _ref_cli_storage_list:

storage.list [-j]
~~~~~~~~~~~~~~~~~
  List storage devices.

  ``-j`` specifies JSON output.

.. _ref_cli_vcl_deps:

vcl.deps [-j]
~~~~~~~~~~~~~
  List all loaded configuration and their dependencies.

  Unless ``-j`` is specified for JSON output, the output format is up to two columns of dynamic width separated by white space with the fields:

  * VCL: a VCL program

  * Dependency: another VCL program it depends on

  Only direct dependencies are listed, and VCLs with multiple dependencies are listed multiple times.

.. _ref_cli_vcl_discard:

vcl.discard <name_pattern>...
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Unload the named configurations (when possible).

  Unload the named configurations and labels matching at least one name pattern. All matching configurations and labels are discarded in the correct order with respect to potential dependencies. If one configuration or label could not be discarded because one of its dependencies would remain, nothing is discarded. Each individual name pattern must match at least one named configuration or label.

.. _ref_cli_vcl_inline:

vcl.inline <configname> <quoted_VCLstring> [auto|cold|warm]
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Compile and load the VCL data under the name provided.

  Multi-line VCL can be input using the here document :ref:`ref_syntax`.

.. _ref_cli_vcl_label:

vcl.label <label> <configname>
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Apply label to configuration.

  A VCL label is like a UNIX symbolic link,  a name without substance, which points to another VCL.

  Labels are mandatory whenever one VCL references another.

.. _ref_cli_vcl_list:

vcl.list [-j]
~~~~~~~~~~~~~
  List all loaded configuration.

  Unless ``-j`` is specified for JSON output,  the output format is five or seven columns of dynamic width,  separated by white space with the fields:

  * status: active, available or discarded

  * state: label, cold, warm, or auto

  * temperature: init, cold, warm, busy or cooling

  * busy: number of references to this vcl (integer)

  * name: the name given to this vcl or label

  * [ ``<-`` | ``->`` ] and label info last two fields)

    * ``->`` <vcl> : label "points to" the named <vcl>

    * ``<-`` (<n> label[s]): the vcl has <n> label(s)



.. _ref_cli_vcl_load:

vcl.load <configname> <filename> [auto|cold|warm]
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Compile and load the VCL file under the name provided.

.. _ref_cli_vcl_show:

vcl.show [-v] [<configname>]
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Display the source code for the specified configuration.

.. _ref_cli_vcl_state:

vcl.state <configname> auto|cold|warm
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    Force the state of the named configuration.

.. _ref_cli_vcl_symtab:

vcl.symtab
~~~~~~~~~~
  Dump the VCL symbol-tables.

.. _ref_cli_vcl_use:

vcl.use <configname|label>
~~~~~~~~~~~~~~~~~~~~~~~~~~
  Switch to the named configuration immediately.

