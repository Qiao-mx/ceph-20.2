..
.. NB:  This file is machine generated, DO NOT EDIT!
..
.. Edit vmod_math.vcc and run make instead
..


:tocdepth: 1


.. _vmod_math(3):

================================
VMOD math - VMOD wrapping math.h
================================

SYNOPSIS
========

.. parsed-literal::

  import math [as name] [from "path"]
  
  :ref:`math.approx()`
   
  :ref:`math.strfromd()`
   
  :ref:`math.constant()`
   
  :ref:`math.fpclass()`
   
  :ref:`math.fpclassify()`
   
  :ref:`math.isfinite()`
   
  :ref:`math.isgreater()`
   
  :ref:`math.isgreaterequal()`
   
  :ref:`math.isinf()`
   
  :ref:`math.isless()`
   
  :ref:`math.islessequal()`
   
  :ref:`math.islessgreater()`
   
  :ref:`math.isnan()`
   
  :ref:`math.isnormal()`
   
  :ref:`math.isunordered()`
   
  :ref:`math.signbit()`
   
  :ref:`math.acos()`
   
  :ref:`math.acosh()`
   
  :ref:`math.asin()`
   
  :ref:`math.asinh()`
   
  :ref:`math.atan()`
   
  :ref:`math.atan2()`
   
  :ref:`math.atanh()`
   
  :ref:`math.cbrt()`
   
  :ref:`math.ceil()`
   
  :ref:`math.copysign()`
   
  :ref:`math.cos()`
   
  :ref:`math.cosh()`
   
  :ref:`math.erf()`
   
  :ref:`math.erfc()`
   
  :ref:`math.exp()`
   
  :ref:`math.exp2()`
   
  :ref:`math.expm1()`
   
  :ref:`math.fabs()`
   
  :ref:`math.fdim()`
   
  :ref:`math.floor()`
   
  :ref:`math.fma()`
   
  :ref:`math.fmax()`
   
  :ref:`math.fmin()`
   
  :ref:`math.fmod()`
   
  :ref:`math.hypot()`
   
  :ref:`math.ilogb()`
   
  :ref:`math.j0()`
   
  :ref:`math.j1()`
   
  :ref:`math.jn()`
   
  :ref:`math.ldexp()`
   
  :ref:`math.lgamma()`
   
  :ref:`math.log()`
   
  :ref:`math.log10()`
   
  :ref:`math.log1p()`
   
  :ref:`math.log2()`
   
  :ref:`math.logb()`
   
  :ref:`math.lrint()`
   
  :ref:`math.lround()`
   
  :ref:`math.nan()`
   
  :ref:`math.nearbyint()`
   
  :ref:`math.nextafter()`
   
  :ref:`math.pow()`
   
  :ref:`math.remainder()`
   
  :ref:`math.rint()`
   
  :ref:`math.round()`
   
  :ref:`math.scalbln()`
   
  :ref:`math.scalbn()`
   
  :ref:`math.sin()`
   
  :ref:`math.sinh()`
   
  :ref:`math.sqrt()`
   
  :ref:`math.tan()`
   
  :ref:`math.tanh()`
   
  :ref:`math.tgamma()`
   
  :ref:`math.trunc()`
   
  :ref:`math.y0()`
   
  :ref:`math.y1()`
   
  :ref:`math.yn()`
   
.. THIS CODE IS AUTO-GENERATED. DO NOT EDIT HERE. see vmod_math_gen.py

DESCRIPTION
===========

This VMOD wraps the functions in `math.h(7)`, provides additional utilities and
includes functions to access macros and constants from `math.h(7)` and
`float.h(7)`.

Utility functions
-----------------

.. source is in vmod_math_util.c

.. _math.approx():

BOOL approx(REAL a, REAL b, REAL maxDiff, REAL maxRelDiff)
----------------------------------------------------------

::

   BOOL approx(REAL a, REAL b, REAL maxDiff=0.0, REAL maxRelDiff=0.0)

.. _`Comparing Floating Point Numbers, 2012 Edition`: https://randomascii.wordpress.com/2012/02/25/comparing-floating-point-numbers-2012-edition/

Return true if the two numbers *a* and *b* are approximately equal per the
``AlmostEqualRelativeAndAbs`` function from `Comparing Floating Point Numbers,
2012 Edition`_ given the additional parameters *maxDiff* and *maxRelDiff*. Read
the blog post for details on why the seemingly simple question of two floating
point numbers being equal has no simple answers.

For their default values of 0, the two additional parameters are initialized to:

- *maxDiff*: ``4 * DBL_EPSILON``
- *maxRelDiff*: ``DBL_EPSILON``

.. _math.strfromd():

STRING strfromd(STRING format, REAL fp)
---------------------------------------

Convert the value *fp* into a string using *format*, see `strfromd(3)`.

An invalid format string results in a VCL error and the ``NULL`` invalid string
returned.

.. Internally, vsnprintf() is called via WS_Printf(). The difference to
   VRT_REAL_string() is the free format and missing call to VRT_REAL_is_valid()

Access to macros and constants
------------------------------

.. _math.constant():

REAL constant(ENUM name)
------------------------

::

   REAL constant(
      ENUM {DBL_MANT_DIG, DBL_DIG, DBL_MIN_EXP, DBL_MIN_10_EXP, DBL_MAX_EXP, DBL_MAX_10_EXP, DBL_MAX, DBL_EPSILON, DBL_MIN, HUGE_VAL, M_E, M_LOG2E, M_LOG10E, M_LN2, M_LN10, M_PI, M_PI_2, M_PI_4, M_1_PI, M_2_PI, M_2_SQRTPI, M_SQRT2, M_SQRT1_2} name
   )

Return the value of the named constant. For ``DBL_*`` see `float.h(7)`,
otherwise `math.h(7)` for details.

.. _math.fpclass():

INT fpclass(ENUM name)
----------------------

::

   INT fpclass(
      ENUM {FP_INFINITE, FP_NAN, FP_NORMAL, FP_SUBNORMAL, FP_ZERO} name
   )

Return the value of the named constant for comparisons of `math.fpclassify()`_
return values.




math.h functions
----------------

The semantics of functions mapping directly to `math.h(7)` are not documented
herein, see the system documentation instead (for example using ``man
<function>``).

.. _math.fpclassify():

INT fpclassify(REAL x)
----------------------



.. _math.isfinite():

INT isfinite(REAL x)
--------------------



.. _math.isgreater():

INT isgreater(REAL x, REAL y)
-----------------------------



.. _math.isgreaterequal():

INT isgreaterequal(REAL x, REAL y)
----------------------------------



.. _math.isinf():

INT isinf(REAL x)
-----------------



.. _math.isless():

INT isless(REAL x, REAL y)
--------------------------



.. _math.islessequal():

INT islessequal(REAL x, REAL y)
-------------------------------



.. _math.islessgreater():

INT islessgreater(REAL x, REAL y)
---------------------------------



.. _math.isnan():

INT isnan(REAL x)
-----------------



.. _math.isnormal():

INT isnormal(REAL x)
--------------------



.. _math.isunordered():

INT isunordered(REAL x, REAL y)
-------------------------------



.. _math.signbit():

INT signbit(REAL x)
-------------------



.. _math.acos():

REAL acos(REAL x)
-----------------



.. _math.acosh():

REAL acosh(REAL x)
------------------



.. _math.asin():

REAL asin(REAL x)
-----------------



.. _math.asinh():

REAL asinh(REAL x)
------------------



.. _math.atan():

REAL atan(REAL x)
-----------------



.. _math.atan2():

REAL atan2(REAL y, REAL x)
--------------------------



.. _math.atanh():

REAL atanh(REAL x)
------------------



.. _math.cbrt():

REAL cbrt(REAL x)
-----------------



.. _math.ceil():

REAL ceil(REAL x)
-----------------



.. _math.copysign():

REAL copysign(REAL x, REAL y)
-----------------------------



.. _math.cos():

REAL cos(REAL x)
----------------



.. _math.cosh():

REAL cosh(REAL x)
-----------------



.. _math.erf():

REAL erf(REAL x)
----------------



.. _math.erfc():

REAL erfc(REAL x)
-----------------



.. _math.exp():

REAL exp(REAL x)
----------------



.. _math.exp2():

REAL exp2(REAL x)
-----------------



.. _math.expm1():

REAL expm1(REAL x)
------------------



.. _math.fabs():

REAL fabs(REAL x)
-----------------



.. _math.fdim():

REAL fdim(REAL x, REAL y)
-------------------------



.. _math.floor():

REAL floor(REAL x)
------------------



.. _math.fma():

REAL fma(REAL x, REAL y, REAL z)
--------------------------------



.. _math.fmax():

REAL fmax(REAL x, REAL y)
-------------------------



.. _math.fmin():

REAL fmin(REAL x, REAL y)
-------------------------



.. _math.fmod():

REAL fmod(REAL x, REAL y)
-------------------------



.. _math.hypot():

REAL hypot(REAL x, REAL y)
--------------------------



.. _math.ilogb():

INT ilogb(REAL x)
-----------------



.. _math.j0():

REAL j0(REAL x)
---------------



.. _math.j1():

REAL j1(REAL x)
---------------



.. _math.jn():

REAL jn(INT x, REAL y)
----------------------



.. _math.ldexp():

REAL ldexp(REAL x, INT e)
-------------------------



.. _math.lgamma():

REAL lgamma(REAL x)
-------------------



.. _math.log():

REAL log(REAL x)
----------------



.. _math.log10():

REAL log10(REAL x)
------------------



.. _math.log1p():

REAL log1p(REAL x)
------------------



.. _math.log2():

REAL log2(REAL x)
-----------------



.. _math.logb():

REAL logb(REAL x)
-----------------



.. _math.lrint():

INT lrint(REAL x)
-----------------



.. _math.lround():

INT lround(REAL x)
------------------



.. _math.nan():

REAL nan(STRING tag)
--------------------



.. _math.nearbyint():

REAL nearbyint(REAL x)
----------------------



.. _math.nextafter():

REAL nextafter(REAL x, REAL y)
------------------------------



.. _math.pow():

REAL pow(REAL x, REAL y)
------------------------



.. _math.remainder():

REAL remainder(REAL x, REAL y)
------------------------------



.. _math.rint():

REAL rint(REAL x)
-----------------



.. _math.round():

REAL round(REAL x)
------------------



.. _math.scalbln():

REAL scalbln(REAL x, INT e)
---------------------------



.. _math.scalbn():

REAL scalbn(REAL x, INT y)
--------------------------



.. _math.sin():

REAL sin(REAL x)
----------------



.. _math.sinh():

REAL sinh(REAL x)
-----------------



.. _math.sqrt():

REAL sqrt(REAL x)
-----------------



.. _math.tan():

REAL tan(REAL x)
----------------



.. _math.tanh():

REAL tanh(REAL x)
-----------------



.. _math.tgamma():

REAL tgamma(REAL x)
-------------------



.. _math.trunc():

REAL trunc(REAL x)
------------------



.. _math.y0():

REAL y0(REAL x)
---------------



.. _math.y1():

REAL y1(REAL x)
---------------



.. _math.yn():

REAL yn(INT n, REAL x)
----------------------



COPYRIGHT
=========

::

  This document is licensed under the same conditions as Vinyl Cache itself.
  See LICENSE for details.
 
  SPDX-License-Identifier: BSD-2-Clause
 
  Author: Nils Goroll <nils.goroll@uplex.de>
