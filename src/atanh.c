/* mpc_atanh -- inverse hyperbolic tangent of a complex number.

Copyright (C) 2009, 2011, 2012 INRIA

This file is part of GNU MPC.

GNU MPC is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the
Free Software Foundation; either version 3 of the License, or (at your
option) any later version.

GNU MPC is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for
more details.

You should have received a copy of the GNU Lesser General Public License
along with this program. If not, see http://www.gnu.org/licenses/ .
*/

#include "mpc-impl.h"

int
mpc_atanh (mpc_ptr rop, mpc_srcptr op, mpc_rnd_t rnd)
{
  /* atanh(op) = -i*atan(i*op) */
  int inex;
  mpfr_t tmp;
  mpc_t z, a;

  /* We follow the special values from the C standard, Annex G.4.3.3,
     https://www.open-std.org/jtc1/sc22/wg14/www/docs/n3435.pdf,
     in particular atanh(1,0) = (+Inf,0) */

  mpc_realref (z)[0] = mpc_imagref (op)[0];
  mpc_imagref (z)[0] = mpc_realref (op)[0];
  MPFR_CHANGE_SIGN (mpc_realref (z));

  /* Note reversal of precisions due to later multiplication by -i */
  mpc_init3 (a, MPC_PREC_IM(rop), MPC_PREC_RE(rop));

  inex = mpc_atan (a, z,
                   MPC_RND (INV_RND (MPC_RND_IM (rnd)), MPC_RND_RE (rnd)));

  /* change a to -i*a, i.e., x+i*y to y-i*x */
  tmp[0] = mpc_realref (a)[0];
  mpc_realref (a)[0] = mpc_imagref (a)[0];
  mpc_imagref (a)[0] = tmp[0];
  MPFR_CHANGE_SIGN (mpc_imagref (a));

  mpc_set (rop, a, rnd);

  mpc_clear (a);

  return MPC_INEX (MPC_INEX_IM (inex), -MPC_INEX_RE (inex));
}
