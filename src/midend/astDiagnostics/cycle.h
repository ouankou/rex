/*
 * Copyright (c) 2003, 2006 Matteo Frigo
 * Copyright (c) 2003, 2006 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

/* $Id: cycle.h,v 1.1 2008/01/08 02:56:38 dquinlan Exp $ */

/* machine-dependent cycle counters code. Needs to be inlined. */

/***************************************************************************/
/* To use the cycle counters in your code, simply #include "cycle.h" (this
   file), and then use the functions/macros:

                 ticks getticks(void);

   ticks is an opaque typedef defined below, representing the current time.
   You extract the elapsed time between two calls to gettick() via:

                 double elapsed(ticks t1, ticks t0);

   which returns a double-precision variable in arbitrary units.  You
   are not expected to convert this into human units like seconds; it
   is intended only for *comparisons* of time intervals.

   (In order to use some of the OS-dependent timer routines like
   gethrtime, you need to define the relevant HAVE_* macros via
   configure-time checks and include "rose_config.h" before cycle.h,
   or define the relevant macros manually.)
*/

/***************************************************************************/
/* This file uses macros like HAVE_GETHRTIME that are assumed to be
   defined according to whether the corresponding function/type/header
   is available on your system. */

/***************************************************************************/

// DQ (10/14/2010): I think this is not used anywhere.
#error                                                                         \
    "cycle.h is an example header file (of timers) that should not be used in ROSE source files."

#if TIME_WITH_SYS_TIME
#include <sys/time.h>
#include <time.h>
#else
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#else
#include <time.h>
#endif
#endif

#define INLINE_ELAPSED(INL)                                                    \
  static INL double elapsed(ticks t1, ticks t0) { return (double)(t1 - t0); }

/*----------------------------------------------------------------*/
/* gethrtime */
#if defined(HAVE_GETHRTIME) && defined(HAVE_HRTIME_T) &&                       \
    !defined(HAVE_TICK_COUNTER)
typedef hrtime_t ticks;

#define getticks gethrtime

INLINE_ELAPSED(inline)

#define HAVE_TICK_COUNTER
#endif

/*----------------------------------------------------------------*/
/*
 * PowerPC ``cycle'' counter using the time base register.
 */
#if defined(__GNUC__) && (defined(__powerpc__) || defined(__ppc__)) &&         \
    !defined(HAVE_TICK_COUNTER)
typedef unsigned long long ticks;

static __inline__ ticks getticks(void) {
  unsigned int tbl, tbu0, tbu1;

  do {
    __asm__ __volatile__("mftbu %0" : "=r"(tbu0));
    __asm__ __volatile__("mftb %0" : "=r"(tbl));
    __asm__ __volatile__("mftbu %0" : "=r"(tbu1));
  } while (tbu0 != tbu1);

  return (((unsigned long long)tbu0) << 32) | tbl;
}

INLINE_ELAPSED(__inline__)

#define HAVE_TICK_COUNTER
#endif

/*----------------------------------------------------------------*/
/*
 * Pentium cycle counter
 */
#if (defined(__GNUC__) || defined(__ICC)) && defined(__i386__) &&              \
    !defined(HAVE_TICK_COUNTER)
typedef unsigned long long ticks;

static __inline__ ticks getticks(void) {
  ticks ret;

  __asm__ __volatile__("rdtsc" : "=A"(ret));
  /* no input, nothing else clobbered */
  return ret;
}

INLINE_ELAPSED(__inline__)

#define HAVE_TICK_COUNTER
#define TIME_MIN 5000.0 /* unreliable pentium IV cycle counter */
#endif

/*----------------------------------------------------------------*/
/*
 * X86-64 cycle counter
 */
#if (defined(__GNUC__) || defined(__ICC)) && defined(__x86_64__) &&            \
    !defined(HAVE_TICK_COUNTER)
typedef unsigned long long ticks;

static __inline__ ticks getticks(void) {
  unsigned a, d;
  asm volatile("rdtsc" : "=a"(a), "=d"(d));
  return ((ticks)a) | (((ticks)d) << 32);
}

INLINE_ELAPSED(__inline__)

#define HAVE_TICK_COUNTER
#endif

/* PGI compiler, courtesy Cristiano Calonaci, Andrea Tarsi, & Roberto Gori.
   NOTE: this code will fail to link unless you use the -Masmkeyword compiler
   option (grrr). */
#if defined(__PGI) && defined(__x86_64__) && !defined(HAVE_TICK_COUNTER)
typedef unsigned long long ticks;
static ticks getticks(void) {
  asm(" rdtsc; shl    $0x20,%rdx; mov    %eax,%eax; or     %rdx,%rax;    ");
}
INLINE_ELAPSED(__inline__)
#define HAVE_TICK_COUNTER
#endif

/*----------------------------------------------------------------*/
/*
 * IA64 cycle counter
 */

/* intel's icc/ecc compiler */
#if defined(__ECC) && defined(__ia64__) && !defined(HAVE_TICK_COUNTER)
typedef unsigned long ticks;
#include <ia64intrin.h>

static __inline__ ticks getticks(void) { return __getReg(_IA64_REG_AR_ITC); }

INLINE_ELAPSED(__inline__)

#define HAVE_TICK_COUNTER
#endif

/* gcc */
#if defined(__GNUC__) && defined(__ia64__) && !defined(HAVE_TICK_COUNTER)
typedef unsigned long ticks;

static __inline__ ticks getticks(void) {
  ticks ret;

  __asm__ __volatile__("mov %0=ar.itc" : "=r"(ret));
  return ret;
}

INLINE_ELAPSED(__inline__)

#define HAVE_TICK_COUNTER
#endif

/*----------------------------------------------------------------*/
/* S390, courtesy of James Treacy */
#if defined(__GNUC__) && defined(__s390__) && !defined(HAVE_TICK_COUNTER)
typedef unsigned long long ticks;

static __inline__ ticks getticks(void) {
  ticks cycles;
  __asm__("stck 0(%0)" : : "a"(&(cycles)) : "memory", "cc");
  return cycles;
}

INLINE_ELAPSED(__inline__)

#define HAVE_TICK_COUNTER
#endif
/*----------------------------------------------------------------*/
#if defined(__GNUC__) && defined(__alpha__) && !defined(HAVE_TICK_COUNTER)
/*
 * The 32-bit cycle counter on alpha overflows pretty quickly,
 * unfortunately.  A 1GHz machine overflows in 4 seconds.
 */
typedef unsigned int ticks;

static __inline__ ticks getticks(void) {
  unsigned long cc;
  __asm__ __volatile__("rpcc %0" : "=r"(cc));
  return (cc & 0xFFFFFFFF);
}

INLINE_ELAPSED(__inline__)

#define HAVE_TICK_COUNTER
#endif

/*----------------------------------------------------------------*/
#if defined(__GNUC__) && defined(__sparc_v9__) && !defined(HAVE_TICK_COUNTER)
typedef unsigned long ticks;

static __inline__ ticks getticks(void) {
  ticks ret;
  __asm__ __volatile__("rd %%tick, %0" : "=r"(ret));
  return ret;
}

INLINE_ELAPSED(__inline__)

#define HAVE_TICK_COUNTER
#endif

/*----------------------------------------------------------------*/
#if (defined(__DECC) || defined(__DECCXX)) && defined(__alpha) &&              \
    defined(HAVE_C_ASM_H) && !defined(HAVE_TICK_COUNTER)
#include <c_asm.h>
typedef unsigned int ticks;

static __inline ticks getticks(void) {
  unsigned long cc;
  cc = asm("rpcc %v0");
  return (cc & 0xFFFFFFFF);
}

INLINE_ELAPSED(__inline)

#define HAVE_TICK_COUNTER
#endif
/*----------------------------------------------------------------*/
