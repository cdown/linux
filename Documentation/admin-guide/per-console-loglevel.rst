.. SPDX-License-Identifier: GPL-2.0

.. _per_console_loglevel:

Per-console loglevel support
============================

Motivation
----------

Consoles can have vastly different latencies and throughputs. For example,
writing a message to the serial console can take on the order of tens of
milliseconds to get the UART to successfully write a message. While this might
be fine for a single, one-off message, this can cause signiifcant
application-level stalls in situations where the kernel writes large amounts of
information to the console.

This means that while you might want to send at least INFO level messages to
(for example) netconsole, which is relatively fast, you may only want to send
at least WARN level messages to the serial console. This permits debugging
using the serial console in cases that netconsole doesn't receive messages
during particularly bad system issues, while still keeping the noise low enough
to avoid inducing latency in userspace applications.

Tunables
--------

In order to allow tuning this, the following controls exist:

Global
~~~~~~

In order of authority:

* ``ignore_loglevel`` on the kernel command line: Emit all messages. Cannot be
  disabled without restarting the kernel. All other controls are ignored if
  this is present.
* ``loglevel=...`` on the kernel command line: Equivalent to sysctl
  ``kernel.default_console_loglevel``.
* ``kernel.minimum_console_loglevel`` sysctl: Clamp all consoles to emit
  messages beyond this loglevel.
* ``kernel.force_console_loglevel`` sysctl: Force all consoles to the given
  loglevel. If this value is lower than ``kernel.minimum_console_loglevel``,
  ``kernel.minimum_console_loglevel`` is respected. Can also be set to the
  special value "unset" which removes any existing forced level.
* ``kernel.default_console_loglevel`` sysctl: The default console loglevel if
  there is no local loglevel for the console, and
  ``kernel.force_console_loglevel`` is unset. If this value is lower than
  ``kernel.minimum_console_loglevel``, ``kernel.minimum_console_loglevel`` is
  respected.
* ``kernel.default_console_loglevel`` sysctl: The default console loglevel if
  there is no local loglevel for the console, and
  ``kernel.force_console_loglevel`` is unset. If this value is lower than
  ``kernel.minimum_console_loglevel``, ``kernel.minimum_console_loglevel`` is
  forced.
* ``kernel.default_message_loglevel`` sysctl: The default loglevel to send
  messages at if they are sent with no explicit loglevel.

The default value for ``kernel.default_console_loglevel`` comes from
``CONFIG_CONSOLE_LOGLEVEL_DEFAULT``, or ``CONFIG_CONSOLE_LOGLEVEL_QUIET`` if
``quiet`` is passed on the kernel command line.

Console attributes
~~~~~~~~~~~~~~~~~~

Registered consoles are exposed at ``/sys/class/console``. For example, if you
are using ``ttyS0``, the console backing it can be viewed at
``/sys/class/console/ttyS/``. The following files are available:

* ``effective_loglevel`` (r): The effective loglevel after considering all
  loglevel authorities. For example, if the local loglevel is 3, but the global
  minimum console loglevel is 5, then the value will be 5.
* ``effective_loglevel_source`` (r): The loglevel authority which resulted in
  the effective loglevel being set. The following values can be present:

    * ``local``: The console-specific loglevel is in effect.
    * ``global``: The global default loglevel
      (``kernel.default_console_loglevel``) is in effect. Set a console-specific
      loglevel to override it.
    * ``forced``: The global forced loglevel (``kernel.force_console_loglevel``)
      is in effect. Write "unset" to ``kernel.force_console_loglevel`` to disable
      it.
    * ``minimum``: The global minimum loglevel
      (``kernel.minimum_console_loglevel``) is in effect. Set a higher
      console-specific loglevel to override it.
    * ``forced_minimum``: The global minimum loglevel
      (``kernel.minimum_console_loglevel``) is in effect. Even if the local
      console-specific loglevel is higher, it is in effect because the global
      forced loglevel (``kernel.force_console_loglevel``) is present, but is
      below ``kernel.minimum_console_loglevel``. Write "unset" to
      ``kernel.force_console_loglevel`` to disable the forcing, and make sure
      ``kernel.minimum_console_loglevel`` is below the local console loglevel
      if you want the per-console loglevel to take effect.
      console-specific loglevel to override it.
    * ``ignore_loglevel``: ``ignore_loglevel`` was specified on the kernel
      command line. Restart without it to use other controls.

* ``enabled`` (r): Whether the console is enabled.
* ``loglevel`` (rw): The local loglevel for this console. This will be in
  effect if no other global control overrides it. Look at
  ``effective_loglevel`` and ``effective_loglevel_source`` to verify that.

Deprecated
~~~~~~~~~~

* ``syslog(SYSLOG_ACTION_CONSOLE_*)``: This sets
  ``kernel.force_console_loglevel``. It is unaware of per-console loglevel
  semantics and is not recommended. A warning will be emitted if it is used
  while local loglevels are in effect.
* ``kernel.printk`` sysctl: this takes four values, setting
  ``kernel.force_console_loglevel``, ``kernel.default_message_loglevel``,
  ``kernel.minimum_console_loglevel``, and ``kernel.default_console_loglevel``
  respectively. The interface is generally considered to quite confusing, and
  is unaware of per-console loglevel semantics.

Chris Down <chris@chrisdown.name>, 17-May-2020
