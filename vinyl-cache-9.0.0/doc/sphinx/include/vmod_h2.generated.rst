..
.. NB:  This file is machine generated, DO NOT EDIT!
..
.. Edit vmod_h2.vcc and run make instead
..


:tocdepth: 1


.. _vmod_h2(3):

========================================================
VMOD h2 - Module to control the built-in HTTP2 transport
========================================================

SYNOPSIS
========

.. parsed-literal::

  import h2 [as name] [from "path"]
  
  :ref:`h2.is()`
   
  :ref:`h2.rapid_reset()`
   
  :ref:`h2.rapid_reset_limit()`
   
  :ref:`h2.rapid_reset_period()`
   
  :ref:`h2.rapid_reset_budget()`
   
DESCRIPTION
===========

This VMOD contains functions to control the HTTP2 transport built into
Vinyl Cache.

.. _h2.is():

BOOL is()
---------



Restricted to: ``client``.

Returns true when called on a session handled by the built-in HTTP2 transport.

.. _h2.rapid_reset():

DURATION rapid_reset([DURATION threshold])
------------------------------------------



Restricted to: ``client``.

Get and optionally set the ``h2_rapid_reset`` parameter (See
:ref:`vinyld(1)`) for this HTTP2 session only.

Returns -1 when used outside the HTTP2 transport. Otherwise returns
the previous value.

If the call leads to a change in the rate limit parameters, the
current budget as retuned by
`h2.rapid_reset_budget()`_ is reset.

.. _h2.rapid_reset_limit():

INT rapid_reset_limit([INT number])
-----------------------------------



Restricted to: ``client``.

Get and optionally set the ``h2_rapid_reset_limit`` parameter (See
:ref:`vinyld(1)`) for this HTTP2 session only.

Returns -1 when used outside the HTTP2 transport. Otherwise returns
the previous value.

If the call leads to a change in the rate limit parameters, the
current budget as retuned by
`h2.rapid_reset_budget()`_ is reset.

.. _h2.rapid_reset_period():

DURATION rapid_reset_period([DURATION duration])
------------------------------------------------



Restricted to: ``client``.

Get and optionally set the ``h2_rapid_reset_period`` parameter (See
:ref:`vinyld(1)`) for this HTTP2 session only.

Returns -1 when used outside the HTTP2 transport. Otherwise returns
the previous value.

If the call leads to a change in the rate limit parameters, the
current budget as retuned by
`h2.rapid_reset_budget()`_ is reset.

.. _h2.rapid_reset_budget():

REAL rapid_reset_budget()
-------------------------



Restricted to: ``client``.

Return how many RST frames classified as "rapid" the client is still
allowed to send before the session is going to be closed.

SEE ALSO
========

* :ref:`vinyld(1)`
* :ref:`vsl(7)`

COPYRIGHT
=========

::

  Copyright 2023 UPLEX - Nils Goroll Systemoptimierung
  All rights reserved.
 
  Author: Nils Goroll <nils.goroll@uplex.de>
 
  SPDX-License-Identifier: BSD-2-Clause
 
  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:
  1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.
 
  THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
  OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
  OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
  SUCH DAMAGE.
