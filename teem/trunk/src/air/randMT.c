/*
  Teem: Tools to process and visualize scientific data and images
  Copyright (C) 2005  Gordon Kindlmann
  Copyright (C) 2004, 2003, 2002, 2001, 2000, 1999, 1998  University of Utah

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public License
  (LGPL) as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.
  The terms of redistributing and/or modifying this software also
  include exceptions to the LGPL that facilitate static linking.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
/*
  This file is a modified version of the MersenneTwister.h source file
  written by Richard J. Wagner.  The original copyright notice follows.

  Mersenne Twister random number generator -- a C++ class MTRand
  Based on code by Makoto Matsumoto, Takuji Nishimura, and Shawn Cokus
  Richard J. Wagner  v1.0  15 May 2003  rjwagner@writeme.com

  The Mersenne Twister is an algorithm for generating random numbers.  It
  was designed with consideration of the flaws in various other generators.
  The period, 2^19937-1, and the order of equidistribution, 623 dimensions,
  are far greater.  The generator is also fast; it avoids multiplication and
  division, and it benefits from caches and pipelines.  For more information
  see the inventors' web page at http://www.math.keio.ac.jp/~matumoto/emt.html

  Reference
  M. Matsumoto and T. Nishimura, "Mersenne Twister: A 623-Dimensionally
  Equidistributed Uniform Pseudo-Random Number Generator", ACM Transactions on
  Modeling and Computer Simulation, Vol. 8, No. 1, January 1998, pp 3-30.

  Copyright (C) 1997 - 2002, Makoto Matsumoto and Takuji Nishimura,
  Copyright (C) 2000 - 2003, Richard J. Wagner
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

  1. Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

  3. The names of its contributors may not be used to endorse or promote
  products derived from this software without specific prior written
  permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
  FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.

  The original code included the following notice:

  When you use this, send an email to: matumoto@math.keio.ac.jp
  with an appropriate reference to your work.

  It would be nice to CC: rjwagner@writeme.com and Cokus@math.washington.edu
  when you write.
*/

#include "air.h"
#include "privateAir.h"

#define AIR_RANDMT_M 397
#define AIR_RANDMT_DEFAULT_SEED 42

/* Inlined class member functions that I made macros */
#define HIBIT( u ) ((u) & 0x80000000U)
#define LOBIT( u ) ((u) & 0x00000001U)
#define LOBITS( u ) ((u) & 0x7fffffffU)
#define MIXBITS( u, v ) (HIBIT(u) | LOBITS(v))
#define TWIST( m, s0, s1 ) \
  ((m) ^ (MIXBITS(s0,s1)>>1) ^ (-LOBIT(s1) & 0x9908b0dfUL))

/* It would be annoying to initialize _airRandMTStateGlobal at
   compile-time, partly because pNext is the address of member field
   (currently set by _airRandMTReload), and partly because we'd have
   to explicitly write AIR_RANDMT_N == 624 values in the source code,
   which would only make things opaque.  So, we use the flag
   _airRandMTStateGlobal_initialized to initialize the global state on
   its first use.  Users paranoid about the time to do this
   initialization check, or who want to ensure thread safety, should
   be using airRandMTStateNew and airDrandMT_r */

airRandMTState _airRandMTStateGlobal;
airRandMTState *airRandMTStateGlobal = &_airRandMTStateGlobal;
static int _airRandMTStateGlobal_initialized = AIR_FALSE;

static void
_airRandMTInitialize(airRandMTState *rng, unsigned int seed) {
  /* Initialize generator state with seed See Knuth TAOCP Vol 2, 3rd
   Ed, p.106 for multiplier.  In previous versions, most significant
   bits (MSBs) of the seed affect only MSBs of the state array.
   Modified 9 Jan 2002 by Makoto Matsumoto.
  */
  register unsigned int *s = rng->state;
  register unsigned int *r = rng->state;
  register int i = 1;
  *s++ = seed & 0xffffffffUL;
  for( ; i < AIR_RANDMT_N; ++i ) {
    *s++ = ( 1812433253UL * ( *r ^ (*r >> 30) ) + i ) & 0xffffffffUL;
    r++;
  }
}

static void
_airRandMTReload(airRandMTState *rng) {
  /* Generate N new values in state.  Made clearer and faster by
     Matthew Bellew (matthew.bellew@home.com) */
  register int i;
  register unsigned int *p = rng->state;
  for (i=AIR_RANDMT_N - AIR_RANDMT_M; i--; ++p) {
    *p = TWIST( p[AIR_RANDMT_M], p[0], p[1] );
  }
  for (i=AIR_RANDMT_M; --i; ++p) {
    *p = TWIST( p[AIR_RANDMT_M-AIR_RANDMT_N], p[0], p[1] );
  }
  *p = TWIST( p[AIR_RANDMT_M-AIR_RANDMT_N], p[0], rng->state[0] );

  rng->left = AIR_RANDMT_N;
  rng->pNext = rng->state;
}

airRandMTState *
airRandMTStateNew(unsigned int seed) {
  airRandMTState* ret;

  ret = AIR_CAST(airRandMTState*, malloc(sizeof(airRandMTState)));
  airSrandMT_r(ret, seed);
  return ret;
}

airRandMTState *
airRandMTStateNix(airRandMTState *state) {
  airFree(state);
  return NULL;
}

void
airSrandMT_r(airRandMTState *rng, unsigned int seed) {
  /* Seed the generator with a simple uint32 */
  _airRandMTInitialize(rng, seed);
  _airRandMTReload(rng);
}

/* Pull a 32-bit integer from the generator state.  Every other access
** function simply transforms the numbers extracted here.
*/
unsigned int
airUIrandMT_r(airRandMTState *rng) {
  register unsigned int s1;

  if (rng->left == 0) {
    _airRandMTReload(rng);
  }
  --rng->left;

  s1 = *rng->pNext++;
  s1 ^= (s1 >> 11);
  s1 ^= (s1 <<  7) & 0x9d2c5680UL;
  s1 ^= (s1 << 15) & 0xefc60000UL;
  return ( s1 ^ (s1 >> 18) );
}

/* This generates the closed interval [0,1] */
double
airDrandMT_r(airRandMTState *rng) {
  double result = airUIrandMT_r(rng);
  return result * (1.0/4294967295.0);
}

/* [0,1) w/ 53 bit precision (capacity of IEEE double precision) */
double
airDrandMT53_r(airRandMTState *rng) {
  unsigned int a, b;
  a = airUIrandMT_r(rng) >> 5;
  b = airUIrandMT_r(rng) >> 6;
  return ( a * 67108864.0 + b ) * (1.0/9007199254740992.0); /* by Isaku Wada */
}

void
airSrandMT(unsigned int seed) {
  airSrandMT_r(airRandMTStateGlobal, seed);
  _airRandMTStateGlobal_initialized = AIR_TRUE;
}

double
airDrandMT() {
  if (!_airRandMTStateGlobal_initialized) {
    airSrandMT(AIR_RANDMT_DEFAULT_SEED);
  }
  return airDrandMT_r(airRandMTStateGlobal);
}

